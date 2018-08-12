#include <limits>
#include <sstream>
#include <vector>

#include "asserts.hpp"
#include "formula.hpp"
#include "formula_function.hpp"
#include "formula_function_registry.hpp"
#include "formula_interface.hpp"
#include "formula_internal.hpp"
#include "formula_vm.hpp"
#include "formula_where.hpp"
#include "random.hpp"
#include "unit_test.hpp"
#include "utf8_to_codepoint.hpp"
#include "variant_type.hpp"

extern int g_max_ffl_recursion;

namespace formula_vm {
using namespace game_logic;

namespace {
int dice_roll(int num_rolls, int faces) {
	int res = 0;
	while(faces > 0 && num_rolls-- > 0) {
		res += (rng::generate()%faces)+1;
	}
	return res;
}

static bool incrementVec(std::vector<int>& v, const std::vector<int>& max_values) {
	int index = 0;
	while(index != v.size()) {
		if(++v[index] < max_values[index]) {
			return true;
		}

		v[index] = 0;
		++index;
	}

	return false;
}

int g_vmDepth = 0;

struct VMOverflowGuard {
	VMOverflowGuard() {
		++g_vmDepth;
	}

	~VMOverflowGuard() {
		--g_vmDepth;
	}
};

}



variant VirtualMachine::execute(const FormulaCallable& variables) const
{
	VMOverflowGuard overflow_guard;

	std::vector<FormulaCallablePtr> variables_stack;
	std::vector<variant> stack;
	std::vector<variant> symbol_stack;
	stack.reserve(8);

	if (g_vmDepth > g_max_ffl_recursion) {
		ASSERT_LOG(false, "Overflow in VM: " << debugPinpointLocation(&instructions_[0], stack));
	}

	executeInternal(variables, variables_stack, stack, symbol_stack, &instructions_[0], &instructions_[0] + instructions_.size());
	return stack.back();
}

void VirtualMachine::executeInternal(const FormulaCallable& variables, std::vector<FormulaCallablePtr>& variables_stack, std::vector<variant>& stack, std::vector<variant>& symbol_stack, const InstructionType* p, const InstructionType* p2) const
{
	for(; p != p2; ++p) {
		switch((unsigned char)*p) {
		case OP_IN:
		case OP_NOT_IN: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];

			bool result = false;
			if(right.is_list()) {
				for(int n = 0; n != right.num_elements(); ++n) {
					if(left == right[n]) {
						result = true;
					}
				}

			} else if(right.is_map()) {
				result = right.has_key(left);
			} else {
				ASSERT_LOG(false, "ILLEGAL OPERAND TO 'in': " << right.write_json() << " AT " << debugPinpointLocation(p, stack));
				
			}

			if(*p == OP_NOT_IN) {
				result = !result;
			}

			stack.pop_back();
			stack.back() = variant::from_bool(result);
			break;
		}

		case OP_AND: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			if(left.as_bool() == false) {
				stack.pop_back();
			} else {
				left = right;
				stack.pop_back();
			}
			break;
		}
		case OP_OR: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			if(left.as_bool()) {
				stack.pop_back();
			} else {
				left = right;
				stack.pop_back();
			}
			break;
		}
		case OP_NEQ: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left != right ? variant::from_bool(true) : variant::from_bool(false);
			stack.pop_back();
			break;
		}
		case OP_LTE: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left <= right ? variant::from_bool(true) : variant::from_bool(false);
			stack.pop_back();
			break;
		}
		case OP_GTE: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left >= right ? variant::from_bool(true) : variant::from_bool(false);
			stack.pop_back();
			break;
		}

		case OP_IS_NOT:
		case OP_IS: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];

			variant_type_ptr t(right.convert_to<variant_type>());
			if (*p == OP_IS) {
				left = variant::from_bool(t->match(left));
			} else {
				assert(*p == OP_IS_NOT);
				left = variant::from_bool(!t->match(left));
			}
			stack.pop_back();
			break;
		}
		case OP_GT: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left > right ? variant::from_bool(true) : variant::from_bool(false);
			stack.pop_back();
			break;
		}
		case OP_LT: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left < right ? variant::from_bool(true) : variant::from_bool(false);
			stack.pop_back();
			break;
		}
		case OP_EQ: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left == right ? variant::from_bool(true) : variant::from_bool(false);
			stack.pop_back();
			break;
		}
		case OP_ADD: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left + right;
			stack.pop_back();
			break;
		}
		case OP_SUB: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left - right;
			stack.pop_back();
			break;
		}
		case OP_MUL: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left * right;
			stack.pop_back();
			break;
		}
		case OP_DIV: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			//this is a very unorthodox hack to guard against divide-by-zero errors.  It returns positive or negative infinity instead of asserting, which (hopefully!) works out for most of the physical calculations that are using this.  We tentatively view this behavior as much more preferable to the game apparently crashing for a user.  This is of course not rigorous outside of a videogame setting.
			if(right == variant(0)) { 
				right = variant(decimal::epsilon());
			}

			left = left / right;
			stack.pop_back();
			break;
		}
		case OP_DICE: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = variant(dice_roll(left.as_int(), right.as_int()));
			stack.pop_back();
			break;
		}
		case OP_POW: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left ^ right;
			stack.pop_back();
			break;
		}
		case OP_MOD: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			left = left % right;
			stack.pop_back();
			break;
		}

		case OP_UNARY_NOT: {
			stack.back() = stack.back().as_bool() ? variant::from_bool(false) : variant::from_bool(true);
			break;
		}

		case OP_UNARY_SUB: {
			stack.back() = -stack.back();
			break;
		}

		case OP_UNARY_STR: {
			if(stack.back().is_string() == false) {
				std::string str;
				stack.back().serializeToString(str);
				stack.back() = variant(str);
			}
			break;
		}

		case OP_UNARY_NUM_ELEMENTS: {
			stack.back() = variant(stack.back().num_elements());
			break;
		}

		case OP_INCREMENT: {
			stack.back() = stack.back() + variant(1);
			break;
		}

		case OP_LOOKUP: {
			//std::cerr << "LOOKUP...\n"  << debugPinpointLocation(p, stack) << "\n";
			const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
			++p;
			stack.push_back(vars.queryValueBySlot(static_cast<int>(*p)));
			break;
		}

		case OP_LOOKUP_STR: {
			const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
			variant value = vars.queryValue(stack.back().as_string());
			stack.back() = value;
			break;
		}

		case OP_INDEX: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];
			variant result = left[right];
			left = result;
			stack.pop_back();
			break;
		}

		case OP_INDEX_0: {
			variant& left = stack.back();
			variant result = left[0];
			left = result;
			break;
		}

		case OP_INDEX_1: {
			variant& left = stack.back();
			variant result = left[1];
			left = result;
			break;
		}

		case OP_INDEX_2: {
			variant& left = stack.back();
			variant result = left[2];
			left = result;
			break;
		}

		case OP_INDEX_STR: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];

			if(left.is_callable()) {
				variant result = left.as_callable()->queryValue(right.as_string());
				left = result;
			} else if(left.is_map()) {
				variant result = left[right];
				left = result;
			}
			else if (left.is_list() && !right.is_string()) {
				variant result = left[right];
				left = result;
			}
			else if (left.is_list()) {
				const std::string& s = right.as_string();
				int index;
				if (s == "x" || s == "r") {
					index = 0;
				}
				else if (s == "y" || s == "g") {
					index = 1;
				}
				else if (s == "z" || s == "b") {
					index = 2;
				}
				else if (s == "a") {
					index = 3;
				}
				else {
					ASSERT_LOG(false, "Illegal string lookup on list: " << s << ": " << debugPinpointLocation(p, stack));
				}

				variant result = left[index];
				left = result;
			}
			else if (left.is_string()) {
				const std::string& s = left.as_string();
				unsigned int index = right.as_int();
				ASSERT_LOG(index < s.length(), "index outside bounds: " << s << "[" << index << "]'\n'"  << debugPinpointLocation(p, stack));
				left = variant(s.substr(index, 1));

			} else {
				ASSERT_LOG(false, "Illegal lookup in bytecode: " << left.to_debug_string() << " indexed by " << right.to_debug_string() << " expected map or object");
			}
			stack.pop_back();
			break;
		}

		case OP_CONSTANT: {
			++p;
			stack.push_back(constants_[*p]);
			break;
		}

		case OP_PUSH_INT: {
			++p;
			stack.push_back(variant(static_cast<int>(*p)));
			break;
		}

		case OP_LIST: {
			const size_t nitems = static_cast<size_t>(stack.back().as_int());
			stack.pop_back();
			if(nitems == stack.size()) {
				variant v(&stack);
				stack.clear();
				stack.push_back(v);
			} else {
				std::vector<variant> items(stack.end() - nitems, stack.end());
				variant v(&items);
				stack.erase(stack.end() - nitems, stack.end());
				stack.push_back(v);
			}
			break;
		}

		case OP_MAP: {
			const size_t nitems = static_cast<size_t>(stack.back().as_int());
			stack.pop_back();

			std::map<variant,variant> res;
			for(size_t n = stack.size() - nitems; n+1 < stack.size(); n += 2) {
				variant key = stack[n];
				variant value = stack[n+1];
				res[key] = value;
			}

			variant result(&res);
			stack.resize(stack.size() - nitems);
			stack.push_back(result);
			break;
		}

		case OP_ARRAY_SLICE: {

			variant& left = stack[stack.size()-3];

			int begin_index = stack[stack.size()-2].as_int();
			int end_index = stack[stack.size()-1].as_int(left.num_elements());

			if(left.is_string()) {
				const std::string& s = left.as_string();
				int s_len = static_cast<int>(s.length());
				if(begin_index > s_len) {
					begin_index = s_len;
				}
				if(end_index > s_len) {
					end_index = s_len;
				}

				std::string result;

				if(s_len != 0 && end_index >= begin_index) {
					result = s.substr(begin_index, end_index-begin_index);
				}

				stack.resize(stack.size()-2);
				stack.back() = variant(result);
			} else {
				if(begin_index > left.num_elements()) {
					begin_index = left.num_elements();
				}

				if(end_index > left.num_elements()) {
					end_index = left.num_elements();
				}

				if(left.is_list()) {
					if(end_index >= begin_index && left.num_elements() > 0) {
						left = left.get_list_slice(begin_index, end_index);
						stack.resize(stack.size()-2);
					} else {
						std::vector<variant> empty;
						stack.resize(stack.size()-2);
						stack.back() = variant(&empty);
					}
				} else {
					ASSERT_LOG(false, "illegal usage of operator [:]: " << debugPinpointLocation(p, stack) << " called on " << variant::variant_type_to_string(left.type()));
				}
			}
			break;
		}

		case OP_CALL: {
			++p;
			const size_t nitems = static_cast<size_t>(*p);

			const variant left = std::move(stack[stack.size()-nitems-1]);
			std::vector<variant> args;
			args.reserve(nitems);
			auto i1 = stack.end()-nitems;
			auto i2 = stack.end();
			for(; i1 != i2; ++i1) {
				args.push_back(std::move(*i1));
			}

			stack.resize(stack.size() - nitems);
			stack.back() = left(&args);
			break;
		}

		case OP_CALL_BUILTIN:
		case OP_CALL_BUILTIN_DYNAMIC:
		{
			//std::cerr << "CALL---\n" << debugPinpointLocation(p, stack) << "\n";
			++p;
			const int nitems = static_cast<size_t>(*p);

			const variant left = stack[stack.size()-nitems-1];
			const variant* begin_args = &stack[stack.size()-nitems];

			game_logic::FunctionExpression* fn = static_cast<game_logic::FunctionExpression*>(left.mutable_callable());
			const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
			variant result = fn->executeWithArgs(vars, begin_args, nitems);

			stack.resize(stack.size() - nitems);
			stack.back() = result;
			break;
		}

		case OP_ASSERT: {
			if(stack.back().is_null()) {
				ASSERT_LOG(false, "Assertion failed: " << stack[stack.size()-2].as_string() << " at " << debugPinpointLocation(p, stack));
			} else {
				ASSERT_LOG(false, "Assertion failed: " << stack[stack.size()-2].as_string() << " message: " << stack.back().write_json() << " at " << debugPinpointLocation(p, stack));
			}
			break;
		}

		case OP_PUSH_SCOPE: {
			variables_stack.push_back(game_logic::FormulaCallablePtr(stack.back().mutable_callable()));
			stack.pop_back();
			break;
		}

		case OP_POP_SCOPE: {
			variables_stack.pop_back();
			break;
		}

		case OP_BREAK: {
			return;
		}

		case OP_BREAK_IF: {
			const bool should_break = stack.back().as_bool();
			stack.pop_back();
			if(should_break) {
				return;
			}

			break;
		}

		case OP_ALGO_MAP: {
			using namespace game_logic;

			const int num_base_slots = stack.back().as_int();
			stack.pop_back();

			if(stack.back().is_string()) {
				std::string s = stack.back().as_string();
				utils::utf8_to_codepoint cp(s);
				auto i1 = cp.begin();
				auto i2 = cp.end();
				std::vector<variant> v;

				for(; i1 != i2; ++i1) {
					std::string str(i1.get_char_as_string());
					v.push_back(variant(str));
				}

				stack.back() = variant(&v);
			}

			if(stack.back().is_list()) {
				variant back = stack.back();
				stack.pop_back();
				const std::vector<variant>& input = back.as_list();

				if(input.empty()) {
					std::vector<variant> res;
					stack.push_back(variant(&res));
					p += *(p+1);
					break;
				}

				const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
				map_callable* callable = new map_callable(vars, num_base_slots);
				variables_stack.push_back(callable);

				int index = 0;
				for(const variant& in : input) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars, num_base_slots);
						variables_stack.back().reset(callable);
					}
					callable->set(in, index);
					executeInternal(variables, variables_stack, stack, symbol_stack, p+2, p + *(p+1) + 1);
					++index;
				}

				variables_stack.pop_back();

				std::vector<variant> res(stack.end() - index, stack.end());

				stack.resize(stack.size() - index);

				stack.push_back(variant(&res));

				p += *(p+1);
			} else if(stack.back().is_map()) {
				variant back = stack.back();
				stack.pop_back();
				const std::map<variant,variant>& input = back.as_map();

				if(input.empty()) {
					std::vector<variant> res;
					stack.push_back(variant(&res));
					p += *(p+1);
					break;
				}

				const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
				map_callable* callable = new map_callable(vars, num_base_slots);
				variables_stack.push_back(callable);

				int index = 0;
				for(const std::pair<variant,variant>& in : input) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars, num_base_slots);
						variables_stack.back().reset(callable);
					}
					callable->set(in.first, in.second, index);
					executeInternal(variables, variables_stack, stack, symbol_stack, p+2, p + *(p+1) + 1);
					++index;
				}

				variables_stack.pop_back();

				std::vector<variant> res(stack.end() - index, stack.end());

				stack.resize(stack.size() - index);

				stack.push_back(variant(&res));

				p += *(p+1);
			} else if(stack.back().is_callable()) {
				//objects just map over the single item in the map.
				//TODO: consider if this is what we really want.
				std::vector<variant> list;
				list.push_back(stack.back());
				stack.back() = variant(&list);
				--p;
			} else {
				ASSERT_LOG(false, "Unexpected type given to map: " << stack.back().to_debug_string());
			}
			break;
		}

		case OP_ALGO_FILTER: {
			using namespace game_logic;

			const int num_base_slots = stack.back().as_int();
			stack.pop_back();

			if(!stack.back().is_list() && !stack.back().is_map()) {
				//not a list or map try to convert to a list.
				std::vector<variant> items;
				items.reserve(stack.back().num_elements());

				for(int n = 0; n != static_cast<int>(stack.back().num_elements()); ++n) {
					items.push_back(stack.back()[n]);
				}

				stack.back() = variant(&items);
			}

			if(stack.back().is_list()) {
				variant back = stack.back();
				stack.pop_back();
				const std::vector<variant>& input = back.as_list();

				if(input.empty()) {
					std::vector<variant> res;
					stack.push_back(variant(&res));
					p += *(p+1);
					break;
				}

				const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
				map_callable* callable = new map_callable(vars, num_base_slots);
				variables_stack.push_back(callable);

				const size_t start_stack_size = stack.size();
				std::vector<variant> res;
				res.reserve(input.size());

				int index = 0;
				for(const variant& in : input) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars, num_base_slots);
						variables_stack.back().reset(callable);
					}
					callable->set(in, index);
					executeInternal(variables, variables_stack, stack, symbol_stack, p+2, p + *(p+1) + 1);

					if(stack.back().as_bool()) {
						res.push_back(in);
					}

					stack.pop_back();

					++index;
				}

				variables_stack.pop_back();

				stack.push_back(variant(&res));

				p += *(p+1);

			} else if(stack.back().is_map()) {
				variant back = stack.back();
				stack.pop_back();
				const std::map<variant,variant>& input = back.as_map();

				if(input.empty()) {
					std::map<variant,variant> res;
					stack.push_back(variant(&res));
					p += *(p+1);
					break;
				}

				const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
				map_callable* callable = new map_callable(vars, num_base_slots);
				variables_stack.push_back(callable);

				std::map<variant,variant> res;

				int index = 0;
				for(const std::pair<variant,variant>& in : input) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars, num_base_slots);
						variables_stack.back().reset(callable);
					}
					callable->set(in.first, in.second, index);
					executeInternal(variables, variables_stack, stack, symbol_stack, p+2, p + *(p+1) + 1);

					if(stack.back().as_bool()) {
						res.insert(in);
					}

					stack.pop_back();

					++index;
				}

				variables_stack.pop_back();

				stack.push_back(variant(&res));

				p += *(p+1);
			} else {
				ASSERT_LOG(false, "Unexpected type given to filter: " << stack.back().to_debug_string() << " " << debugPinpointLocation(p, stack));
			}
			break;
		}

		case OP_ALGO_FIND: {
			using namespace game_logic;

			const int num_base_slots = stack.back().as_int();
			stack.pop_back();

			variant back = stack.back();
			stack.pop_back();
			const std::vector<variant>& items = back.as_list();


			int index = 0;

			if(items.empty() == false) {
				const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
				map_callable* callable = new map_callable(vars, num_base_slots);
				variables_stack.push_back(callable);

				for(const variant& item : items) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars, num_base_slots);
						variables_stack.back().reset(callable);
					}
					callable->set(item, index);
					executeInternal(variables, variables_stack, stack, symbol_stack, p+2, p + *(p+1) + 1);
					if(stack.back().as_bool()) {
						stack.pop_back();
						break;
					}

					stack.pop_back();

					++index;
				}

				variables_stack.pop_back();
			}

			if(index == items.size()) {
				index = -1;
				stack.push_back(variant());
			} else {
				stack.push_back(items[index]);
			}

			stack.push_back(variant(index));

			p += *(p+1);

			break;
		}

		case OP_ALGO_COMPREHENSION: {
			using namespace game_logic;

			const int base_slot = stack.back().as_int();
			stack.pop_back();

			const int nlists = stack.back().as_int();
			stack.pop_back();

			std::vector<variant> lists(stack.end() - nlists, stack.end());
			stack.resize(stack.size() - nlists);

			std::vector<int> nelements;

			bool exit_loop = false;
			for(const variant& list : lists) {
				nelements.push_back(list.num_elements());
				if(list.num_elements() == 0) {
					exit_loop = true;
				}
			}

			if(exit_loop) {
				std::vector<variant> res;
				stack.push_back(variant(&res));
				p += *(p+1);
				break;
			}

			const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();

			std::vector<variant*> args;

			ffl::IntrusivePtr<SlotFormulaCallable> callable(new SlotFormulaCallable);
			callable->setFallback(&vars);
			callable->setBaseSlot(base_slot);
			callable->reserve(lists.size());
			for(const variant& list : lists) {
				callable->add(variant());
				args.push_back(&callable->backDirectAccess());
			}

			variables_stack.push_back(callable);

			const int start_stack = stack.size();

			std::vector<int> indexes(lists.size());

			for(;;) {
				for(int n = 0; n != indexes.size(); ++n) {
					*args[n] = lists[n][indexes[n]];
				}

				executeInternal(variables, variables_stack, stack, symbol_stack, p+2, p + *(p+1) + 1);

				if(!incrementVec(indexes, nelements)) {
					break;
				}
			}

			variables_stack.pop_back();

			std::vector<variant> res(stack.begin() + start_stack, stack.end());
			stack.resize(start_stack);
			stack.push_back(variant(&res));

			p += *(p+1);

			break;
		}

		case OP_POP: {
			stack.pop_back();
			break;
		}

		case OP_DUP: {
			stack.push_back(stack.back());
			break;
		}

		case OP_DUP2: {
			stack.push_back(stack[stack.size()-2]);
			stack.push_back(stack[stack.size()-2]);
			break;
		}

		case OP_SWAP: {
			stack.back().swap(stack[stack.size()-2]);
			break;
		}

		case OP_UNDER: {
			variant v = std::move(stack.back());
			stack.pop_back();
			++p;
			stack.insert(stack.end() - *p, v);
			break;
		}

		case OP_PUSH_NULL: {
			stack.push_back(variant());
			break;
		}

		case OP_PUSH_0: {
			stack.push_back(variant(0));
			break;
		}

		case OP_PUSH_1: {
			stack.push_back(variant(1));
			break;
		}

		case OP_WHERE: {
			using namespace game_logic;

			++p;

			if(*p >= 0) {
				const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();

				ffl::IntrusivePtr<SlotFormulaCallable> callable(new SlotFormulaCallable);
				callable->setFallback(&vars);
				callable->setBaseSlot(*p);

				variables_stack.push_back(callable);
			}

			static_cast<SlotFormulaCallable*>(variables_stack.back().get())->add(stack.back());
			stack.pop_back();

			break;
		}

		case OP_INLINE_FUNCTION: {
			using namespace game_logic;

			++p;

			ffl::IntrusivePtr<SlotFormulaCallable> callable(new SlotFormulaCallable);

			if(stack[stack.size()-2].is_callable()) {
				callable->setFallback(stack[stack.size()-2].as_callable());
			}
			callable->setBaseSlot(*p);

			variables_stack.push_back(callable);

			const int nitems = stack.back().int_addr();

			auto i1 = stack.end() - nitems - 2;
			auto i2 = stack.end() - 2;
			callable->reserve(i2 - i1);
			while(i1 != i2) {
				callable->add(*i1);
				++i1;
			}

			stack.resize(stack.size() - nitems - 2);

			break;
		}

		case OP_JMP_IF:
		case OP_JMP_UNLESS: {
			if(stack.back().as_bool() == (*p == OP_JMP_IF)) {
				p += *(p+1);
			} else {
				++p;
			}
			break;
		}

		case OP_POP_JMP_IF:
		case OP_POP_JMP_UNLESS: {
			if(stack.back().as_bool() == (*p == OP_POP_JMP_IF)) {
				p += *(p+1);
			} else {
				++p;
			}
			stack.pop_back();
			break;
		}

		case OP_JMP: {
			p += *(p+1);
			break;
		}

		case OP_LAMBDA_WITH_CLOSURE: {
			const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
			stack.back() = stack.back().change_function_callable(vars);
			break;
		}

		case OP_CREATE_INTERFACE: {
			stack[stack.size()-2] = stack.back().convert_to<FormulaInterfaceInstanceFactory>()->create(stack[stack.size()-2]);
			stack.pop_back();
			break;
		}

		case OP_PUSH_SYMBOL_STACK: {
			symbol_stack.emplace_back(std::move(stack.back()));
			stack.pop_back();
			break;
		}

		case OP_POP_SYMBOL_STACK: {
			symbol_stack.pop_back();
			break;
		}

		case OP_LOOKUP_SYMBOL_STACK: {
			++p;
			const int index = static_cast<int>(*p);
			ASSERT_LOG(index >= 0 && index < static_cast<int>(symbol_stack.size()), "Illegal symbol stack index: " << index << " / " << symbol_stack.size());
			stack.push_back(symbol_stack[static_cast<int>(*p)]);
			break;
		}
			
		}
	}
}

