#include "bitorder_propagation/plugin_bitorder_propagation.h"

#include "boost/functional/hash.hpp"
#include "hal_core/netlist/gate.h"
#include "hal_core/netlist/module.h"

#include <deque>

namespace hal
{

    extern std::unique_ptr<BasePluginInterface> create_plugin_instance()
    {
        return std::make_unique<BitorderPropagationPlugin>();
    }

    std::string BitorderPropagationPlugin::get_name() const
    {
        return std::string("bitorder_propagation");
    }

    std::string BitorderPropagationPlugin::get_version() const
    {
        return std::string("0.1");
    }

    void BitorderPropagationPlugin::initialize()
    {
    }

    namespace
    {
        typedef std::pair<Module*, PinGroup<ModulePin>*> MPG;
        typedef std::map<MPG, std::set<u32>> POSSIBLE_BITINDICES;

        /*
         * This function tries to find a offset between to origins with the help of a previously generated offset matrix.
         * That matrix stores every known offset between two origins.
         * By building a chain of known origin-offset pairs we try to find offsets even for origins that do not share an already known offset.
         * During the chain building we populate the matrix along the way incase we find a valid offset.
         */
        Result<i32> get_offset(MPG& org1, MPG& org2, std::map<MPG, std::map<MPG, i32>>& m, std::set<std::set<MPG>>& v)
        {
            if (v.find({org1, org2}) != v.end())
            {
                return ERR("Already tried to follow that offset.");
            }

            v.insert({org1, org2});

            if (org1 == org2)
            {
                m[org1][org2] = 0;
                return OK(0);
            }

            if (m.find(org1) == m.end())
            {
                return ERR("No valid offset to other origins.");
            }

            if (m.at(org1).find(org2) != m.at(org1).end())
            {
                return OK(m.at(org1).at(org2));
            }

            for (auto& [dst_c, first_proxy_offset] : m.at(org1))
            {
                // dirty workaround to lose the const qualifier
                MPG dst                      = {dst_c.first, dst_c.second};
                auto second_proxy_offset_res = get_offset(dst, org2, m, v);
                if (second_proxy_offset_res.is_error())
                {
                    continue;
                }
                i32 second_proxy_offset = second_proxy_offset_res.get();

                m[org1][org2] = first_proxy_offset + second_proxy_offset;
                return OK(first_proxy_offset + second_proxy_offset);
            }

            return ERR("Not able to find a offset connection.");
        }

        /*
         * This function tries to build an offset matrix that maps each module-pin_group origin to all the other module-pin_group origins that overlap by providing an index for the same net.
         * Since that index maybe different we calulate an offset and check whether that offset is the same for all nets where the two origins overlap.
         * The matrix is populated in a way that the offsetat matrix[org_0][org_1] allow the user to calculate the index_1 = index_0 + offset.
         */
        Result<std::map<MPG, std::map<MPG, i32>>> build_offset_matrix(const std::map<Net*, POSSIBLE_BITINDICES>& reduced_indices)
        {
            // offset at matrix[org_0][org_1] means index_0 + offset = index_1
            std::map<MPG, std::map<MPG, i32>> origin_offset_matrix;

            for (const auto& [net, possible_bitindices] : reduced_indices)
            {
                std::map<MPG, u32> all_possible_indices;

                // fill all possible indices
                for (const auto& [org_mpg, indices] : possible_bitindices)
                {
                    all_possible_indices[org_mpg] = *(indices.begin());
                }

                // check whether all possible indices are just shifted version of each other with a stable offset
                for (const auto& [org_mpg, indices] : possible_bitindices)
                {
                    for (const auto& [already_set_org, already_set_index] : all_possible_indices)
                    {
                        // there does not yet exist an offset between the already set index and the one to be added next
                        if (origin_offset_matrix[org_mpg].find(already_set_org) == origin_offset_matrix[org_mpg].end())
                        {
                            i32 new_index = *indices.begin();
                            i32 offset    = already_set_index - new_index;

                            origin_offset_matrix[org_mpg][already_set_org] = offset;
                            origin_offset_matrix[already_set_org][org_mpg] = -offset;
                        }
                        // check wether the already existing offset leads to the same index
                        else
                        {
                            i32 new_index = *indices.begin();
                            i32 offset    = origin_offset_matrix.at(org_mpg).at(already_set_org);

                            if (new_index + offset != i32(already_set_index))
                            {
                                return ERR("unable to build offset matrix: failed to find valid offset between " + std::to_string(org_mpg.first->get_id()) + "-" + org_mpg.second->get_name() + " and "
                                           + std::to_string(already_set_org.first->get_id()) + "-" + already_set_org.second->get_name());
                            }
                        }
                    }
                }
            }

            return OK(origin_offset_matrix);
        }

