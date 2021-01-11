#include "solve_fsm/plugin_solve_fsm.h"

#include "hal_core/netlist/boolean_function.h"
#include "hal_core/netlist/endpoint.h"
#include "hal_core/netlist/gate.h"
#include "hal_core/netlist/gate_library/gate_type.h"
#include "hal_core/netlist/module.h"
#include "hal_core/netlist/net.h"
#include "hal_core/netlist/netlist.h"
#include "hal_core/netlist/persistent/netlist_serializer.h"
#include "hal_core/plugin_system/plugin_manager.h"

#include "z3_utils/include/plugin_z3_utils.h"

#include <iostream>
#include <memory>
#include <queue>
#include <set>
#include <vector>

namespace hal
{
    extern std::unique_ptr<BasePluginInterface> create_plugin_instance()
    {
        return std::make_unique<SolveFsmPlugin>();
    }

    std::string SolveFsmPlugin::get_name() const
    {
        return std::string("solve_fsm");
    }

    std::string SolveFsmPlugin::get_version() const
    {
        return std::string("0.1");
    }

    void SolveFsmPlugin::initialize()
    {
    }

    std::string SolveFsmPlugin::solve_fsm(Netlist* nl, const std::vector<Gate*> state_reg, const std::vector<Gate*> transition_logic, const std::map<Gate*, bool> initial_state, const u32 timeout) {
        // create mapping between (negated) output nets and data input nets of state flip-flops in in order to later replace them.
        const std::map<hal::Net*, hal::Net*> output_net_to_input_net = find_output_net_to_input_net({state_reg.begin(), state_reg.end()});

        z3::context ctx;

        // set timeout for z3 solvers
        z3::params p(ctx);
        p.set("timeout", (unsigned)timeout);
        
        // extract z3 functions for each state flip-flop
        std::map<u32, z3::expr> state_net_to_expr;
        std::map<u32, z3::expr> external_ids_to_expr;

        z3_utils::SubgraphFunctionGenerator g;
        std::vector<Gate*> subgraph_gates = {transition_logic.begin(), transition_logic.end()};

        for (const auto& ff : state_reg) {
            const std::unordered_set<std::string> d_ports = ff->get_type()->get_pins_of_type(GateType::PinType::data);
            if (d_ports.size() != 1) {
                log_error("Fsm solver", "currently not supporting flip-flops with multiple or no data inputs. ({})", d_ports.size());
            }
            const hal::Net* input_net = ff->get_fan_in_net(*d_ports.begin());

            z3::expr r(ctx);
            std::unordered_set<u32> ids;
            g.get_subgraph_z3_function(input_net, subgraph_gates, ctx, r, ids);
            r.simplify();

            // find all external inputs. We define external inputs as nets that are inputs to the transition logic but are not bits from the previous state.
            for (const auto& id : ids) {
                hal::Net* net = nl->get_net_by_id(id);
                if (output_net_to_input_net.find(net) == output_net_to_input_net.end()) {
                    external_ids_to_expr.insert({id, ctx.bv_const(std::to_string(id).c_str(), 1)});
                }
            }

            // in the transition logic expressions of the next state bits we substitue the output nets of the state flip-flops with their (negated) input net.
            for (const auto& [out, in] : output_net_to_input_net) {
                // check wether output net is part of the expression
                if (ids.find(out->get_id()) == ids.end()) {
                    continue;
                }

                z3::expr from = ctx.bv_const(std::to_string(out->get_id()).c_str(), 1);
                z3::expr to   = ctx.bv_const(std::to_string(in->get_id()).c_str(), 1);

                // check for multidriven nets
                if (out->get_sources().size() != 1) {
                    log_error("Fsm solver", "Multidriven nets are not supported! Aborting at Net {}.", out->get_id());
                    return {};
                }

                // negate if the output stems from the negated state output
                const std::string src_pin = out->get_sources().front()->get_pin();
                const std::unordered_set<std::string> neg_state_pins = out->get_sources().front()->get_gate()->get_type()->get_pins_of_type(GateType::PinType::neg_state);
                if (neg_state_pins.find(src_pin) != neg_state_pins.end()) {
                    to = ~to;
                } 

                z3::expr_vector from_vec(ctx);
                z3::expr_vector to_vec(ctx);

                from_vec.push_back(from);
                to_vec.push_back(to);

                r = r.substitute(from_vec, to_vec);
            }

            state_net_to_expr.insert({input_net->get_id(), r});
        }

        // construct bit vector expressions for the previous state and the next state
        z3::expr next_state_vec(ctx);
        z3::expr prev_state_vec(ctx);

        for (const auto& gate : state_reg) {
            // reconstruct input net id
            const std::unordered_set<std::string> d_ports = gate->get_type()->get_pins_of_type(GateType::PinType::data);
            if (d_ports.size() != 1) {
                log_error("Fsm solver", "currently not supporting flip-flops with multiple or no data inputs. ({})", d_ports.size());
            }
            const u32 net_id = gate->get_fan_in_net(*d_ports.begin())->get_id();

            // bitvector representing the previous state
            z3::expr prev_expr = ctx.bv_const((std::to_string(net_id)).c_str(), 1);
            if (prev_state_vec.to_string() == "null") {
                prev_state_vec = prev_expr;
            } else {
                prev_state_vec = z3::concat(prev_state_vec, prev_expr);
            }

            // bitvector including all the functions to calculate the next state
            z3::expr func = state_net_to_expr.at(net_id);
            if (next_state_vec.to_string() == "null") {
                next_state_vec = func;
            } else {
                next_state_vec = z3::concat(next_state_vec, func);
            }
        }

        const u32 state_size = state_net_to_expr.size();
        z3::expr inital_state_expr(ctx);

        // generate initial state
        if (initial_state.empty()) {
            inital_state_expr = ctx.bv_val(0x0, state_size);
        } else {
            z3::expr init(ctx);
            for (const auto& gate : state_reg) {
                z3::expr temp(ctx);
                if (initial_state.find(gate) == initial_state.end()) {
                    log_error("Fsm solver", "Initial state map does not contain value for gate {}. Set to zero.", gate->get_id());
                    temp = ctx.bv_val(0x0, 1);
                } else {
                    temp = ctx.bv_val(initial_state.at(gate), 1);
                }

                if (init.to_string() == "null") {
                    init = temp;
                } else {
                    init = z3::concat(init, temp);
                }
            }
            inital_state_expr = init.simplify();
        }

        // generate all transitions that are reachable from the inital state.
        std::vector<FsmTransition> all_transitions;

        std::deque<z3::expr> q;
        std::vector<z3::expr> current_successors;
        std::vector<z3::expr> next_successors;
        std::unordered_set<std::string> visited;

        current_successors.push_back(inital_state_expr);
        q.push_back(inital_state_expr);

        while (!q.empty()) {
            z3::expr n = q.front();
            q.pop_front();

            if (visited.find(n.to_string()) != visited.end()) {
                continue;
            }
            visited.insert(n.to_string());

            // generate new transitions and add them to the queue
            std::vector<hal::FsmTransition> new_transitions = get_state_successors(prev_state_vec, next_state_vec, n, external_ids_to_expr);
            all_transitions.insert(all_transitions.end(), new_transitions.begin(), new_transitions.end());

            for (const auto& t : new_transitions) {
                q.push_back(t.end_state_expr);
            }
        }

        // in order to safe space when printing the new state transitions we merge transitions with the same start and end state and just update the conditions.
        all_transitions = merge_transitions(all_transitions);

        const std::string table = generate_state_transition_table(nl, all_transitions, external_ids_to_expr);
        const std::string graph = generate_dot_graph(nl, all_transitions);

        return graph;
    }