void VirtualMachine::replaceInstructions(Iterator i1, Iterator i2, const std::vector<InstructionType>& new_instructions)
{
	const int diff = static_cast<int>(new_instructions.size()) - (static_cast<int>(i2.get_index()) - static_cast<int>(i1.get_index()));

	for(DebugInfo& info : debug_info_) {
		if(info.bytecode_pos >= i2.get_index()) {
			info.bytecode_pos += diff;
		}
	}

	for(Iterator i = begin_itor(); i.at_end() == false; i.next()) {
		if(i.get_index() >= i1.get_index() && i.get_index() < i2.get_index()) {
			continue;
		}

		if(isInstructionJump(i.get()) == false) {
			continue;
		}

		const int src_index = static_cast<int>(i.get_index());
		const int dst_index = src_index + static_cast<int>(i.arg()) + 1;
		if(src_index < i1.get_index() && dst_index >= i2.get_index()) {
			i.arg_mutable() += diff;
		} else if(src_index >= i2.get_index() && dst_index <= i1.get_index()) {
			i.arg_mutable() -= diff;
		}
	}

	instructions_.erase(instructions_.begin() + i1.get_index(), instructions_.begin() + i2.get_index());
	instructions_.insert(instructions_.begin() + i1.get_index(), new_instructions.begin(), new_instructions.end());
}