        /*
         * This function checks whether a net is a output/input pin of a module and incase it is checks whether the pin group that it leads to has an already known bit order and returns the index. 
         */
        Result<std::pair<MPG, u32>> gather_bit_index_from_origin(Net* n, Module* m, const std::map<MPG, std::map<Net*, u32>>& wellformed_module_pin_groups, const bool successors)
        {
            bool is_border_pin = successors ? m->is_input_net(n) : m->is_output_net(n);
            if (is_border_pin)
            {
                auto border_pin = m->get_pin_by_net(n);
                if (border_pin == nullptr)
                {
                    return ERR("cannot get bit index information for net with ID " + std::to_string(n->get_id()) + " from module with ID " + std::to_string(m->get_id())
                               + ": net is border net but does not have a pin.");
                }
                auto pg = border_pin->get_group().first;

                if (auto it = wellformed_module_pin_groups.find({m, pg}); it != wellformed_module_pin_groups.end())
                {
                    auto wellformed_bitorder = it->second;
                    if (auto bitorder_it = wellformed_bitorder.find(n); bitorder_it != wellformed_bitorder.end())
                    {
                        return OK({{m, pg}, bitorder_it->second});
                    }
                }
            }

            return OK({{nullptr, nullptr}, 0});
        }

        /*
         * This function gathers bit index information for a net by propagating to the neighboring gates and searches for module pin groups with already known pin groups.
         */
        Result<POSSIBLE_BITINDICES> gather_bit_indices_for_net(Net* n,
                                                               Module* module_border,
                                                               const std::map<MPG, std::map<Net*, u32>>& wellformed_module_pin_groups,
                                                               std::unordered_set<Gate*>& visited,
                                                               bool successors = true)
        {
            POSSIBLE_BITINDICES origin_to_bit_indices;

            const bool print = false;

            if (print)
            {
                std::cout << "Gathering bit index for net " << n->get_id() << " with current module border " << (module_border ? std::to_string(module_border->get_id()) : "null")
                          << " in direction: " << (successors ? "forwards" : "backwards") << std::endl;
            }

            // check whether the net is a global input or global output net (has no sources or destinations, but might have a bitorder annotated at the top module)
            if ((successors && n->is_global_output_net()) || (!successors && n->is_global_input_net()))
            {
                auto top_module = n->get_netlist()->get_top_module();
                const auto res  = gather_bit_index_from_origin(n, top_module, wellformed_module_pin_groups, !successors);
                if (res.is_error())
                {
                    return ERR_APPEND(res.get_error(),
                                      "cannot gather bit indices for net with ID " + std::to_string(n->get_id()) + " failed to gather index at module with ID " + std::to_string(top_module->get_id()));
                }
                const auto [mpg, index] = res.get();

                // check whether we actually found a new index
                if (mpg.first != nullptr)
                {
                    origin_to_bit_indices[mpg].insert(index);
                }

                return OK(origin_to_bit_indices);
            }

            const auto neighbors = successors ? n->get_destinations() : n->get_sources();
            for (const auto& ep : neighbors)
            {
                Gate* g = ep->get_gate();

                if (g == nullptr)
                {
                    continue;
                }

                if (print)
                {
                    std::cout << "Checking gate " << g->get_id() << std::endl;
                }

                if (visited.find(g) != visited.end())
                {
                    continue;
                }
                visited.insert(g);

                Module* found_module = g->get_module();

                // check whether we left a previously entered module
                if (!module_border->contains_gate(g, true))
                {
                    if (print)
                    {
                        std::cout << "Encountered gate " << g->get_id() << " that was not part of the module border." << std::endl;
                    }

                    const auto res = gather_bit_index_from_origin(n, module_border, wellformed_module_pin_groups, !successors);
                    if (res.is_error())
                    {
                        return ERR_APPEND(res.get_error(),
                                          "cannot gather bit indices for net with ID " + std::to_string(n->get_id()) + " failed to gather index at module with ID "
                                              + std::to_string(module_border->get_id()));
                    }
                    const auto [mpg, index] = res.get();

                    // check whether we actually found a new index
                    if (mpg.first != nullptr)
                    {
                        if (print)
                        {
                            std::cout << "Found index " << index << " at module " << mpg.first->get_id() << " and pin group " << mpg.second->get_name() << std::endl;
                        }
                        origin_to_bit_indices[mpg].insert(index);
                    }

                    continue;
                }

                // reached another module that is not the module we are currently in
                if (found_module != module_border)
                {
                    if (print)
                    {
                        std::cout << "Found new module  " << found_module->get_id() << std::endl;
                    }

                    const auto res = gather_bit_index_from_origin(n, found_module, wellformed_module_pin_groups, successors);
                    if (res.is_error())
                    {
                        return ERR_APPEND(res.get_error(),
                                          "cannot gather bit indices for net with ID " + std::to_string(n->get_id()) + " failed to gather index at module with ID "
                                              + std::to_string(found_module->get_id()));
                    }
                    const auto [mpg, index] = res.get();

                    // check whether we actually found a new index
                    if (mpg.first != nullptr)
                    {
                        if (print)
                        {
                            std::cout << "Found index " << index << " at module " << mpg.first->get_id() << " and pin group " << mpg.second->get_name() << std::endl;
                        }
                        origin_to_bit_indices[mpg].insert(index);
                        continue;
                    }

                    // only stop propagation at modules with no submodules
                    if (found_module->get_submodules().size() == 0)
                    {
                        continue;
                    }
                }

                // propagate and stop at gates that have unsupported gates types
                std::vector<Net*> next_nets;
                if (g->get_type()->has_property(GateTypeProperty::combinational))
                {
                    next_nets = successors ? g->get_fan_out_nets() : g->get_fan_in_nets();
                }
                else if (g->get_type()->has_property(GateTypeProperty::sequential) && (g->get_type()->has_property(GateTypeProperty::ff) || g->get_type()->has_property(GateTypeProperty::latch)))
                {
                    for (const auto& next_ep : successors ? g->get_fan_out_endpoints() : g->get_fan_in_endpoints())
                    {
                        const GatePin* pin = next_ep->get_pin();
                        if (PinType t = pin->get_type(); t == PinType::data || t == PinType::state || t == PinType::neg_state)
                        {
                            next_nets.push_back(next_ep->get_net());
                        }
                    }
                }

                std::unordered_set<Gate*> new_visited = visited;

                for (Net* next_n : next_nets)
                {
                    auto res = gather_bit_indices_for_net(next_n, found_module, wellformed_module_pin_groups, new_visited, successors);
                    if (res.is_error())
                    {
                        return res;
                    }

                    for (auto& [org_mpg, possible_indices] : res.get())
                    {
                        origin_to_bit_indices[org_mpg].insert(possible_indices.begin(), possible_indices.end());
                    }
                }
            }

            return OK(origin_to_bit_indices);
        }