    std::map<Net*, Net*> SolveFsmPlugin::find_output_net_to_input_net(const std::set<Gate*> state_reg) {
        std::map<Net*, Net*> output_net_to_input_net;
        for (const auto& ff : state_reg) {
            for (const auto& back_net : ff->get_fan_out_nets()) {
                const std::unordered_set<std::string> d_ports = ff->get_type()->get_pins_of_type(GateType::PinType::data);
                if (d_ports.size() != 1) {
                    log_error("Fsm solver", "currently not supporting flip-flops with multiple or no data inputs. ({})", d_ports.size());
                }
                hal::Net* input_net = ff->get_fan_in_net(*d_ports.begin());
                output_net_to_input_net.insert({back_net, input_net});
            }
        }

        return output_net_to_input_net;
    }

    std::vector<FsmTransition> SolveFsmPlugin::get_state_successors(const z3::expr& prev_state_vec, const z3::expr& next_state_vec, const z3::expr& start_state, const std::map<u32, z3::expr>& external_ids_to_expr) {
        std::vector<FsmTransition> successor_transitions;
        
        z3::solver s = z3::solver(prev_state_vec.ctx());

        // initialize the previous state vec with the state currently under inspection
        s.add(prev_state_vec == start_state);

        // find all possible values for the next state vec
        while (true) {
            z3::check_result result = s.check();
            
            if (result == z3::sat) {
                z3::model m = s.get_model();
                z3::expr n  = m.eval(next_state_vec);

                // check wether the next state only depends on the prev state (is numeral) or contains external inputs
                if (n.is_numeral()) {
                    successor_transitions.push_back({start_state, n, {}});
                } else {
                    // if the next state bit is dependent on external inputs we find those and brute force all combinations of inputs.
                    // this could be ommitted if we just want to know the transitions without the conditions attached to them.
                    std::vector<u32> relevant_inputs = get_relevant_external_inputs(n, external_ids_to_expr);
                    for (u64 i = 0; i < pow(2, relevant_inputs.size()); i++) {
                        hal::FsmTransition transition = generate_transition_with_inputs(start_state,n, relevant_inputs, i);
                        successor_transitions.push_back(transition);
                    }
                }
                s.add(next_state_vec != n);
            } else {
                break;
            }
        }

        return successor_transitions;
    }