void VirtualMachine::addInstruction(OP op)
{
	instructions_.push_back(op);
}

void VirtualMachine::addConstant(const variant& v)
{
	instructions_.push_back(static_cast<InstructionType>(constants_.size()));
	constants_.push_back(v);
}

void VirtualMachine::addInt(InstructionType i)
{
	instructions_.push_back(i);
}

void VirtualMachine::addLoadConstantInstruction(const variant& v)
{
	if(v.is_null()) {
		addInstruction(OP_PUSH_NULL);
		return;
	}

	if(v.is_int()) {
		if(v == variant(0)) {
			addInstruction(OP_PUSH_0);
			return;
		}

		if(v == variant(1)) {
			addInstruction(OP_PUSH_1);
			return;
		}

		if(v.as_int() <= std::numeric_limits<InstructionType>::max() && v.as_int() >= std::numeric_limits<InstructionType>::min()) {
			addInstruction(OP_PUSH_INT);
			addInt(v.as_int());
			return;
		}
	}

	auto itor = std::find(constants_.begin(), constants_.end(), v);
	if(itor == constants_.end()) {
		constants_.push_back(v);
		itor = constants_.end()-1;
	}

	addInstruction(OP_CONSTANT);
	addInt(static_cast<int>(itor - constants_.begin()));
}

