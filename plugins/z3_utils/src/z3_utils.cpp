#include "z3_utils/z3_utils.h"

#include "hal_core/netlist/decorators/boolean_function_net_decorator.h"
#include "hal_core/utilities/log.h"
#include "z3_api.h"
#include "z3_utils/converter/cpp_converter.h"
#include "z3_utils/converter/verilog_converter.h"

#include <sstream>

namespace hal
{
    namespace z3_utils
    {
        z3::expr from_bf(const BooleanFunction& bf, z3::context& context, const std::map<std::string, z3::expr>& var2expr)
        {
            /// Helper function to reduce a abstract syntax subtree to z3 expressions
            ///
            /// @param[in] node - Boolean function node.
            /// @param[in] p - Boolean function node parameters.
            /// @returns (1) status (true on success, false otherwise),
            ///          (2) SMT-LIB string representation of node and operands.
            auto reduce_to_z3 = [&context, &var2expr](const auto& node, auto&& p) -> std::tuple<bool, z3::expr> {
                if (node.get_arity() != p.size())
                {
                    return {false, z3::expr(context)};
                }

                switch (node.type)
                {
                    case BooleanFunction::NodeType::Index:
                        return {true, context.bv_val(node.index, node.size)};
                    case BooleanFunction::NodeType::Constant: {
                        std::vector<u8> bits;
                        for (u32 i = 0; i < node.constant.size(); i++)
                        {
                            if (node.constant.at(i) == BooleanFunction::Value::ONE)
                            {
                                bits.push_back(1);
                            }
                            else if (node.constant.at(i) == BooleanFunction::Value::ZERO)
                            {
                                bits.push_back(0);
                            }
                            else
                            {
                                return {false, z3::expr(context)};
                            }
                        }

                        return {true, context.bv_val(bits.size(), reinterpret_cast<bool*>(bits.data()))};
                    }
                    case BooleanFunction::NodeType::Variable: {
                        if (auto it = var2expr.find(node.variable); it != var2expr.end())
                        {
                            return {true, it->second};
                        }
                        return {true, context.bv_const(node.variable.c_str(), node.size)};
                    }

                    case BooleanFunction::NodeType::And:
                        return {true, p[0] & p[1]};
                    case BooleanFunction::NodeType::Or:
                        return {true, p[0] | p[1]};
                    case BooleanFunction::NodeType::Not:
                        return {true, ~p[0]};
                    case BooleanFunction::NodeType::Xor:
                        return {true, p[0] ^ p[1]};
                    case BooleanFunction::NodeType::Add:
                        return {true, p[0] + p[1]};
                    case BooleanFunction::NodeType::Sub:
                        return {true, p[0] - p[1]};
                    case BooleanFunction::NodeType::Mul:
                        return {true, p[0] * p[1]};
                    case BooleanFunction::NodeType::Sdiv:
                        return {true, p[0] / p[1]};
                    case BooleanFunction::NodeType::Udiv:
                        return {true, z3::udiv(p[0], p[1])};
                    case BooleanFunction::NodeType::Srem:
                        return {true, z3::srem(p[0], p[1])};
                    case BooleanFunction::NodeType::Urem:
                        return {true, z3::urem(p[0], p[1])};
                    case BooleanFunction::NodeType::Concat:
                        return {true, z3::concat(p[0], p[1])};
                    case BooleanFunction::NodeType::Slice:
                        return {true, p[0].extract(p[2].get_numeral_uint(), p[1].get_numeral_uint())};
                    case BooleanFunction::NodeType::Zext:
                        return {true, z3::zext(p[0], p[1].get_numeral_uint() - p[0].get_sort().bv_size())};
                    case BooleanFunction::NodeType::Sext:
                        return {true, z3::sext(p[0], p[1].get_numeral_uint() - p[0].get_sort().bv_size())};
                    case BooleanFunction::NodeType::Shl:
                        return {true, z3::shl(p[0], p[1])};
                    case BooleanFunction::NodeType::Lshr:
                        return {true, z3::lshr(p[0], p[1])};
                    case BooleanFunction::NodeType::Ashr:
                        return {true, z3::ashr(p[0], p[1])};
                    case BooleanFunction::NodeType::Rol:
                        return {true, p[0].rotate_left(p[1].get_numeral_uint())};
                    case BooleanFunction::NodeType::Ror:
                        return {true, p[0].rotate_right(p[1].get_numeral_uint())};
                    case BooleanFunction::NodeType::Eq:
                        return {true, p[0] == p[1]};
                    case BooleanFunction::NodeType::Sle:
                        return {true, z3::sle(p[0], p[1])};
                    case BooleanFunction::NodeType::Slt:
                        return {true, z3::slt(p[0], p[1])};
                    case BooleanFunction::NodeType::Ule:
                        return {true, z3::ule(p[0], p[1])};
                    case BooleanFunction::NodeType::Ult:
                        return {true, z3::ult(p[0], p[1])};
                    case BooleanFunction::NodeType::Ite:
                        return {true, z3::ite(p[0], p[1], p[2])};
                    default:
                        log_error("netlist", "Not implemented reached for nodetype {} in z3 conversion", node.type);
                        return {false, z3::expr(context)};
                }
            };

            std::vector<z3::expr> stack;
            for (const auto& node : bf.get_nodes())
            {
                std::vector<z3::expr> operands;
                std::move(stack.end() - static_cast<i64>(node.get_arity()), stack.end(), std::back_inserter(operands));
                stack.erase(stack.end() - static_cast<i64>(node.get_arity()), stack.end());

                if (auto [ok, reduction] = reduce_to_z3(node, std::move(operands)); ok)
                {
                    stack.emplace_back(reduction);
                }
                else
                {
                    return z3::expr(context);
                }
            }

            switch (stack.size())
            {
                case 1:
                    return stack.back();
                default:
                    return z3::expr(context);
            }
        }

