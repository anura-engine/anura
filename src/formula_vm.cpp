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
#include "variant_type.hpp"

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

}

variant VirtualMachine::execute(const FormulaCallable& variables) const
{
	std::vector<FormulaCallablePtr> variables_stack;
	std::vector<variant> stack;
	stack.reserve(8);
	executeInternal(variables, variables_stack, stack, &instructions_[0], &instructions_[0] + instructions_.size());
	return stack.back();
}

void VirtualMachine::executeInternal(const FormulaCallable& variables, std::vector<FormulaCallablePtr>& variables_stack, std::vector<variant>& stack, const InstructionType* p, const InstructionType* p2) const
{
	for(; p != p2; ++p) {
		switch(*p) {
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

		case OP_IS: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];

			variant_type_ptr t(right.convert_to<variant_type>());
			left = variant::from_bool(t->match(left));
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

		case OP_INDEX_STR: {
			variant& left = stack[stack.size()-2];
			variant& right = stack[stack.size()-1];

			if(left.is_callable()) {
				variant result = left.as_callable()->queryValue(right.as_string());
				left = result;
			} else if(left.is_list() || left.is_map()) {
				variant result = left[right];
				left = result;
			} else if(left.is_string()) {
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

			const variant left = stack[stack.size()-nitems-1];
			std::vector<variant> args(stack.end()-nitems, stack.end());

			stack.resize(stack.size() - nitems);
			stack.back() = left(args);
			break;
		}

		case OP_CALL_BUILTIN: {
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
			ASSERT_LOG(false, "Assertion failed: " << stack.back().as_string() << " at " << debugPinpointLocation(p, stack));
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

		case OP_LOOP_BEGIN: {
			using namespace game_logic;
			if(stack.back().num_elements() == 0) {
				p += *(p+1);
			} else {
				const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
				variables_stack.push_back(new map_callable(vars, stack.back()));
				++p;
			}
			break;
		}

		case OP_LOOP_NEXT: {
			using namespace game_logic;
			map_callable* m = static_cast<map_callable*>(variables_stack.back().get());
			if(m->refcount() > 1) {
				m = new map_callable(*m);
				variables_stack.back().reset(m);
			}
			const bool result = m->next(stack.back());
			if(result) {
				p += *(p+1);
			} else {
				++p;
			}
			break;
		}

		case OP_ALGO_MAP: {
			using namespace game_logic;
			if(stack.back().is_string()) {
				std::string s = stack.back().as_string();
				std::vector<variant> v;
				for(auto c : s) {
					std::string str;
					str.resize(1);
					str[0] = c;
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
				map_callable* callable = new map_callable(vars);
				variables_stack.push_back(callable);

				int index = 0;
				for(const variant& in : input) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars);
						variables_stack.back().reset(callable);
					}
					callable->set(in, index);
					executeInternal(variables, variables_stack, stack, p+2, p + *(p+1) + 1);
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
				map_callable* callable = new map_callable(vars);
				variables_stack.push_back(callable);

				int index = 0;
				for(const std::pair<variant,variant>& in : input) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars);
						variables_stack.back().reset(callable);
					}
					callable->set(in.first, in.second, index);
					executeInternal(variables, variables_stack, stack, p+2, p + *(p+1) + 1);
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
				map_callable* callable = new map_callable(vars);
				variables_stack.push_back(callable);

				const size_t start_stack_size = stack.size();
				std::vector<variant> res;
				res.reserve(input.size());

				int index = 0;
				for(const variant& in : input) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars);
						variables_stack.back().reset(callable);
					}
					callable->set(in, index);
					executeInternal(variables, variables_stack, stack, p+2, p + *(p+1) + 1);

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
				map_callable* callable = new map_callable(vars);
				variables_stack.push_back(callable);

				std::map<variant,variant> res;

				int index = 0;
				for(const std::pair<variant,variant>& in : input) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars);
						variables_stack.back().reset(callable);
					}
					callable->set(in.first, in.second, index);
					executeInternal(variables, variables_stack, stack, p+2, p + *(p+1) + 1);

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
				ASSERT_LOG(false, "Unexpected type given to filter: " << stack.back().to_debug_string());
			}
			break;
		}

		case OP_ALGO_FIND: {
			using namespace game_logic;
			variant back = stack.back();
			stack.pop_back();
			const std::vector<variant>& items = back.as_list();


			int index = 0;

			if(items.empty() == false) {
				const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
				map_callable* callable = new map_callable(vars);
				variables_stack.push_back(callable);

				for(const variant& item : items) {
					if(callable->refcount() != 1) {
						callable = new map_callable(vars);
						variables_stack.back().reset(callable);
					}
					callable->set(item, index);
					executeInternal(variables, variables_stack, stack, p+2, p + *(p+1) + 1);
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

			boost::intrusive_ptr<SlotFormulaCallable> callable(new SlotFormulaCallable);
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

				executeInternal(variables, variables_stack, stack, p+2, p + *(p+1) + 1);

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
			std::swap(stack.back(), stack[stack.size()-2]);
			break;
		}

		case OP_UNDER: {
			variant v = stack.back();
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
			WhereVariablesInfoPtr info(constants_[*p].convert_to<WhereVariablesInfo>());
			const FormulaCallable& vars = variables_stack.empty() ? variables : *variables_stack.back();
			FormulaCallablePtr wrapped_variables(new WhereVariables(vars, info));
			variables_stack.push_back(wrapped_variables);
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

		case OP_LAMBDA: {
			static boost::intrusive_ptr<SlotFormulaCallable> callable(new SlotFormulaCallable);
			stack.back() = stack.back().change_function_callable(*callable);
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
			
		}
	}
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
	VirtualMachine::InstructionType g_arg_instructions[] = { OP_LOOKUP, OP_JMP_IF, OP_JMP, OP_JMP_UNLESS, OP_POP_JMP_IF, OP_POP_JMP_UNLESS, OP_CALL, OP_CALL_BUILTIN, OP_LOOP_NEXT, OP_ALGO_MAP, OP_ALGO_FILTER, OP_ALGO_FIND, OP_ALGO_COMPREHENSION, OP_UNDER };
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

	for(size_t i = 0; i != other.instructions_.size(); ++i) {
		instructions_.push_back(other.instructions_[i]);
		if(instructions_.back() == OP_CONSTANT || instructions_.back() == OP_WHERE) {
			++i;
			instructions_.push_back(constants_.size() + other.instructions_[i]);
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

	constants_.insert(constants_.end(), other.constants_.begin(), other.constants_.end());
}

static const std::string OpNames[] = {
"OP_INVALID",
"OP_IN", "OP_NOT_IN", "OP_AND", "OP_OR", "OP_NEQ", "OP_LTE", "OP_GTE", "OP_IS",

		  "OP_UNARY_NOT", "OP_UNARY_SUB", "OP_UNARY_STR", "OP_UNARY_NUM_ELEMENTS",
		  "OP_INCREMENT",
		  "OP_LOOKUP", "OP_LOOKUP_STR",
		  "OP_INDEX", "OP_INDEX_STR", "OP_CONSTANT", "OP_PUSH_INT",
		  "OP_LIST", "OP_MAP",

		  "OP_ARRAY_SLICE",

		  "OP_CALL",
		  "OP_CALL_BUILTIN",
		  "OP_ASSERT",
		  "OP_PUSH_SCOPE", "OP_POP_SCOPE", "OP_BREAK", "OP_BREAK_IF",
		  "OP_LOOP_BEGIN", "OP_LOOP_NEXT", "OP_ALGO_MAP", "OP_ALGO_FILTER", "OP_ALGO_FIND", "OP_ALGO_COMPREHENSION",
		  "OP_POP", "OP_DUP", "OP_DUP2", "OP_SWAP", "OP_UNDER", "OP_PUSH_NULL", "OP_PUSH_0", "OP_PUSH_1",

		  "OP_WHERE", "OP_JMP_IF", "OP_JMP_UNLESS", "OP_POP_JMP_IF", "OP_POP_JMP_UNLESS", "OP_JMP",
		  "OP_LAMBDA", "OP_LAMBDA_WITH_CLOSURE", "OP_CREATE_INTERFACE",
};

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
		} else if(op == OP_LOOP_NEXT) {
			s << ": OP_LOOP_NEXT ";
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
		} else if(op == OP_LOOP_BEGIN) {
			s << ": OP_LOOP_BEGIN ";
			++n;
			s << static_cast<int>(instructions_[n]) << "\n";
		} else if(op < OP_INVALID) {
			s << ": " << "OP(" << static_cast<char>(op) << ")\n";
		} else if((op-OP_INVALID) < sizeof(OpNames)/sizeof(*OpNames)) {
			s << ": " << OpNames[(op-OP_INVALID)] << "\n";
		} else {
			s << ": UNKNOWN: " << op << "\n";
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

}