        /*
         * This function tries to extract valid bit orders from the bit index information that was gathered during the propagation step.
         * First conflicting information is deleted, second offsets between different information origins are calculated and lastly the resulting bitorder is validated in terms of continuity and completeness.
         * The Validation strictness can be tweaked with the parameter 'only_allow_consecutive_bitorders'.
         */
        std::map<MPG, std::map<Net*, u32>> extract_well_formed_bitorder(const std::map<MPG, std::map<Net*, POSSIBLE_BITINDICES>>& collected_bitindices, bool only_allow_consecutive_bitorders = true)
        {
            std::map<MPG, std::map<Net*, u32>> wellformed_pin_groups;

            bool print = true;

            for (auto& [mpg, net_to_possible_bitindices] : collected_bitindices)
            {
                auto m  = mpg.first;
                auto pg = mpg.second;

                // ############################################### //
                // ############### CONFLICT FINDING ############## //
                // ############################################### //

                if (print)
                {
                    std::cout << "Consens finding for Module " << m->get_name() << " with id " << m->get_id() << " and pingroup " << pg->get_name() << ": " << std::endl;
                    std::cout << "\tPossible Indices for Pin group " << pg->get_name() << ": " << std::endl;
                    for (const auto& [net, possible_bitindices] : net_to_possible_bitindices)
                    {
                        std::cout << "\t\tNet " << net->get_id() << ": " << std::endl;
                        u32 origins = 0;
                        for (const auto& [org_mpg, indices] : possible_bitindices)
                        {
                            auto org_m  = org_mpg.first;
                            auto org_pg = org_mpg.second;

                            std::cout << "\t\t\t" << org_m->get_id() << "-" << org_pg->get_name() << ": [";
                            for (const auto& index : indices)
                            {
                                std::cout << index << ", ";
                            }
                            std::cout << "]" << std::endl;
                            origins += 1;
                        }

                        std::cout << "\t\tORIGINS: [" << origins << "]" << std::endl;
                    }
                }

                // delete non-valid possible indices
                // indices are considered non valid when:
                //  - a pingroup annotates different indices for the same pin
                //  - a pingroup annotates the same index to different pins

                // 1)  Checks whether a net has multiple indices annotated from the same origin mpg
                auto reduced_collected_indices = collected_bitindices.at(mpg);

                for (auto& [net, possible_bitindices] : collected_bitindices.at(mpg))
                {
                    for (auto& [org_mpg, indices] : possible_bitindices)
                    {
                        if (indices.size() != 1)
                        {
                            reduced_collected_indices.at(net).erase(org_mpg);
                        }
                    }

                    if (reduced_collected_indices.at(net).empty())
                    {
                        reduced_collected_indices.erase(net);
                    }
                }

                if (reduced_collected_indices.empty())
                {
                    continue;
                }

                // 2) Checks whether the mpg has annotated the same index to different nets
                std::set<std::pair<MPG, u32>> origin_indices;
                std::set<std::pair<MPG, u32>> origin_indices_to_remove;

                for (auto& [net, possible_bitindices] : reduced_collected_indices)
                {
                    for (auto& [org_mpg, indices] : possible_bitindices)
                    {
                        u32 index = *(indices.begin());
                        if (origin_indices.find({org_mpg, index}) != origin_indices.end())
                        {
                            origin_indices_to_remove.insert({org_mpg, index});
                        }
                        else
                        {
                            origin_indices.insert({org_mpg, index});
                        }
                    }
                }

                if (print)
                {
                    for (const auto& [org_mpg, index] : origin_indices_to_remove)
                    {
                        std::cout << "Found org " << org_mpg.first->get_id() << "-" << org_mpg.second->get_name() << " index " << index << " pair to remove!" << std::endl;
                    }
                }

                auto further_reduced_collected_indices = reduced_collected_indices;
                for (auto& [net, possible_bitindices] : reduced_collected_indices)
                {
                    for (auto& [org_mpg, indices] : possible_bitindices)
                    {
                        u32 index = *(indices.begin());
                        if (origin_indices_to_remove.find({org_mpg, index}) != origin_indices_to_remove.end())
                        {
                            further_reduced_collected_indices.at(net).erase(org_mpg);
                        }
                    }

                    if (further_reduced_collected_indices.at(net).empty())
                    {
                        further_reduced_collected_indices.erase(net);
                    }
                }

                if (further_reduced_collected_indices.empty())
                {
                    continue;
                }

                // TODO remove debug printing
                if (print)
                {
                    std::cout << "\tReduced Possible Indices for Module " << m->get_name() << " with id " << m->get_id() << " and pingroup " << pg->get_name() << ": " << std::endl;
                    for (const auto& [net, possible_bitindices] : net_to_possible_bitindices)
                    {
                        std::cout << "\t\tNet " << net->get_id() << ": " << std::endl;
                        u32 origins = 0;
                        for (const auto& [org_mpg, indices] : possible_bitindices)
                        {
                            auto org_m  = org_mpg.first;
                            auto org_pg = org_mpg.second;

                            std::cout << "\t\t\t" << org_m->get_id() << "-" << org_pg->get_name() << ": [";
                            for (const auto& index : indices)
                            {
                                std::cout << index << ", ";
                            }
                            std::cout << "]" << std::endl;
                        }
                    }
                }
                // End debug printing

                // ######################################################## //
                // ############### CONSENS FINDING - OFFSET ############### //
                // ######################################################## //

                // try to find a consens between the different possible indices
                std::map<Net*, i32> consens_bitindices;

                auto offset_matrix_res = build_offset_matrix(further_reduced_collected_indices);
                if (offset_matrix_res.is_error())
                {
                    if (print)
                    {
                        std::cout << "Failed to build offset matrix : " << offset_matrix_res.get_error().get() << std::endl;
                    }
                    continue;
                }
                auto offset_matrix = offset_matrix_res.get();

                auto base_line = offset_matrix.begin()->first;

                // TODO remove
                if (print)
                {
                    std::cout << "Found valid offsets pingroup " << pg->get_name() << ": " << std::endl;
                    std::cout << "Baseline: " << base_line.first->get_id() << "-" << base_line.second->get_name() << std::endl;
                    for (const auto& [org1, col] : offset_matrix)
                    {
                        std::cout << org1.first->get_id() << "-" << org1.second->get_name() << ": ";
                        for (const auto& [org2, offset] : col)
                        {
                            std::cout << org2.first->get_id() << "-" << org2.second->get_name() << "[" << offset << "] ";
                        }
                        std::cout << std::endl;
                    }
                }

                for (const auto& [net, possible_bitindices] : further_reduced_collected_indices)
                {
                    // pair of first possible org_mod and org_pin_group
                    MPG org = possible_bitindices.begin()->first;
                    // index at first possible origin
                    i32 org_index = *(possible_bitindices.begin()->second.begin());
                    std::set<std::set<MPG>> v;
                    auto offset_res = get_offset(org, base_line, offset_matrix, v);
                    if (offset_res.is_error())
                    {
                        if (possible_bitindices.size() == 1)
                        {
                            // if there cannot be found any valid offset to the baseline, but there is just one possible index annotated, we still allow it
                            // -> this wont break anything, since this only allows for bitorders that we otherwise would have discarded because of a missing net
                            consens_bitindices[net] = org_index;
                        }
                        else
                        {
                            // TODO remove
                            // std::cout << "Cannot find connection from origin " << org.first->get_id() << "-" << org.second->get_name() << " to baseline!" << std::endl;
                            break;
                        }
                    }
                    else
                    {
                        i32 offset              = offset_res.get();
                        consens_bitindices[net] = org_index + offset;
                        //std::cout << "Org Index: " << org_index << " Offset: " << offset << std::endl;
                    }
                }

                if (print)
                {
                    std::cout << "Found offset bitorder for pingroup " << pg->get_name() << ": " << std::endl;
                    for (const auto& [net, index] : consens_bitindices)
                    {
                        std::cout << net->get_id() << ": " << index << std::endl;
                    }
                }

                // ############################################################## //
                // ############### CONSENS FINDING - COMPLETENESS ############### //
                // ############################################################## //

                bool complete_pin_group_bitorder = true;
                std::map<Net*, i32> complete_consens;

                for (auto& pin : pg->get_pins())
                {
                    Net* net = pin->get_net();
                    // Currently also ignoring power/gnd nets but a more optimal approach would be to optimize them away where they are not needed (but we only got LUT4)
                    // -> maybe not, we would destroy 16 bit muxes if the top most MUX
                    if (net->is_gnd_net() || net->is_vcc_net())
                    {
                        continue;
                    }

                    if (consens_bitindices.find(net) == consens_bitindices.end())
                    {
                        complete_pin_group_bitorder = false;

                        // TODO remove
                        // std::cout << "Missing in net " << in_net->get_id() << " for complete bitorder." << std::endl;

                        break;
                    }
                    else
                    {
                        complete_consens[net] = consens_bitindices.at(net);
                    }
                }

                if (!complete_pin_group_bitorder)
                {
                    continue;
                }

                // TODO remove
                if (print)
                {
                    std::cout << "Found complete bitorder for pingroup " << pg->get_name() << std::endl;
                    for (const auto& [net, index] : complete_consens)
                    {
                        std::cout << net->get_id() << ": " << index << std::endl;
                    }
                }

                // ########################################################### //
                // ############### CONSENS FINDING - ALIGNMENT ############### //
                // ########################################################### //

                std::map<Net*, u32> aligned_consens;

                // align consens from m:m+n to 0:n
                i32 max_index = 0x80000000;
                i32 min_index = 0x7fffffff;
                std::set<i32> unique_indices;
                for (const auto& [_n, index] : complete_consens)
                {
                    unique_indices.insert(index);

                    if (index > max_index)
                    {
                        max_index = index;
                    }

                    if (index < min_index)
                    {
                        min_index = index;
                    }
                }

                // when the range is larger than pin group size there are holes in the bitorder
                if (only_allow_consecutive_bitorders && ((max_index - min_index) > (i32(complete_consens.size()) - 1)))
                {
                    continue;
                }

                // when there are less unique indices in the range than nets, there are duplicates
                if (unique_indices.size() < complete_consens.size())
                {
                    continue;
                }

                std::map<i32, Net*> index_to_net;
                for (const auto& [net, index] : complete_consens)
                {
                    index_to_net[index] = net;
                }

                u32 index_counter = 0;
                for (const auto& [_unaligned_index, net] : index_to_net)
                {
                    aligned_consens[net] = index_counter++;
                }

                // TODO remove
                if (print)
                {
                    std::cout << "Found valid input bitorder for pingroup " << pg->get_name() << std::endl;
                    for (const auto& [net, index] : aligned_consens)
                    {
                        std::cout << net->get_id() << ": " << index << std::endl;
                    }
                }

                wellformed_pin_groups[mpg] = aligned_consens;
            }

            return wellformed_pin_groups;
        }

    }    // namespace