int VirtualMachine::addJumpSource(InstructionType i)
{
	instructions_.push_back(i);
	addInt(0);
	return static_cast<int>(instructions_.size())-1;
}

void VirtualMachine::jumpToEnd(int source)
{
	instructions_[source] = static_cast<InstructionType>(instructions_.size()) - source;
}

int VirtualMachine::getPosition() const
{
	return static_cast<int>(instructions_.size());
}

void VirtualMachine::addJumpToPosition(InstructionType i, int pos)
{
	const int value = pos - getPosition() - 1;
	instructions_.push_back(i);
	addInt(value);
}

namespace {
	VirtualMachine::InstructionType g_arg_instructions[] = { OP_LOOKUP, OP_JMP_IF, OP_JMP, OP_JMP_UNLESS, OP_POP_JMP_IF, OP_POP_JMP_UNLESS, OP_CALL, OP_CALL_BUILTIN, OP_CALL_BUILTIN_DYNAMIC, OP_ALGO_MAP, OP_ALGO_FILTER, OP_ALGO_FIND, OP_ALGO_COMPREHENSION, OP_UNDER, OP_PUSH_INT, OP_LOOKUP_SYMBOL_STACK, OP_WHERE, OP_INLINE_FUNCTION, OP_CONSTANT };
}