    std::vector<u32> SolveFsmPlugin::get_relevant_external_inputs(const z3::expr& state, const std::map<u32, z3::expr>& external_ids_to_expr) {
        std::vector<u32> relevant_inputs;
        
        const std::string str = state.to_string();
        for (const auto& [id, _] : external_ids_to_expr) {
            if (str.find("|" + std::to_string(id) + "|") != std::string::npos) {
                relevant_inputs.push_back(id);
            }
        }

        return relevant_inputs;
    }

    FsmTransition SolveFsmPlugin::generate_transition_with_inputs(const z3::expr& start_state, const z3::expr& state, const std::vector<u32>& inputs, const u64 input_values) {
        z3::solver s = z3::solver(state.ctx());

        // generate a mapping from external input net to value and initialize it in the solver.
        std::map<u32, u8> input_id_to_val;
        for (u32 i = 0; i < inputs.size(); i++) {
            u64 val = (input_values >> i) & 0x1;
            z3::expr val_expr = state.ctx().bv_val(val, 1);
            z3::expr id_expr = state.ctx().bv_const(std::to_string(inputs.at(i)).c_str(), 1);

            s.add(id_expr == val_expr);
            input_id_to_val.insert({inputs.at(i), val});
        }

        // evaluate the state reachable under the set input values.
        s.check();
        z3::model m = s.get_model();
        z3::expr eval_state = m.eval(state);

        return {start_state, eval_state, input_id_to_val};
    }

    std::vector<FsmTransition> SolveFsmPlugin::merge_transitions(const std::vector<FsmTransition>& transitions) {
        std::vector<FsmTransition> m_transitions;

        std::set<u32> already_merged;

        for (u32 i = 0; i < transitions.size(); i++) {
            if (already_merged.find(i) != already_merged.end()) {
                continue;
            }

            hal::FsmTransition n_transition = transitions.at(i);
            
            for (u32 j = 0; j < transitions.size(); j++) {
                if (i == j) {
                    continue;
                }

                if (transitions.at(i).starting_state == transitions.at(j).starting_state && transitions.at(i).end_state == transitions.at(j).end_state) {
                    n_transition = n_transition.merge(transitions.at(j));
                    already_merged.insert(j);
                }
            }
            m_transitions.push_back(n_transition);
        }

        log_info("Fsm solver", "Merged transitions. ({} -> {})", transitions.size(), m_transitions.size());

        return m_transitions;
    }

    std::string SolveFsmPlugin::generate_state_transition_table(const Netlist* nl, const std::vector<FsmTransition>& transitions, const std::map<u32, z3::expr>& external_ids_to_expr) {
        std::string header_str = " CURRENT STATE | ";
        for (const auto& [ex_id, _] : external_ids_to_expr) {
            header_str += nl->get_net_by_id(ex_id)->get_name() + " | ";
        }
        header_str += "NEXT STATE";

        std::string body_str;
        for (const auto& t : transitions) {
            if (t.input_ids_to_values.empty()) {
                body_str += std::to_string(t.starting_state) + " | ";
                for (u32 i = 0; i < external_ids_to_expr.size(); i++) {
                        body_str += "X | ";
                }
                body_str += std::to_string(t.end_state) + "\n";
            }


            for (const auto& mapping : t.input_ids_to_values) {
                body_str += std::to_string(t.starting_state) + " | ";
                for (const auto& [ex_id, _] : external_ids_to_expr) {
                    if (mapping.find(ex_id) != mapping.end()) {
                        body_str += std::to_string(mapping.at(ex_id)) + " | ";
                    } else {
                        body_str += "X | ";
                    }
                }
                body_str += std::to_string(t.end_state) + "\n";
            }
        }

        return header_str + "\n" + body_str;
    }

    std::string SolveFsmPlugin::generate_dot_graph(const Netlist* nl, const std::vector<FsmTransition>& transitions) {
        std::string graph_str = "digraph {\n";

        for (const auto& t : transitions) {
            graph_str += t.to_dot_string(nl);
        }

        graph_str += "}";

        return graph_str;
    }

}    // namespace hal