        Result<z3::expr> value_from_binary_string(z3::context& context, const std::string& bit_string)
        {
            std::vector<u8> bits;
            for (u32 i = 0; i < bit_string.length(); i++)
            {
                if (bit_string.at(i) == '1')
                {
                    bits.push_back(1);
                }
                else if (bit_string.at(i) == '0')
                {
                    bits.push_back(0);
                }
                else
                {
                    return ERR("cannot generate value from binary string: encountered unexpected character " + bit_string.at(i));
                }
            }

            return OK(context.bv_val(bits.size(), reinterpret_cast<bool*>(bits.data())));
        }

        namespace
        {
            Result<BooleanFunction> to_bf_internal(const z3::expr& e, std::map<z3::expr, BooleanFunction>& cache)
            {
                u64 size;
                if (e.is_bv())
                {
                    size = e.get_sort().bv_size();

                    if (e.is_numeral())
                    {
                        if (size <= 64)
                        {
                            return OK(BooleanFunction::Const(e.get_numeral_uint64(), size));
                        }
                        else
                        {
                            std::vector<BooleanFunction::Value> boolean_values;
                            const std::string val_str = Z3_get_numeral_binary_string(e.ctx(), e);
                            for (u32 idx = 0; idx < val_str.size(); idx++)
                            {
                                if (val_str.at(idx) == '1')
                                {
                                    boolean_values.push_back(BooleanFunction::Value::ONE);
                                }
                                else if (val_str.at(idx) == '0')
                                {
                                    boolean_values.push_back(BooleanFunction::Value::ZERO);
                                }
                                else
                                {
                                    return ERR("cannot convert expression to boolean function: failed to translate character " + std::to_string(val_str.at(idx)) + " to Boolean value.");
                                }
                            }

                            return OK(BooleanFunction::Const(boolean_values));
                        }
                    }
                    else if (e.is_const())
                    {
                        // std::cout << e << std::endl;
                        const std::string name = e.decl().name().str();
                        return OK(BooleanFunction::Var(name, size));
                    }
                    else if (e.is_var())
                    {
                        const std::string name = e.decl().name().str();
                        return OK(BooleanFunction::Var(name, size));
                    }
                }

                // if (const auto it = cache.find(e); it != cache.end())
                // {
                //     return OK(it->second);
                // }

                const auto op = e.decl().decl_kind();
                auto num_args = e.num_args();
                std::vector<BooleanFunction> args;

                for (u32 i = 0; i < e.num_args(); i++)
                {
                    const auto arg = e.arg(i);
                    if (const auto res = to_bf_internal(arg, cache); res.is_ok())
                    {
                        // const auto [it, _] = cache.insert({arg, res.get()});
                        // args.push_back(it->second.clone());
                        args.push_back(res.get());
                    }
                    else
                    {
                        return ERR(res.get_error());
                    }
                }

                switch (op)
                {
                    case Z3_OP_BAND: {
                        auto bf_res = BooleanFunction::And(std::move(args.at(0)), std::move(args.at(1)), size);
                        for (u64 i = 2; i < num_args; i++)
                        {
                            bf_res =
                                bf_res.map<BooleanFunction>([arg = std::move(args.at(i)), size](BooleanFunction&& bf) mutable { return BooleanFunction::And(std::move(bf), std::move(arg), size); });
                        }
                        return bf_res;
                    }
                    case Z3_OP_BOR: {
                        auto bf_res = BooleanFunction::Or(std::move(args.at(0)), std::move(args.at(1)), size);
                        for (u64 i = 2; i < num_args; i++)
                        {
                            bf_res =
                                bf_res.map<BooleanFunction>([arg = std::move(args.at(i)), size](BooleanFunction&& bf) mutable { return BooleanFunction::Or(std::move(bf), std::move(arg), size); });
                        }
                        return bf_res;
                    }
                    case Z3_OP_BNOT: {
                        if (num_args != 1)
                        {
                            return ERR("operation 'NOT' must have arity 1");
                        }
                        return BooleanFunction::Not(std::move(args.at(0)), size);
                    }
                    case Z3_OP_BXOR: {
                        auto bf_res = BooleanFunction::Xor(std::move(args.at(0)), std::move(args.at(1)), size);
                        for (u64 i = 2; i < num_args; i++)
                        {
                            bf_res =
                                bf_res.map<BooleanFunction>([arg = std::move(args.at(i)), size](BooleanFunction&& bf) mutable { return BooleanFunction::Xor(std::move(bf), std::move(arg), size); });
                        }
                        return bf_res;
                    }
                    case Z3_OP_BNEG: {
                        if (num_args != 1)
                        {
                            return ERR("operation 'NEG' must have arity 1");
                        }
                        return ERR("Negation not implemented");
                    }
                    case Z3_OP_BADD: {
                        auto bf_res = BooleanFunction::Add(std::move(args.at(0)), std::move(args.at(1)), size);
                        for (u64 i = 2; i < num_args; i++)
                        {
                            bf_res =
                                bf_res.map<BooleanFunction>([arg = std::move(args.at(i)), size](BooleanFunction&& bf) mutable { return BooleanFunction::Add(std::move(bf), std::move(arg), size); });
                        }
                        return bf_res;
                    }
                    case Z3_OP_BSUB: {
                        auto bf_res = BooleanFunction::Sub(std::move(args.at(0)), std::move(args.at(1)), size);
                        for (u64 i = 2; i < num_args; i++)
                        {
                            bf_res =
                                bf_res.map<BooleanFunction>([arg = std::move(args.at(i)), size](BooleanFunction&& bf) mutable { return BooleanFunction::Sub(std::move(bf), std::move(arg), size); });
                        }
                        return bf_res;
                    }
                    case Z3_OP_BMUL: {
                        auto bf_res = BooleanFunction::Mul(std::move(args.at(0)), std::move(args.at(1)), size);
                        for (u64 i = 2; i < num_args; i++)
                        {
                            bf_res =
                                bf_res.map<BooleanFunction>([arg = std::move(args.at(i)), size](BooleanFunction&& bf) mutable { return BooleanFunction::Mul(std::move(bf), std::move(arg), size); });
                        }
                        return bf_res;
                    }
                    case Z3_OP_BSDIV:
                        if (num_args != 2)
                        {
                            return ERR("operation 'SDIV' must have arity 2");
                        }
                        return BooleanFunction::Sdiv(std::move(args.at(0)), std::move(args.at(1)), size);
                    case Z3_OP_BUDIV:
                        if (num_args != 2)
                        {
                            return ERR("operation 'UDIV' must have arity 2");
                        }
                        return BooleanFunction::Udiv(std::move(args.at(0)), std::move(args.at(1)), size);
                    case Z3_OP_BSREM:
                        if (num_args != 2)
                        {
                            return ERR("operation 'SREM' must have arity 2");
                        }
                        return BooleanFunction::Srem(std::move(args.at(0)), std::move(args.at(1)), size);
                    case Z3_OP_BUREM:
                        if (num_args != 2)
                        {
                            return ERR("operation 'UREM' must have arity 2");
                        }
                        return BooleanFunction::Urem(std::move(args.at(0)), std::move(args.at(1)), size);
                    case Z3_OP_CONCAT: {
                        auto bf_res = BooleanFunction::Concat(std::move(args.at(0)), std::move(args.at(1)), args.at(0).size() + args.at(1).size());
                        for (u64 i = 2; i < num_args; i++)
                        {
                            bf_res = bf_res.map<BooleanFunction>(
                                [arg = std::move(args.at(i)), size](BooleanFunction&& bf) mutable { return BooleanFunction::Concat(std::move(bf), std::move(arg), bf.size() + arg.size()); });
                        }
                        return bf_res;
                    }
                    case Z3_OP_EXTRACT: {
                        if (num_args != 1)
                        {
                            return ERR("operation 'SLICE' must have arity 1");
                        }

                        const u32 operand_size = args.at(0).size();

                        return BooleanFunction::Slice(std::move(args.at(0)), BooleanFunction::Index(e.lo(), operand_size), BooleanFunction::Index(e.hi(), operand_size), size);
                    }
                    case Z3_OP_ZERO_EXT: {
                        if (num_args != 1)
                        {
                            return ERR("operation 'ZEXT' must have arity 1");
                        }

                        return BooleanFunction::Zext(std::move(args.at(0)), BooleanFunction::Index(size, size), size);
                    }
                    case Z3_OP_SIGN_EXT: {
                        if (num_args != 1)
                        {
                            return ERR("operation 'SEXT' must have arity 1");
                        }

                        return BooleanFunction::Sext(std::move(args.at(0)), BooleanFunction::Index(size, size), size);
                    }
                    case Z3_OP_BSHL:
                        if (num_args != 2)
                        {
                            return ERR("operation 'SHL' must have arity 2");
                        }
                        return BooleanFunction::Shl(std::move(args.at(0)), BooleanFunction::Index((u16)args.at(1).get_constant_value_u64().get(), size), size);
                    case Z3_OP_BLSHR:
                        if (num_args != 2)
                        {
                            return ERR("operation 'LSHR' must have arity 2");
                        }
                        return BooleanFunction::Lshr(std::move(args.at(0)), BooleanFunction::Index((u16)args.at(1).get_constant_value_u64().get(), size), size);
                    case Z3_OP_BASHR:
                        if (num_args != 2)
                        {
                            return ERR("operation 'ASHR' must have arity 2");
                        }
                        return BooleanFunction::Ashr(std::move(args.at(0)), BooleanFunction::Index((u16)args.at(1).get_constant_value_u64().get(), size), size);
                    case Z3_OP_ROTATE_LEFT:
                        if (num_args != 1)
                        {
                            return ERR("operation 'ROL' must have arity 1");
                        }
                        return BooleanFunction::Rol(std::move(args.at(0)), BooleanFunction::Index((u16)Z3_get_decl_int_parameter(Z3_context(e.ctx()), Z3_func_decl(e.decl()), 0), size), size);
                    case Z3_OP_ROTATE_RIGHT:
                        if (num_args != 1)
                        {
                            return ERR("operation 'ROR' must have arity 1");
                        }
                        return BooleanFunction::Ror(std::move(args.at(0)), BooleanFunction::Index((u16)Z3_get_decl_int_parameter(Z3_context(e.ctx()), Z3_func_decl(e.decl()), 0), size), size);
                    case Z3_OP_EQ:
                        if (num_args != 2)
                        {
                            return ERR("operation 'EQ' must have arity 2");
                        }
                        return BooleanFunction::Eq(std::move(args.at(0)), std::move(args.at(1)), 1);
                    case Z3_OP_SLEQ:
                        if (num_args != 2)
                        {
                            return ERR("operation 'SLE' must have arity 2");
                        }
                        return BooleanFunction::Sle(std::move(args.at(0)), std::move(args.at(1)), 1);
                    case Z3_OP_SLT:
                        if (num_args != 2)
                        {
                            return ERR("operation 'SLT' must have arity 2");
                        }
                        return BooleanFunction::Slt(std::move(args.at(0)), std::move(args.at(1)), 1);
                    case Z3_OP_ULEQ:
                        if (num_args != 2)
                        {
                            return ERR("operation 'ULE' must have arity 2");
                        }
                        return BooleanFunction::Ule(std::move(args.at(0)), std::move(args.at(1)), 1);
                    case Z3_OP_ULT:
                        if (num_args != 2)
                        {
                            return ERR("operation 'ULT' must have arity 2");
                        }
                        return BooleanFunction::Ult(std::move(args.at(0)), std::move(args.at(1)), 1);
                    case Z3_OP_ITE:
                        if (num_args != 3)
                        {
                            return ERR("operation 'ITE' must have arity 3");
                        }
                        return BooleanFunction::Ite(std::move(args.at(0)), std::move(args.at(1)), std::move(args.at(2)), size);
                    default:
                        return ERR("operation '" + e.decl().name().str() + "' with arity " + std::to_string(num_args) + " is not yet implemented");
                }
            }
        }    // namespace