void VirtualMachine::append(const VirtualMachine& other)
{
	for(DebugInfo d : other.debug_info_) {
		d.bytecode_pos += instructions_.size();
		debug_info_.push_back(d);
	}

	if(other.parent_formula_.is_string() && !parent_formula_.is_string()) {
		parent_formula_ = other.parent_formula_;
	}

	//try to map constants from the other vm into our vm.
	std::map<int,int> map_constants;
	std::vector<variant> other_constants = other.constants_;
	while(other_constants.empty() == false) {
		auto itor = std::find(constants_.begin(), constants_.end(), other_constants.back());
		if(itor == constants_.end()) {
			break;
		}
		map_constants[static_cast<int>(other_constants.size())-1] = itor - constants_.begin();

		other_constants.pop_back();
	}

	for(size_t i = 0; i < other.instructions_.size(); ++i) {
		instructions_.push_back(other.instructions_[i]);
		if(instructions_.back() == OP_CONSTANT) {
			++i;

			auto mapping = map_constants.find(static_cast<int>(other.instructions_[i]));
			if(mapping != map_constants.end()) {
				instructions_.push_back(mapping->second);
			} else {
				instructions_.push_back(constants_.size() + other.instructions_[i]);
			}
		} else {
		
			bool need_skip = false;
			for(auto in : g_arg_instructions) {
				if(in == instructions_.back()) {
					need_skip = true;
					break;
				}
			}

			if(need_skip) {
				++i;
				instructions_.push_back(other.instructions_[i]);
			}
		}
	}

	constants_.insert(constants_.end(), other_constants.begin(), other_constants.end());
}