    Result<std::map<MPG, std::map<Net*, u32>>> BitorderPropagationPlugin::propagate_module_pingroup_bitorder(const std::map<MPG, std::map<Net*, u32>>& known_bitorders,
                                                                                                             const std::set<MPG>& unknown_bitorders,
                                                                                                             const bool strict_consens_finding)
    {
        std::map<MPG, std::map<Net*, u32>> wellformed_module_pin_groups = known_bitorders;
        std::map<MPG, std::map<Net*, POSSIBLE_BITINDICES>> previous_collected_bitindices;

        u32 iteration_ctr = 0;

        //const auto nl   = known_bitorders.begin()->first.first->get_netlist();
        //auto top_module = nl->get_top_module();

        // TODO remove
        // std::cout << "Searching bitorders for: " << std::endl;
        // for (const auto& [m, pin_groups] : unknown_bitorders)
        // {
        //     std::cout << "Module " << m->get_id() << ": " << std::endl;
        //     for (const auto& pin_group : pin_groups)
        //     {
        //         std::cout << pin_group->get_name() << std::endl;
        //     }
        //     std::cout << std::endl;
        // }

        while (true)
        {
            // find modules that are neither blocked nor are they already wellformed
            std::vector<MPG> modules_and_pingroup;
            for (const auto& mpg : unknown_bitorders)
            {
                if (mpg.first->is_top_module())
                {
                    log_error("iphone_tools", "Top module is part of the unknown bitorders!");
                    continue;
                }

                // NOTE We can skip module/pin group pairs that are already wellformed
                if (wellformed_module_pin_groups.find(mpg) == wellformed_module_pin_groups.end())
                {
                    modules_and_pingroup.push_back(mpg);
                }
            };

            std::map<MPG, std::map<Net*, POSSIBLE_BITINDICES>> collected_bitindices;

            std::deque<MPG> q = {modules_and_pingroup.begin(), modules_and_pingroup.end()};

            // std::cout << "Searching bitorders in Iteration " << iteration_ctr << " for: " << std::endl;
            // for (const auto& [m, pin_groups] : unknown_bitorders)
            // {
            //     std::cout << "Module " << m->get_id() << ": " << std::endl;
            //     for (const auto& pin_group : pin_groups)
            //     {
            //         std::cout << pin_group->get_name() << std::endl;
            //     }
            //     std::cout << std::endl;
            // }

            if (q.empty())
            {
                break;
            }

            log_info("iphone_tools", "Starting {}bitorder propagation iteration {}.", (strict_consens_finding ? "strict " : ""), iteration_ctr);

            // std::cout << "Blocklist: " << std::endl;

            // std::set<u32> blocked_ids;
            // for (const auto& g : gate_block_list)
            // {
            //     blocked_ids.insert(g->get_id());
            // }
            // for (const auto& id : blocked_ids)
            // {
            //     std::cout << id << ", ";
            // }
            // std::cout << std::endl;

            // Todo remove
            // for (const auto& [m, pin_group_to_bitorder] : wellformed_module_pin_groups)
            // {
            //     std::cout << "Bitorder for module " << m->get_id() << " in iteration " << iteration_ctr << ": " << std::endl;
            //     for (const auto& [pin_group, bitorder] : pin_group_to_bitorder)
            //     {
            //         std::cout << pin_group->get_name() << ": " << std::endl;
            //         for (const auto& [net, index] : bitorder)
            //         {
            //             std::cout << "Net " << net->get_id() << ": " << index << std::endl;
            //         }
            //     }
            // }

            while (!q.empty())
            {
                auto [m, pg] = q.front();
                q.pop_front();

                // TODO REMOVE
                // if (m != nullptr)
                // {
                //     std::cout << "Propagate for module " << m->get_id() << " and pingroup " << pin_group->get_name() << std::endl;
                // }
                // else
                // {
                //     std::cout << "Found nullptr in module/pin_group q!" << std::endl;
                //     std::cout << "Pingroup: " << pin_group->get_name() << std::endl;
                // }

                // check wether m has submodules that are in the q
                bool no_submodules_in_q = true;
                for (const auto& sub_m : m->get_submodules(nullptr, true))
                {
                    for (const auto& [sm, sp] : q)
                    {
                        if (sm == sub_m)
                        {
                            no_submodules_in_q = false;
                            break;
                        }
                    }
                }

                if (!no_submodules_in_q)
                {
                    q.push_back({m, pg});
                    continue;
                }

                bool successors = pg->get_direction() == PinDirection::output;

                for (const auto& pin : pg->get_pins())
                {
                    Net* starting_net = pin->get_net();

                    std::unordered_set<Gate*> visited_outwards;
                    const auto res_outwards = gather_bit_indices_for_net(starting_net, m->get_parent_module(), wellformed_module_pin_groups, visited_outwards, successors);
                    if (res_outwards.is_error())
                    {
                        return ERR_APPEND(res_outwards.get_error(),
                                          "cannot porpagate bitorder: failed to gather bit indices outwards starting from the module with ID " + std::to_string(m->get_id()) + " and pin group "
                                              + pg->get_name());
                    }
                    const auto indices_outwards = res_outwards.get();
                    collected_bitindices[{m, pg}][starting_net].insert(indices_outwards.begin(), indices_outwards.end());

                    std::unordered_set<Gate*> visited_inwards;
                    const auto res_inwards = gather_bit_indices_for_net(starting_net, m, wellformed_module_pin_groups, visited_inwards, !successors);
                    if (res_inwards.is_error())
                    {
                        return ERR_APPEND(res_inwards.get_error(),
                                          "cannot porpagate bitorder: failed to gather bit indices inwwards starting from the module with ID " + std::to_string(m->get_id()) + " and pin group "
                                              + pg->get_name());
                    }
                    const auto indices_inwards = res_inwards.get();
                    collected_bitindices[{m, pg}][starting_net].insert(indices_inwards.begin(), indices_inwards.end());
                }

                // TODO remove debug printing
                // std::cout << "Neighbour indices for module " << m->get_id() << " and pingroup " << pin_group->get_name() << " : " << std::endl;
                // for (const auto& [net, possible_bitindices] : bit_indices_backwards)
                // {
                //     std::cout << "IN Net " << net->get_id() << ": ";
                //     for (const auto& [org_mod, org_pin_group_to_indices] : possible_bitindices)
                //     {
                //         for (const auto& [org_pin_group, indices] : org_pin_group_to_indices)
                //         {
                //             std::cout << org_mod->get_id() << "(" << org_pin_group->get_name() << ")" << " [" << (indices.size() == 1 ? std::to_string(*indices.begin()) : "CONFLICT") << "], ";
                //             // std::cout << org_mod->get_id() << " / " << org_mod->get_name() << "(" << org_pin_group->get_name() << ")" << " [" << (indices.size() == 1 ? std::to_string(*indices.begin()) : "CONFLICT") << "], ";
                //         }
                //     }
                //     std::cout << std::endl;
                // }
                // std::cout << std::endl;

                // std::cout << "Submodule indices for module " << m->get_id() << " and pingroup " << pin_group->get_name() << ": " << std::endl;
                // for (const auto& [net, possible_bitindices] : bit_indices_inwards_in)
                // {
                //     std::cout << "IN Net " << net->get_id() << ": ";
                //     for (const auto& [org_mod, org_pin_group_to_indices] : possible_bitindices)
                //     {
                //         for (const auto& [org_pin_group, indices] : org_pin_group_to_indices)
                //         {
                //             std::cout << org_mod->get_id()   << "(" << org_pin_group->get_name() << ")" << " [" << (indices.size() == 1 ? std::to_string(*indices.begin()) : "CONFLICT") << "], ";
                //             //std::cout << org_mod->get_id() << " / " << org_mod->get_name() << "(" << org_pin_group->get_name() << ")" << " [" << (indices.size() == 1 ? std::to_string(*indices.begin()) : "CONFLICT") << "], ";
                //         }
                //     }
                //     std::cout << std::endl;
                // }
                // std::cout << std::endl;
                // END debug printing
            }

            const auto newly_wellformed_module_pin_groups = extract_well_formed_bitorder(collected_bitindices, strict_consens_finding);

            for (const auto& [m, pin_group_to_bitorder] : newly_wellformed_module_pin_groups)
            {
                wellformed_module_pin_groups[m].insert(pin_group_to_bitorder.begin(), pin_group_to_bitorder.end());
            }

            // TODO remove debug printing
            // for (const auto& [m, pg_to_bitorder] : newly_wellformed_module_pin_groups)
            // {
            //     std::cout << "Newly found consens for module " << m->get_id() << std::endl;
            //     for (const auto& [pg, bitorder] : pg_to_bitorder)
            //     {
            //         std::cout << pg->get_name() << ": " << std::endl;
            //         for (const auto& [n, index] : bitorder)
            //         {
            //             std::cout << n->get_id()  << ": " << index << std::endl;
            //         }
            //     }
            // }

            // for (const auto& [m, pg_to_bitorder] : wellformed_module_pin_groups)
            // {
            //     std::cout << "Overall found consens for module " << m->get_id() << std::endl;
            //     for (const auto& [pg, bitorder] : pg_to_bitorder)
            //     {
            //         std::cout << pg->get_name() << ": " << std::endl;
            //         for (const auto& [n, index] : bitorder)
            //         {
            //             std::cout << n->get_id() << ": " << index << std::endl;
            //         }
            //     }
            // }
            // END DEBUG PRINTING

            if (previous_collected_bitindices == collected_bitindices)
            {
                break;
            }

            // NOTE could think about merging if we find that information is lost between iteration
            previous_collected_bitindices = collected_bitindices;

            iteration_ctr++;
        }

        u32 wellformed_pingroups_counter = 0;
        for (const auto& [_m, wellformed_pingroups] : wellformed_module_pin_groups)
        {
            wellformed_pingroups_counter += wellformed_pingroups.size();
        }

        log_info("iphone_tools", "Found a valid bitorder for {} pingroups.", wellformed_pingroups_counter);

        return OK(wellformed_module_pin_groups);
    }

