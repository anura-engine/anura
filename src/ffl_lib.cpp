#include "formula_callable_definition.hpp"
#include "formula_object.hpp"

using namespace game_logic;

class StandardMathLib : public FormulaCallable {
	DECLARE_CALLABLE(StandardMathLib);
};

BEGIN_DEFINE_CALLABLE_NOBASE(StandardMathLib)
	BEGIN_DEFINE_FN(is_sorted, "(list) ->bool")
		const std::vector<variant>& v = FN_ARG(0).as_list();
		for(int i = 0; i < static_cast<int>(v.size())-1; ++i) {
			if(v[i] > v[i+1]) {
				return variant::from_bool(false);
			}
		}

		return variant::from_bool(true);
	END_DEFINE_FN
	BEGIN_DEFINE_FN(linear, "(decimal) ->decimal")
		return FN_ARG(0);
	END_DEFINE_FN
	BEGIN_DEFINE_FN(ease_in_quad, "(decimal) ->decimal")
		variant v = FN_ARG(0);
		return v^variant(2);
	END_DEFINE_FN
END_DEFINE_CALLABLE(StandardMathLib)

DEFINE_CALLABLE_CONSTRUCTOR(StandardMathLib, arg)
	return FormulaCallablePtr(new StandardMathLib);
END_DEFINE_CALLABLE_CONSTRUCTOR(StandardMathLib)