void VirtualMachine::append(Iterator i1, Iterator i2, const VirtualMachine& other)
{
	std::vector<InstructionType> old_instructions = instructions_;
	append(other);
	std::vector<InstructionType> new_instructions(instructions_.begin() + old_instructions.size(), instructions_.end());
	instructions_.resize(old_instructions.size());

	replaceInstructions(i1, i2, new_instructions);
}

namespace {

const char* getOpName(VirtualMachine::InstructionType op) {
#define DEF_OP(n) case n: return #n;

	switch(op) {
		  DEF_OP(OP_IN) DEF_OP(OP_NOT_IN) DEF_OP(OP_AND) DEF_OP(OP_OR) DEF_OP(OP_NEQ) DEF_OP(OP_LTE) DEF_OP(OP_GTE) DEF_OP(OP_IS)
	DEF_OP(OP_IS_NOT)

		  DEF_OP(OP_UNARY_NOT) DEF_OP(OP_UNARY_SUB) DEF_OP(OP_UNARY_STR) DEF_OP(OP_UNARY_NUM_ELEMENTS)

		  DEF_OP(OP_INCREMENT)

		  DEF_OP(OP_LOOKUP)

		  DEF_OP(OP_LOOKUP_STR)
		  
		  DEF_OP(OP_INDEX)

		  DEF_OP(OP_INDEX_0)
		  DEF_OP(OP_INDEX_1)
		  DEF_OP(OP_INDEX_2)
		  
		  DEF_OP(OP_INDEX_STR)

		  DEF_OP(OP_CONSTANT)

		  DEF_OP(OP_PUSH_INT)

		  DEF_OP(OP_LIST) DEF_OP(OP_MAP)

		  DEF_OP(OP_ARRAY_SLICE)

		  DEF_OP(OP_CALL)

		  DEF_OP(OP_CALL_BUILTIN)

		  DEF_OP(OP_CALL_BUILTIN_DYNAMIC)

		  DEF_OP(OP_ASSERT)

		  DEF_OP(OP_PUSH_SCOPE)
		  
		  DEF_OP(OP_POP_SCOPE)

		  DEF_OP(OP_BREAK)

		  DEF_OP(OP_BREAK_IF)

		  DEF_OP(OP_ALGO_MAP)
		  DEF_OP(OP_ALGO_FILTER)

		  DEF_OP(OP_ALGO_FIND)

		  DEF_OP(OP_ALGO_COMPREHENSION)

		  DEF_OP(OP_POP)

		  DEF_OP(OP_MOD)
          DEF_OP(OP_MUL) DEF_OP(OP_ADD) DEF_OP(OP_SUB) DEF_OP(OP_DIV)
		  DEF_OP(OP_LT) DEF_OP(OP_EQ) DEF_OP(OP_GT)

		  DEF_OP(OP_DUP)

		  DEF_OP(OP_DUP2)

		  DEF_OP(OP_SWAP)

		  DEF_OP(OP_UNDER)

		  DEF_OP(OP_PUSH_NULL)
		  DEF_OP(OP_PUSH_0)
		  DEF_OP(OP_PUSH_1)

		  DEF_OP(OP_WHERE)

		  DEF_OP(OP_INLINE_FUNCTION)

		  DEF_OP(OP_JMP_IF) DEF_OP(OP_JMP_UNLESS)

		  DEF_OP(OP_POP_JMP_IF) DEF_OP(OP_POP_JMP_UNLESS)


		  DEF_OP(OP_JMP)

		  DEF_OP(OP_LAMBDA_WITH_CLOSURE)

		  DEF_OP(OP_CREATE_INTERFACE)

		  DEF_OP(OP_PUSH_SYMBOL_STACK)

		  DEF_OP(OP_POP_SYMBOL_STACK)

		  DEF_OP(OP_LOOKUP_SYMBOL_STACK)
		  
		  
		  DEF_OP(OP_POW) DEF_OP(OP_DICE)
		  default:
		  	return "UNKNOWN";
	}

#undef DEF_OP
}
}