    Result<std::monostate> BitorderPropagationPlugin::reorder_module_pin_groups(const std::map<MPG, std::map<Net*, u32>>& ordered_module_pin_groups)
    {
        // reorder pin groups to match found bitorders
        for (const auto& [mpg, bitorder] : ordered_module_pin_groups)
        {
            auto m  = mpg.first;
            auto pg = mpg.second;

            std::map<u32, ModulePin*> index_to_pin;

            for (const auto& [net, index] : bitorder)
            {
                ModulePin* pin = m->get_pin_by_net(net);
                if (pin != nullptr)
                {
                    auto [current_pin_group, _old_index] = pin->get_group();
                    if (pg == current_pin_group)
                    {
                        index_to_pin[index] = pin;
                    }
                    else
                    {
                        return ERR("cannot reorder module pin groups: pin " + pin->get_name() + " appears in bit order of pin group " + pg->get_name() + " for module with ID "
                                   + std::to_string(m->get_id()) + " but belongs to pin group " + current_pin_group->get_name());
                    }
                }
            }

            for (const auto& [index, pin] : index_to_pin)
            {
                auto move_res = m->move_pin_within_group(pg, pin, index);
                if (move_res.is_error())
                {
                    return ERR_APPEND(move_res.get_error(),
                                      "cannot reorder module pin groups: failed to move pin " + pin->get_name() + " in pin group " + pg->get_name() + " of module with ID "
                                          + std::to_string(m->get_id()) + " to new index " + std::to_string(index));
                }

                const auto pin_name = pg->get_name() + "(" + std::to_string(index) + ")";
                if (auto collision_pins = m->get_pins([pin_name](const ModulePin* pin) { return pin->get_name() == pin_name; }); !collision_pins.empty())
                {
                    m->set_pin_name(collision_pins.front(), pin_name + "_OLD");
                }

                m->set_pin_name(pin, pin_name);
            }
        }

        return OK({});
    }

