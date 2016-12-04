/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include "intrusive_ptr.hpp"

#include "formula.hpp"
#include "formula_callable.hpp"
#include "unit_test.hpp"

namespace 
{
	using namespace game_logic;

	class MockChar : public FormulaCallable {
		variant getValue(const std::string& key) const override {
			if(key == "strength") {
				return variant(15);
			} else if(key == "agility") {
				return variant(12);
			}

			return variant(10);
		}
	};
	class MockParty : public FormulaCallable {
	public:
		MockParty() : c_(new MockChar) {
			for(int i = 0; i != 3; ++i) {
				i_.push_back(ffl::IntrusivePtr<MapFormulaCallable>(new MapFormulaCallable));
			}
		}
	private:
		variant getValue(const std::string& key) const override {
			if(key == "members") {
				i_[0]->add("strength",variant(12));
				i_[1]->add("strength",variant(16));
				i_[2]->add("strength",variant(14));
				std::vector<variant> members;
				for(int n = 0; n != 3; ++n) {
					members.push_back(variant(i_[n].get()));
				}

				return variant(&members);
			} else if(key == "char") {
				return variant(c_.get());
			} else {
				return variant(0);
			}
		}

		ffl::IntrusivePtr<MockChar> c_;
		std::vector<ffl::IntrusivePtr<MapFormulaCallable> > i_;

	};
}

UNIT_TEST(formula)
{
	ffl::IntrusivePtr<MockChar> cp(new MockChar);
	ffl::IntrusivePtr<MockParty> pp(new MockParty);
#define FML(a) Formula(variant(a))
	MockChar& c = *cp;
	MockParty& p = *pp;
	CHECK_EQ(FML("strength").execute(c).as_int(), 15);
	CHECK_EQ(FML("17").execute(c).as_int(), 17);
	CHECK_EQ(FML("strength/2 + agility").execute(c).as_int(), 19);
	CHECK_EQ(FML("(strength+agility)/2").execute(c).as_int(), 13);
	CHECK_EQ(FML("strength > 12").execute(c).as_int(), 1);
	CHECK_EQ(FML("strength > 18").execute(c).as_int(), 0);
	CHECK_EQ(FML("if(strength > 12, 7, 2)").execute(c).as_int(), 7);
	CHECK_EQ(FML("if(strength > 18, 7, 2)").execute(c).as_int(), 2);
	CHECK_EQ(FML("2 and 1").execute(c).as_int(), 1);
	CHECK_EQ(FML("2 and 0").execute(c).as_int(), 0);
	CHECK_EQ(FML("2 or 0").execute(c).as_int(), 2);
	CHECK_EQ(FML("-5").execute(c).as_int(),-5);
	CHECK_EQ(FML("not 5").execute(c).as_int(), 0);
	CHECK_EQ(FML("not 0").execute(c).as_int(), 1);
	CHECK_EQ(FML("abs(5)").execute(c).as_int(), 5);
	CHECK_EQ(FML("abs(-5)").execute(c).as_int(), 5);
	CHECK_EQ(FML("sign(5)").execute(c).as_int(), 1);
	CHECK_EQ(FML("sign(0)").execute(c).as_int(), 0);
	CHECK_EQ(FML("sign(-5)").execute(c).as_int(), -1);
	CHECK_EQ(FML("min(3,5)").execute(c).as_int(), 3);
	CHECK_EQ(FML("min(5,2)").execute(c).as_int(), 2);
	CHECK_EQ(FML("max(3,5)").execute(c).as_int(), 5);
	CHECK_EQ(FML("max(5,2)").execute(c).as_int(), 5);
	CHECK_EQ(FML("char.strength").execute(p).as_int(), 15);
	CHECK_EQ(FML("4^2").execute().as_int(), 16);
	CHECK_EQ(FML("2+3^3").execute().as_int(), 29);
	CHECK_EQ(FML("2*3^3+2").execute().as_int(), 56);
	CHECK_EQ(FML("9^3").execute().as_int(), 729);
	CHECK_EQ(FML("x*5 where x=1").execute().as_int(), 5);
	CHECK_EQ(FML("x*(a*b where a=2,b=1) where x=5").execute().as_int(), 10);
	CHECK_EQ(FML("char.strength * ability where ability=3").execute(p).as_int(), 45);
	CHECK_EQ(FML("'abcd' = 'abcd'").execute(p).as_bool(), true);
	CHECK_EQ(FML("'abcd' = 'acd'").execute(p).as_bool(), false);
	CHECK_EQ(FML("~strength, agility: ${strength}, ${agility}~").execute(c).as_string(),
	               "strength, agility: 15, 12");
	for(int n = 0; n != 128; ++n) {
		const int dice_roll = FML("3d6").execute().as_int();
		CHECK_GE(dice_roll, 3);
   		CHECK_LE(dice_roll, 18);
	}

	variant myarray = FML("[1,2,3]").execute();
	CHECK_EQ(myarray.num_elements(), 3);
	CHECK_EQ(myarray[0].as_int(), 1);
	CHECK_EQ(myarray[1].as_int(), 2);
	CHECK_EQ(myarray[2].as_int(), 3);

}

BENCHMARK(construct_int_variant)
{
	BENCHMARK_LOOP {
		variant v(0);
	}
}

BENCHMARK_ARG(formula, const std::string& fm)
{
	static MockParty p;
	Formula f = Formula(variant(fm));
	BENCHMARK_LOOP {
		f.execute(p);
	}
}

BENCHMARK_ARG_CALL(formula, integer, "0");
BENCHMARK_ARG_CALL(formula, where, "x where x = 5");
BENCHMARK_ARG_CALL(formula, add, "5 + 4");
BENCHMARK_ARG_CALL(formula, arithmetic, "(5 + 4)*17 + 12*9 - 5/2");
BENCHMARK_ARG_CALL(formula, read_input, "char");
BENCHMARK_ARG_CALL(formula, read_input_sub, "char.strength");
BENCHMARK_ARG_CALL(formula, array, "[4, 5, 8, 12, 17, 0, 19]");
BENCHMARK_ARG_CALL(formula, array_str, "['stand', 'walk', 'run', 'jump']");
BENCHMARK_ARG_CALL(formula, string, "'blah'");
BENCHMARK_ARG_CALL(formula, null_function, "null()");
BENCHMARK_ARG_CALL(formula, if_function, "if(4 > 5, 7, 8)");