        Result<BooleanFunction> to_bf(const z3::expr& e)
        {
            std::map<z3::expr, BooleanFunction> cache;
            return to_bf_internal(e, cache);
        }

        std::string to_smt2(const z3::expr& e)
        {
            auto s = z3::solver(e.ctx());
            if (e.get_sort().is_bv())
            {
                s.add(e == e.ctx().bv_val(0, e.get_sort().bv_size()));
            }
            else
            {
                s.add(e == e.ctx().bool_val(true));
            }
            return s.to_smt2();
        }

        std::string to_cpp(const z3::expr& e)
        {
            const auto converter = Cpp_Converter();
            const auto c_file    = converter.convert_z3_expr_to_func(e);

            return c_file;
        }

        std::string to_verilog(const z3::expr& e, const std::map<std::string, bool>& control_mapping)
        {
            auto converter = VerilogConverter();
            converter.set_control_mapping(control_mapping);

            const auto verilog_file = converter.convert_z3_expr_to_func(e);

            return verilog_file;
        }

        std::set<std::string> get_variable_names(const z3::expr& e)
        {
            std::set<u32> visited       = {e.id()};
            std::vector<z3::expr> stack = {e};

            std::set<std::string> var_names;

            while (!stack.empty())
            {
                const auto n = stack.back();
                stack.pop_back();

                if (n.is_numeral())
                {
                    continue;
                }

                if (n.is_var() || n.is_const())
                {
                    var_names.insert(n.to_string());
                }
                else
                {
                    for (u32 i = 0; i < n.num_args(); i++)
                    {
                        const auto a_i = n.arg(i);
                        if (visited.find(a_i.id()) == visited.end())
                        {
                            visited.insert(a_i.id());
                            stack.push_back(a_i);
                        }
                    }
                }
            }

            return var_names;
        }

        std::set<u32> extract_net_ids(const z3::expr& e)
        {
            return extract_net_ids(get_variable_names(e));
        }

        // TODO make this return a result
        std::set<u32> extract_net_ids(const std::set<std::string>& variable_names)
        {
            std::set<u32> net_ids;

            for (const auto& var : variable_names)
            {
                const auto id_res = BooleanFunctionNetDecorator::get_net_id_from(var);
                if (id_res.is_error())
                {
                    log_error("z3_utils", "{}", id_res.get_error().get());
                }
                net_ids.insert(id_res.get());
            }

            return net_ids;
        }

        z3::expr get_expr_in_ctx(const z3::expr& e, z3::context& ctx)
        {
            auto expr_vec = ctx.parse_string(to_smt2(e).c_str());
            return expr_vec.back().arg(0).simplify();
        }
    }    // namespace z3_utils
}    // namespace hal
