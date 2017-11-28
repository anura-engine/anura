/*
	Copyright (C) 2003-2016 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <vector>

#include "formula_callable.hpp"

namespace formula_vm {

enum OP {

		  //Binary operations which operate on the top two items
		  //on the stack. Pop those items and push the result.
		  // POP: 2
		  // PUSH: 1
		  // ARGS: NONE
		  OP_IN, OP_NOT_IN, OP_AND, OP_OR, OP_NEQ, OP_LTE, OP_GTE, OP_IS,
	OP_IS_NOT,

		  //Unary operations which operate on the top item on
		  //the stack replacing it with the result
		  // POP: 1
		  // PUSH: 1
		  // ARGS: NONE
		  OP_UNARY_NOT, OP_UNARY_SUB, OP_UNARY_STR, OP_UNARY_NUM_ELEMENTS,

		  //Increment the top item on the stack.
		  OP_INCREMENT,

		  //Lookup a symbol and place the value on the stack,
		  //the index of the symbol to lookup is
		  //given as an argument.
		  // POP: 0
		  // PUSH: 1
		  // ARGS: 1
		  OP_LOOKUP,

		  //Lookup a symbol by string and place the value on the stack,
		  //the string is given on the top of the stack
		  // POP: 1
		  // PUSH: 1
		  // ARGS: NONE
		  OP_LOOKUP_STR,
		  
		  //Binary operator which indexes a map or list.
		  // POP: 2
		  // PUSH: 1
		  // ARGS: NONE
		  OP_INDEX,

		  OP_INDEX_0,
		  OP_INDEX_1,
		  OP_INDEX_2,
		  
		  //Binary operator which indexes a map, list, or callable by string.
		  // POP: 2
		  // PUSH: 1
		  // ARGS: NONE
		  OP_INDEX_STR,

		  //Loads a constant given in the VM's static region
		  //onto the stack. Takes the index into the static region
		  //as an argument.
		  // POP: 0
		  // PUSH: 1
		  // ARGS: NONE
		  OP_CONSTANT,

		  //Push a given integer onto the stack.
		  // POP: 0
		  // PUSH: 1
		  // ARGS: 1
		  OP_PUSH_INT,

		  //Pops the top n items off the stack and creates a list or map
		  //out of them, pushing that onto the stack. n is given as an argument.
		  // POP: n
		  // PUSH: 1
		  // ARGS: 1
		  OP_LIST, OP_MAP,

		  //Pops the top 3 items off the stack. The first of these items
		  //is a list (or string), the next two are indexes used to slice the
		  //list. The list slice is pushed onto the stack.
		  // POP: 3
		  // PUSH: 1
		  // ARGS: NONE
		  OP_ARRAY_SLICE,

		  //Pops the top n+1 items off the stack. The first of these items
		  //is a function, and the rest are arguments. The function is invoked
		  //and the return value is pushed onto the stack.
		  // POP: n+1
		  // PUSH: 1
		  // ARGS: 1
		  OP_CALL,

		  OP_CALL_BUILTIN,

		  //Asserts with the top message on the stack.
		  // POP: 1
		  // PUSH: --
		  // ARGS: NONE
		  OP_ASSERT,

		  //Pops the top item off the stack and sets it as the current symbol scope
		  // POP: 1
		  // PUSH: 0
		  // ARGS: NONE
		  OP_PUSH_SCOPE,
		  
		  //Pops top symbol scope
		  // POP: 0
		  // PUSH: 0
		  // ARGS: NONE
		  OP_POP_SCOPE,

		  OP_BREAK,

		  //Breaks if the item on the stack is true. Pops the item on the stack.
		  OP_BREAK_IF,

		  //Map algorithm: next n instructions maps a single item.
		  //TOS: number of slots in callab,e
		  //TOS+1: item to map over
		  // POP: 2
		  // PUSH: 1
		  // ARGS: 1
		  OP_ALGO_MAP,
		  OP_ALGO_FILTER,

		  //Find algorithm: next n instructions should push true/false onto the stack
		  //if a given item matches or not. Will push the first found item onto the stack
		  //or null/-1 if nothing is found.
		  // POP: 1 (list of items to search)
		  // PUSH: 2 (item found, item index)
		  // ARGS: 1
		  OP_ALGO_FIND,

		  //Implementation of a list comprehension.
		  //Input stack state:
		  // (TOS): The base_slot of the parent callable.
		  // (TOS+1): The number of lists input
		  // (TOS+2...m): The lists which will be input to the comprehension.
		  //
		  //Will load a scope with items from the lists available. Will use
		  //all possible combinations of items from the list.
		  //
		  //The next n instructions will be executed for each combination.
		  //these instructions should either push a single item onto the stack
		  //or not touch the stack at all (if the item is filtered).
		  //
		  //All the input items will be popped off the stack and a list of
		  //the resulting items will be pushed onto the stack.
		  OP_ALGO_COMPREHENSION,

		  //Pops the top item off the stack.
		  // POP: 1
		  // PUSH: 0
		  // ARGS: NONE
	//   Previously at position 36, now listed at position 37.
	// Because enum ordinal 37 is already taken explicity by OP_MOD
	// (because of '%' being the character number 37), this is
	// getting an explicit value too, in order to avoid collisions
	// in C++ `switch` constructions.
	OP_POP = 38,

		  //Binary operators
		  // POP: 2
		  // PUSH: 1
		  // ARGS: NONE
		  OP_MOD='%',
          OP_MUL='*', OP_ADD='+', OP_SUB='-', OP_DIV='/',
		  OP_LT='<', OP_EQ='=', OP_GT='>',

		  //Duplicates the top item on the stack
		  // POP: 0 (PEEK 1)
		  // PUSH: 1
		  // ARGS: NONE
		  OP_DUP,

		  //Duplicates the top two items on the stack
		  // POP: 0 (PEEK 2)
		  // PUSH: 2
		  // ARGS: NONE
		  OP_DUP2,

		  //Swaps the top two items on the stack.
		  // POP: 0
		  // PUSH: 0 (pokes 2)
		  // ARGS: NONE
		  OP_SWAP,

		  //Inserts the top item on the stack to position n in the stack
		  // (0 = leave it unchanged)
		  // POP: 1
		  // PUSH: 1 (but not on the top)
		  // ARGS: 1
		  OP_UNDER,

		  //Pushes null onto the stack
		  // POP: 0
		  // PUSH: 1
		  // ARGS: NONE
		  OP_PUSH_NULL,
		  OP_PUSH_0,
		  OP_PUSH_1,

		  //Looks up the nth item in the static region, which must be a WhereVariableInfo
		  //pushes it onto the scope stack.
		  // POP: 0
		  // PUSH: 0
		  // ARGS: 1
		  OP_WHERE,

		  // POP: 1 + n (n = value on top of stack)
		  // PUSH: 1
		  // ARGS: 1
		  OP_INLINE_FUNCTION,

		  //Jumps n spaces forward if (or unless) the top item on the stack is true
		  //Note: doesn't pop the item it considers off the stack
		  // POP: 0
		  // PUSH: 0
		  // ARGS: 1
		  OP_JMP_IF, OP_JMP_UNLESS,

		  //versions of jump which also pop the item
		  OP_POP_JMP_IF, OP_POP_JMP_UNLESS,


		  //Jump unconditionally n spaces forward
		  // POP: 0
		  // PUSH: 0
		  // ARGS: 1
		  OP_JMP,

		  //Given TOS, a function, replaces it with a lambda instance.
		  // POP: 1
		  // PUSH: 1
		  // ARGS: NONE
		  OP_LAMBDA_WITH_CLOSURE,

		  //TOS is a FormulaInterfaceInstanceFactory and TOS+1 is an argument.
		  //Pops these two off the stack and pushes an interface created with this factory.
		  // POP: 2
		  // PUSH: 1,
		  // ARGS: NONE
		  OP_CREATE_INTERFACE,

		  //Pushes the top item from the stack onto the symbol stack.
		  OP_PUSH_SYMBOL_STACK,

		  OP_POP_SYMBOL_STACK,

		  OP_LOOKUP_SYMBOL_STACK,

		  OP_CALL_BUILTIN_DYNAMIC,
		  
		  OP_POW='^', OP_DICE='d',

		  };


class VirtualMachine
{
public:
	typedef short InstructionType;
	typedef unsigned short UnsignedInstructionType;
	typedef int ExtInstructionType;

	static bool isInstructionLoop(InstructionType instruction);
	static bool isInstructionJump(InstructionType instruction);

	class Iterator {
		const VirtualMachine* vm_;
		size_t index_;
	public:

		explicit Iterator(const VirtualMachine* vm) : vm_(vm), index_(0)
		{}

		const VirtualMachine* get_vm() const { return vm_; }

		InstructionType get() const;
		bool has_arg() const;
		InstructionType arg() const;
		InstructionType& arg_mutable();

		size_t get_index() const { return index_; }

		void next();
		bool at_end() const;
	};

	Iterator begin_itor() const {
		return Iterator(this);
	}

	variant execute(const game_logic::FormulaCallable& variables) const;

	void replaceInstructions(Iterator i1, Iterator i2, const std::vector<InstructionType>& new_instructions);

	void addInstruction(OP op);
	void addConstant(const variant& v);
	void addInt(InstructionType i);

	void addLoadConstantInstruction(const variant& v);

	//Add a jump instruction at the current position.
	//Use jumpToEnd later to get it to jump to that point
	//InstructionType should be OP_JMP_IF or OP_JMP_UNLESS
	int addJumpSource(InstructionType i);

	//Jump to the current position from the source position
	//given that was previously added with addJumpSource
	void jumpToEnd(int source);

	int getPosition() const;
	void addJumpToPosition(InstructionType i, int pos);

	void append(const VirtualMachine& other);

	void append(Iterator i1, Iterator i2, const VirtualMachine& other);

	std::string debugOutput(const InstructionType* p=nullptr) const;

	void setDebugInfo(const variant& parent_formula, unsigned short begin, unsigned short end);
private:
	void executeInternal(const game_logic::FormulaCallable& variables, std::vector<game_logic::FormulaCallablePtr>& variables_stack, std::vector<variant>& stack, std::vector<variant>& symbol_stack, const InstructionType* p, const InstructionType* p2) const;
	std::string debugPinpointLocation(const InstructionType* p, const std::vector<variant>& stack) const;
	std::vector<InstructionType> instructions_;
	std::vector<variant> constants_;

	struct DebugInfo {
		unsigned short bytecode_pos;
		unsigned short formula_pos;
	};

	std::vector<DebugInfo> debug_info_;
	variant parent_formula_;
};

}