std::string VirtualMachine::debugOutput(const VirtualMachine::InstructionType* instruction_ptr) const
{
	std::ostringstream s;
	for(size_t n = 0; n != instructions_.size(); ++n) {
		auto op = instructions_[n];
		if(instruction_ptr == &instructions_[n]) {
			s << "-->" << n;
		} else {
			s << "   " << n;
		}

		if(op == OP_CONSTANT) {
			s << ": OP_CONSTANT ";
			++n;
			if(instructions_[n] < constants_.size()) {
				std::string j = constants_[instructions_[n]].write_json();
				if(j.size() > 80) {
					j.resize(80);
					j += "...";
				}
				s << instructions_[n] << " ( " << j << " )\n";
			} else {
				s << "ILLEGAL (" << instructions_[n] << " / " << constants_.size() << ")\n";
			}
		} else if(op == OP_PUSH_INT) {
			s << ": OP_PUSH_INT ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_LOOKUP) {
			s << ": OP_LOOKUP ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_CALL) {
			s << ": OP_CALL ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_CALL_BUILTIN) {
			s << ": OP_CALL_BUILTIN ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_CALL_BUILTIN_DYNAMIC) {
			s << ": OP_CALL_BUILTIN_DYNAMIC ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_JMP_IF) {
			s << ": OP_JMP_IF ";
			++n;
			s << instructions_[n] << " ( -> " << (n + static_cast<int>(instructions_[n])) << ")\n";
		} else if(op == OP_JMP_UNLESS) {
			s << ": OP_JMP_UNLESS ";
			++n;
			s << instructions_[n] << " ( -> " << (n + static_cast<int>(instructions_[n])) << ")\n";
		} else if(op == OP_POP_JMP_IF) {
			s << ": OP_POP_JMP_IF ";
			++n;
			s << instructions_[n] << " ( -> " << (n + static_cast<int>(instructions_[n])) << ")\n";
		} else if(op == OP_POP_JMP_UNLESS) {
			s << ": OP_POP_JMP_UNLESS ";
			++n;
			s << instructions_[n] << " ( -> " << (n + static_cast<int>(instructions_[n])) << ")\n";
		} else if(op == OP_JMP) {
			s << ": OP_JMP ";
			++n;
			s << instructions_[n] << " ( -> " << (n + static_cast<int>(instructions_[n])) << ")\n";
		} else if(op == OP_WHERE) {
			s << ": OP_WHERE ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_INLINE_FUNCTION) {
			s << ": OP_INLINE_FUNCTION ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_ALGO_MAP) {
			s << ": OP_ALGO_MAP ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_ALGO_FILTER) {
			s << ": OP_ALGO_FILTER ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_ALGO_FIND) {
			s << ": OP_ALGO_FIND ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_ALGO_COMPREHENSION) {
			s << ": OP_ALGO_COMPREHENSION ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_UNDER) {
			s << ": OP_UNDER ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op == OP_LOOKUP_SYMBOL_STACK) {
			s << ": OP_LOOKUP_SYMBOL_STACK ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else {
			s << ": " << getOpName(op) << "\n";
		}
	}

	return s.str();
}