    Result<bool> BitorderPropagationPlugin::propagate_bitorder(Netlist* nl, const std::pair<u32, std::string>& src, const std::pair<u32, std::string>& dst)
    {
        const std::vector<std::pair<u32, std::string>> src_vec = {src};
        const std::vector<std::pair<u32, std::string>> dst_vec = {dst};
        return propagate_bitorder(nl, src_vec, dst_vec);
    }

    Result<bool> BitorderPropagationPlugin::propagate_bitorder(const std::pair<Module*, PinGroup<ModulePin>*>& src, const std::pair<Module*, PinGroup<ModulePin>*>& dst)
    {
        const std::vector<std::pair<Module*, PinGroup<ModulePin>*>> src_vec = {src};
        const std::vector<std::pair<Module*, PinGroup<ModulePin>*>> dst_vec = {dst};
        return propagate_bitorder(src_vec, dst_vec);
    }

    Result<bool> BitorderPropagationPlugin::propagate_bitorder(Netlist* nl, const std::vector<std::pair<u32, std::string>>& src, const std::vector<std::pair<u32, std::string>>& dst)
    {
        std::vector<std::pair<Module*, PinGroup<ModulePin>*>> internal_src;
        std::vector<std::pair<Module*, PinGroup<ModulePin>*>> internal_dst;

        for (const auto& [mod_id, pg_name] : src)
        {
            auto src_mod = nl->get_module_by_id(mod_id);
            if (src_mod == nullptr)
            {
                return ERR("Cannot propagate bitorder: failed to find a module with id " + std::to_string(mod_id));
            }

            PinGroup<ModulePin>* src_pin_group = nullptr;
            for (const auto& pin_group : src_mod->get_pin_groups())
            {
                if (pin_group->get_name() == pg_name)
                {
                    // Check wether there are multiple pin groups with the same name
                    if (src_pin_group != nullptr)
                    {
                        return ERR("Cannot propagate bitorder: found multiple pin groups with name " + pg_name + " at module with ID " + std::to_string(mod_id));
                    }

                    src_pin_group = pin_group;
                }
            }

            if (src_pin_group == nullptr)
            {
                return ERR("Cannot propagate bitorder: failed to find a pin group with the name " + pg_name + " at module with ID " + std::to_string(mod_id));
            }

            internal_src.push_back({src_mod, src_pin_group});
        }

        for (const auto& [mod_id, pg_name] : dst)
        {
            auto src_mod = nl->get_module_by_id(mod_id);
            if (src_mod == nullptr)
            {
                return ERR("Cannot propagate bitorder: failed to find a module with id " + std::to_string(mod_id));
            }

            PinGroup<ModulePin>* src_pin_group = nullptr;
            for (const auto& pin_group : src_mod->get_pin_groups())
            {
                if (pin_group->get_name() == pg_name)
                {
                    // Check wether there are multiple pin groups with the same name
                    if (src_pin_group != nullptr)
                    {
                        return ERR("Cannot propagate bitorder: found multiple pin groups with name " + pg_name + " at module with ID " + std::to_string(mod_id));
                    }

                    src_pin_group = pin_group;
                }
            }

            if (src_pin_group == nullptr)
            {
                return ERR("Cannot propagate bitorder: failed to find a pin group with the name " + pg_name + " at module with ID " + std::to_string(mod_id));
            }

            internal_dst.push_back({src_mod, src_pin_group});
        }

        return propagate_bitorder(internal_src, internal_dst);
    }

    Result<bool> BitorderPropagationPlugin::propagate_bitorder(const std::vector<std::pair<Module*, PinGroup<ModulePin>*>>& src, const std::vector<std::pair<Module*, PinGroup<ModulePin>*>>& dst)
    {
        std::map<MPG, std::map<Net*, u32>> known_bitorders;
        std::set<MPG> unknown_bitorders = {dst.begin(), dst.end()};

        for (auto& [m, pg] : src)
        {
            std::map<Net*, u32> src_bitorder;
            for (u32 index = 0; index < pg->get_pins().size(); index++)
            {
                auto pin_res = pg->get_pin_at_index(index);
                if (pin_res.is_error())
                {
                    return ERR_APPEND(pin_res.get_error(), "cannot propagate bitorder: failed to get pin at index " + std::to_string(index) + " inside of pin group " + pg->get_name());
                }
                const ModulePin* pin = pin_res.get();

                src_bitorder.insert({pin->get_net(), index});
            }

            known_bitorders.insert({{m, pg}, src_bitorder});
        }

        const auto res = propagate_module_pingroup_bitorder(known_bitorders, unknown_bitorders);
        if (res.is_error())
        {
            return ERR_APPEND(res.get_error(), "cannot propagate bitorder: failed propagation");
        }

        const auto all_wellformed_module_pin_groups = res.get();

        reorder_module_pin_groups(all_wellformed_module_pin_groups);

        u32 all_wellformed_bitorders_count = 0;
        for (const auto& [mpg, bitorder] : all_wellformed_module_pin_groups)
        {
            auto m  = mpg.first;
            auto pg = mpg.second;

            std::cout << "Module: " << m->get_id() << " / " << m->get_name() << ": " << std::endl;
            std::cout << "Pingroup: " << pg->get_name() << ": " << std::endl;

            for (const auto& [net, index] : bitorder)
            {
                std::cout << net->get_id() << ": " << index << std::endl;
            }
            all_wellformed_bitorders_count++;
        }

        const u32 new_bit_order_count = all_wellformed_bitorders_count - src.size();

        log_info("bitorder_propagation", "With {} known bitorder, {} unknown bitorders got reconstructed.", src.size(), new_bit_order_count);
        log_info("bitorder_propagation", "{} / {} = {} of all unknown bitorders.", new_bit_order_count, dst.size(), double(new_bit_order_count) / double(dst.size()));
        log_info("bitorder_propagation",
                 "{} / {} = {} of all pin group bitorders.",
                 all_wellformed_bitorders_count,
                 dst.size() + src.size(),
                 double(all_wellformed_bitorders_count) / double(dst.size() + src.size()));

        return OK(new_bit_order_count > 0);
    }
}    // namespace hal