void VirtualMachine::setDebugInfo(const variant& parent_formula, unsigned short begin, unsigned short end)
{
	parent_formula_ = parent_formula;
	DebugInfo info;
	info.bytecode_pos = 0;
	info.formula_pos = begin;
	debug_info_.push_back(info);
}

std::string VirtualMachine::debugPinpointLocation(const InstructionType* p, const std::vector<variant>& stack) const
{
	if(debug_info_.empty()) {
		return "Unknown VM location";
	}

	const int pos = p - &instructions_[0];
	DebugInfo info = debug_info_.front();
	for(auto in : debug_info_) {
		if(in.bytecode_pos > pos) {
			continue;
		}

		if(info.bytecode_pos > pos || in.bytecode_pos > info.bytecode_pos) {
			info = in;
		}
	}

	const std::string& s = parent_formula_.as_string();

	std::ostringstream stream;
	stream << "in Virtual Machine: " << pinpoint_location(parent_formula_, s.begin() + info.formula_pos) << "\n---VM:\n" << debugOutput(p) << "\n---STACK---\n";

	std::vector<variant> reverse_stack(stack.rbegin(), stack.rend());
	int index = 0;
	for(const variant& v : reverse_stack) {
		stream << "  --TOS+" << index << "--\n" << v.to_debug_string() << "\n";
		++index;
	}

	return stream.str();
}

VirtualMachine::InstructionType VirtualMachine::Iterator::get() const
{
	return vm_->instructions_[index_];
}

bool VirtualMachine::Iterator::has_arg() const
{
	auto cur = get();
	for(auto in : g_arg_instructions) {
		if(cur == in) {
			return true;
		}
	}

	return false;
}

VirtualMachine::InstructionType VirtualMachine::Iterator::arg() const
{
	return vm_->instructions_[index_+1];
}

VirtualMachine::InstructionType& VirtualMachine::Iterator::arg_mutable()
{
	return const_cast<VirtualMachine*>(vm_)->instructions_[index_+1];
}

void VirtualMachine::Iterator::next()
{
	if(has_arg()) {
		++index_;
	}

	++index_;
}

bool VirtualMachine::Iterator::at_end() const
{
	return index_ == vm_->instructions_.size();
}

bool VirtualMachine::isInstructionLoop(InstructionType i)
{
	return i >= OP_ALGO_MAP && i <= OP_ALGO_COMPREHENSION;
}

bool VirtualMachine::isInstructionJump(InstructionType i)
{
	return isInstructionLoop(i) || (i >= OP_JMP_IF && i <= OP_JMP);
}

UNIT_TEST(formula_vm) {
	MapFormulaCallable* callable = new MapFormulaCallable;
	variant ref(callable);
	{
		VirtualMachine vm;
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(5));
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(8));
		vm.addInstruction(OP_ADD);
		CHECK_EQ(vm.execute(*callable), variant(13));
	}
}

UNIT_TEST(formula_vm_and_0) {
	const MapFormulaCallable * callable = new MapFormulaCallable;
	const variant ref(callable);
	{
		VirtualMachine vm;
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(true));
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(false));
		vm.addInstruction(OP_AND);
		CHECK_EQ(vm.execute(* callable), variant(false));
	}
}

UNIT_TEST(formula_vm_and_1) {
	const MapFormulaCallable * callable = new MapFormulaCallable;
	const variant ref(callable);
	{
		VirtualMachine vm;
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(false));
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(true));
		vm.addInstruction(OP_AND);
		CHECK_EQ(vm.execute(* callable), variant(false));
	}
}

UNIT_TEST(formula_vm_or_0) {
	const MapFormulaCallable * callable = new MapFormulaCallable;
	const variant ref(callable);
	{
		VirtualMachine vm;
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(true));
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(false));
		vm.addInstruction(OP_OR);
		CHECK_EQ(vm.execute(* callable), variant(true));
	}
}

UNIT_TEST(formula_vm_or_1) {
	const MapFormulaCallable * callable = new MapFormulaCallable;
	const variant ref(callable);
	{
		VirtualMachine vm;
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(false));
		vm.addInstruction(OP_CONSTANT);
		vm.addConstant(variant(true));
		vm.addInstruction(OP_OR);
		CHECK_EQ(vm.execute(* callable), variant(true));
	}
}

}
