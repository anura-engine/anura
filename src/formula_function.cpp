/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <glm/gtx/intersect.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/sha1.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception_ptr.hpp>
#include <iomanip>
#include <iostream>
#include <iomanip>
#include <stack>
#include <cmath>
#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	round
#endif

#include <stdlib.h>

#include "geometry.hpp"

#ifdef USE_SVG
#include "cairo.hpp"
#endif

#include "array_callable.hpp"
#include "asserts.hpp"
#include "base64.hpp"
#include "code_editor_dialog.hpp"
#include "compress.hpp"
#include "custom_object.hpp"
#include "dialog.hpp"
#include "debug_console.hpp"
#include "draw_primitive.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula_callable_utils.hpp"
#include "formula_function.hpp"
#include "formula_function_registry.hpp"
#include "formula_internal.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"
#include "formula_vm.hpp"
#include "hex.hpp"
#include "hex_helper.hpp"
#include "level_runner.hpp"
#include "lua_iface.hpp"
#include "md5.hpp"
#include "module.hpp"
#include "random.hpp"
#include "rectangle_rotator.hpp"
#include "string_utils.hpp"
#include "unit_test.hpp"
#include "variant_callable.hpp"
#include "controls.hpp"
#include "pathfinding.hpp"
#include "preferences.hpp"
#include "random.hpp"
#include "level.hpp"
#include "json_parser.hpp"
#include "utf8_to_codepoint.hpp"
#include "uuid.hpp"
#include "variant_type_check.hpp"
#include "variant_utils.hpp"
#include "voxel_model.hpp"
#include "widget_factory.hpp"

#include "Texture.hpp"
#include "Cursor.hpp"

#include <boost/regex.hpp>
#if defined(_MSC_VER) && _MSC_VER < 1800
#include <boost/math/special_functions/asinh.hpp>
#include <boost/math/special_functions/acosh.hpp>
#include <boost/math/special_functions/atanh.hpp>
using boost::math::asinh;
using boost::math::acosh;
using boost::math::atanh;
#endif

using namespace formula_vm;

PREF_BOOL(log_instrumentation, false, "Make instrument() FFL calls log to the console as well as the F7 profiler");
PREF_BOOL(dump_to_console, true, "Send dump() to the console");
PREF_STRING(log_console_filter, "", "");
PREF_STRING(auto_update_status, "", "");
PREF_INT(fake_time_adjust, 0, "Adjusts the time known to the game by the specified number of seconds.");
extern variant g_auto_update_info;

std::map<std::string, variant> g_user_info_registry;

namespace 
{
	//these global variables make the EVAL_ARG/NUM_ARGS macros work
	//for functions not yet converted to the new syntax. Remove eventually.
	const variant* passed_args = nullptr;
	int num_passed_args = -1;

	const std::string FunctionModule = "core";

	const float radians_to_degrees = 57.29577951308232087f;
	const std::string& EmptyStr() {
		static std::string* empty_str = new std::string;
		return *empty_str;
	}

	using namespace game_logic;
	std::string read_identifier_expression(const FormulaExpression& expr) {
		variant literal;
		expr.isLiteral(literal);
		if(literal.is_string()) {
			return literal.as_string();
		} else {
			std::string result;
			if(expr.isIdentifier(&result)) {
				return result;
			}

			ASSERT_LOG(false, "Expected identifier, found " << expr.str() << expr.debugPinpointLocation());
			return "";
		}
	}
}

namespace game_logic 
{
	ExpressionPtr createVMExpression(const formula_vm::VirtualMachine vm, variant_type_ptr t, const FormulaExpression& o);
	
	FormulaExpression::FormulaExpression(const char* name) : FormulaCallable(GARBAGE_COLLECTOR_EXCLUDE), name_(name ? name : "unknown"), begin_str_(EmptyStr().begin()), end_str_(EmptyStr().end()), ntimes_called_(0)
	{
	}

	FormulaExpression::~FormulaExpression()
	{
	}

	std::vector<ConstExpressionPtr> FormulaExpression::queryChildren() const {
		std::vector<ConstExpressionPtr> result = getChildren();
		result.erase(std::remove(result.begin(), result.end(), ConstExpressionPtr()), result.end());
		return result;
	}

	std::vector<ConstExpressionPtr> FormulaExpression::queryChildrenRecursive() const {
		std::vector<ConstExpressionPtr> result;
		result.push_back(ConstExpressionPtr(this));
		for(ConstExpressionPtr child : queryChildren()) {
			if(child.get() != this) {
				std::vector<ConstExpressionPtr> items = child->queryChildrenRecursive();
				result.insert(result.end(), items.begin(), items.end());
			}
		}

		return result;
	}

	void FormulaExpression::emitVM(formula_vm::VirtualMachine& vm) const
	{
		for(auto p : queryChildrenRecursive()) {
			LOG_ERROR("  Sub-expr: " << p->name() << ": ((" << p->str() << ")) -> can_vm = " << (p->canCreateVM() ? "yes" : "no") << "\n");
		}
		ASSERT_LOG(false, "Trying to emit VM from non-VMable expression: " << name() << " :: " << str());
	}

	void FormulaExpression::copyDebugInfoFrom(const FormulaExpression& o)
	{
		setDebugInfo(o.parent_formula_, o.begin_str_, o.end_str_);
	}

	void FormulaExpression::setDebugInfo(const variant& parent_formula,
											std::string::const_iterator begin_str,
											std::string::const_iterator end_str)
	{
		parent_formula_ = parent_formula;
		begin_str_ = begin_str;
		end_str_ = end_str;
	}

	void FormulaExpression::setVMDebugInfo(formula_vm::VirtualMachine& vm) const
	{
		if(!hasDebugInfo()) {
			return;
		}

		const std::string& s = parent_formula_.as_string();

		vm.setDebugInfo(parent_formula_, begin_str_ - s.begin(), end_str_ - s.begin());
	}

	void FormulaExpression::setDebugInfo(const FormulaExpression& o)
	{
		setDebugInfo(o.parent_formula_, o.begin_str_, o.end_str_);
	}

	bool FormulaExpression::hasDebugInfo() const
	{
		return parent_formula_.is_string() && parent_formula_.get_debug_info();
	}

	ConstFormulaCallableDefinitionPtr FormulaExpression::getTypeDefinition() const
	{
		variant_type_ptr type = queryVariantType();
		if(type) {
			return ConstFormulaCallableDefinitionPtr(type->getDefinition());
		}

		return nullptr;
	}

	std::string pinpoint_location(variant v, std::string::const_iterator begin)
	{
		return pinpoint_location(v, begin, begin);
	}

	std::string pinpoint_location(variant v, std::string::const_iterator begin,
											 std::string::const_iterator end,
											 PinpointedLoc* pos_info)
	{
		std::string str(begin, end);
		if(!v.is_string() || !v.get_debug_info()) {
			return "Unknown location (" + str + ")\n";
		}

		int line_num = v.get_debug_info()->line;
		int begin_line_base = v.get_debug_info()->column;

		std::string::const_iterator begin_line = v.as_string().begin();
		while(std::find(begin_line, begin, '\n') != begin) {
			begin_line_base = 0;
			begin_line = std::find(begin_line, begin, '\n')+1;
			++line_num;
		}

		//this is the real start of the line. begin_line will advance
		//to the first non-whitespace character.
		std::string::const_iterator real_start_of_line = begin_line;

		while(begin_line != begin && util::c_isspace(*begin_line)) {
			++begin_line;
		}

		std::string::const_iterator end_line = std::find(begin_line, v.as_string().end(), '\n');

		std::string line(begin_line, end_line);
		auto pos = begin - begin_line;

		if(pos_info) {
			const int col = static_cast<int>((begin - real_start_of_line) + begin_line_base);
			pos_info->begin_line = line_num;
			pos_info->begin_col = col+1;

			int end_line = line_num;
			int end_col = col+1;
			for(auto itor = begin; itor != end; ++itor) {
				if(*itor == '\n') {
					end_col = 1;
					end_line++;
				} else {
					end_col++;
				}
			}

			pos_info->end_line = end_line;
			pos_info->end_col = end_col;
		}

		if(pos > 40) {
			line.erase(line.begin(), line.begin() + pos - 40);
			pos = 40;
			std::fill(line.begin(), line.begin() + 3, '.');
		}

		if(line.size() > 78) {
			line.resize(78);
			std::fill(line.end() - 3, line.end(), '.');
		}

		std::ostringstream s;
		s << "At " << *v.get_debug_info()->filename << " " << line_num << ":\n";
		s << line << "\n";
		for(int n = 0; n != pos; ++n) {
			s << " ";
		}
		s << "^";

		if(end > begin && static_cast<unsigned>(pos + (end - begin)) < line.size()) {
			for(int n = 0; n < (end - begin)-1; ++n) {
				s << "-";
			}
			s << "^";
		}

		s << "\n";


		return s.str();
	}

	std::string FormulaExpression::debugPinpointLocation(PinpointedLoc* loc) const
	{
		if(!hasDebugInfo()) {
			return "Unknown Location (" + str() + ")\n";
		}

		return pinpoint_location(parent_formula_, begin_str_, end_str_, loc);
	}

	std::pair<int,int> FormulaExpression::debugLocInFile() const
	{
		if(!hasDebugInfo()) {
			return std::pair<int,int>(-1,-1);
		}
		return std::pair<int,int>(static_cast<int>(begin_str_ - parent_formula_.as_string().begin()),
								  static_cast<int>(end_str_ - parent_formula_.as_string().begin()));
	}

	variant FormulaExpression::executeMember(const FormulaCallable& variables, std::string& id, variant* variant_id) const
	{
		Formula::failIfStaticContext();
		ASSERT_LOG(false, "Trying to set illegal value: " << str() << "\n" << debugPinpointLocation());
		return variant();
	}

	void FormulaExpression::optimizeChildToVM(ExpressionPtr& expr)
	{
		if(expr) {
			bool can_vm = expr->canCreateVM();
			auto opt = expr->optimizeToVM();
			if(opt.get() != nullptr) {
				ASSERT_LOG(can_vm == opt->canCreateVM(), "Expression says it cannot be made into a VM but it can: " << expr->str());
				expr = opt;

			}

			if(can_vm && expr->isVM() == false) {
				ASSERT_LOG(false, "Expressions says it can be made into a VM but it cannot: " << expr->name() << " :: " << expr->str());
			}
		}
	}
	

	namespace 
	{
		variant split_variant_if_str(const variant& s)
		{
			if(!s.is_string()) {
				return s;
			}

			std::vector<std::string> v = util::split(s.as_string(), "");
			std::vector<variant> res;
			res.reserve(v.size());
			for(const std::string& str : v) {
				res.push_back(variant(str));
			}

			return variant(&res);
		}

		std::set<class ffl_cache*>& get_all_ffl_caches()
		{
			static std::set<class ffl_cache*>* caches = new std::set<class ffl_cache*>;
			return *caches;
		}

		class ffl_cache : public FormulaCallable
		{
		public:
			struct Entry {
				Entry() : use_weak(false) {}
				variant key;
				variant obj;
				ffl::weak_ptr<FormulaCallable> weak;
				bool use_weak;
			};

			void setName(const std::string& name) { name_ = name; }
			const std::string& getName() const { return name_; }

			explicit ffl_cache(int max_entries) : max_entries_(max_entries)
			{
				get_all_ffl_caches().insert(this);
			}

			~ffl_cache()
			{
				get_all_ffl_caches().erase(this);
			}

			const variant* get(const variant& key) const {
				std::map<variant, std::list<Entry>::iterator>::iterator i = cache_.find(key);
				if(i != cache_.end()) {
					if(i->second->use_weak && i->second->weak.get() == nullptr) {
						lru_.erase(i->second);
						cache_.erase(i);
						return nullptr;
					} else if(i->second->use_weak) {
						auto weak = i->second->weak.get();
						i->second->use_weak = false;
						i->second->obj = variant(weak.get());
						i->second->weak.reset();
					}

					lru_.splice(lru_.begin(), lru_, i->second);

					const Entry& entry = *i->second;
					return &entry.obj;
				} else {
					return nullptr;
				}
			}

			void store(const variant& key, const variant& value) const {
				lru_.push_front(Entry());
				lru_.front().obj = value;
				lru_.front().key = key;

				bool succeeded = cache_.insert(std::pair<variant,std::list<Entry>::iterator>(key, lru_.begin())).second;
				ASSERT_LOG(succeeded, "Inserted into cache when there is already a valid entry: " << key.write_json());

				if(cache_.size() > max_entries_) {
					int num_delete = std::max(1, max_entries_/5);
					int looked = 0;
					while(num_delete > 0 && looked < static_cast<int>(cache_.size()) && !lru_.empty()) {
						auto end = lru_.end();
						--end;
						Entry& entry = *end;
						if(entry.use_weak) {
							if(entry.weak.get() == nullptr) {
								cache_.erase(entry.key);
								lru_.erase(end);
								--num_delete;
							} else {
								lru_.splice(lru_.begin(), lru_, end);
							}
						} else if( false && entry.obj.refcount() > 1) {
							lru_.splice(lru_.begin(), lru_, end);
						} else {
							cache_.erase(entry.key);
							lru_.erase(end);
							--num_delete;
						}

						++looked;
					}

					if(cache_.size() > max_entries_) {
						for(Entry& entry : lru_) {
							if(entry.use_weak == false && entry.obj.is_callable()) {
								entry.weak.reset(entry.obj.mutable_callable());
								entry.obj = variant();
								entry.use_weak = true;
							}
						}
						LOG_ERROR("Failed to delete all objects from cache. " << cache_.size() << "/" << max_entries_ << " remain");
					}
				}
			}

			void clear() {
				lru_.clear();
				cache_.clear();
			}

			void surrenderReferences(GarbageCollector* collector) override {
				for(std::pair<const variant, std::list<Entry>::iterator>& p : cache_) {
					collector->surrenderVariant(&p.first);
					collector->surrenderVariant(&p.second->key);
					collector->surrenderVariant(&p.second->obj);
				}
			}

			std::string debugObjectName() const override {
				std::ostringstream s;
				s << "ffl_cache(" << name_ << ", " << lru_.size() << "/" << max_entries_ << ")";
				return s.str();
			}
		private:
			DECLARE_CALLABLE(ffl_cache);

			mutable std::list<Entry> lru_;
			mutable std::map<variant, std::list<Entry>::iterator> cache_;
			std::string name_;
			int max_entries_;
		};

		BEGIN_DEFINE_CALLABLE_NOBASE(ffl_cache)
		DEFINE_FIELD(name, "string")
			return variant(obj.name_);
		DEFINE_FIELD(enumerate, "[any]")
			std::vector<variant> result;
			for(auto item : obj.lru_) {
				result.push_back(item.obj);
			}

			return variant(&result);

		DEFINE_FIELD(keys, "[any]")
			std::vector<variant> result;
			for(auto item : obj.lru_) {
				result.push_back(item.key);
			}

			return variant(&result);
		DEFINE_FIELD(num_entries, "int")
			return variant(obj.cache_.size());
		DEFINE_FIELD(max_entries, "int")
			return variant(obj.max_entries_);
		DEFINE_FIELD(all, "[builtin ffl_cache]")
			std::vector<variant> v;
			for(auto item : get_all_ffl_caches()) {
				v.push_back(variant(item));
			}

			return variant(&v);

		BEGIN_DEFINE_FN(get, "(any) ->any")
			variant key = FN_ARG(0);
			const variant* result = obj.get(key);
			if(result != nullptr) {
				return *result;
			}

			return variant();
		END_DEFINE_FN

		BEGIN_DEFINE_FN(contains, "(any) ->bool")
			variant key = FN_ARG(0);
			const variant* result = obj.get(key);

			return variant::from_bool(result != nullptr);
		END_DEFINE_FN

		BEGIN_DEFINE_FN(store, "(any, any) ->commands")
			variant key = FN_ARG(0);
			variant value = FN_ARG(1);

			ffl::IntrusivePtr<ffl_cache> ptr(const_cast<ffl_cache*>(&obj));
			return variant(new game_logic::FnCommandCallable("cache_store", [=]() {
				if(ptr->get(key) == nullptr) {
					ptr->store(key, value);
				}
			}));
		END_DEFINE_FN

		BEGIN_DEFINE_FN(clear, "() ->commands")
			ffl::IntrusivePtr<ffl_cache> ptr(const_cast<ffl_cache*>(&obj));
			return variant(new game_logic::FnCommandCallable("cache_clear", [=]() {
				ptr->clear();
			}));
		END_DEFINE_FN
		END_DEFINE_CALLABLE(ffl_cache)

		class Geometry : public game_logic::FormulaCallable {
		public:
			Geometry() {}
		private:
			DECLARE_CALLABLE(Geometry);
		};

		BEGIN_DEFINE_CALLABLE_NOBASE(Geometry)

		BEGIN_DEFINE_FN(line_segment_intersection, "(decimal,decimal,decimal,decimal,decimal,decimal,decimal,decimal)->[decimal,decimal]|null")
			const decimal a_x1 = FN_ARG(0).as_decimal();
			const decimal a_y1 = FN_ARG(1).as_decimal();
			const decimal a_x2 = FN_ARG(2).as_decimal();
			const decimal a_y2 = FN_ARG(3).as_decimal();
			const decimal b_x1 = FN_ARG(4).as_decimal();
			const decimal b_y1 = FN_ARG(5).as_decimal();
			const decimal b_x2 = FN_ARG(6).as_decimal();
			const decimal b_y2 = FN_ARG(7).as_decimal();

			decimal d = (a_x1-a_x2)*(b_y1-b_y2) - (a_y1-a_y2)*(b_x1-b_x2);
			if(d == decimal::from_int(0)) {
				return variant();
			}

			decimal xi = ((b_x1-b_x2)*(a_x1*a_y2-a_y1*a_x2)-(a_x1-a_x2)*(b_x1*b_y2-b_y1*b_x2))/d;
			decimal yi = ((b_y1-b_y2)*(a_x1*a_y2-a_y1*a_x2)-(a_y1-a_y2)*(b_x1*b_y2-b_y1*b_x2))/d;

			if(xi < std::min(a_x1,a_x2) || xi > std::max(a_x1,a_x2)) {
				return variant();
			}

			if(xi < std::min(b_x1,b_x2) || xi > std::max(b_x1,b_x2)) {
				return variant();
			}

			std::vector<variant> v;
			v.push_back(variant(xi));
			v.push_back(variant(yi));
			return variant(&v);

		END_DEFINE_FN

		END_DEFINE_CALLABLE(Geometry)

		FUNCTION_DEF(geometry_api, 0, 1, "geometry_api()")
			static Geometry* geo = new Geometry;
			static variant holder(geo);
			return holder;
		FUNCTION_ARGS_DEF
		RETURN_TYPE("builtin geometry")
		END_FUNCTION_DEF(geometry_api)

#ifdef USE_SVG
	FUNCTION_DEF(canvas, 0, 0, "canvas() -> canvas object")
		static variant result(new graphics::cairo_callable());
		return result;
	FUNCTION_ARGS_DEF
		RETURN_TYPE("builtin cairo_callable")

	END_FUNCTION_DEF(canvas)
#endif

		class DateTime : public game_logic::FormulaCallable {
		public:
			DateTime(time_t unix, const tm* tm) : unix_(unix), tm_(*tm)
			{}
			
		private:
			DECLARE_CALLABLE(DateTime);

			time_t unix_;
			tm tm_;
		};

		BEGIN_DEFINE_CALLABLE_NOBASE(DateTime)
		DEFINE_FIELD(unix, "int")
			return variant(static_cast<int>(obj.unix_));
		DEFINE_FIELD(second, "int")
			return variant(obj.tm_.tm_sec);
		DEFINE_FIELD(minute, "int")
			return variant(obj.tm_.tm_min);
		DEFINE_FIELD(hour, "int")
			return variant(obj.tm_.tm_hour);
		DEFINE_FIELD(day, "int")
			return variant(obj.tm_.tm_mday);
		DEFINE_FIELD(yday, "int")
			return variant(obj.tm_.tm_yday);
		DEFINE_FIELD(month, "int")
			return variant(obj.tm_.tm_mon + 1);
		DEFINE_FIELD(year, "int")
			return variant(obj.tm_.tm_year + 1900);
		DEFINE_FIELD(is_dst, "bool")
			return variant::from_bool(obj.tm_.tm_isdst != 0);
		DEFINE_FIELD(weekday, "string")
			std::string weekday;
			switch(obj.tm_.tm_wday) {
				case 0: weekday = "Sunday"; break;
				case 1: weekday = "Monday"; break;
				case 2: weekday = "Tuesday"; break;
				case 3: weekday = "Wednesday"; break;
				case 4: weekday = "Thursday"; break;
				case 5: weekday = "Friday"; break;
				case 6: weekday = "Saturday"; break;
			};
			return variant(weekday);

		END_DEFINE_CALLABLE(DateTime)

		FUNCTION_DEF(time, 0, 1, "time(int unix_time) -> date_time: returns the current real time")
			Formula::failIfStaticContext();
			time_t t;
			
			if(NUM_ARGS == 0) {
				t = time(nullptr) + g_fake_time_adjust;
			} else {
				t = EVAL_ARG(0).as_int();
			}
			tm* ltime = localtime(&t);
			if(ltime == nullptr) {
				t = time(nullptr) + g_fake_time_adjust;
				ltime = localtime(&t);
				ASSERT_LOG(ltime != nullptr, "Could not get time()");
				
			}
			return variant(new DateTime(t, ltime));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int");
		RETURN_TYPE("builtin date_time")
		END_FUNCTION_DEF(time)

		FUNCTION_DEF(get_debug_info, 1, 1, "get_debug_info(value)")

			variant value = EVAL_ARG(0);

			const variant::debug_info* info = value.get_debug_info();

			if(info == nullptr) {
				return variant();
			}

			variant_builder b;
			if(info->filename) {
				b.add("filename", variant(*info->filename));
			}

			b.add("line", info->line);
			b.add("col", info->column);
			b.add("end_line", info->end_line);
			b.add("end_col", info->end_column);

			return b.build();

		FUNCTION_ARGS_DEF
			ARG_TYPE("any");
			RETURN_TYPE("null|{filename: string|null, line: int, col: int, end_line: int, end_col: int}")
		END_FUNCTION_DEF(get_debug_info)

		FUNCTION_DEF(set_user_info, 2, 2, "set_user_info(string, any): sets some user info used in stats collection")
			std::string key = EVAL_ARG(0).as_string();
			variant value = EVAL_ARG(1);

			return variant(new FnCommandCallable("set_user_info", [=]() { g_user_info_registry[key] = value; }));
			
		RETURN_TYPE("commands")
		END_FUNCTION_DEF(set_user_info)

		FUNCTION_DEF(current_level, 0, 0, "current_level(): return the current level the game is in")
			Formula::failIfStaticContext();
			return variant(&Level::current());
		RETURN_TYPE("builtin level")
		END_FUNCTION_DEF(current_level)

		FUNCTION_DEF(cancel, 0, 0, "cancel(): cancel the current command pipeline")
			return variant(new FnCommandCallable("cancel", [=]() { deferCurrentCommandSequence(); }));
		RETURN_TYPE("commands")
		END_FUNCTION_DEF(cancel)

		FUNCTION_DEF(overload, 1, -1, "overload(fn...): makes an overload of functions")
			std::vector<variant> functions;
			for(int n = 0; n != NUM_ARGS; ++n) {
				functions.push_back(EVAL_ARG(n));
				ASSERT_LOG(functions.back().is_function(), "CALL TO overload() WITH NON-FUNCTION VALUE " << functions.back().write_json());
			}

			return variant::create_function_overload(functions);

		FUNCTION_TYPE_DEF
			int min_args = -1;
			std::vector<std::vector<variant_type_ptr> > arg_types;
			std::vector<variant_type_ptr> return_types;
			std::vector<variant_type_ptr> function_types;
			for(int n = 0; n != NUM_ARGS; ++n) {
				variant_type_ptr t = args()[n]->queryVariantType();
				function_types.push_back(t);
				std::vector<variant_type_ptr> a;
				variant_type_ptr return_type;
				int nargs = -1;
				if(t->is_function(&a, &return_type, &nargs) == false) {
					ASSERT_LOG(false, "CALL to overload() with non-function type: " << args()[n]->debugPinpointLocation());
				}

				return_types.push_back(return_type);
				if(min_args == -1 || nargs < min_args) {
					min_args = nargs;
				}

				for(unsigned m = 0; m != a.size(); ++m) {
					if(arg_types.size() <= m) {
						arg_types.resize(m+1);
					}

					arg_types[m].push_back(a[m]);
				}
			}

			if(min_args < 0) {
				min_args = 0;
			}

			variant_type_ptr return_union = variant_type::get_union(return_types);
			std::vector<variant_type_ptr> arg_union;
			for(int n = 0; n != arg_types.size(); ++n) {
				arg_union.push_back(variant_type::get_union(arg_types[n]));
			}

			return variant_type::get_function_overload_type(variant_type::get_function_type(arg_union, return_union, min_args), function_types);
		END_FUNCTION_DEF(overload)

		FUNCTION_DEF(addr, 1, 1, "addr(obj): Provides the address of the given object as a string. Useful for distinguishing objects")
	
			variant v = EVAL_ARG(0);
			FormulaCallable* addr = nullptr;
			if(!v.is_null()) {
				addr = v.convert_to<FormulaCallable>();
			}
			char buf[128];
			sprintf(buf, "%p", addr);
			return variant(std::string(buf));

		FUNCTION_ARGS_DEF
			ARG_TYPE("object|null");
			RETURN_TYPE("string");
		END_FUNCTION_DEF(addr)

		FUNCTION_DEF(get_call_stack, 0, 0, "get_call_stack()")
			return variant(get_call_stack());
		FUNCTION_ARGS_DEF
			RETURN_TYPE("string");
		END_FUNCTION_DEF(get_call_stack)

		FUNCTION_DEF(get_full_call_stack, 0, 0, "get_full_call_stack()")
			return variant(get_full_call_stack());
		FUNCTION_ARGS_DEF
			RETURN_TYPE("string");
		END_FUNCTION_DEF(get_full_call_stack)

		FUNCTION_DEF(create_cache, 0, 1, "create_cache(max_entries=4096): makes an FFL cache object")
			Formula::failIfStaticContext();
			std::string name = "";
			int max_entries = 4096;
			if(NUM_ARGS >= 1) {
				variant arg = EVAL_ARG(0);
				if(arg.is_int()) {
					max_entries = arg.as_int();
				} else {
					const std::map<variant,variant>& m = arg.as_map();
					max_entries = arg[variant("size")].as_int(max_entries);
					name = arg[variant("name")].as_string_default("");
				}
			}

			auto cache = new ffl_cache(max_entries);
			cache->setName(name);
			return variant(cache);
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|{size: int|null, name: string|null}");
			RETURN_TYPE("object");
		END_FUNCTION_DEF(create_cache)

		FUNCTION_DEF(global_cache, 0, 2, "create_cache(max_entries=4096): makes an FFL cache object")
			std::string name = "global";
			int max_entries = 4096;
			for(int n = 0; n < NUM_ARGS; ++n) {
				variant arg = EVAL_ARG(n);
				if(arg.is_int()) {
					max_entries = arg.as_int();
				} else if(arg.is_string()) {
					name = arg.as_string();
				}
			}
			auto cache = new ffl_cache(max_entries);
			cache->setName(name);
			return variant(cache);
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|string");
			ARG_TYPE("int");
			RETURN_TYPE("object");
		END_FUNCTION_DEF(global_cache)

		FUNCTION_DEF_CTOR(query_cache, 3, 3, "query_cache(ffl_cache, key, expr): ")
		FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
				return narg != 2;
			}
		FUNCTION_DEF_IMPL
			const variant key = EVAL_ARG(1);

			variant cache_variant = EVAL_ARG(0);
			const ffl_cache* cache = cache_variant.try_convert<ffl_cache>();
			ASSERT_LOG(cache != nullptr, "ILLEGAL CACHE ARGUMENT TO query_cache");
	
			const variant* result = cache->get(key);
			if(result != nullptr) {
				return *result;
			}

			const variant value = args()[2]->evaluate(variables);
			cache->store(key, value);
			return value;

		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_TYPE_DEF
			return args()[2]->queryVariantType();
		END_FUNCTION_DEF(query_cache)

		FUNCTION_DEF(game_preferences, 0, 0, "game_preferences() ->builtin game_preferences")
			Formula::failIfStaticContext();
			return preferences::ffl_interface();
		FUNCTION_ARGS_DEF
			RETURN_TYPE("builtin game_preferences");
		END_FUNCTION_DEF(game_preferences)

		FUNCTION_DEF(md5, 1, 1, "md5(string) ->string")
			return variant(md5::sum(EVAL_ARG(0).as_string()));
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			RETURN_TYPE("string");
		END_FUNCTION_DEF(md5)

		FUNCTION_DEF(if, 2, -1, "if(a,b,c)")

			const int nargs = static_cast<int>(NUM_ARGS);
			for(int n = 0; n < nargs-1; n += 2) {
				const bool result = EVAL_ARG(n).as_bool();
				if(result) {
					return EVAL_ARG(n+1);
				}
			}

			if((nargs % 2) == 0) {
				return variant();
			}

			return EVAL_ARG(nargs-1);

		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_OPTIMIZE

			variant v;
			if(NUM_ARGS <= 3 && args()[0]->canReduceToVariant(v)) {
				if(v.as_bool()) {
					return args()[1];
				} else {
					if(NUM_ARGS == 3) {
						return args()[2];
					} else {
						return ExpressionPtr(new VariantExpression(variant()));
					}
				}
			}

			return ExpressionPtr();

		CAN_VM
			return canChildrenVM();
		FUNCTION_VM

			for(ExpressionPtr& a : args_mutable()) {
				optimizeChildToVM(a);
			}

			for(auto a : args()) {
				if(a->canCreateVM() == false) {
					return ExpressionPtr();
				}
			}

			std::vector<int> jump_to_end_sources;

			for(int n = 0; n+1 < static_cast<int>(NUM_ARGS); n += 2) {
				args()[n]->emitVM(vm);
				const int jump_source = vm.addJumpSource(OP_JMP_UNLESS);
				vm.addInstruction(OP_POP);
				args()[n+1]->emitVM(vm);
				jump_to_end_sources.push_back(vm.addJumpSource(OP_JMP));

				vm.jumpToEnd(jump_source);
				vm.addInstruction(OP_POP);
			}

			if(NUM_ARGS%2 == 1) {
				args().back()->emitVM(vm);
			} else {
				vm.addInstruction(OP_PUSH_NULL);
			}

			for(int j : jump_to_end_sources) {
				vm.jumpToEnd(j);
			}

			return createVMExpression(vm, queryVariantType(), *this);

		FUNCTION_TYPE_DEF
			std::vector<variant_type_ptr> types;
			types.push_back(args()[1]->queryVariantType());
			const int nargs = static_cast<int>(NUM_ARGS);
			for(int n = 1; n < nargs; n += 2) {
				types.push_back(args()[n]->queryVariantType());
			}

			if((nargs % 2) == 1) {
				types.push_back(args()[nargs-1]->queryVariantType());
			} else {
				types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
			}

			return variant_type::get_union(types);
		END_FUNCTION_DEF(if)

		class bound_command : public game_logic::CommandCallable
		{
		public:
			bound_command(variant target, const std::vector<variant>& args)
			  : target_(target), args_(args)
			{}
			virtual void execute(game_logic::FormulaCallable& ob) const override {
				ob.executeCommand(target_(args_));
			}
		private:
			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderVariant(&target_);
				for(variant& v : args_) {
					collector->surrenderVariant(&v);
				}
			};

			variant target_;
			std::vector<variant> args_;
		};

		FUNCTION_DEF(bind, 1, -1, "bind(fn, args...)")
			variant fn = EVAL_ARG(0);

			std::vector<variant> arg_values;
			for(int n = 1; n != NUM_ARGS; ++n) {
				arg_values.push_back(EVAL_ARG(n));
			}

			return fn.bind_args(arg_values);
		FUNCTION_TYPE_DEF
			variant_type_ptr type = args()[0]->queryVariantType();

			std::vector<variant_type_ptr> fn_args;
			variant_type_ptr return_type;
			int min_args = 0;

			if(type->is_function(&fn_args, &return_type, &min_args)) {
				const int nargs = static_cast<int>(NUM_ARGS - 1);
				min_args = std::max<int>(0, min_args - nargs);
				if(nargs <= static_cast<int>(fn_args.size())) {
					fn_args.erase(fn_args.begin(), fn_args.begin() + nargs);
				} else {
					ASSERT_LOG(false, "bind called with too many arguments: " << fn_args.size() << " vs " << nargs);
				}

				return variant_type::get_function_type(fn_args, return_type, min_args);
			} else {
				return variant_type::get_type(variant::VARIANT_TYPE_FUNCTION);
			}

		FUNCTION_ARGS_DEF
			ARG_TYPE("function");
		END_FUNCTION_DEF(bind)

		FUNCTION_DEF(bind_command, 1, -1, "bind_command(fn, args..)")
			variant fn = EVAL_ARG(0);
			if(fn.type() != variant::VARIANT_TYPE_MULTI_FUNCTION) {
				fn.must_be(variant::VARIANT_TYPE_FUNCTION);
			}
			std::vector<variant> args_list;
			for(int n = 1; n != NUM_ARGS; ++n) {
				args_list.push_back(EVAL_ARG(n));
			}

			std::string message;
			ASSERT_LOG(fn.function_call_valid(args_list, &message), "Error in bind_command: functions args do not match: " << message);
	
			return variant(new bound_command(fn, args_list));

		FUNCTION_ARGS_DEF
			ARG_TYPE("function");
		FUNCTION_TYPE_DEF
			return variant_type::get_commands();
		END_FUNCTION_DEF(bind_command)

		FUNCTION_DEF(bind_closure, 2, 2, "bind_closure(fn, obj): binds the given lambda fn to the given object closure")
			variant fn = EVAL_ARG(0);
			return fn.bind_closure(EVAL_ARG(1).as_callable());

		FUNCTION_ARGS_DEF
			ARG_TYPE("function");
		END_FUNCTION_DEF(bind_closure)

		FUNCTION_DEF(singleton, 1, 1, "singleton(string typename): create a singleton object with the given typename")
			variant type = EVAL_ARG(0);

			static std::map<variant, ffl::IntrusivePtr<FormulaObject> > cache;
			if(cache.count(type)) {
				return variant(cache[type].get());
			}

			ffl::IntrusivePtr<FormulaObject> obj(FormulaObject::create(type.as_string(), variant()));
			cache[type] = obj;
			return variant(obj.get());
		FUNCTION_TYPE_DEF
			variant literal;
			args()[0]->isLiteral(literal);
			if(literal.is_string()) {
				return variant_type::get_class(literal.as_string());
			} else {
				return variant_type::get_any();
			}
		END_FUNCTION_DEF(singleton)

		FUNCTION_DEF(construct, 1, 2, "construct(string typename, arg): construct an object with the given typename")
			Formula::failIfStaticContext();
			variant type = EVAL_ARG(0);
			variant arg;
			if(NUM_ARGS >= 2) {
				arg = EVAL_ARG(1);
			}

			ffl::IntrusivePtr<FormulaObject> obj(FormulaObject::create(type.as_string(), arg));
			return variant(obj.get());
		FUNCTION_TYPE_DEF
			variant literal;
			args()[0]->isLiteral(literal);
			if(literal.is_string()) {
				return variant_type::get_class(literal.as_string());
			} else {
				return variant_type::get_any();
			}
		END_FUNCTION_DEF(construct)

		class update_object_command : public game_logic::CommandCallable
		{
			ffl::IntrusivePtr<FormulaObject> target_, src_;
		public:
			update_object_command(ffl::IntrusivePtr<FormulaObject> target,
								  ffl::IntrusivePtr<FormulaObject> src)
			  : target_(target), src_(src)
			{}
			virtual void execute(game_logic::FormulaCallable& ob) const override {
				target_->update(*src_);
			}
		};

		FUNCTION_DEF(update_object, 2, 2, "update_object(target_instance, src_instance)")

			ffl::IntrusivePtr<FormulaObject> target = EVAL_ARG(0).convert_to<FormulaObject>();
			ffl::IntrusivePtr<FormulaObject> src = EVAL_ARG(1).convert_to<FormulaObject>();
			return variant(new update_object_command(target, src));

		FUNCTION_TYPE_DEF
			return variant_type::get_commands();
		END_FUNCTION_DEF(update_object)

		FUNCTION_DEF(apply_delta, 2, 2, "apply_delta(instance, delta)")
			ffl::IntrusivePtr<FormulaObject> target = EVAL_ARG(0).convert_to<FormulaObject>();
			variant clone = FormulaObject::deepClone(variant(target.get()));
			FormulaObject* obj = clone.try_convert<FormulaObject>();
			obj->applyDiff(EVAL_ARG(1));
			return clone;
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType();
		END_FUNCTION_DEF(apply_delta)

		FUNCTION_DEF(delay_until_end_of_loading, 1, 1, "delay_until_end_of_loading(string): delays evaluation of the enclosed until loading is finished")
			Formula::failIfStaticContext();
			variant s = EVAL_ARG(0);
			ConstFormulaPtr f(Formula::createOptionalFormula(s));
			if(!f) {
				return variant();
			}

			ConstFormulaCallablePtr callable(&variables);

			return variant::create_delayed(f, callable);
		END_FUNCTION_DEF(delay_until_end_of_loading)

		#if defined(USE_LUA)
		FUNCTION_DEF(eval_lua, 1, 1, "eval_lua(str)")
			Formula::failIfStaticContext();
			variant value = EVAL_ARG(0);

			return variant(new FnCommandCallableArg("eval_lua", [=](FormulaCallable* callable) {
				lua::LuaContext context;
				context.execute(value, callable);
			}));

		FUNCTION_ARGS_DEF
			ARG_TYPE("string|builtin lua_compiled");
		FUNCTION_TYPE_DEF
			return variant_type::get_commands();
		END_FUNCTION_DEF(eval_lua)

		FUNCTION_DEF(compile_lua, 1, 1, "compile_lua(str)")
			Formula::failIfStaticContext();
			const std::string s = EVAL_ARG(0).as_string();

			lua::LuaContext ctx;
			return variant(ctx.compile("", s).get());
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			RETURN_TYPE("builtin lua_compiled");
		END_FUNCTION_DEF(compile_lua)
		#endif

		FUNCTION_DEF(eval_no_recover, 1, 2, "eval_no_recover(str, [arg]): evaluate the given string as FFL")
			ConstFormulaCallablePtr callable(&variables);

			if(NUM_ARGS > 1) {
				const variant v = EVAL_ARG(1);
				if(v.is_map()) {
					callable = map_into_callable(v);
				} else {
					callable.reset(v.try_convert<const FormulaCallable>());
					ASSERT_LOG(callable.get() != nullptr, "COULD NOT CONVERT TO CALLABLE: " << v.string_cast());
				}
			}

			variant s = EVAL_ARG(0);

			static std::map<std::string, ConstFormulaPtr> cache;
			ConstFormulaPtr& f = cache[s.as_string()];
			if(!f) {
				f = ConstFormulaPtr(Formula::createOptionalFormula(s));
			}

			ASSERT_LOG(f.get() != nullptr, "ILLEGAL FORMULA GIVEN TO eval: " << s.as_string());
			return f->execute(*callable);

		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
		END_FUNCTION_DEF(eval_no_recover)

		FUNCTION_DEF(eval, 1, 2, "eval(str, [arg]): evaluate the given string as FFL")
			ConstFormulaCallablePtr callable(&variables);

			if(NUM_ARGS > 1) {
				const variant v = EVAL_ARG(1);
				if(v.is_map()) {
					callable = map_into_callable(v);
				} else {
					callable.reset(v.try_convert<const FormulaCallable>());
					ASSERT_LOG(callable.get() != nullptr, "COULD NOT CONVERT TO CALLABLE: " << v.string_cast());
				}
			}

			variant s = EVAL_ARG(0);
			try {
				static std::map<std::string, ConstFormulaPtr> cache;
				const assert_recover_scope recovery_scope;

				ConstFormulaPtr& f = cache[s.as_string()];
				if(!f) {
					f = ConstFormulaPtr(Formula::createOptionalFormula(s));
				}

				if(!f) {
					return variant();
				}

				return f->execute(*callable);
			} catch(type_error&) {
			} catch(validation_failure_exception&) {
			}
			LOG_ERROR("ERROR IN EVAL");
			return variant();

		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
		END_FUNCTION_DEF(eval)

		namespace 
		{
			int g_formula_timeout = -1;

			struct timeout_scope 
			{
				int old_value;
				explicit timeout_scope(int deadline) : old_value(g_formula_timeout) {
					if(g_formula_timeout == -1 || deadline > g_formula_timeout) {
						g_formula_timeout = deadline;
					}
				}

				~timeout_scope() {
					g_formula_timeout = old_value;
				}
			};
		}

	FUNCTION_DEF(set_mouse_cursor, 1, 1, "set_mouse_cursor(string cursor)")
		std::string cursor = EVAL_ARG(0).as_string();
		return variant(new FnCommandCallable("set_mouse_cursor", [=]() {
			if(!KRE::are_cursors_initialized()) {
				if(sys::file_exists(module::map_file("data/cursors.cfg"))) {
					variant data = json::parse_from_file("data/cursors.cfg");
					KRE::initialize_cursors(data);
				}
			}
			KRE::set_cursor(cursor);
		}));

	FUNCTION_ARGS_DEF
		ARG_TYPE("string")
	RETURN_TYPE("commands")
	END_FUNCTION_DEF(set_mouse_cursor)

	void parse_xml_to_json_internal(const boost::property_tree::ptree& ptree, std::vector<variant>& res)
	{
		static const variant TextEnum = variant::create_enum("text");
		static const variant StartElementEnum = variant::create_enum("start_element");
		static const variant EndElementEnum = variant::create_enum("end_element");

		static const variant TypeStr("type");
		static const variant DataStr("data");
		static const variant AttrStr("attr");

		static const std::string XMLText = "<xmltext>";
		static const std::string XMLAttr = "<xmlattr>";

		for(auto itor = ptree.begin(); itor != ptree.end(); ++itor) {
			if(itor->first == XMLText) {
				if(itor->second.data().empty() == false) {
					std::map<variant,variant> m;
					m[TypeStr] = TextEnum;
					m[DataStr] = variant(itor->second.data());

					std::map<variant,variant> m_empty;
					m[AttrStr] = variant(&m_empty);
					res.push_back(variant(&m));
				}

				continue;
			} else if(itor->first == XMLAttr) {

				for(auto a = itor->second.begin(); a != itor->second.end(); ++a) {
					const std::string& attr = a->first;
					const std::string& value = a->second.data();

					assert(res.empty() == false);

					variant m = res.back()[AttrStr];
					m.add_attr_mutation(variant(attr), variant(value));
				}

				continue;
			}

			std::map<variant,variant> m;
			m[TypeStr] = StartElementEnum;
			m[DataStr] = variant(itor->first);

			std::map<variant,variant> m_empty;
			m[AttrStr] = variant(&m_empty);

			res.push_back(variant(&m));

			parse_xml_to_json_internal(itor->second, res);

			m[TypeStr] = EndElementEnum;
			m[DataStr] = variant(itor->first);

			m[AttrStr] = variant(&m_empty);
			res.push_back(variant(&m));
		}
	}

	FUNCTION_DEF(parse_xml, 1, 1, "parse_xml(str): Parses XML into a JSON structure")
		const std::string markup = EVAL_ARG(0).as_string();
		std::istringstream s(markup);
		boost::property_tree::ptree ptree;

		try {
			boost::property_tree::xml_parser::read_xml(s, ptree, boost::property_tree::xml_parser::no_concat_text);
		} catch(...) {
			std::ostringstream out;
			out << "Error parsing XML: " << markup;
			return variant(out.str());
		}

		std::vector<variant> res;

		parse_xml_to_json_internal(ptree, res);

		return variant(&res);

	FUNCTION_ARGS_DEF
		ARG_TYPE("string");
	RETURN_TYPE("string|[{ type: enum { text, start_element, end_element }, data: string, attr: {string -> string} }]")
	END_FUNCTION_DEF(parse_xml)

	FUNCTION_DEF_CTOR(eval_with_timeout, 2, 2, "eval_with_timeout(int time_ms, expr): evals expr, but with a timeout of time_ms. This will not pre-emptively time out, but while expr is evaluating, has_timed_out() will start evaluating to true if the timeout has elapsed.")
	FUNCTION_DEF_MEMBERS
	bool optimizeArgNumToVM(int narg) const override {
		return narg != 1;
	}
	FUNCTION_DEF_IMPL

		const int time_ms = SDL_GetTicks() + EVAL_ARG(0).as_int();
		const timeout_scope scope(time_ms);
		return args()[1]->evaluate(variables);

	FUNCTION_DYNAMIC_ARGUMENTS
	FUNCTION_ARGS_DEF
		ARG_TYPE("int");
	FUNCTION_TYPE_DEF
		return args()[1]->queryVariantType();
	END_FUNCTION_DEF(eval_with_timeout)

	FUNCTION_DEF(has_timed_out, 0, 0, "has_timed_out(): will evaluate to true iff the timeout specified by an enclosing eval_with_timeout() has elapsed.")
		game_logic::Formula::failIfStaticContext();
		if(g_formula_timeout == false) {
			return variant::from_bool(false);
		}

		const int ticks = SDL_GetTicks();

		return variant::from_bool(ticks >= g_formula_timeout);
	FUNCTION_TYPE_DEF
		return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
	END_FUNCTION_DEF(has_timed_out)

	std::string g_handle_errors_error_message;

	FUNCTION_DEF(get_error_message, 0, 0, "get_error_message: called after handle_errors() to get the error message")
		return variant(g_handle_errors_error_message);
	FUNCTION_TYPE_DEF
		return variant_type::get_type(variant::VARIANT_TYPE_STRING);
	END_FUNCTION_DEF(get_error_message)

		FUNCTION_DEF_CTOR(handle_errors, 2, 2, "handle_errors(expr, failsafe): evaluates 'expr' and returns it. If expr has fatal errors in evaluation, return failsafe instead. 'failsafe' is an expression which receives 'error_msg' and 'context' as parameters.")
		FUNCTION_DEF_MEMBERS
		bool optimizeArgNumToVM(int narg) const override {
			return false;
		}
		FUNCTION_DEF_IMPL
			const assert_recover_scope recovery_scope;
			try {
				return args()[0]->evaluate(variables);
			} catch(validation_failure_exception& e) {
				g_handle_errors_error_message = e.msg;
				return args()[1]->evaluate(variables);
			}
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType();
		END_FUNCTION_DEF(handle_errors)

		FUNCTION_DEF(switch, 3, -1, "switch(value, case1, result1, case2, result2 ... casen, resultn, default) -> value: returns resultn where value = casen, or default otherwise.")
			variant var = EVAL_ARG(0);
			for(size_t n = 1; n < NUM_ARGS-1; n += 2) {
				variant val = EVAL_ARG(n);
				if(val == var) {
					return EVAL_ARG(n+1);
				}
			}

			if((NUM_ARGS%2) == 0) {
				return EVAL_ARG(NUM_ARGS-1);
			} else {
				return variant();
			}
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_TYPE_DEF
			std::vector<variant_type_ptr> types;
			for(unsigned n = 2; n < NUM_ARGS; ++n) {
				if(n%2 == 0 || n == NUM_ARGS-1) {
					types.push_back(args()[n]->queryVariantType());
				}
			}

			return variant_type::get_union(types);

		CAN_VM
			return canChildrenVM();
		FUNCTION_VM

			for(ExpressionPtr& a : args_mutable()) {
				optimizeChildToVM(a);
			}

			for(auto a : args()) {
				if(a->canCreateVM() == false) {
					return ExpressionPtr();
				}
			}

			std::vector<int> jump_to_end_sources;
			args().front()->emitVM(vm);

			int n;
			for(n = 1; n+1 < static_cast<int>(NUM_ARGS); n += 2) {
				vm.addInstruction(OP_DUP);
				args()[n]->emitVM(vm);
				vm.addInstruction(OP_EQ);

				const int jump_source = vm.addJumpSource(OP_POP_JMP_UNLESS);

				args()[n+1]->emitVM(vm);
				jump_to_end_sources.push_back(vm.addJumpSource(OP_JMP));

				vm.jumpToEnd(jump_source);
			}

			if(n < static_cast<int>(NUM_ARGS)) {
				args().back()->emitVM(vm);
			} else {
				vm.addInstruction(OP_PUSH_NULL);
			}

			for(int j : jump_to_end_sources) {
				vm.jumpToEnd(j);
			}

			vm.addInstruction(OP_SWAP);
			vm.addInstruction(OP_POP);

			return createVMExpression(vm, queryVariantType(), *this);
		END_FUNCTION_DEF(switch)

		FUNCTION_DEF(query, 2, 2, "query(object, str): evaluates object.str")
			variant callable = EVAL_ARG(0);
			variant str = EVAL_ARG(1);
			return callable.as_callable()->queryValue(str.as_string());
		END_FUNCTION_DEF(query)

		FUNCTION_DEF(call, 2, 2, "call(fn, list): calls the given function with 'list' as the arguments")
			variant fn = EVAL_ARG(0);
			variant a = EVAL_ARG(1);
			return fn(a.as_list());
		FUNCTION_TYPE_DEF
			variant_type_ptr fn_type = args()[0]->queryVariantType();
			variant_type_ptr return_type;
			if(fn_type->is_function(nullptr, &return_type, nullptr)) {
				return return_type;
			}

			return variant_type_ptr();

		FUNCTION_ARGS_DEF
			ARG_TYPE("function");
			ARG_TYPE("list");
		END_FUNCTION_DEF(call)


		FUNCTION_DEF(abs, 1, 1, "abs(value) -> value: evaluates the absolute value of the value given")
			variant v = EVAL_ARG(0);
			if(v.is_decimal()) {
				const decimal d = v.as_decimal();
				return variant(d >= 0 ? d : -d);
			} else {
				const int n = v.as_int();
				return variant(n >= 0 ? n : -n);
			}

		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType();
		END_FUNCTION_DEF(abs)

		FUNCTION_DEF(sign, 1, 1, "sign(value) -> value: evaluates to 1 if positive, -1 if negative, and 0 if 0")
			const decimal n = EVAL_ARG(0).as_decimal();
			if(n > 0) {
				return variant(1);
			} else if(n < 0) {
				return variant(-1);
			} else {
				return variant(0);
			}

		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(sign)

		FUNCTION_DEF(median, 1, -1, "median(args...) -> value: evaluates to the median of the given arguments. If given a single argument list, will evaluate to the median of the member items.")
			if(NUM_ARGS == 3) {
				//special case for 3 arguments since it's a common case.
				variant a = EVAL_ARG(0);
				variant b = EVAL_ARG(1);
				variant c = EVAL_ARG(2);
				if(a < b) {
					if(b < c) {
						return b;
					} else if(a < c) {
						return c;
					} else {
						return a;
					}
				} else {
					if(a < c) {
						return a;
					} else if(b < c) {
						return c;
					} else {
						return b;
					}
				}
			}

			std::vector<variant> items;
			if(NUM_ARGS != 1) {
				items.reserve(NUM_ARGS);
			}

			for(size_t n = 0; n != NUM_ARGS; ++n) {
				const variant v = EVAL_ARG(n);
				if(NUM_ARGS == 1 && v.is_list()) {
					items = v.as_list();
				} else {
					items.push_back(v);
				}
			}

			std::sort(items.begin(), items.end());
			if(items.empty()) {
				return variant();
			} else if(items.size()&1) {
				return items[items.size()/2];
			} else {
				return (items[items.size()/2-1] + items[items.size()/2])/variant(2);
			}
		FUNCTION_TYPE_DEF
			if(NUM_ARGS == 1) {
				return args()[0]->queryVariantType()->is_list_of();
			} else {
				std::vector<variant_type_ptr> types;
				for(int n = 0; n != NUM_ARGS; ++n) {
					types.push_back(args()[n]->queryVariantType());
				}
        
				return variant_type::get_union(types);
			}
		END_FUNCTION_DEF(median)

		FUNCTION_DEF(min, 1, -1, "min(args...) -> value: evaluates to the minimum of the given arguments. If given a single argument list, will evaluate to the minimum of the member items.")

			bool found = false;
			variant res;
			for(size_t n = 0; n != NUM_ARGS; ++n) {
				const variant v = EVAL_ARG(n);
				if(v.is_list() && NUM_ARGS == 1) {
					for(size_t m = 0; m != v.num_elements(); ++m) {
						if(!found || v[m] < res) {
							res = v[m];
							found = true;
						}
					}
				} else {
					if(!found || v < res) {
						res = v;
						found = true;
					}
				}
			}

			return res;
		FUNCTION_TYPE_DEF
			if(NUM_ARGS == 1) {
				return args()[0]->queryVariantType()->is_list_of();
			} else {
				std::vector<variant_type_ptr> types;
				for(int n = 0; n != NUM_ARGS; ++n) {
					types.push_back(args()[n]->queryVariantType());
				}

				return variant_type::get_union(types);
			}
		END_FUNCTION_DEF(min)

		FUNCTION_DEF(max, 1, -1, "max(args...) -> value: evaluates to the maximum of the given arguments. If given a single argument list, will evaluate to the maximum of the member items.")

			bool found = false;
			variant res;
			for(size_t n = 0; n != NUM_ARGS; ++n) {
				const variant v = EVAL_ARG(n);
				if(v.is_list() && NUM_ARGS == 1) {
					for(size_t m = 0; m != v.num_elements(); ++m) {
						if(!found || v[m] > res) {
							res = v[m];
							found = true;
						}
					}
				} else {
					if(!found || v > res) {
						res = v;
						found = true;
					}
				}
			}

			return res;
		FUNCTION_TYPE_DEF
			if(NUM_ARGS == 1) {
				std::vector<variant_type_ptr> items;
				variant_type_ptr result = args()[0]->queryVariantType()->is_list_of();
				ASSERT_LOG(result.get(), "Single argument to max must be a list, found " << args()[0]->queryVariantType()->to_string());
				items.push_back(result);
				items.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
				return variant_type::get_union(items);
			} else {
				std::vector<variant_type_ptr> types;
				for(int n = 0; n != NUM_ARGS; ++n) {
					types.push_back(args()[n]->queryVariantType());
				}

				return variant_type::get_union(types);
			}
		END_FUNCTION_DEF(max)

			UNIT_TEST(min_max_decimal) {
				CHECK(game_logic::Formula(variant("max(1,1.4)")).execute() == game_logic::Formula(variant("1.4")).execute(), "test failed");
			}

		FUNCTION_DEF(mix, 3, 3, "mix(x, y, ratio): equal to x*(1-ratio) + y*ratio")
			decimal ratio = EVAL_ARG(2).as_decimal();
			return interpolate_variants(EVAL_ARG(0), EVAL_ARG(1), ratio);

		FUNCTION_ARGS_DEF
			ARG_TYPE("decimal|[decimal]");
			ARG_TYPE("decimal|[decimal]");
			ARG_TYPE("decimal");

		FUNCTION_TYPE_DEF
			variant_type_ptr type_a = args()[0]->queryVariantType();
			variant_type_ptr type_b = args()[1]->queryVariantType();

			if(type_b->is_compatible(type_a)) {
				return type_a;
			}

			if(type_a->is_compatible(type_b)) {
				return type_b;
			}

			ASSERT_LOG(false, "Types given to mix incompatible " << type_a->str() << " vs " << type_b->str() << ": " << debugPinpointLocation());

			return type_a;
	
		END_FUNCTION_DEF(mix)

		FUNCTION_DEF(disassemble, 1, 1, "disassemble function")
			variant arg = EVAL_ARG(0);
			std::string result;
			if(arg.disassemble(&result)) {
				return variant(result);
			}

			return variant();
		FUNCTION_ARGS_DEF
			ARG_TYPE("function");
			RETURN_TYPE("string|null")
		END_FUNCTION_DEF(disassemble)


		FUNCTION_DEF(rgb_to_hsv, 1, 1, "convert rgb to hsv")
			variant a = EVAL_ARG(0);
			KRE::Color c(a[0].as_float(), a[1].as_float(), a[2].as_float());
			auto vec = c.to_hsv_vec4();
			std::vector<variant> res;
			res.push_back(variant(vec[0]));
			res.push_back(variant(vec[1]));
			res.push_back(variant(vec[2]));
			return variant(&res);
		FUNCTION_ARGS_DEF
			ARG_TYPE("[decimal,decimal,decimal]");
			RETURN_TYPE("[decimal,decimal,decimal]")
		END_FUNCTION_DEF(rgb_to_hsv)

		FUNCTION_DEF(hsv_to_rgb, 1, 1, "convert hsv to rgb")
			variant a = EVAL_ARG(0);
			KRE::Color c = KRE::Color::from_hsv(a[0].as_float(), a[1].as_float(), a[2].as_float());
			std::vector<variant> res;
			res.push_back(variant(c.r()));
			res.push_back(variant(c.g()));
			res.push_back(variant(c.b()));
			return variant(&res);
		FUNCTION_ARGS_DEF
			ARG_TYPE("[decimal,decimal,decimal]");
			RETURN_TYPE("[decimal,decimal,decimal]")
		END_FUNCTION_DEF(hsv_to_rgb)

		FUNCTION_DEF(keys, 1, 1, "keys(map|custom_obj|level) -> list: gives the keys for a map")
			const variant map = EVAL_ARG(0);
			if(map.is_callable()) {
				std::vector<variant> v;
				const std::vector<FormulaInput> inputs = map.as_callable()->inputs();
				for(const FormulaInput& in : inputs) {
					v.push_back(variant(in.name));
				}

				return variant(&v);
			}

			return map.getKeys();

		FUNCTION_ARGS_DEF
			ARG_TYPE("map|object|level");
		FUNCTION_TYPE_DEF
			return variant_type::get_list(args()[0]->queryVariantType()->is_map_of().first);
		END_FUNCTION_DEF(keys)

		FUNCTION_DEF(values, 1, 1, "values(map) -> list: gives the values for a map")
			const variant map = EVAL_ARG(0);
			return map.getValues();
		FUNCTION_ARGS_DEF
			ARG_TYPE("map");
		FUNCTION_TYPE_DEF
			return variant_type::get_list(args()[0]->queryVariantType()->is_map_of().second);
		END_FUNCTION_DEF(values)

		FUNCTION_DEF(wave, 1, 1, "wave(int) -> int: a wave with a period of 1000 and height of 1000")
			const int value = EVAL_ARG(0).as_int()%1000;
			const double angle = 2.0*3.141592653589*(static_cast<double>(value)/1000.0);
			return variant(static_cast<int>(sin(angle)*1000.0));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(wave)

		FUNCTION_DEF(decimal, 1, 1, "decimal(value) -> decimal: converts the value to a decimal")
			variant v = EVAL_ARG(0);
			if(v.is_string()) {
				try {
					return variant(boost::lexical_cast<double>(v.as_string()));
				} catch(...) {
					ASSERT_LOG(false, "Could not parse string as integer: " << v.write_json());
				}
			}

			return variant(v.as_decimal());
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(decimal)

		FUNCTION_DEF(int, 1, 1, "int(value) -> int: converts the value to an integer")
			variant v = EVAL_ARG(0);
			if(v.is_string()) {
				try {
					return variant(boost::lexical_cast<int>(v.as_string()));
				} catch(...) {
					ASSERT_LOG(false, "Could not parse string as integer: " << v.write_json());
				}
			}
			return variant(v.as_int());
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(int)

		FUNCTION_DEF(bool, 1, 1, "bool(value) -> bool: converts the value to a boolean")
			variant v = EVAL_ARG(0);
			return variant::from_bool(v.as_bool());
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		END_FUNCTION_DEF(bool)

		FUNCTION_DEF(sin, 1, 1, "sin(x): Standard sine function.")
			const float angle = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(sin(angle/radians_to_degrees)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(sin)

		FUNCTION_DEF(cos, 1, 1, "cos(x): Standard cosine function.")
			const float angle = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(cos(angle/radians_to_degrees)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(cos)

		FUNCTION_DEF(tan, 1, 1, "tan(x): Standard tangent function.")
			const float angle = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(tan(angle/radians_to_degrees)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(tan)

		FUNCTION_DEF(asin, 1, 1, "asin(x): Standard arc sine function.")
			const float ratio = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(asin(ratio)*radians_to_degrees));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(asin)

		FUNCTION_DEF(acos, 1, 1, "acos(x): Standard arc cosine function.")
			const float ratio = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(acos(ratio)*radians_to_degrees));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(acos)

		FUNCTION_DEF(atan, 1, 1, "atan(x): Standard arc tangent function.")
			const float ratio = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(atan(ratio)*radians_to_degrees));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(atan)

		FUNCTION_DEF(atan2, 2, 2, "atan2(x,y): Standard two-param arc tangent function (to allow determining the quadrant of the resulting angle by passing in the sign value of the operands).")
			const float ratio1 = EVAL_ARG(0).as_float();
			const float ratio2 = EVAL_ARG(1).as_float();
			return variant(atan2(ratio1, ratio2) * radians_to_degrees);
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(atan2)
    
		FUNCTION_DEF(sinh, 1, 1, "sinh(x): Standard hyperbolic sine function.")
			const float angle = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(sinh(angle)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(sinh)

		FUNCTION_DEF(cosh, 1, 1, "cosh(x): Standard hyperbolic cosine function.")
			const float angle = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(cosh(angle)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(cosh)

		FUNCTION_DEF(tanh, 1, 1, "tanh(x): Standard hyperbolic tangent function.")
			const float angle = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(tanh(angle)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(tanh)

		FUNCTION_DEF(asinh, 1, 1, "asinh(x): Standard arc hyperbolic sine function.")
			const float ratio = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(asinh(ratio)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(asinh)

		FUNCTION_DEF(acosh, 1, 1, "acosh(x): Standard arc hyperbolic cosine function.")
			const float ratio = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(acosh(ratio)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(acosh)

		FUNCTION_DEF(atanh, 1, 1, "atanh(x): Standard arc hyperbolic tangent function.")
			const float ratio = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(atanh(ratio)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(atanh)

		FUNCTION_DEF(sqrt, 1, 1, "sqrt(x): Returns the square root of x.")
			const double value = EVAL_ARG(0).as_double();
			ASSERT_LOG(value >= 0, "We don't support the square root of negative numbers: " << value);
			return variant(decimal(sqrt(value)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(sqrt)

		FUNCTION_DEF(hypot, 2, 2, "hypot(x,y): Compute the hypotenuse of a triangle without the normal loss of precision incurred by using the pythagoream theorem.")
			const double x = EVAL_ARG(0).as_double();
			const double y = EVAL_ARG(1).as_double();
			return variant(hypot(x,y));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(hypot)
	
	
		FUNCTION_DEF(exp, 1, 1, "exp(x): Calculate the exponential function of x, whatever that means.")
			const float input = EVAL_ARG(0).as_float();
			return variant(static_cast<decimal>(expf(input)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_DECIMAL);
		END_FUNCTION_DEF(exp)
    
		FUNCTION_DEF(angle, 4, 4, "angle(x1, y1, x2, y2) -> int: Returns the angle, from 0°, made by the line described by the two points (x1, y1) and (x2, y2).")
			const float a = EVAL_ARG(0).as_float();
			const float b = EVAL_ARG(1).as_float();
			const float c = EVAL_ARG(2).as_float();
			const float d = EVAL_ARG(3).as_float();
			return variant(static_cast<int64_t>(bmround((atan2(a-c, b-d)*radians_to_degrees+90)*VARIANT_DECIMAL_PRECISION)*-1), variant::DECIMAL_VARIANT);
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(angle)

		FUNCTION_DEF(angle_delta, 2, 2, "angle_delta(a, b) -> int: Given two angles, returns the smallest rotation needed to make a equal to b.")
			int a = EVAL_ARG(0).as_int();
			int b = EVAL_ARG(1).as_int();
			while(abs(a - b) > 180) {
				if(a < b) {
					a += 360;
				} else {
					b += 360;
				}
			}

			return variant(b - a);
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(angle_delta)

		FUNCTION_DEF(orbit, 4, 4, "orbit(x, y, angle, dist) -> [x,y]: Returns the point as a list containing an x/y pair which is dist away from the point as defined by x and y passed in, at the angle passed in.")
			const float x = EVAL_ARG(0).as_float();
			const float y = EVAL_ARG(1).as_float();
			const float ang = EVAL_ARG(2).as_float();
			const float dist = EVAL_ARG(3).as_float();
	
			const float u = (dist * cos(ang/radians_to_degrees)) + x;   //TODO Find out why whole number decimals are returned.
			const float v = (dist * sin(ang/radians_to_degrees)) + y;

			std::vector<variant> result;
			result.reserve(2);
			result.push_back(variant(decimal(u)));
			result.push_back(variant(decimal(v)));
	
			return variant(&result);
		FUNCTION_ARGS_DEF
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
			ARG_TYPE("int|decimal");
			RETURN_TYPE("[decimal,decimal]")
		END_FUNCTION_DEF(orbit)


		FUNCTION_DEF(floor, 1, 1, "Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3")
			const float a = EVAL_ARG(0).as_float();
			return variant(static_cast<int>(floor(a)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(floor)

		FUNCTION_DEF(round, 1, 1, "Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3")
			const double a = EVAL_ARG(0).as_float();
			return variant(static_cast<int>(bmround(a)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(round)

		FUNCTION_DEF(round_to_even, 1, 1, "Returns the nearest integer that is even")
			const double a = EVAL_ARG(0).as_float();
			int result = static_cast<int>(a);
			if(result&1) {
				++result;
			}

			return variant(result);
		FUNCTION_ARGS_DEF
			ARG_TYPE("decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(round_to_even)

		FUNCTION_DEF(ceil, 1, 1, "Returns the smaller near integer. 3.9 -> 3, 3.3 -> 3, 3 -> 3")
			const float a = EVAL_ARG(0).as_float();
			return variant(static_cast<int>(ceil(a)));
		FUNCTION_ARGS_DEF
			ARG_TYPE("decimal");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(ceil)


		FUNCTION_DEF(regex_replace, 3, 4, "regex_replace(string, string, string, [string] flags=[]) -> string: Unknown.")

			int flags = 0;

			if(NUM_ARGS > 3) {
				std::vector<variant> items = EVAL_ARG(3).as_list();
				for(variant arg : items) {
					if(arg.as_string() == "icase") {
						flags = flags | boost::regex::icase;
					} else {
						ASSERT_LOG(false, "Unrecognized regex arg: " << arg.as_string());
					}
				}
			}

			const std::string str = EVAL_ARG(0).as_string();
			const boost::regex re(EVAL_ARG(1).as_string(), flags);
			const std::string value = EVAL_ARG(2).as_string();

			return variant(boost::regex_replace(str, re, value));
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			ARG_TYPE("string");
			ARG_TYPE("string");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_STRING);
		END_FUNCTION_DEF(regex_replace)

		FUNCTION_DEF(regex_match, 2, 2, "regex_match(string, re_string) -> string: returns null if not found, else returns the whole string or a list of sub-strings depending on whether blocks were demarcated.")
			const std::string str = EVAL_ARG(0).as_string();
			const boost::regex re(EVAL_ARG(1).as_string());
			 boost::match_results<std::string::const_iterator> m;
			if(boost::regex_match(str, m, re) == false) {
				return variant();
			}
			if(m.size() == 1) {
				return variant(std::string(m[0].first, m[0].second));
			} 
			std::vector<variant> v;
			for(int i = 1; i < static_cast<int>(m.size()); i++) {
				v.emplace_back(std::string(m[i].first, m[i].second));
			}
			return variant(&v);
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			ARG_TYPE("string");
		FUNCTION_TYPE_DEF
			std::vector<variant_type_ptr> types;
			types.push_back(variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_STRING)));
			types.push_back(variant_type::get_type(variant::VARIANT_TYPE_STRING));
			types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
			return variant_type::get_union(types);
		END_FUNCTION_DEF(regex_match)

		namespace 
		{

			class variant_comparator_definition : public FormulaCallableDefinition
			{
			public:
				variant_comparator_definition(ConstFormulaCallableDefinitionPtr base, variant_type_ptr type)
				  : base_(base), type_(type), num_slots_(numBaseSlots() + 2)
				{
					for(int n = 0; n != 2; ++n) {
						const std::string name = (n == 0) ? "a" : "b";
						entries_.push_back(Entry(name));
						entries_.back().setVariantType(type_);
					}
				}

				int numBaseSlots() const { return base_ ? base_->getNumSlots() : 0; }

				int getSlot(const std::string& key) const override {
					if(key == "a") { return numBaseSlots() + 0; }
					if(key == "b") { return numBaseSlots() + 1; }

					if(base_) {
						int result = base_->getSlot(key);
						return result;
					} else {
						return -1;
					}
				}

				Entry* getEntry(int slot) override {
					if(slot < 0) {
						return nullptr;
					}

					if(base_ && slot < numBaseSlots()) {
						return const_cast<FormulaCallableDefinition*>(base_.get())->getEntry(slot);
					}

					slot -= numBaseSlots();

					if(static_cast<unsigned>(slot) < entries_.size()) {
						return &entries_[slot];
					}

					return nullptr;
				}

				const Entry* getEntry(int slot) const override {
					if(slot < 0) {
						return nullptr;
					}

					if(base_ && slot < numBaseSlots()) {
						return const_cast<FormulaCallableDefinition*>(base_.get())->getEntry(slot);
					}

					slot -= numBaseSlots();

					if(static_cast<unsigned>(slot) < entries_.size()) {
						return &entries_[slot];
					}

					return nullptr;
				}

				bool getSymbolIndexForSlot(int slot, int* index) const override {
					if(slot < 0) {
						return false;
					}

					if(base_ && slot < numBaseSlots()) {
						return base_->getSymbolIndexForSlot(slot, index);
					}

					slot -= numBaseSlots();

					if(static_cast<unsigned>(slot) < entries_.size()) {

						if(!hasSymbolIndexes()) {
							return false;
						}

						*index = getBaseSymbolIndex() + slot;
						return true;
					}

					return false;
				}

				int getBaseSymbolIndex() const override {
					int result = 0;
					if(base_) {
						result += base_->getBaseSymbolIndex();
					}

					if(hasSymbolIndexes()) {
						result += entries_.size();
					}

					return result;
				}

				int getNumSlots() const override {
					return num_slots_;
				}

				int getSubsetSlotBase(const FormulaCallableDefinition* subset) const override
				{
					if(!base_) {
						return -1;
					}

					const int slot = base_->querySubsetSlotBase(subset);
					if(slot == -1) {
						return -1;
					}

					return slot;
				}

			private:
				ConstFormulaCallableDefinitionPtr base_;
				variant_type_ptr type_;

				std::vector<Entry> entries_;

				int num_slots_;
			};

			class variant_comparator : public FormulaCallable {
				//forbid these so they can't be passed by value.
				variant_comparator(const variant_comparator&);
				void operator=(const variant_comparator&);

				ExpressionPtr expr_;
				const FormulaCallable* fallback_;
				mutable variant a_, b_;
				int num_slots_;
				variant getValue(const std::string& key) const override {
					if(key == "a") {
						return a_;
					} else if(key == "b") {
						return b_;
					} else {
						return fallback_->queryValue(key);
					}
				}

				variant getValueBySlot(int slot) const override {
					if(slot == num_slots_-2) {
						return a_;
					} else if(slot == num_slots_-1) {
						return b_;
					}

					return fallback_->queryValueBySlot(slot);
				}

				void setValue(const std::string& key, const variant& value) override {
					const_cast<FormulaCallable*>(fallback_)->mutateValue(key, value);
				}

				void setValueBySlot(int slot, const variant& value) override {
					const_cast<FormulaCallable*>(fallback_)->mutateValueBySlot(slot, value);
				}

				void getInputs(std::vector<FormulaInput>* inputs) const override {
					fallback_->getInputs(inputs);
				}
			public:
				variant_comparator(const ExpressionPtr& expr, const FormulaCallable& fallback) : FormulaCallable(false), expr_(expr), fallback_(&fallback), num_slots_(0)
				{
					auto p = expr->getDefinitionUsedByExpression();
					if(p) {
						num_slots_ = p->getNumSlots();
					}
				}

				bool operator()(const variant& a, const variant& b) const {
					a_ = a;
					b_ = b;
					return expr_->evaluate(*this).as_bool();
				}

				variant eval(const variant& a, const variant& b) const {
					a_ = a;
					b_ = b;
					return expr_->evaluate(*this);
				}

				void surrenderReferences(GarbageCollector* collector) override {
					collector->surrenderVariant(&a_);
					collector->surrenderVariant(&b_);
				}
			};
		}

FUNCTION_DEF_CTOR(fold, 2, 3, "fold(list, expr, [default]) -> value")
	if(NUM_ARGS == 2) {
		variant_type_ptr type = args()[1]->queryVariantType();
		if(type->is_type(variant::VARIANT_TYPE_INT)) {
			default_ = variant(0);
		} else if(type->is_numeric()) {
			default_ = variant(decimal(0));
		} else if(type->is_type(variant::VARIANT_TYPE_STRING)) {
			default_ = variant("");
		} else if(type->is_type(variant::VARIANT_TYPE_LIST) || type->is_list_of()) {
			std::vector<variant> v;
			default_ = variant(&v);
		} else if(type->is_type(variant::VARIANT_TYPE_MAP) || type->is_map_of().first) {
			std::map<variant,variant> m;
			default_ = variant(&m);
		}
	}

FUNCTION_DYNAMIC_ARGUMENTS
FUNCTION_DEF_MEMBERS
	variant default_;
	bool optimizeArgNumToVM(int narg) const override {
		return narg != 1;
	}

FUNCTION_DEF_IMPL
			variant list = EVAL_ARG(0);
			const int size = list.num_elements();
			if(size == 0) {
				if(NUM_ARGS >= 3) {
					return EVAL_ARG(2);
				} else {
					return default_;
				}
			} else if(size == 1) {
				return list[0];
			}

			ffl::IntrusivePtr<variant_comparator> callable(new variant_comparator(args()[1], variables));

			variant a = list[0];
			for(int n = 1; n < list.num_elements(); ++n) {
				a = callable->eval(a, list[n]);
			}

			return a;
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			std::vector<variant_type_ptr> types;
			types.push_back(args()[1]->queryVariantType());

			variant_type_ptr list_type = args()[0]->queryVariantType();
			variant_type_ptr list_element_type = list_type->is_list_of();
			ASSERT_LOG(list_element_type.get() != nullptr, "First argument to fold() must be a list: " << debugPinpointLocation());
			ASSERT_LOG(variant_types_compatible(list_element_type, types.back()), "fold() given argument of type " << list_type->to_string() << " must return type " << list_element_type->to_string() << " but returns type " << types.back()->to_string() << ": " << debugPinpointLocation());

			if(NUM_ARGS > 2) {
				types.push_back(args()[2]->queryVariantType());
			} else if(default_.is_null()) {
				types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
			}

			return variant_type::get_union(types);
		END_FUNCTION_DEF(fold)

		FUNCTION_DEF(unzip, 1, 1, "unzip(list of lists) -> list of lists: Converts [[1,4],[2,5],[3,6]] -> [[1,2,3],[4,5,6]]")
			variant item1 = EVAL_ARG(0);
			ASSERT_LOG(item1.is_list(), "unzip function arguments must be a list");

			// Calculate breadth and depth of new list.
			const int depth = item1.num_elements();
			int breadth = 0;
			for(int n = 0; n < item1.num_elements(); ++n) {
				ASSERT_LOG(item1[n].is_list(), "Item " << n << " on list isn't list");
				breadth = std::max(item1[n].num_elements(), breadth);
			}

			std::vector<std::vector<variant> > v;
			for(int n = 0; n < breadth; ++n) {
				std::vector<variant> e1;
				e1.resize(depth);
				v.push_back(e1);
			}

			for(int n = 0; n < item1.num_elements(); ++n) {
				for(int m = 0; m < item1[n].num_elements(); ++m) {
					v[m][n] = item1[n][m];
				}
			}

			std::vector<variant> vl;
			for(int n = 0; n < static_cast<int>(v.size()); ++n) {
				vl.push_back(variant(&v[n]));
			}
			return variant(&vl);
		FUNCTION_ARGS_DEF
			ARG_TYPE("[list]");
		END_FUNCTION_DEF(unzip)

		FUNCTION_DEF_CTOR(zip, 2, 3, "zip(list1, list2, expr=null) -> list")
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
		bool optimizeArgNumToVM(int narg) const override {
			return narg != 2;
		}
		FUNCTION_DEF_IMPL
			const variant item1 = EVAL_ARG(0);
			const variant item2 = EVAL_ARG(1);

			ASSERT_LOG(item1.type() == item2.type(), "zip function arguments must both be the same type.");
			ASSERT_LOG(item1.is_list() || item1.is_map(), "zip function arguments must be either lists or maps");

			ffl::IntrusivePtr<variant_comparator> callable;
	
			if(NUM_ARGS > 2) {
				callable.reset(new variant_comparator(args()[2], variables));
			}
			const int size = std::min(item1.num_elements(), item2.num_elements());

			if(item1.is_list()) {
				std::vector<variant> result;
				// is list
				for(int n = 0; n < size; ++n) {
					if(callable) {
						result.push_back(callable->eval(item1[n], item2[n]));
					} else {
						result.push_back(item1[n] + item2[n]);
					}
				}
				return variant(&result);
			} else {
				std::map<variant,variant> retMap(item1.as_map());
				variant keys = item2.getKeys();
				for(int n = 0; n != keys.num_elements(); n++) {
					if(retMap[keys[n]].is_null() == false) {
						if(callable) {
							retMap[keys[n]] = callable->eval(retMap[keys[n]], item2[keys[n]]);
						} else {
							retMap[keys[n]] = retMap[keys[n]] + item2[keys[n]];
						}
					} else {
						retMap[keys[n]] = item2[keys[n]];
					}
				}
				return variant(&retMap);
			}
			return variant();
		FUNCTION_ARGS_DEF
			ARG_TYPE("list|map");
			ARG_TYPE("list|map");
		FUNCTION_TYPE_DEF
			variant_type_ptr type_a = args()[0]->queryVariantType();
			variant_type_ptr type_b = args()[1]->queryVariantType();

			if(NUM_ARGS <= 2) {
				std::vector<variant_type_ptr> v;
				v.push_back(type_a);
				v.push_back(type_b);
				return variant_type::get_union(v);
			}

			if(type_a->is_specific_list() && type_b->is_specific_list()) {
				std::vector<variant_type_ptr> types;
				const auto num_elements = std::min(type_a->is_specific_list()->size(), type_b->is_specific_list()->size());
				const variant_type_ptr type = args()[2]->queryVariantType();
				for(auto n = 0; n != num_elements; ++n) {
					types.push_back(type);
				}

				return variant_type::get_specific_list(types);
			} else if(type_a->is_list_of()) {
				return variant_type::get_list(args()[2]->queryVariantType());
			} else {
				std::pair<variant_type_ptr,variant_type_ptr> map_a = type_a->is_map_of();
				std::pair<variant_type_ptr,variant_type_ptr> map_b = type_b->is_map_of();
				if(map_a.first && map_b.first) {
					std::vector<variant_type_ptr> key;
					key.push_back(map_a.first);
					key.push_back(map_b.first);
					return variant_type::get_map(variant_type::get_union(key), args()[2]->queryVariantType());
				}
			}

			return variant_type::get_any();
		END_FUNCTION_DEF(zip)

		FUNCTION_DEF(float_array, 1, 1, "float_array(list) -> callable: Converts a list of floating point values into an efficiently accessible object.")
			game_logic::Formula::failIfStaticContext();
			variant f = EVAL_ARG(0);
			std::vector<float> floats;
			for(int n = 0; n < f.num_elements(); ++n) {
				floats.push_back(f[n].as_float());
			}
			return variant(new FloatArrayCallable(&floats));
		FUNCTION_ARGS_DEF
			ARG_TYPE("[decimal|int]");
		END_FUNCTION_DEF(float_array)

		FUNCTION_DEF(short_array, 1, 1, "short_array(list) -> callable: Converts a list of integer values into an efficiently accessible object.")
			game_logic::Formula::failIfStaticContext();
			variant s = EVAL_ARG(0);
			std::vector<short> shorts;
			for(int n = 0; n < s.num_elements(); ++n) {
				shorts.push_back(static_cast<short>(s[n].as_int()));
			}
			return variant(new ShortArrayCallable(&shorts));
		FUNCTION_ARGS_DEF
			ARG_TYPE("[int]");
		END_FUNCTION_DEF(short_array)

		FUNCTION_DEF(generate_uuid, 0, 0, "generate_uuid() -> string: generates a unique string")
			game_logic::Formula::failIfStaticContext();
			return variant(write_uuid(generate_uuid()));
		FUNCTION_ARGS_DEF
			RETURN_TYPE("string")
		END_FUNCTION_DEF(generate_uuid)

		/* XXX Krista to be reworked
		FUNCTION_DEF(update_controls, 1, 1, "update_controls(map) : Updates the controls based on a list of id:string, pressed:bool pairs")
			const variant map = EVAL_ARG(0);
			for(const auto& p : map.as_map()) {
				LOG_INFO("Button: " << p.first.as_string() << " " << (p.second.as_bool() ? "Pressed" : "Released"));
				controls::update_control_state(p.first.as_string(), p.second.as_bool());
			}
			return variant();
		END_FUNCTION_DEF(update_controls)

		FUNCTION_DEF(map_controls, 1, 1, "map_controls(map) : Creates or updates the mapping on controls to keys")
			const variant map = EVAL_ARG(0);
			for(const auto& p : map.as_map()) {
				controls::set_mapped_key(p.first.as_string(), static_cast<SDL_Keycode>(p.second.as_int()));
			}
			return variant();
		END_FUNCTION_DEF(map_controls)*/

		FUNCTION_DEF(get_hex_editor_info, 0, 0, "get_hex_editor_info() ->[builtin hex_tile]")
			auto ei = hex::get_editor_info();
			return variant(&ei);
		FUNCTION_ARGS_DEF
			RETURN_TYPE("[builtin hex_tile]")
		END_FUNCTION_DEF(get_hex_editor_info)

		FUNCTION_DEF(tile_pixel_pos_from_loc, 1, 2, "tile_pixel_pos_from_loc(loc) -> [x,y]")
			point p(EVAL_ARG(0));
			auto tp = hex::get_pixel_pos_from_tile_pos_evenq(p, hex::g_hex_tile_size);
			return tp.write();
			FUNCTION_ARGS_DEF
				ARG_TYPE("[int, int]")
			RETURN_TYPE("[int, int]")
		END_FUNCTION_DEF(tile_pixel_pos_from_loc)

		FUNCTION_DEF(tile_loc_from_pixel_pos, 1, 2, "tile_pixel_pos_from_loc(loc) -> [x,y]")
			point p(EVAL_ARG(0));
			auto tp = hex::get_tile_pos_from_pixel_pos_evenq(p, hex::g_hex_tile_size);
			return tp.write();
		FUNCTION_ARGS_DEF
			ARG_TYPE("[int, int]")
			RETURN_TYPE("[int, int]")
		END_FUNCTION_DEF(tile_loc_from_pixel_pos)				

		FUNCTION_DEF(directed_graph, 2, 2, "directed_graph(list_of_vertexes, adjacent_expression) -> a directed graph")
			variant vertices = EVAL_ARG(0);
			pathfinding::graph_edge_list edges;
	
			std::vector<variant> vertex_list;
			ffl::IntrusivePtr<MapFormulaCallable> callable(new MapFormulaCallable(&variables));
			variant& a = callable->addDirectAccess("v");
			for(variant v : vertices.as_list()) {
				a = v;
				variant res = args()[1]->evaluate(*callable);
				if(res.is_function()) {
					std::vector<variant> args;
					args.push_back(v);
					edges[v] = res(args).as_list();
				} else {
					edges[v] = res.as_list();
				}
				vertex_list.push_back(v);
			}
			pathfinding::DirectedGraph* dg = new pathfinding::DirectedGraph(&vertex_list, &edges);
			return variant(dg);
		FUNCTION_DYNAMIC_ARGUMENTS
		CAN_VM
			return false;
		FUNCTION_VM
			return ExpressionPtr();
		FUNCTION_ARGS_DEF
			ARG_TYPE("list")
			ARG_TYPE("any")
			RETURN_TYPE("builtin directed_graph")
		END_FUNCTION_DEF(directed_graph)

		FUNCTION_DEF(weighted_graph, 2, 2, "weighted_graph(directed_graph, weight_expression) -> a weighted directed graph")
				variant graph = EVAL_ARG(0);		
				pathfinding::DirectedGraphPtr dg = ffl::IntrusivePtr<pathfinding::DirectedGraph>(graph.try_convert<pathfinding::DirectedGraph>());
				ASSERT_LOG(dg, "Directed graph given is not of the correct type. " /*<< variant::variant_type_to_string(graph.type())*/);
				pathfinding::edge_weights w;
 
 				variant cmp(EVAL_ARG(1));
				std::vector<variant> fn_args;
				fn_args.push_back(variant());
				fn_args.push_back(variant());
 
				for(auto edges = dg->getEdges()->begin();
						edges != dg->getEdges()->end();
						edges++) {
						fn_args[0] = edges->first;
						for(auto e2 : edges->second) {

							fn_args[1] = e2;
							variant v = cmp(fn_args);
							if(v.is_null() == false) {
									w[pathfinding::graph_edge(edges->first, e2)] = v.as_decimal();
							}
						}
				}
				return variant(new pathfinding::WeightedDirectedGraph(dg, &w));
		FUNCTION_ARGS_DEF
				ARG_TYPE("builtin directed_graph")
				ARG_TYPE("function")
				RETURN_TYPE("builtin weighted_directed_graph")
		END_FUNCTION_DEF(weighted_graph)

		FUNCTION_DEF(a_star_search, 4, 4, "a_star_search(weighted_directed_graph, src_node, dst_node, heuristic) -> A list of nodes which represents the 'best' path from src_node to dst_node.")
			variant graph = EVAL_ARG(0);
			pathfinding::WeightedDirectedGraphPtr wg = graph.try_convert<pathfinding::WeightedDirectedGraph>();
			ASSERT_LOG(wg, "Weighted graph given is not of the correct type.");
			variant src_node = EVAL_ARG(1);
			variant dst_node = EVAL_ARG(2);
			variant heuristic_fn = EVAL_ARG(3);
			return pathfinding::a_star_search(wg, src_node, dst_node, heuristic_fn);
		FUNCTION_ARGS_DEF
			ARG_TYPE("builtin weighted_directed_graph")
			ARG_TYPE("any")
			ARG_TYPE("any")
			ARG_TYPE("function")
			RETURN_TYPE("list")
		END_FUNCTION_DEF(a_star_search)

		FUNCTION_DEF(path_cost_search, 3, 3, "path_cost_search(weighted_directed_graph, src_node, max_cost) -> A list of all possible points reachable from src_node within max_cost.")
			variant graph = EVAL_ARG(0);
			pathfinding::WeightedDirectedGraphPtr wg = graph.try_convert<pathfinding::WeightedDirectedGraph>();
			ASSERT_LOG(wg, "Weighted graph given is not of the correct type.");
			variant src_node = EVAL_ARG(1);
			decimal max_cost(EVAL_ARG(2).as_decimal());
			return pathfinding::path_cost_search(wg, src_node, max_cost);
		FUNCTION_ARGS_DEF
			ARG_TYPE("builtin weighted_directed_graph")
			ARG_TYPE("any")
			ARG_TYPE("decimal|int")
			RETURN_TYPE("list")
		END_FUNCTION_DEF(path_cost_search)

		FUNCTION_DEF(create_graph_from_level, 1, 3, "create_graph_from_level(level, (optional) tile_size_x, (optional) tile_size_y) -> directed graph : Creates a directed graph based on the current level.")
			int tile_size_x = TileSize;
			int tile_size_y = TileSize;
			if(NUM_ARGS == 2) {
				tile_size_y = tile_size_x = EVAL_ARG(1).as_int();
			} else if(NUM_ARGS == 3) {
				tile_size_x = EVAL_ARG(1).as_int();
				tile_size_y = EVAL_ARG(2).as_int();
			}
			ASSERT_LOG((tile_size_x%2)==0 && (tile_size_y%2)==0, "The tile_size_x and tile_size_y values *must* be even. (" << tile_size_x << "," << tile_size_y << ")");
			variant curlevel = EVAL_ARG(0);
			LevelPtr lvl = curlevel.try_convert<Level>();
			ASSERT_LOG(lvl, "The level parameter passed to the function was couldn't be converted.");
			rect b = lvl->boundaries();
			b.from_coordinates(b.x() - b.x()%tile_size_x, 
				b.y() - b.y()%tile_size_y, 
				b.x2()+(tile_size_x-b.x2()%tile_size_x), 
				b.y2()+(tile_size_y-b.y2()%tile_size_y));

			pathfinding::graph_edge_list edges;
			std::vector<variant> vertex_list;
			const rect& b_rect = Level::current().boundaries();

			for(int y = b.y(); y < b.y2(); y += tile_size_y) {
				for(int x = b.x(); x < b.x2(); x += tile_size_x) {
					if(!lvl->solid(x, y, tile_size_x, tile_size_y)) {
						variant l(pathfinding::point_as_variant_list(point(x,y)));
						vertex_list.push_back(l);
						std::vector<variant> e;
						point po(x,y);
						for(const point& p : pathfinding::get_neighbours_from_rect(po, tile_size_x, tile_size_y, b_rect)) {
							if(!lvl->solid(p.x, p.y, tile_size_x, tile_size_y)) {
								e.push_back(pathfinding::point_as_variant_list(p));
							}
						}
						edges[l] = e;
					}
				}
			}
			return variant(new pathfinding::DirectedGraph(&vertex_list, &edges));
		END_FUNCTION_DEF(create_graph_from_level)

		FUNCTION_DEF(plot_path, 6, 9, "plot_path(level, from_x, from_y, to_x, to_y, heuristic, (optional) weight_expr, (optional) tile_size_x, (optional) tile_size_y) -> list : Returns a list of points to get from (from_x, from_y) to (to_x, to_y)")
			int tile_size_x = TileSize;
			int tile_size_y = TileSize;
			ExpressionPtr weight_expr = ExpressionPtr();
			variant curlevel = EVAL_ARG(0);
			LevelPtr lvl = curlevel.try_convert<Level>();
			if(NUM_ARGS > 6) {
				weight_expr = args()[6];
			}
			if(NUM_ARGS == 8) {
				tile_size_y = tile_size_x = EVAL_ARG(6).as_int();
			} else if(NUM_ARGS == 9) {
				tile_size_x = EVAL_ARG(6).as_int();
				tile_size_y = EVAL_ARG(7).as_int();
			}
			ASSERT_LOG((tile_size_x%2)==0 && (tile_size_y%2)==0, "The tile_size_x and tile_size_y values *must* be even. (" << tile_size_x << "," << tile_size_y << ")");
			point src(EVAL_ARG(1).as_int(), EVAL_ARG(2).as_int());
			point dst(EVAL_ARG(3).as_int(), EVAL_ARG(4).as_int());
			ExpressionPtr heuristic = args()[4];
			ffl::IntrusivePtr<MapFormulaCallable> callable(new MapFormulaCallable(&variables));
			return variant(pathfinding::a_star_find_path(lvl, src, dst, heuristic, weight_expr, callable, tile_size_x, tile_size_y));
		FUNCTION_DYNAMIC_ARGUMENTS
		END_FUNCTION_DEF(plot_path)

		FUNCTION_DEF_CTOR(sort, 1, 2, "sort(list, criteria): Returns a nicely-ordered list. If you give it an optional formula such as 'a>b' it will sort it according to that. This example favours larger numbers first instead of the default of smaller numbers first.")
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
				return narg != 1;
			}
		FUNCTION_DEF_IMPL

			variant list = EVAL_ARG(0);
			std::vector<variant> vars;
			vars.reserve(list.num_elements());
			for(size_t n = 0; n != list.num_elements(); ++n) {
				vars.push_back(list[n]);
			}

			if(NUM_ARGS == 1) {
				std::stable_sort(vars.begin(), vars.end());
			} else {
				ffl::IntrusivePtr<variant_comparator> comparator(new variant_comparator(args()[1], variables));
				std::stable_sort(vars.begin(), vars.end(), [=](const variant& a, const variant& b) { return (*comparator)(a,b); });
			}

			return variant(&vars);

		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
			ARG_TYPE("bool");
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType();
		END_FUNCTION_DEF(sort)

		namespace 
		{
			//our own shuffle, to guarantee consistency across different machines.
			template<typename RnIt>
			void myshuffle(RnIt i1, RnIt i2)
			{
				while(i2 - i1 > 1) {
					std::swap(*(i2-1), i1[rng::generate()%(i2-i1)]);
					--i2;
				}
			}
		}

		FUNCTION_DEF(shuffle, 1, 1, "shuffle(list) - Returns a shuffled version of the list. Like shuffling cards.")
			variant list = EVAL_ARG(0);
			ffl::IntrusivePtr<FloatArrayCallable> f = list.try_convert<FloatArrayCallable>();
			if(f != nullptr) {
				std::vector<float> floats(f->floats().begin(), f->floats().end());
				myshuffle(floats.begin(), floats.end());
				return variant(new FloatArrayCallable(&floats));
			}
	
			ffl::IntrusivePtr<ShortArrayCallable> s = list.try_convert<ShortArrayCallable>();
			if(s != nullptr) {
				std::vector<short> shorts(s->shorts().begin(), s->shorts().end());
				myshuffle(shorts.begin(), shorts.end());
				return variant(new ShortArrayCallable(&shorts));
			}

			std::vector<variant> vars;
			vars.reserve(list.num_elements());
			for(size_t n = 0; n != list.num_elements(); ++n) {
				vars.push_back(list[n]);
			}

			myshuffle(vars.begin(), vars.end());

			return variant(&vars);
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType();
		END_FUNCTION_DEF(shuffle)

		FUNCTION_DEF(remove_from_map, 2, 2, "remove_from_map(map, key): Removes the given key from the map and returns it.")
			variant m = EVAL_ARG(0);
			ASSERT_LOG(m.is_map(), "ARG PASSED TO remove_from_map() IS NOT A MAP");
			variant key = EVAL_ARG(1);
			return m.remove_attr(key);
		FUNCTION_ARGS_DEF
			ARG_TYPE("map");
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType();
		END_FUNCTION_DEF(remove_from_map)
	
		namespace
		{
			void flatten_items( variant items, std::vector<variant>* output){
				for(size_t n = 0; n != items.num_elements(); ++n) {
			
					if( items[n].is_list() ){
						flatten_items(items[n], output);
					} else {
						output->push_back(items[n]);
					}
			
				}
			}

			variant_type_ptr flatten_type(variant_type_ptr type) {

				const std::vector<variant_type_ptr>* items = type->is_union();
				if(items) {
					std::vector<variant_type_ptr> result;
					for(auto item : *items) {
						result.push_back(flatten_type(item));
					}

					return variant_type::get_union(result);
				}

				variant_type_ptr result = type->is_list_of();
				if(result) {
					return flatten_type(result);
				} else {
					return type;
				}
			}
	
		}

		FUNCTION_DEF(flatten, 1, 1, "flatten(list): Returns a list with a depth of 1 containing the elements of any list passed in.")
			variant input = EVAL_ARG(0);
			std::vector<variant> output;
			flatten_items(input, &output);
			return variant(&output);
		FUNCTION_TYPE_DEF
			return variant_type::get_list(flatten_type(args()[0]->queryVariantType()));
		END_FUNCTION_DEF(flatten)

		static const std::string MapCallableFields[] = { "value", "index", "context", "key" };

		class MapCallableDefinition : public FormulaCallableDefinition
		{
		public:
			MapCallableDefinition(ConstFormulaCallableDefinitionPtr base, variant_type_ptr key_type, variant_type_ptr value_type, const std::string& value_name)
			  : base_(base), key_type_(key_type), value_type_(value_type)
			{
				for(int n = 0; n != NUM_MAP_CALLABLE_SLOTS; ++n) {
					entries_.push_back(Entry(MapCallableFields[n]));
					std::string class_name;
					switch(n) {
					case MAP_CALLABLE_VALUE:
						if(!value_name.empty()) {
							entries_.back().id = value_name;
						}

						if(value_type_) {
							entries_.back().variant_type = value_type_;
							if(entries_.back().variant_type->is_class(&class_name)) {
								entries_.back().type_definition = get_class_definition(class_name);
							}
						}
						break;
					case MAP_CALLABLE_INDEX:
						entries_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_INT);
						break;
					case MAP_CALLABLE_CONTEXT:
						entries_.back().variant_type = variant_type::get_type(variant::VARIANT_TYPE_CALLABLE);
						entries_.back().type_definition = base;
						break;
					case MAP_CALLABLE_KEY:
						if(key_type_) {
							entries_.back().variant_type = key_type_;
							if(key_type_->is_class(&class_name)) {
								entries_.back().type_definition = get_class_definition(class_name);
							}
						}
						break;
					}
				}
			}

			int getSlot(const std::string& key) const override {
				for(int n = 0; n != entries_.size(); ++n) {
					if(entries_[n].id == key) {
						return baseNumSlots() + n;
					}
				}

				if(base_) {
					return base_->getSlot(key);
				} else {
					return -1;
				}
			}

			Entry* getEntry(int slot) override {
				if(slot < 0) {
					return nullptr;
				}

				if(slot < baseNumSlots()) {
					return const_cast<FormulaCallableDefinition*>(base_.get())->getEntry(slot);
				}

				slot -= baseNumSlots();

				if(slot < 0 || static_cast<unsigned>(slot) >= entries_.size()) {
					return nullptr;
				}

				return &entries_[slot];
			}

			const Entry* getEntry(int slot) const override {
				if(slot < 0) {
					return nullptr;
				}

				if(slot < baseNumSlots()) {
					return const_cast<FormulaCallableDefinition*>(base_.get())->getEntry(slot);
				}

				slot -= baseNumSlots();

				if(slot < 0 || static_cast<unsigned>(slot) >= entries_.size()) {
					return nullptr;
				}

				return &entries_[slot];
			}

			bool getSymbolIndexForSlot(int slot, int* index) const override {
				if(slot < baseNumSlots()) {
					return base_->getSymbolIndexForSlot(slot, index);
				}

				slot -= baseNumSlots();

				if(static_cast<unsigned>(slot) < entries_.size()) {

					if(!hasSymbolIndexes()) {
						return false;
					}

					*index = getBaseSymbolIndex() + slot;
					return true;
				}

				return false;
			}

			int getBaseSymbolIndex() const override {
				int result = 0;
				if(base_) {
					result += base_->getBaseSymbolIndex();
				}

				if(hasSymbolIndexes()) {
					result += entries_.size();
				}

				return result;
			}

			int baseNumSlots() const {
				return base_ ? base_->getNumSlots() : 0;
			}

			int getNumSlots() const override {
				return NUM_MAP_CALLABLE_SLOTS + baseNumSlots();
			}

			int getSubsetSlotBase(const FormulaCallableDefinition* subset) const override
			{
				if(!base_) {
					return -1;
				}

				return base_->querySubsetSlotBase(subset);
			}

		private:
			ConstFormulaCallableDefinitionPtr base_;
			variant_type_ptr key_type_, value_type_;

			std::vector<Entry> entries_;
		};

		FUNCTION_DEF_CTOR(count, 2, 2, "count(list, expr): Returns an integer count of how many items in the list 'expr' returns true for.")
			if(!args().empty()) {
				def_ = this->args().back()->getDefinitionUsedByExpression();
			}
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
			ConstFormulaCallableDefinitionPtr def_;
		FUNCTION_DEF_IMPL
			const variant items = split_variant_if_str(EVAL_ARG(0));
			const int callable_num_slots = def_ ? def_->getNumSlots() : 0;
			if(items.is_map()) {
				int res = 0;
				ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, callable_num_slots));
				int index = 0;
				for(const auto& p : items.as_map()) {
					callable->set(p.first, p.second, index);
					const variant val = args().back()->evaluate(*callable);
					if(val.as_bool()) {
						++res;
					}

					++index;
				}

				return variant(res);
			} else {
				int res = 0;
				ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, callable_num_slots));
				for(int n = 0; n != items.num_elements(); ++n) {
					callable->set(items[n], n);
					const variant val = args().back()->evaluate(*callable);
					if(val.as_bool()) {
						++res;
					}
				}

				return variant(res);
			}

		CAN_VM
			return NUM_ARGS == 2 && canChildrenVM() && args().back()->getDefinitionUsedByExpression();
		FUNCTION_VM

			if(NUM_ARGS != 2) {
				return ExpressionPtr();
			}

			if(!def_) {
				return ExpressionPtr();
			}

			for(ExpressionPtr& a : args_mutable()) {
				optimizeChildToVM(a);
			}

			for(auto a : args()) {
				if(a->canCreateVM() == false) {
					return ExpressionPtr();
				}
			}

			args()[0]->emitVM(vm);
			vm.addInstruction(OP_PUSH_INT);
			vm.addInt(def_->getNumSlots());
			const int jump_from = vm.addJumpSource(OP_ALGO_FILTER);
			args()[1]->emitVM(vm);
			vm.jumpToEnd(jump_from);

			vm.addInstruction(OP_UNARY_NUM_ELEMENTS);

			return createVMExpression(vm, queryVariantType(), *this);
		FUNCTION_ARGS_DEF
			ARG_TYPE("list|map");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(count)

		FUNCTION_DEF_CTOR(filter, 2, 3, "filter(list, expr): ")
			if(args().size() == 3) {
				identifier_ = read_identifier_expression(*args()[1]);
			}

			if(!args().empty()) {
				def_ = args().back()->getDefinitionUsedByExpression();
			}
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
			std::string identifier_;
			ConstFormulaCallableDefinitionPtr def_;
		FUNCTION_DEF_IMPL
			std::vector<variant> vars;
			const variant items = EVAL_ARG(0);
			const int callable_base_slots = def_ ? def_->getNumSlots() : 0;

			if(NUM_ARGS == 2) {

				if(items.is_map()) {
					ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, callable_base_slots));
					std::map<variant,variant> m;
					int index = 0;
					for(const variant_pair& p : items.as_map()) {
						callable->set(p.first, p.second, index);
						const variant val = args().back()->evaluate(*callable);
						if(val.as_bool()) {
							m[p.first] = p.second;
						}

						++index;
					}

					return variant(&m);
				} else {
					ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, callable_base_slots));
					for(int n = 0; n != items.num_elements(); ++n) {
						callable->set(items[n], n);
						const variant val = args().back()->evaluate(*callable);
						if(val.as_bool()) {
							vars.push_back(items[n]);
						}
					}
				}
			} else {
				ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, callable_base_slots));
				const std::string self = identifier_.empty() ? EVAL_ARG(1).as_string() : identifier_;
				callable->setValue_name(self);

				for(int n = 0; n != items.num_elements(); ++n) {
					callable->set(items[n], n);
					const variant val = args().back()->evaluate(*callable);
					if(val.as_bool()) {
						vars.push_back(items[n]);
					}
				}
			}

			return variant(&vars);
		CAN_VM
			return NUM_ARGS == 2 && canChildrenVM() && args().back()->getDefinitionUsedByExpression().get() != nullptr;
		FUNCTION_VM

			if(NUM_ARGS != 2 || !def_) {
				return ExpressionPtr();
			}

			for(ExpressionPtr& a : args_mutable()) {
				optimizeChildToVM(a);
			}

			for(auto a : args()) {
				if(a->canCreateVM() == false) {
					return ExpressionPtr();
				}
			}

			args()[0]->emitVM(vm);
			vm.addInstruction(OP_PUSH_INT);
			vm.addInt(def_->getNumSlots());
			const int jump_from = vm.addJumpSource(OP_ALGO_FILTER);
			args()[1]->emitVM(vm);
			vm.jumpToEnd(jump_from);

			return createVMExpression(vm, queryVariantType(), *this);

		DEFINE_RETURN_TYPE
			variant_type_ptr list_type = args()[0]->queryVariantType();
			if(def_) {
				auto def = args()[1]->queryModifiedDefinitionBasedOnResult(true, def_);
				if(def) {
					const game_logic::FormulaCallableDefinition::Entry* value_entry = def->getEntryById("value");
					if(value_entry != nullptr && value_entry->variant_type && list_type->is_list_of()) {
						return variant_type::get_list(value_entry->variant_type);
					}
				}
			}

			if(list_type->is_list_of()) {
				return variant_type::get_list(list_type->is_list_of());
			} else if(list_type->is_map_of().first) {
				return variant_type::get_map(list_type->is_map_of().first, list_type->is_map_of().second);
			} else {
				std::vector<variant_type_ptr> v;
				v.push_back(variant_type::get_type(variant::VARIANT_TYPE_LIST));
				v.push_back(variant_type::get_type(variant::VARIANT_TYPE_MAP));
				return variant_type::get_union(v);
			}

		FUNCTION_ARGS_DEF
			bool found_valid_expr = false;
			std::vector<ConstExpressionPtr> expressions = args().back()->queryChildrenRecursive();
			for(ConstExpressionPtr expr : expressions) {
				const std::string& s = expr->str();
				if(s == "value" || s == "key" || s == "index" || s == identifier_) {
					found_valid_expr = true;
					break;
				}
			}
			ASSERT_LOG(found_valid_expr, "Last argument to filter() function does not contain 'value' or 'index' " << debugPinpointLocation());
		END_FUNCTION_DEF(filter)


		FUNCTION_DEF(unique, 1, 1, "unique(list): returns unique elements of list")
			std::vector<variant> v = EVAL_ARG(0).as_list();
			std::sort(v.begin(), v.end());
			v.erase(std::unique(v.begin(), v.end()), v.end());
			return variant(&v);
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			variant_type_ptr list_type = args()[0]->queryVariantType();
			if(list_type->is_list_of()) {
				return variant_type::get_list(list_type->is_list_of());
			} else {
				return variant_type::get_type(variant::VARIANT_TYPE_LIST);
			}
		END_FUNCTION_DEF(unique)

		FUNCTION_DEF(binary_search, 2, 2, "binary_search(list, item) ->bool: returns true iff item is in the list. List must be sorted.")
			variant v = EVAL_ARG(0);
			variant item = EVAL_ARG(1);
			size_t a = 0, b = v.num_elements();
			size_t iterations = 0;
			
			while(a < b) {
				size_t mid = (a + b) / 2;
				const variant& value = v[mid];
				if(item < value) {
					b = mid;
				} else if(value < item) {
					if(a == mid) {
						break;
					}

					a = mid;
				} else {
					return variant::from_bool(true);
				}

				ASSERT_LOG(iterations <= v.num_elements(), "Illegal binary search: " << v.write_json() << " item: " << item.write_json());
				++iterations;
			}

			return variant::from_bool(false);
			
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
			ARG_TYPE("any");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
		END_FUNCTION_DEF(binary_search)
	
		FUNCTION_DEF(mapping, -1, -1, "mapping(x): Turns the args passed in into a map. The first arg is a key, the second a value, the third a key, the fourth a value and so on and so forth.")
			MapFormulaCallable* callable = new MapFormulaCallable;
			for(size_t n = 0; n < NUM_ARGS-1; n += 2) {
				callable->add(EVAL_ARG(n).as_string(), EVAL_ARG(n+1));
			}
	
			return variant(callable);
		END_FUNCTION_DEF(mapping)


		FUNCTION_DEF_CTOR(find, 2, 3, "find")
			if(args().size() == 3) {
				identifier_ = read_identifier_expression(*args()[1]);
			}
			if(!args().empty()) {
				def_ = args().back()->getDefinitionUsedByExpression();
			}
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
				if(NUM_ARGS > 2 && narg == 1) {
					return false;
				}
				return true;
			}
			std::string identifier_;
			ConstFormulaCallableDefinitionPtr def_;
		FUNCTION_DEF_IMPL
			const variant items = EVAL_ARG(0);

			if(NUM_ARGS == 2) {
				ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
				for(int n = 0; n != items.num_elements(); ++n) {
					callable->set(items[n], n);
					const variant val = args().back()->evaluate(*callable);
					if(val.as_bool()) {
						return items[n];
					}
				}
			} else {
				ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));

				const std::string self = identifier_.empty() ? EVAL_ARG(1).as_string() : identifier_;
				callable->setValue_name(self);

				for(int n = 0; n != items.num_elements(); ++n) {
					callable->set(items[n], n);
					const variant val = args().back()->evaluate(*callable);
					if(val.as_bool()) {
						return items[n];
					}
				}
			}

			return variant();
		CAN_VM
			return NUM_ARGS == 2 && canChildrenVM();
		FUNCTION_VM

			if(NUM_ARGS != 2 || !def_) {
				return ExpressionPtr();
			}

			for(ExpressionPtr& a : args_mutable()) {
				optimizeChildToVM(a);
			}
			
			for(auto a : args()) {
				if(a->canCreateVM() == false) {
					return ExpressionPtr();
				}
			}

			args()[0]->emitVM(vm);
			vm.addInstruction(OP_PUSH_INT);
			vm.addInt(def_ ? def_->getNumSlots() : 0);
			const int jump_from = vm.addJumpSource(OP_ALGO_FIND);
			args()[1]->emitVM(vm);
			vm.jumpToEnd(jump_from);

			vm.addInstruction(OP_POP);

			return createVMExpression(vm, queryVariantType(), *this);

		DEFINE_RETURN_TYPE

			std::string value_str = "value";
			if(NUM_ARGS > 2) {
				variant literal;
				args()[1]->isLiteral(literal);
				if(literal.is_string()) {
					value_str = literal.as_string();
				} else if(args()[1]->isIdentifier(&value_str) == false) {
					ASSERT_LOG(false, "find function requires a literal as its second argument");
				}
			}

			ConstFormulaCallableDefinitionPtr def = def_;
			if(def) {
				ConstFormulaCallableDefinitionPtr modified = args().back()->queryModifiedDefinitionBasedOnResult(true, def);
				if(modified) {
					def = modified;
				}

				const game_logic::FormulaCallableDefinition::Entry* value_entry = def->getEntryById(value_str);
				if(value_entry != nullptr && value_entry->variant_type) {
					std::vector<variant_type_ptr> types;
					types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
					types.push_back(value_entry->variant_type);
					return variant_type::get_union(types);
				}
			}

			return variant_type::get_any();
		FUNCTION_ARGS_DEF

			bool found_valid_expr = false;
			std::vector<ConstExpressionPtr> expressions = args().back()->queryChildrenRecursive();
			for(ConstExpressionPtr expr : expressions) {
				const std::string& s = expr->str();
				if(s == "value" || s == "key" || s == "index" || s == identifier_) {
					found_valid_expr = true;
					break;
				}
			}

			ASSERT_LOG(found_valid_expr, "Last argument to find() function does not contain 'value' or 'index' " << debugPinpointLocation());
		END_FUNCTION_DEF(find)

		FUNCTION_DEF_CTOR(find_or_die, 2, 3, "find_or_die")
			if(!args().empty()) {
				def_ = args().back()->getDefinitionUsedByExpression();
			}
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
				return true;
			}
			ConstFormulaCallableDefinitionPtr def_;
		FUNCTION_DEF_IMPL
			const variant items = EVAL_ARG(0);

			ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
			for(int n = 0; n != items.num_elements(); ++n) {
				callable->set(items[n], n);
				const variant val = args().back()->evaluate(*callable);
				if(val.as_bool()) {
					return items[n];
				}
			}

			if(NUM_ARGS > 2) {
				ASSERT_LOG(false, "Failed to find expected item: " << args()[1]->evaluate(*callable) << " " << debugPinpointLocation());
			} else {
				ASSERT_LOG(false, "Failed to find expected item. List has: " << items.write_json() << " " << debugPinpointLocation());
			}

		CAN_VM
			return canChildrenVM() && def_;
		FUNCTION_VM

			if(!def_) {
				return ExpressionPtr();
			}

			for(ExpressionPtr& a : args_mutable()) {
				optimizeChildToVM(a);
			}
			
			for(auto a : args()) {
				if(a->canCreateVM() == false) {
					return ExpressionPtr();
				}
			}

			args()[0]->emitVM(vm);
			vm.addInstruction(OP_PUSH_INT);
			vm.addInt(def_ ? def_->getNumSlots() : 0);
			const int jump_from = vm.addJumpSource(OP_ALGO_FIND);
			args().back()->emitVM(vm);
			vm.jumpToEnd(jump_from);

			vm.addLoadConstantInstruction(variant(-1));
			vm.addInstruction(OP_EQ);

			const int jump_from_assert = vm.addJumpSource(OP_POP_JMP_UNLESS);

			vm.addLoadConstantInstruction(variant("Could not find item in find_or_die"));

			if(NUM_ARGS > 2) {
				args()[1]->emitVM(vm);
			} else {
				args()[0]->emitVM(vm);
			}

			vm.addInstruction(OP_ASSERT);

			vm.jumpToEnd(jump_from_assert);

			return createVMExpression(vm, queryVariantType(), *this);
		DEFINE_RETURN_TYPE

			std::string value_str = "value";
			ConstFormulaCallableDefinitionPtr def = def_;
			if(def) {
				ConstFormulaCallableDefinitionPtr modified = args().back()->queryModifiedDefinitionBasedOnResult(true, def);
				if(modified) {
					def = modified;
				}

				const game_logic::FormulaCallableDefinition::Entry* value_entry = def->getEntryById(value_str);
				if(value_entry != nullptr && value_entry->variant_type) {
					return value_entry->variant_type;
				}
			}

			return variant_type::get_any();
		FUNCTION_ARGS_DEF

			bool found_valid_expr = false;
			std::vector<ConstExpressionPtr> expressions = args().back()->queryChildrenRecursive();
			for(ConstExpressionPtr expr : expressions) {
				const std::string& s = expr->str();
				if(s == "value" || s == "key" || s == "index") {
					found_valid_expr = true;
					break;
				}
			}

			ASSERT_LOG(found_valid_expr, "Last argument to find() function does not contain 'value' or 'index' " << debugPinpointLocation());
		END_FUNCTION_DEF(find_or_die)

		FUNCTION_DEF_CTOR(find_index, 2, 2, "find_index")
		if (!args().empty()) {
			def_ = args().back()->getDefinitionUsedByExpression();
		}
		FUNCTION_DYNAMIC_ARGUMENTS
			FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
			return true;
		}
		std::string identifier_;
		ConstFormulaCallableDefinitionPtr def_;
		FUNCTION_DEF_IMPL
			const variant items = EVAL_ARG(0);

			ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
			for (int n = 0; n != items.num_elements(); ++n) {
				callable->set(items[n], n);
				const variant val = args().back()->evaluate(*callable);
				if (val.as_bool()) {
					return variant(n);
				}
			}

		return variant(-1);
		CAN_VM
			return false; // canChildrenVM();
		FUNCTION_VM
			return ExpressionPtr();
		DEFINE_RETURN_TYPE
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		FUNCTION_ARGS_DEF

			bool found_valid_expr = false;
		std::vector<ConstExpressionPtr> expressions = args().back()->queryChildrenRecursive();
		for (ConstExpressionPtr expr : expressions) {
			const std::string& s = expr->str();
			if (s == "value" || s == "key" || s == "index" || s == identifier_) {
				found_valid_expr = true;
				break;
			}
		}

		ASSERT_LOG(found_valid_expr, "Last argument to find_index() function does not contain 'value' or 'index' " << debugPinpointLocation());
		END_FUNCTION_DEF(find_index)

		FUNCTION_DEF_CTOR(find_index_or_die, 2, 2, "find_index_or_die")
			if (!args().empty()) {
				def_ = args().back()->getDefinitionUsedByExpression();
			}
		FUNCTION_DYNAMIC_ARGUMENTS
			FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
			return true;
		}
		std::string identifier_;
		ConstFormulaCallableDefinitionPtr def_;
		FUNCTION_DEF_IMPL
			const variant items = EVAL_ARG(0);

		ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
		for (int n = 0; n != items.num_elements(); ++n) {
			callable->set(items[n], n);
			const variant val = args().back()->evaluate(*callable);
			if (val.as_bool()) {
				return variant(n);
			}
		}

		ASSERT_LOG(false, "Failed to find expected item in find_index_or_die: " << args()[1]->evaluate(*callable) << " " << debugPinpointLocation());

		return variant(-1);
		CAN_VM
			return false; // canChildrenVM();
		FUNCTION_VM
			return ExpressionPtr();
		DEFINE_RETURN_TYPE
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		FUNCTION_ARGS_DEF

			bool found_valid_expr = false;
		std::vector<ConstExpressionPtr> expressions = args().back()->queryChildrenRecursive();
		for (ConstExpressionPtr expr : expressions) {
			const std::string& s = expr->str();
			if (s == "value" || s == "key" || s == "index" || s == identifier_) {
				found_valid_expr = true;
				break;
			}
		}

		ASSERT_LOG(found_valid_expr, "Last argument to find_index() function does not contain 'value' or 'index' " << debugPinpointLocation());
		END_FUNCTION_DEF(find_index_or_die)


		namespace 
		{
			void visit_objects(variant v, std::vector<variant>& res) {
				if(v.is_map()) {
					res.push_back(v);
					for(const variant_pair& value : v.as_map()) {
						visit_objects(value.second, res);
					}
				} else if(v.is_list()) {
					for(const variant& value : v.as_list()) {
						visit_objects(value, res);
					}
				} else if(v.try_convert<variant_callable>()) {
					res.push_back(v);
					variant keys = v.try_convert<variant_callable>()->getValue().getKeys();
					for(variant k : keys.as_list()) {
						visit_objects(v.try_convert<variant_callable>()->queryValue(k.as_string()), res);
					}
				}
			}
		}

		FUNCTION_DEF(visit_objects, 1, 1, "visit_objects")
			const variant v = EVAL_ARG(0);
			std::vector<variant> result;
			visit_objects(v, result);
			return variant(&result);
		END_FUNCTION_DEF(visit_objects)

		FUNCTION_DEF_CTOR(choose, 1, 2, "choose(list, (optional)scoring_expr) -> value: choose an item from the list according to which scores the highest according to the scoring expression, or at random by default.")
			if(!args().empty()) {
				def_ = args().back()->getDefinitionUsedByExpression();
			}
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
				return narg != 1;
			}
			ConstFormulaCallableDefinitionPtr def_;
		FUNCTION_DEF_IMPL

			if(NUM_ARGS == 1) {
				Formula::failIfStaticContext();
			}

			const variant items = EVAL_ARG(0);
			if(items.num_elements() == 0) {
				return variant();
			}

			if(NUM_ARGS == 1) {
				return items[rng::generate()%items.num_elements()];
			}

			int max_index = -1;
			variant max_value;
			ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
			for(int n = 0; n != items.num_elements(); ++n) {
				variant val;
		
				callable->set(items[n], n);
				val = args().back()->evaluate(*callable);

				if(n == 0 || val > max_value) {
					max_index = n;
					max_value = val;
				}
			}

			return items[max_index];
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType()->is_list_of();
		END_FUNCTION_DEF(choose)

		class map_function : public FunctionExpression {
		public:
			explicit map_function(const args_list& args)
				: FunctionExpression("map", args, 2, 3)
			{
				ASSERT_LOG(args.size() > 1, "attempted to create the mapping of a function to an iterable without passing at least two arguments (the iterable to map the function to, and the function to be mapped)");
				if(args.size() == 3) {
					identifier_ = read_identifier_expression(*args[1]);
				}
				def_ = args.back()->getDefinitionUsedByExpression();
			}

			bool dynamicArguments() const override { return true; }

			bool canCreateVM() const override {
				return args().size() == 2 && canChildrenVM() && def_.get() != nullptr;
			}

			ExpressionPtr optimizeToVM() override {
				if(NUM_ARGS != 2 || !def_) {
					return ExpressionPtr();
				}

				for(ExpressionPtr& a : args_mutable()) {
					optimizeChildToVM(a);
				}

				for(auto a : args()) {
					if(a->canCreateVM() == false) {
						return ExpressionPtr();
					}
				}

				formula_vm::VirtualMachine vm;

				args()[0]->emitVM(vm);
				vm.addInstruction(OP_PUSH_INT);
				vm.addInt(def_->getNumSlots());
				const int jump_from = vm.addJumpSource(OP_ALGO_MAP);
				args()[1]->emitVM(vm);
				vm.jumpToEnd(jump_from);

				return createVMExpression(vm, queryVariantType(), *this);

			}

		private:
			std::string identifier_;
			ConstFormulaCallableDefinitionPtr def_;

			variant execute(const FormulaCallable& variables) const override {
				std::vector<variant> vars;
				const variant items = EVAL_ARG(0);

				vars.reserve(items.num_elements());

				if(NUM_ARGS == 2) {

					if(items.is_map()) {
						ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
						int index = 0;
						for(const variant_pair& p : items.as_map()) {
							if(callable->refcount() > 1) {
								callable.reset(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
							}
							callable->set(p.first, p.second, index);
							const variant val = args().back()->evaluate(*callable);
							vars.push_back(val);
							++index;
						}
					} else if(items.is_string()) {
						const std::string& s = items.as_string();
						ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
						utils::utf8_to_codepoint cp(s);
						auto i1 = cp.begin();
						auto i2 = cp.end();
						for(int n = 0; i1 != i2; ++i1, ++n) {
							if(callable->refcount() > 1) {
								callable.reset(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
							}
							variant v(i1.get_char_as_string());
							callable->set(v, n);
							const variant val = args().back()->evaluate(*callable);
							vars.push_back(val);
						}
					} else {
						ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
						for(int n = 0; n != items.num_elements(); ++n) {
							if(callable->refcount() > 1) {
								callable.reset(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
							}
							callable->set(items[n], n);
							const variant val = args().back()->evaluate(*callable);
							vars.push_back(val);
						}
					}
				} else {
					ffl::IntrusivePtr<map_callable> callable(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
					const std::string self = identifier_.empty() ? EVAL_ARG(1).as_string() : identifier_;
					callable->setValue_name(self);
					for(int n = 0; n != items.num_elements(); ++n) {
						if(callable->refcount() > 1) {
							callable.reset(new map_callable(variables, def_ ? def_->getNumSlots() : 0));
							callable->setValue_name(self);
						}

						callable->set(items[n], n);
						const variant val = args().back()->evaluate(*callable);
						vars.push_back(val);
					}
				}

				return variant(&vars);
			}

			variant_type_ptr getVariantType() const override {
				variant_type_ptr spec_type = args()[0]->queryVariantType();
				if(spec_type->is_specific_list()) {
					std::vector<variant_type_ptr> types;
					variant_type_ptr type = args().back()->queryVariantType();
					const auto num_items = spec_type->is_specific_list()->size();
					for(size_t n = 0; n != num_items; ++n) {
						types.push_back(type);
					}

					return variant_type::get_specific_list(types);
				}

				return variant_type::get_list(args().back()->queryVariantType());
			}

		};

		FUNCTION_DEF(sum, 1, 2, "sum(list[, counter]): Adds all elements of the list together. If counter is supplied, all elements of the list are added to the counter instead of to 0.")
			variant res(0);
			const variant items = EVAL_ARG(0);
			if(NUM_ARGS >= 2) {
				res = EVAL_ARG(1);
			}
			for(int n = 0; n != items.num_elements(); ++n) {
				res = res + items[n];
			}

			return res;

		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			std::vector<variant_type_ptr> types;
			types.push_back(args()[0]->queryVariantType()->is_list_of());
			if(NUM_ARGS > 1) {
				types.push_back(args()[1]->queryVariantType());
			} else {
				types.push_back(variant_type::get_type(variant::VARIANT_TYPE_INT));
			}

			return variant_type::get_union(types);

			END_FUNCTION_DEF(sum)

				static const int StaticRangeListSize = 10000;
		variant create_static_range_list() {
			std::vector<variant> result;
			for (int i = 0; i < StaticRangeListSize; ++i) {
				result.push_back(variant(i));
			}

			return variant(&result);
		}

		FUNCTION_DEF(range, 1, 3, "range([start, ]finish[, step]): Returns a list containing all numbers smaller than the finish value and and larger than or equal to the start value. The start value defaults to 0.")

			static variant static_list = create_static_range_list();
			if (NUM_ARGS == 1) {
				int size = EVAL_ARG(0).as_int();
				if (size >= 0 && size <= StaticRangeListSize) {
					return static_list.get_list_slice(0, size);
				}
			}
			else if (NUM_ARGS == 2) {
				int begin = EVAL_ARG(0).as_int();
				int end = EVAL_ARG(1).as_int();
				if (begin >= 0 && end >= begin && end <= StaticRangeListSize) {
					return static_list.get_list_slice(begin, end);
				}
			}
			
			int start = NUM_ARGS > 1 ? EVAL_ARG(0).as_int() : 0;
			int end = EVAL_ARG(NUM_ARGS > 1 ? 1 : 0).as_int();
			int step = NUM_ARGS < 3 ? 1 : EVAL_ARG(2).as_int();
			ASSERT_LOG(step > 0, "ILLEGAL STEP VALUE IN RANGE: " << step);
			bool reverse = false;
			if(end < start) {
				std::swap(start, end);
				++start;
				++end;
				reverse = true;
			}
			const int nelem = end - start;

			std::vector<variant> v;

			if(nelem > 0) {
				v.reserve(nelem/step);

				for(int n = 0; n < nelem; n += step) {
					v.push_back(variant(start+n));
				}
			}

			if(reverse) {
				std::reverse(v.begin(), v.end());
			}

			return variant(&v);
		FUNCTION_TYPE_DEF
			return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_INT));
		END_FUNCTION_DEF(range)

		FUNCTION_DEF(reverse, 1, 1, "reverse(list): reverses the given list")
			std::vector<variant> items = EVAL_ARG(0).as_list();
			std::reverse(items.begin(), items.end());
			return variant(&items);
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			variant_type_ptr list_type = args()[0]->queryVariantType();
			if(list_type->is_list_of()) {
				return variant_type::get_list(list_type->is_list_of());
			} else {
				return variant_type::get_list(variant_type::get_any());
			}
		END_FUNCTION_DEF(reverse)

		FUNCTION_DEF(head, 1, 1, "head(list): gives the first element of a list, or null for an empty list")
			const variant items = EVAL_ARG(0);
			if(items.num_elements() >= 1) {
				return items[0];
			} else {
				return variant();
			}
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			std::vector<variant_type_ptr> types;
			types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
			types.push_back(args()[0]->queryVariantType()->is_list_of());
			return variant_type::get_union(types);
		END_FUNCTION_DEF(head)

		FUNCTION_DEF(head_or_die, 1, 1, "head_or_die(list): gives the first element of a list, or die for an empty list")
			const variant items = EVAL_ARG(0);
			ASSERT_LOG(items.num_elements() >= 1, "head_or_die() called on empty list");
			return items[0];
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType()->is_list_of();
		END_FUNCTION_DEF(head_or_die)

		FUNCTION_DEF(back, 1, 1, "back(list): gives the last element of a list, or null for an empty list")
			const variant items = EVAL_ARG(0);
			if(items.num_elements() >= 1) {
				return items[items.num_elements()-1];
			} else {
				return variant();
			}
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			std::vector<variant_type_ptr> types;
			types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
			types.push_back(args()[0]->queryVariantType()->is_list_of());
			return variant_type::get_union(types);
		END_FUNCTION_DEF(back)

		FUNCTION_DEF(back_or_die, 1, 1, "back_or_die(list): gives the last element of a list, or die for an empty list")
			const variant items = EVAL_ARG(0);
			ASSERT_LOG(items.num_elements() >= 1, "back_or_die() called on empty list");
			return items[items.num_elements()-1];
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			return args()[0]->queryVariantType()->is_list_of();
		END_FUNCTION_DEF(back_or_die)

		FUNCTION_DEF(get_all_files_under_dir, 1, 1, "get_all_files_under_dir(path): Returns a list of all the files in and under the given directory")
			std::vector<variant> v;
			std::map<std::string, std::string> file_paths;
			module::get_unique_filenames_under_dir(EVAL_ARG(0).as_string(), &file_paths);
			for(std::map<std::string, std::string>::const_iterator i = file_paths.begin(); i != file_paths.end(); ++i) {
				v.push_back(variant(i->second));
			}
			return variant(&v);
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
		FUNCTION_TYPE_DEF
			return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_STRING));
		END_FUNCTION_DEF(get_all_files_under_dir)

		FUNCTION_DEF(get_files_in_dir, 1, 1, "get_files_in_dir(path): Returns a list of the files in the given directory")
			Formula::failIfStaticContext();

			std::vector<variant> v;
			std::vector<std::string> files;
			std::string dirname = EVAL_ARG(0).as_string();
			if(dirname[dirname.size()-1] != '/') {
				dirname += '/';
			}
			module::get_files_in_dir(dirname, &files);
			for(std::vector<std::string>::const_iterator i = files.begin(); i != files.end(); ++i) {
				v.push_back(variant(*i));
			}
			return variant(&v);
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
		FUNCTION_TYPE_DEF
			return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_STRING));
		END_FUNCTION_DEF(get_files_in_dir)

		FUNCTION_DEF(dialog, 2, 2, "dialog(obj, template): Creates a dialog given an object to operate on and a template for the dialog.")
			variant environment = EVAL_ARG(0);
			variant dlg_template = EVAL_ARG(1);
			FormulaCallable* e = environment.try_convert<FormulaCallable>();
			variant v;
			if(dlg_template.is_string()) {
				std::string s = dlg_template.as_string();
				if(s.length() <= 4 || s.substr(s.length()-4) != ".cfg") {
					s += ".cfg";
				}
				v = json::parse_from_file(gui::get_dialog_file(s));
			} else {
				v = dlg_template;
			}
			auto d = widget_factory::create(v, e);
			return variant(d.get());
		FUNCTION_ARGS_DEF
			ARG_TYPE("object");
			ARG_TYPE("map|string");
		FUNCTION_TYPE_DEF
			return variant_type::get_builtin("dialog");
		END_FUNCTION_DEF(dialog)

		FUNCTION_DEF(show_modal, 1, 1, "show_modal(dialog): Displays a modal dialog on the screen.")
			variant graph = EVAL_ARG(0);
			gui::DialogPtr dialog = ffl::IntrusivePtr<gui::Dialog>(graph.try_convert<gui::Dialog>());
			ASSERT_LOG(dialog, "Dialog given is not of the correct type.");
			dialog->showModal();
			return variant::from_bool(dialog->cancelled() == false);
		FUNCTION_ARGS_DEF
			ARG_TYPE("builtin dialog|builtin file_chooser_dialog");
		RETURN_TYPE("bool")
		END_FUNCTION_DEF(show_modal)

		FUNCTION_DEF(index, 2, 2, "index(list, value) -> index of value in list: Returns the index of the value in the list or -1 if value wasn't found in the list.")
			variant value = EVAL_ARG(1);
			variant li = EVAL_ARG(0);
			for(int n = 0; n < li.num_elements(); n++) {
				if(value == li[n]) {
					return variant(n);
				}
			}
			return variant(-1);
		FUNCTION_ARGS_DEF
			ARG_TYPE("list");
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_INT);
		END_FUNCTION_DEF(index)

#if defined(USE_LUA)
		FUNCTION_DEF(CompileLua, 3, 3, "CompileLua(object, string, string) Compiles a lua script against a lua-enabled object. Returns the compiled script as an object with an execute method. The second argument is the 'name' of the script as will appear in lua debugging output (normally a filename). The third argument is the script.")
			game_logic::FormulaCallable* callable = const_cast<game_logic::FormulaCallable*>(EVAL_ARG(0).as_callable());
			ASSERT_LOG(callable != nullptr, "Argument to CompileLua was not a formula callable");
			game_logic::FormulaObject * object = dynamic_cast<game_logic::FormulaObject*>(callable);
			ASSERT_LOG(object != nullptr, "Argument to CompileLua was not a formula object");
			ffl::IntrusivePtr<lua::LuaContext> ctx = object->get_lua_context();
			ASSERT_LOG(ctx, "Argument to CompileLua was not a formula object with a lua context. (Check class definition?)");
			std::string name = EVAL_ARG(1).as_string();
			std::string script = EVAL_ARG(2).as_string();
			lua::LuaCompiledPtr result = ctx->compile(name, script);
			return variant(result.get());
		FUNCTION_ARGS_DEF
			ARG_TYPE("object")
			ARG_TYPE("string")
			ARG_TYPE("string")
		END_FUNCTION_DEF(CompileLua)
#endif

		namespace 
		{
			void evaluate_expr_for_benchmark(const FormulaExpression* expr, const FormulaCallable* variables, int ntimes)
			{
				for(int n = 0; n < ntimes; ++n) {
					expr->evaluate(*variables);
				}
			}
		}

		FUNCTION_DEF_CTOR(benchmark, 1, 1, "benchmark(expr): Executes expr in a benchmark harness and returns a string describing its benchmark performance")
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
				return false;
			}
		FUNCTION_DEF_IMPL
			using std::placeholders::_1;
			return variant(test::run_benchmark("benchmark", std::bind(evaluate_expr_for_benchmark, args()[0].get(), &variables, _1)));
		END_FUNCTION_DEF(benchmark)

		FUNCTION_DEF_CTOR(benchmark_once, 1, 1, "benchmark_once(expr): Executes expr once and returns a string giving the timing")
		FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
				return false;
			}
		FUNCTION_DEF_IMPL
			const int start_time = SDL_GetTicks();
			EVAL_ARG(0);
			const int end_time = SDL_GetTicks();
			std::ostringstream results;
			results << "Ran expression in " << (end_time - start_time) << "ms";
			return variant(results.str());
		END_FUNCTION_DEF(benchmark_once)

		FUNCTION_DEF(eval_with_lag, 2, 2, "eval_with_lag")
			Formula::failIfStaticContext();
			SDL_Delay(EVAL_ARG(0).as_int());
			return EVAL_ARG(1);
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_ARGS_DEF
			ARG_TYPE("int");
			ARG_TYPE("any");
		FUNCTION_TYPE_DEF
			return args()[1]->queryVariantType();
		END_FUNCTION_DEF(eval_with_lag)


		FUNCTION_DEF_CTOR(instrument, 2, 2, "instrument(string, expr): Executes expr and outputs debug instrumentation on the time it took with the given string")
		FUNCTION_DYNAMIC_ARGUMENTS
		FUNCTION_DEF_MEMBERS
			bool optimizeArgNumToVM(int narg) const override {
				return narg != 1;
			}
		FUNCTION_DEF_IMPL
			variant name = EVAL_ARG(0);
			variant result;
			uint64_t time_ns;
			{
				formula_profiler::Instrument instrument(name.as_string().c_str());
				result = args()[1]->evaluate(variables);
				time_ns = instrument.get_ns();
			}

			if(g_log_instrumentation) {
				double time_ms = time_ns/1000000.0;
				LOG_INFO("Instrument: " << name.as_string() << ": " <<  time_ms << "ms");
			}

			return result;

		CAN_VM
			return false;
		FUNCTION_VM
			return ExpressionPtr();
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			ARG_TYPE("any");
		FUNCTION_TYPE_DEF
			return args()[1]->queryVariantType();
		END_FUNCTION_DEF(instrument)

		class instrument_command : public game_logic::CommandCallable
		{
		public:
			instrument_command(variant name, variant cmd)
			  : name_(name), cmd_(cmd)
			{}
			virtual void execute(game_logic::FormulaCallable& ob) const override {
				const int begin = SDL_GetTicks();
				{
				formula_profiler::Instrument instrument(name_.as_string().c_str());
				ob.executeCommand(cmd_);
				}
				if(g_log_instrumentation) {
					const int end = SDL_GetTicks();
					LOG_INFO("Instrument Command: " << name_.as_string() << ": " << (end - begin) << "ms");
				}
			}
		private:
			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderVariant(&cmd_);
			};

			variant name_;
			variant cmd_;
		};
	
		FUNCTION_DEF(instrument_command, 2, 2, "instrument_command(string, expr): Executes expr and outputs debug instrumentation on the time it took with the given string")
			variant name = EVAL_ARG(0);
			const int begin = SDL_GetTicks();
			variant result;
			{
			formula_profiler::Instrument instrument(name.as_string().c_str());
			result = EVAL_ARG(1);
			}
			if(g_log_instrumentation) {
				const int end = SDL_GetTicks();
				LOG_INFO("Instrument: " << name.as_string() << ": " << (end - begin) << "ms");
			}
			return variant(new instrument_command(name, result));

		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			ARG_TYPE("any");
		FUNCTION_TYPE_DEF
			return variant_type::get_commands();
		END_FUNCTION_DEF(instrument_command)

		FUNCTION_DEF(start_profiling, 0, 0, "start_profiling()")
			Formula::failIfStaticContext();

			if(formula_profiler::Manager::get()) {
				formula_profiler::Manager::get()->init("profile.dat");
			}

			return variant();

		END_FUNCTION_DEF(start_profiling)
		

		FUNCTION_DEF(compress, 1, 2, "compress(string, (optional) compression_level): Compress the given string object")
			int compression_level = -1;
			if(NUM_ARGS > 1) {
				compression_level = EVAL_ARG(1).as_int();
			}
			const std::string s = EVAL_ARG(0).as_string();
			return variant(new zip::CompressedData(std::vector<char>(s.begin(), s.end()), compression_level));
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
		END_FUNCTION_DEF(compress)

		FUNCTION_DEF(size, 1, 1, "size(list)")

			const variant items = EVAL_ARG(0);
			return variant(static_cast<int>(items.num_elements()));
			RETURN_TYPE("int");

		CAN_VM
			return canChildrenVM();
		FUNCTION_VM
			for(ExpressionPtr& a : args_mutable()) {
				optimizeChildToVM(a);
			}

			for(auto a : args()) {
				if(a->canCreateVM() == false) {
					return ExpressionPtr();
				}
			}

			args()[0]->emitVM(vm);
			vm.addInstruction(OP_UNARY_NUM_ELEMENTS);

			return createVMExpression(vm, queryVariantType(), *this);

		END_FUNCTION_DEF(size)

		FUNCTION_DEF(split, 1, 2, "split(list, divider")

			std::vector<std::string> chopped;
			if(NUM_ARGS >= 2) {
				const std::string thestring = EVAL_ARG(0).as_string();
				const std::string delimiter = EVAL_ARG(1).as_string();
				chopped = util::split(thestring, delimiter);
			} else {
				const std::string thestring = EVAL_ARG(0).as_string();
				chopped = util::split(thestring);
			}

			std::vector<variant> res;
			for(size_t i=0; i<chopped.size(); ++i) {
				const std::string& part = chopped[i];
				res.push_back(variant(part));
			}
	
			return variant(&res);
		FUNCTION_TYPE_DEF
			return variant_type::get_list(args()[0]->queryVariantType());
		END_FUNCTION_DEF(split)

		FUNCTION_DEF(str, 1, 1, "str(s)")
			const variant item = EVAL_ARG(0);
			if(item.is_string()) {
				//just return as-is for something that's already a string.
				return item;
			}

			std::string str;
			item.serializeToString(str);
			return variant(str);
		FUNCTION_ARGS_DEF
			ARG_TYPE("any");
			RETURN_TYPE("string");
		END_FUNCTION_DEF(str)

		FUNCTION_DEF(strstr, 2, 2, "strstr(haystack, needle)")


			const std::string haystack = EVAL_ARG(0).as_string();
			const std::string needle = EVAL_ARG(1).as_string();

			const size_t pos = haystack.find(needle);

			if(pos == std::string::npos) {
				return variant(0);
			} else {
				return variant(static_cast<int>(pos + 1));
			}

		RETURN_TYPE("int")
		END_FUNCTION_DEF(strstr)

		FUNCTION_DEF(refcount, 1, 1, "refcount(obj)")
			return variant(EVAL_ARG(0).refcount());
		RETURN_TYPE("int")
		END_FUNCTION_DEF(refcount)

		FUNCTION_DEF(deserialize, 1, 1, "deserialize(obj)")

			Formula::failIfStaticContext();	
			return variant::create_variant_under_construction(addr_to_uuid(EVAL_ARG(0).as_string()));
		RETURN_TYPE("any")
		END_FUNCTION_DEF(deserialize)

			FUNCTION_DEF(is_string, 1, 1, "is_string(any)")
				return variant::from_bool(EVAL_ARG(0).is_string());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_string)

			FUNCTION_DEF(is_null, 1, 1, "is_null(any)")
				return variant::from_bool(EVAL_ARG(0).is_null());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_null)

			FUNCTION_DEF(is_int, 1, 1, "is_int(any)")
				return variant::from_bool(EVAL_ARG(0).is_int());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_int)

			FUNCTION_DEF(is_bool, 1, 1, "is_bool(any)")
				return variant::from_bool(EVAL_ARG(0).is_bool());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_bool)

			FUNCTION_DEF(is_decimal, 1, 1, "is_decimal(any)")
				return variant::from_bool(EVAL_ARG(0).is_decimal());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_decimal)

			FUNCTION_DEF(is_number, 1, 1, "is_number(any)")
				return variant::from_bool(EVAL_ARG(0).is_decimal() || EVAL_ARG(0).is_int());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_number)

			FUNCTION_DEF(is_map, 1, 1, "is_map(any)")
				return variant::from_bool(EVAL_ARG(0).is_map());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_map)

			FUNCTION_DEF(is_function, 1, 1, "is_function(any)")
				return variant::from_bool(EVAL_ARG(0).is_function());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_function)

			FUNCTION_DEF(is_list, 1, 1, "is_list(any)")
				return variant::from_bool(EVAL_ARG(0).is_list());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_list)

			FUNCTION_DEF(is_callable, 1, 1, "is_callable(any)")
				return variant::from_bool(EVAL_ARG(0).is_callable());
			FUNCTION_ARGS_DEF
				ARG_TYPE("any");
				RETURN_TYPE("bool")
			END_FUNCTION_DEF(is_callable)

			FUNCTION_DEF(mod, 2, 2, "mod(num,den)")
				int left = EVAL_ARG(0).as_int();
				int right = EVAL_ARG(1).as_int();
		
				return variant((left%right + right)%right);
			FUNCTION_ARGS_DEF
				ARG_TYPE("int|decimal")
				ARG_TYPE("int|decimal")
				RETURN_TYPE("int")
			END_FUNCTION_DEF(mod)

		class set_command : public game_logic::CommandCallable
		{
		public:
			set_command(variant target, const std::string& attr, const variant& variant_attr, variant val)
			  : target_(target), attr_(attr), variant_attr_(variant_attr), val_(val)
			{}
			virtual void execute(game_logic::FormulaCallable& ob) const override {
				if(target_.is_callable()) {
					ASSERT_LOG(!attr_.empty(), "ILLEGAL KEY IN SET OF CALLABLE: " << val_.write_json());
					target_.mutable_callable()->mutateValue(attr_, val_);
				} else if(target_.is_map()) {
					if(!attr_.empty()) {
						target_.add_attr_mutation(variant(attr_), val_);
					} else {
						target_.add_attr_mutation(variant_attr_, val_);
					}
				} else {
					ASSERT_LOG(!attr_.empty(), "ILLEGAL KEY IN SET OF CALLABLE: " << val_.write_json());
					ob.mutateValue(attr_, val_);
				}
			}

			std::string toDebugString() const override {
				std::ostringstream s;
				s << "(set command: " << attr_ << " -> " << val_.to_debug_string() << ")";
				return s.str();
			}

		protected:
			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderVariant(&target_, "TARGET");
				collector->surrenderVariant(&val_, "VALUE");
				collector->surrenderVariant(&variant_attr_, "VARIANT_ATTR");
			}
		private:
			mutable variant target_;
			std::string attr_;
			variant variant_attr_;
			variant val_;
		};

		class add_command : public game_logic::CommandCallable
		{
		public:
			add_command(variant target, const std::string& attr, const variant& variant_attr, variant val)
			  : target_(target), attr_(attr), variant_attr_(variant_attr), val_(val)
			{}
			virtual void execute(game_logic::FormulaCallable& ob) const override {
				if(target_.is_callable()) {
					ASSERT_LOG(!attr_.empty(), "ILLEGAL KEY IN ADD OF CALLABLE: " << val_.write_json());
					target_.mutable_callable()->mutateValue(attr_, target_.mutable_callable()->queryValue(attr_) + val_);
				} else if(target_.is_map()) {
					if(!attr_.empty()) {
						variant key(attr_);
						target_.add_attr_mutation(key, target_[key] + val_);
					} else {
						target_.add_attr_mutation(variant_attr_, target_[variant_attr_] + val_);
					}
				} else {
					ASSERT_LOG(!attr_.empty(), "ILLEGAL KEY IN ADD OF CALLABLE: " << val_.write_json());
					ob.mutateValue(attr_, ob.queryValue(attr_) + val_);
				}
			}

			std::string toDebugString() const override {
				std::ostringstream s;
				s << "(add command: " << attr_ << " -> +" << val_.to_debug_string() << ")";
				return s.str();
			}
		protected:
			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderVariant(&target_, "TARGET");
				collector->surrenderVariant(&val_, "VALUE");
				collector->surrenderVariant(&variant_attr_, "VARIANT_ATTR");
			}
		private:
			mutable variant target_;
			std::string attr_;
			variant variant_attr_;
			variant val_;
		};

		class set_by_slot_command : public game_logic::CommandCallable
		{
		public:
			set_by_slot_command(int slot, const variant& value)
			  : slot_(slot), value_(value)
			{}

			virtual void execute(game_logic::FormulaCallable& obj) const override {
				obj.mutateValueBySlot(slot_, value_);
			}

			void setValue(const variant& value) { value_ = value; }

			std::string toDebugString() const override {
				std::ostringstream s;
				s << "(set command (optimized): " << value_.to_debug_string() << ")";
				return s.str();
			}

		protected:
			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderVariant(&value_, "VALUE");
			}

		private:
			int slot_;
			variant value_;
		};

		class set_target_by_slot_command : public game_logic::CommandCallable
		{
		public:
			set_target_by_slot_command(variant target, int slot, const variant& value)
			  : target_(target.mutable_callable()), slot_(slot), value_(value)
			{
				ASSERT_LOG(target_.get(), "target of set is not a callable");
			}

			virtual void execute(game_logic::FormulaCallable& obj) const override {
				target_->mutateValueBySlot(slot_, value_);
			}

			void setValue(const variant& value) { value_ = value; }

			std::string toDebugString() const override {
				std::ostringstream s;
				s << "(set target command (optimized): " << value_.to_debug_string() << ")";
				return s.str();
			}

		protected:
			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderPtr(&target_, "TARGET");
				collector->surrenderVariant(&value_, "VALUE");
			}

		private:
			game_logic::FormulaCallablePtr target_;
			int slot_;
			variant value_;
		};

		class add_target_by_slot_command : public game_logic::CommandCallable
		{
		public:
			add_target_by_slot_command(variant target, int slot, const variant& value)
			  : target_(target.mutable_callable()), slot_(slot), value_(value)
			{
				ASSERT_LOG(target_.get(), "target of set is not a callable");
			}

			virtual void execute(game_logic::FormulaCallable& obj) const override {
				target_->mutateValueBySlot(slot_, target_->queryValueBySlot(slot_) + value_);
			}

			void setValue(const variant& value) { value_ = value; }

			std::string toDebugString() const override {
				std::ostringstream s;
				s << "(add target command (optimized): " << value_.to_debug_string() << ")";
				return s.str();
			}

		protected:
			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderPtr(&target_, "TARGET");
				collector->surrenderVariant(&value_, "VALUE");
			}

		private:
			game_logic::FormulaCallablePtr target_;
			int slot_;
			variant value_;
		};

		class add_by_slot_command : public game_logic::CommandCallable
		{
		public:
			add_by_slot_command(int slot, const variant& value)
			  : slot_(slot), value_(value)
			{}

			virtual void execute(game_logic::FormulaCallable& obj) const override {
				obj.mutateValueBySlot(slot_, obj.queryValueBySlot(slot_) + value_);
			}

			void setValue(const variant& value) { value_ = value; }

			std::string toDebugString() const override {
				std::ostringstream s;
				s << "(add command (optimized): " << value_.to_debug_string() << ")";
				return s.str();
			}

		protected:
			void surrenderReferences(GarbageCollector* collector) override {
				collector->surrenderVariant(&value_, "VALUE");
			}

		private:
			int slot_;
			variant value_;
		};

		class set_function : public FunctionExpression {
		public:
			set_function(const args_list& args, ConstFormulaCallableDefinitionPtr callable_def)
			  : FunctionExpression("set", args, 2, 2), slot_(-1) {
				variant literal;
				args[0]->isLiteral(literal);
				if(literal.is_string()) {
					key_ = literal.as_string();
				} else {
					args[0]->isIdentifier(&key_);
				}

				if(!key_.empty() && callable_def) {
					slot_ = callable_def->getSlot(key_);
				}
			}

			bool dynamicArguments() const override { return true; }

			bool optimizeArgNumToVM(int narg) const override {
				return narg != 0;
			}
		private:
			variant execute(const FormulaCallable& variables) const override {
				return executeWithArgs(variables, nullptr, -1);
			}
			
			variant executeWithArgs(const FormulaCallable& variables, const variant* passed_args, int num_passed_args) const override {
				if(slot_ != -1) {
					variant target(&variables);
					return variant(new set_target_by_slot_command(target, slot_, EVAL_ARG(1)));
				}

				if(!key_.empty()) {
					static const std::string MeKey = "me";
					variant target = variables.queryValue(MeKey);
					set_command* cmd = new set_command(target, key_, variant(), EVAL_ARG(1));
					cmd->setExpression(this);
					return variant(cmd);
				}

				std::string member;
				variant variant_member;
				variant target = args()[0]->evaluateWithMember(variables, member, &variant_member);
				set_command* cmd = new set_command(
				  target, member, variant_member, EVAL_ARG(1));
				cmd->setExpression(this);
				return variant(cmd);
			}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_commands();
			}

			void staticErrorAnalysis() const override {
				variant_type_ptr target_type = args()[0]->queryMutableType();
				if(!target_type || target_type->is_none()) {
					ASSERT_LOG(false, "Writing to non-writeable value: " << args()[0]->queryVariantType()->to_string() << " in " << str() << " " << debugPinpointLocation() << "\n");
					return;
				}

				if(!variant_types_compatible(target_type, args()[1]->queryVariantType())) {
					ASSERT_LOG(false, "Writing to value with invalid type " << target_type->to_string() << " <- " << args()[1]->queryVariantType()->to_string() << " in " << str() << " " << debugPinpointLocation() << "\n");
				}
			}

			std::string key_;
			int slot_;
		};

		class add_function : public FunctionExpression {
		public:
			add_function(const args_list& args, ConstFormulaCallableDefinitionPtr callable_def)
			  : FunctionExpression("add", args, 2, 2), slot_(-1) {
				variant literal;
				args[0]->isLiteral(literal);
				if(literal.is_string()) {
					key_ = literal.as_string();
				} else {
					args[0]->isIdentifier(&key_);
				}

				if(!key_.empty() && callable_def) {
					slot_ = callable_def->getSlot(key_);
					if(slot_ != -1) {
						cmd_ = ffl::IntrusivePtr<add_by_slot_command>(new add_by_slot_command(slot_, variant()));
					}
				}
			}

			bool dynamicArguments() const override { return true; }

			bool optimizeArgNumToVM(int narg) const override {
				return narg != 0;
			}
		private:
			variant execute(const FormulaCallable& variables) const override {
				return executeWithArgs(variables, nullptr, -1);
			}

			variant executeWithArgs(const FormulaCallable& variables, const variant* passed_args, int num_passed_args) const override {
				if(slot_ != -1) {
					variant target(&variables);
					return variant(new add_target_by_slot_command(target, slot_, EVAL_ARG(1)));
				}

				if(!key_.empty()) {
					static const std::string MeKey = "me";
					variant target = variables.queryValue(MeKey);
					add_command* cmd = new add_command(target, key_, variant(), EVAL_ARG(1));
					cmd->setExpression(this);
					return variant(cmd);
				}

				std::string member;
				variant variant_member;
				variant target = args()[0]->evaluateWithMember(variables, member, &variant_member);
				add_command* cmd = new add_command(
					  target, member, variant_member, EVAL_ARG(1));
				cmd->setExpression(this);
				return variant(cmd);
			}

			variant_type_ptr getVariantType() const override {
				return variant_type::get_commands();
			}

			void staticErrorAnalysis() const override {
				variant_type_ptr target_type = args()[0]->queryMutableType();
				if(!target_type || target_type->is_none()) {
					ASSERT_LOG(false, "Writing to non-writeable value: " << args()[0]->queryVariantType()->to_string() << " in " << str() << " " << debugPinpointLocation() << "\n");
					return;
				}

				if(!variant_types_compatible(target_type, args()[1]->queryVariantType())) {
					ASSERT_LOG(false, "Writing to value with invalid type " << args()[1]->queryVariantType()->to_string() << " -> " << args()[0]->queryVariantType()->to_string() << " in " << str() << " " << debugPinpointLocation() << "\n");
				}
			}

			std::string key_;
			int slot_;
			mutable ffl::IntrusivePtr<add_by_slot_command> cmd_;
		};


		class debug_command : public game_logic::CommandCallable
		{
		public:
			explicit debug_command(const std::string& str) : str_(str)
			{}
			virtual void execute(FormulaCallable& ob) const override {
		#ifndef NO_EDITOR
				debug_console::addMessage(str_);
		#endif
				LOG_INFO("CONSOLE: " << str_);
			}
		private:
			std::string str_;
		};

		FUNCTION_DEF_CTOR(debug, 1, -1, "debug(...): outputs arguments to the console")

		FUNCTION_DEF_MEMBERS
			std::string loc_;

		virtual bool useSingletonVM() const override { return false; }
		virtual void setDebugInfo(const variant& parent_formula,
									std::string::const_iterator begin_str,
									std::string::const_iterator end_str) override
		{
			FunctionExpression::setDebugInfo(parent_formula, begin_str, end_str);

			auto info = getParentFormula().get_debug_info();
			if(info && info->filename != nullptr) {
				std::string fname = *info->filename;

				//cut off everything but the filename
				std::reverse(fname.begin(), fname.end());
				auto itor = fname.begin();
				while(itor != fname.end() && *itor != '/' && *itor != '\\') {
					++itor;
				}

				if(itor != fname.end()) {
					fname.erase(itor, fname.end());
				}
				std::reverse(fname.begin(), fname.end());

				//output debug() call site
				loc_ = (formatter() << fname << ":" << info->line << ": ");
			}
		}

		FUNCTION_DEF_IMPL
			if(!preferences::debug()) {
				return variant();
			}

			std::string str = loc_;

			for(int n = 0; n != NUM_ARGS; ++n) {
				if(n > 0) {
					str += " ";
				}

				str += EVAL_ARG(n).to_debug_string();
			}

			return variant(new debug_command(str));

		FUNCTION_TYPE_DEF
			return variant_type::get_commands();
		END_FUNCTION_DEF(debug)

		FUNCTION_DEF(clear, 0, 0, "clear(): clears debug messages")
			return variant(new FnCommandCallableArg("clear", [=](FormulaCallable* callable) {
				debug_console::clearMessages();
			}));
		FUNCTION_TYPE_DEF
			return variant_type::get_commands();
		END_FUNCTION_DEF(clear)

		FUNCTION_DEF(log, 1, -1, "log(...): outputs arguments to stderr")
			Formula::failIfStaticContext();

			std::string str;
			for(int n = 0; n != NUM_ARGS; ++n) {
				if(n > 0) {
					str += " ";
				}

				str += EVAL_ARG(n).to_debug_string();
			}

			LOG_INFO("LOG: " << str);

			if(g_log_console_filter.empty() == false) {
				static const boost::regex re(g_log_console_filter);
			 	boost::match_results<std::string::const_iterator> m;
				if(boost::regex_match(str, m, re)) {
					return variant(new debug_command(str));
				}
			}

			return variant();
			
		FUNCTION_TYPE_DEF
			return variant_type::get_commands();
		END_FUNCTION_DEF(log)


		namespace 
		{
			void debug_side_effect(variant v, const variant* v2=nullptr)
			{
				std::string s = v.to_debug_string();
				if(v2) {
					std::string s2 = v2->to_debug_string();
					s += ": " + s2;
				}
			#ifndef NO_EDITOR
				bool write_to_console = g_dump_to_console;

				if(!write_to_console && g_log_console_filter.empty() == false) {
					static const boost::regex re(g_log_console_filter);
				 	boost::match_results<std::string::const_iterator> m;
					if(boost::regex_match(s, m, re)) {
						write_to_console = true;
					}
				}

				if(write_to_console) {
					debug_console::addMessage(s);
				}
			#endif
				LOG_INFO("CONSOLE: " << s);
			}
		}

		FUNCTION_DEF(dump, 1, 2, "dump(msg[, expr]): evaluates and returns expr. Will print 'msg' to stderr if it's printable, or execute it if it's an executable command.")

			variant a = EVAL_ARG(0);

			variant b;
			if(NUM_ARGS > 1) {
				b = EVAL_ARG(1);

				if(a.is_string() && a != b) {
					a = variant(a.as_string() + ": " + b.to_debug_string());
				}
			} else {
				b = a;
			}

			debug_side_effect(a);
			return b;

		FUNCTION_TYPE_DEF
			return args().back()->queryVariantType();
		END_FUNCTION_DEF(dump)

		bool consecutive_periods(char a, char b) {
			return a == '.' && b == '.';
		}

std::map<std::string, variant>& get_doc_cache(bool prefs_dir) {

	if(prefs_dir) {
		static std::map<std::string, variant> cache;
		return cache;
	} else {
		static std::map<std::string, variant> cache2;
		return cache2;
	}
}

		PREF_BOOL(write_backed_maps, false, "Write to backed maps such as used in Citadel's evolutionary system");

		class backed_map : public game_logic::FormulaCallable {
		public:
			static void flush_all() {
				for(backed_map* m : all_backed_maps) {
					m->write_file();
				}
			}

			backed_map(const std::string& docname, variant generator, variant m)
			  : docname_(docname), generator_(generator)
			{
				all_backed_maps.insert(this);

				if(m.is_map()) {
					for(const variant::map_pair& p : m.as_map()) {
						map_[p.first.as_string()].value = p.second;
					}
				}

				if(sys::file_exists(docname_)) {
					try {
						variant v = json::parse(sys::read_file(docname_));

						if(sys::file_exists(docname_ + ".stats")) {
							try {
								variant stats = json::parse(sys::read_file(docname_ + ".stats"));
								for(const variant::map_pair& p : stats.as_map()) {
									map_[p.first.as_string()] = NodeInfo(p.second);
								}
							} catch(json::ParseError&) {
							}
						}

						for(const variant::map_pair& p : v.as_map()) {
							if(p.first.as_string() != "_stats") {
								map_[p.first.as_string()].value = p.second;
							}
						}
					} catch(json::ParseError& e) {
						ASSERT_LOG(false, "Error parsing json for backed map in " << docname_ << ": " << e.errorMessage());
					}
				}

				write_file();
			}

			~backed_map() {
				write_file();
				all_backed_maps.erase(this);
			}
		private:
			variant getValue(const std::string& key) const override {
				std::map<std::string, NodeInfo>::const_iterator i = map_.find(key);
				if(i != map_.end()) {
					i->second.last_session_reads++;
					i->second.lifetime_reads++;
					return i->second.value;
				}

				std::vector<variant> args;
				variant new_value = generator_(args);
				const_cast<backed_map*>(this)->mutateValue(key, new_value);
				return new_value;
			}

			void setValue(const std::string& key, const variant& value) override {
				map_[key].value = value;

				write_file();
			}

			void write_file()
			{
				if(!g_write_backed_maps) {
					return;
				}

				std::map<variant, variant> v;
				std::map<variant, variant> stats;
				for(std::map<std::string,NodeInfo>::const_iterator i = map_.begin();
					i != map_.end(); ++i) {
					v[variant(i->first)] = i->second.value;
					stats[variant(i->first)] = i->second.write();
				}

				sys::write_file(docname_, variant(&v).write_json());
				sys::write_file(docname_ + ".stats", variant(&stats).write_json());
			}

			struct NodeInfo {
				NodeInfo() : last_session_reads(0), lifetime_reads(0), value(0)
				{}

				NodeInfo(variant v) : last_session_reads(0), lifetime_reads(v["lifetime_reads"].as_int()), value(0)
				{}
		
				variant write() const {
					std::map<variant,variant> m;
					m[variant("last_session_reads")] = variant(last_session_reads);
					m[variant("lifetime_reads")] = variant(lifetime_reads);
					return variant(&m);
				}

				mutable int last_session_reads;
				mutable int lifetime_reads;

				variant value;
			};

			std::string docname_;
			std::map<std::string, NodeInfo> map_;
			variant generator_;
			static std::set<backed_map*> all_backed_maps;
		};

		std::set<backed_map*> backed_map::all_backed_maps;
	} //namespace {

	void flush_all_backed_maps()
	{
		backed_map::flush_all();
	}

	namespace 
	{
		FUNCTION_DEF(file_backed_map, 2, 3, "file_backed_map(string filename, function generate_new, map initial_values)")
			Formula::failIfStaticContext();
			std::string docname = EVAL_ARG(0).as_string();

			if(docname.empty()) {
				return variant("DOCUMENT NAME GIVEN TO write_document() IS EMPTY");
			}
			if(sys::is_path_absolute(docname)) {
				return variant(formatter() << "DOCUMENT NAME IS ABSOLUTE PATH " << docname);
			}
			if(std::adjacent_find(docname.begin(), docname.end(), consecutive_periods) != docname.end()) {
				return variant(formatter() << "RELATIVE PATH OUTSIDE ALLOWED " << docname);
			}

			if(sys::file_exists(module::map_file(docname))) {
				docname = module::map_file(docname);
			} else {
				docname = preferences::user_data_path() + docname;
			}

			variant fn = EVAL_ARG(1);

			variant m;
			if(NUM_ARGS > 2) {
				m = EVAL_ARG(2);
			}

			return variant(new backed_map(docname, fn, m));
		FUNCTION_TYPE_DEF
			return variant_type::get_type(variant::VARIANT_TYPE_CALLABLE);
		END_FUNCTION_DEF(file_backed_map)

		FUNCTION_DEF(remove_document, 1, 2, "remove_document(string filename, [enum{game_dir}]): deletes document at the given filename")

			bool prefs_directory = true;

			if(NUM_ARGS > 1) {
				const variant flags = EVAL_ARG(1);
				for(int n = 0; n != flags.num_elements(); ++n) {
					variant f = flags[n];
					const std::string& flag = f.is_enum() ? f.as_enum() : f.as_string();
					if(flag == "game_dir") {
						prefs_directory = false;
					} else {
						ASSERT_LOG(false, "Illegal flag to write_document: " << flag);
					}
				}
			}
			Formula::failIfStaticContext();
			std::string docname = EVAL_ARG(0).as_string();


			std::string path_error;
			if(!sys::is_safe_write_path(docname, &path_error)) {
				ASSERT_LOG(false, "ERROR in write_document(" + docname + "): " + path_error);
			}

			if(docname.empty()) {
				ASSERT_LOG(false, "DOCUMENT NAME GIVEN TO write_document() IS EMPTY");
			}
			if(sys::is_path_absolute(docname)) {
				ASSERT_LOG(false, "DOCUMENT NAME IS ABSOLUTE PATH " << docname);
			}
			if(std::adjacent_find(docname.begin(), docname.end(), consecutive_periods) != docname.end()) {
				ASSERT_LOG(false, "RELATIVE PATH OUTSIDE ALLOWED " << docname);
			}

			return variant(new FnCommandCallableArg("remove_document", [=](FormulaCallable* callable) {
				get_doc_cache(prefs_directory).erase(docname);

				std::string real_docname = preferences::user_data_path() + docname;
				if(prefs_directory == false) {
					real_docname = module::map_file(docname);
				}

				sys::remove_file(real_docname);
			}));

		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			ARG_TYPE("[enum{game_dir}]");
			RETURN_TYPE("commands")
		END_FUNCTION_DEF(remove_document)


		FUNCTION_DEF(write_document, 2, 3, "write_document(string filename, doc, [enum{game_dir}]): writes 'doc' to the given filename")

			bool prefs_directory = true;

			if(NUM_ARGS > 2) {
				const variant flags = EVAL_ARG(2);
				for(int n = 0; n != flags.num_elements(); ++n) {
					variant f = flags[n];
					const std::string& flag = f.is_enum() ? f.as_enum() : f.as_string();
					if(flag == "game_dir") {
						prefs_directory = false;
					} else {
						ASSERT_LOG(false, "Illegal flag to write_document: " << flag);
					}
				}
			}

			Formula::failIfStaticContext();
			std::string docname = EVAL_ARG(0).as_string();
			variant doc = EVAL_ARG(1);


			std::string path_error;
			if(!sys::is_safe_write_path(docname, &path_error)) {
				ASSERT_LOG(false, "ERROR in write_document(" + docname + "): " + path_error);
			}

			if(docname.empty()) {
				ASSERT_LOG(false, "DOCUMENT NAME GIVEN TO write_document() IS EMPTY");
			}
			if(sys::is_path_absolute(docname)) {
				ASSERT_LOG(false, "DOCUMENT NAME IS ABSOLUTE PATH " << docname);
			}
			if(std::adjacent_find(docname.begin(), docname.end(), consecutive_periods) != docname.end()) {
				ASSERT_LOG(false, "RELATIVE PATH OUTSIDE ALLOWED " << docname);
			}

			return variant(new FnCommandCallableArg("write_document", [=](FormulaCallable* callable) {
				get_doc_cache(prefs_directory)[docname] = doc;

				std::string real_docname = preferences::user_data_path() + docname;
				if(prefs_directory == false) {
					real_docname = module::map_write_path(docname);
				}

				sys::write_file(real_docname, game_logic::serialize_doc_with_objects(doc).write_json());
			}));
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			ARG_TYPE("any");
			ARG_TYPE("[enum{game_dir}]|[string]");
			RETURN_TYPE("commands")
		END_FUNCTION_DEF(write_document)

		FUNCTION_DEF(get_document_from_str, 1, 1, "get_document_from_str(string doc)")
			return deserialize_doc_with_objects(EVAL_ARG(0).as_string());
		FUNCTION_ARGS_DEF
			ARG_TYPE("string")
			RETURN_TYPE("any")
		END_FUNCTION_DEF(get_document_from_str)

		FUNCTION_DEF(get_document, 1, 2, "get_document(string filename, [enum{null_on_failure,user_preferences_dir,uncached,json}] flags): return reference to the given JSON document.")
			if(NUM_ARGS != 1) {
				Formula::failIfStaticContext();
			}

			variant base_docname_var = EVAL_ARG(0);
			const std::string& base_docname = base_docname_var.as_string();
			ASSERT_LOG(base_docname.empty() == false, "DOCUMENT NAME GIVEN TO get_document() IS EMPTY");

			bool allow_failure = false;
			bool prefs_directory = false;
			bool use_cache = true;
			bool straight_json = false;

			if(NUM_ARGS > 1) {
				const variant flags = EVAL_ARG(1);
				for(int n = 0; n != flags.num_elements(); ++n) {
					variant f = flags[n];
					const std::string& flag = f.is_enum() ? flags[n].as_enum() : flags[n].as_string();
					if(flag == "null_on_failure") {
						allow_failure = true;
					} else if(flag == "user_preferences_dir") {
						prefs_directory = true;
					} else if(flag == "uncached") {
						use_cache = false;
					} else if(flag == "json") {
						straight_json = true;
					} else {
						ASSERT_LOG(false, "illegal flag given to get_document: " << flag);
					}
				}
			}

			if(use_cache) {
				auto itor = get_doc_cache(prefs_directory).find(base_docname);
				if(itor != get_doc_cache(prefs_directory).end()) {
					return itor->second;
				}
			}

			std::string docname = base_docname;

			ASSERT_LOG(std::adjacent_find(docname.begin(), docname.end(), consecutive_periods) == docname.end(), "DOCUMENT NAME CONTAINS ADJACENT PERIODS " << docname);

			if(prefs_directory) {
				//docname = sys::compute_relative_path(preferences::user_data_path(), docname);
				docname = preferences::user_data_path() + docname;
			} else {
				ASSERT_LOG(!sys::is_path_absolute(docname), "DOCUMENT NAME USES AN ABSOLUTE PATH WHICH IS NOT ALLOWED: " << docname);
				docname = module::map_file(docname);
			}

			try {
				variant result;
				if(straight_json) {
					result = json::parse_from_file(docname, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
				} else {
					result = game_logic::deserialize_file_with_objects(docname);
				}

				if(use_cache) {
					get_doc_cache(prefs_directory)[docname] = result;
				}
				return result;
			} catch(json::ParseError& e) {
				if(allow_failure) {
					if(use_cache) {
						get_doc_cache(prefs_directory)[docname] = variant();
					}
					return variant();
				}

				ASSERT_LOG(false, "COULD NOT LOAD DOCUMENT: " << e.errorMessage());
				return variant();
			}
		FUNCTION_ARGS_DEF
			ARG_TYPE("string");
			ARG_TYPE("[enum{null_on_failure,user_preferences_dir,uncached,json}]|[string]");
		FUNCTION_TYPE_DEF
			std::vector<variant_type_ptr> types;
			types.push_back(variant_type::get_type(variant::VARIANT_TYPE_MAP));
			types.push_back(variant_type::get_type(variant::VARIANT_TYPE_NULL));
			return variant_type::get_union(types);
		END_FUNCTION_DEF(get_document)
	}

	void remove_formula_function_cached_doc(const std::string& name)
	{
	get_doc_cache(true).erase(name);
	get_doc_cache(false).erase(name);
	}

	void FunctionExpression::clearUnusedArguments()
	{
		int index = 0;
		for(ExpressionPtr& a : args_) {
			if(optimizeArgNumToVM(index)) {
				a.reset();
			}
			++index;
		}
	}

	void FunctionExpression::check_arg_type(int narg, const std::string& type_str) const
	{
		variant type_v(type_str);
		variant_type_ptr type;

		try {
			type = parse_variant_type(type_v);
		} catch(...) {
			ASSERT_LOG(false, "BAD ARG TYPE SPECIFIED: " << type);
		}

		check_arg_type(narg, type);
	}

	void FunctionExpression::check_arg_type(int narg, variant_type_ptr type) const
	{
		if(static_cast<unsigned>(narg) >= NUM_ARGS) {
			return;
		}

		variant_type_ptr provided = args()[narg]->queryVariantType();
		assert(provided);

		if(!variant_types_compatible(type, provided)) {
			std::ostringstream reason;
			variant_types_compatible(type, provided, &reason);
			std::string msg = reason.str();
			if(msg.empty() == false) {
				msg = " (" + msg + ")";
			}
			ASSERT_LOG(variant_types_compatible(type, provided), "Function call argument " << (narg+1) << " does not match. Function expects " << type->to_string() << " provided " << provided->to_string() << msg << " " << debugPinpointLocation());
		}
	}

	FormulaFunctionExpression::FormulaFunctionExpression(const std::string& name, const args_list& args, ConstFormulaPtr formula, ConstFormulaPtr precondition, const std::vector<std::string>& arg_names, const std::vector<variant_type_ptr>& variant_types)
		: FunctionExpression(name, args, static_cast<int>(arg_names.size()), static_cast<int>(arg_names.size())),
		  formula_(formula), 
		  precondition_(precondition), 
		  arg_names_(arg_names), 
		  variant_types_(variant_types), 
		  star_arg_(-1), 
		  has_closure_(false), 
		  base_slot_(0)
	{
		assert(!precondition_ || !precondition_->str().empty());
		for(size_t n = 0; n != arg_names_.size(); ++n) {
			if(arg_names_.empty() == false && arg_names_[n][arg_names_[n].size()-1] == '*') {
				arg_names_[n].resize(arg_names_[n].size()-1);
				star_arg_ = static_cast<int>(n);
				break;
			}
		}
	}

	namespace 
	{
		std::stack<const FormulaFunctionExpression*> formula_fn_stack;
		struct formula_function_scope 
		{
			explicit formula_function_scope(const FormulaFunctionExpression* f) {
				formula_fn_stack.push(f);
			}

			~formula_function_scope() {
				formula_fn_stack.pop();
			}
		};

		bool is_calculating_recursion = false;

		struct recursion_calculation_scope 
		{
			recursion_calculation_scope() { is_calculating_recursion = true; }
			~recursion_calculation_scope() { is_calculating_recursion = false; }
		};
	}

	ffl::IntrusivePtr<SlotFormulaCallable> FormulaFunctionExpression::calculate_args_callable(const FormulaCallable& variables) const
	{
		if(!callable_ || callable_->refcount() != 1) {
			callable_ = ffl::IntrusivePtr<SlotFormulaCallable>(new SlotFormulaCallable);
			callable_->reserve(arg_names_.size());
			callable_->setBaseSlot(base_slot_);
		}

		callable_->setNames(&arg_names_);

		//we reset callable_ to nullptr during any calls so that recursive calls
		//will work properly.
		ffl::IntrusivePtr<SlotFormulaCallable> tmp_callable(callable_);
		callable_.reset(nullptr);

		for(unsigned n = 0; n != arg_names_.size(); ++n) {
			variant var = EVAL_ARG(n);

			if(n < variant_types_.size() && variant_types_[n]) {
				ASSERT_LOG(variant_types_[n]->match(var), "FUNCTION ARGUMENT " << (n+1) << " EXPECTED TYPE " << variant_types_[n]->str() << " BUT FOUND " << var.write_json() << " TYPE " << get_variant_type_from_value(var)->to_string() << " AT " << debugPinpointLocation());
			}

			tmp_callable->add(var);
			if(n == star_arg_) {
				tmp_callable->setFallback(var.as_callable());
			}
		}

		return tmp_callable;
	}

	variant FormulaFunctionExpression::execute(const FormulaCallable& variables) const
	{
		if(fed_result_) {
			variant result = *fed_result_;
			fed_result_.reset();
			return result;
		}

		ffl::IntrusivePtr<SlotFormulaCallable> tmp_callable = calculate_args_callable(variables);

		if(precondition_) {
			if(!precondition_->execute(*tmp_callable).as_bool()) {
				std::ostringstream ss;
				ss << "FAILED function precondition (" << precondition_->str() << ") for function '" << formula_->str() << "' with arguments: ";
				for(size_t n = 0; n != arg_names_.size(); ++n) {
					ss << "  arg " << (n+1) << ": " << EVAL_ARG(n).to_debug_string();
				}
				LOG_ERROR(ss.str());
			}
		}

		if(!is_calculating_recursion && formula_->hasGuards() && !formula_fn_stack.empty() && formula_fn_stack.top() == this) {
			const recursion_calculation_scope recursion_scope;

			typedef ffl::IntrusivePtr<FormulaCallable> call_ptr;
			std::vector<call_ptr> invocations;
			invocations.push_back(tmp_callable);
			while(formula_->guardMatches(*invocations.back()) == -1) {
				invocations.push_back(calculate_args_callable(*formula_->wrapCallableWithGlobalWhere(*invocations.back())));
			}

			invocations.pop_back();

			if(invocations.size() > 2) {
				while(invocations.empty() == false) {
					fed_result_.reset(new variant(formula_->expr()->evaluate(*formula_->wrapCallableWithGlobalWhere(*invocations.back()))));
					invocations.pop_back();
				}

				variant result = *fed_result_;
				fed_result_.reset();
				return result;
			}
		}

		formula_function_scope scope(this);
		variant res = formula_->execute(*tmp_callable);

		callable_ = tmp_callable;
		callable_->clear();

		return res;
	}

		FormulaFunctionExpressionPtr FormulaFunction::generateFunctionExpression(const std::vector<ExpressionPtr>& args_input) const
		{
			std::vector<ExpressionPtr> args = args_input;
			if(args.size() + default_args_.size() >= args_.size()) {
				const auto base = args_.size() - default_args_.size();
				while(args.size() < args_.size()) {
					const auto index = args.size() - base;
					ASSERT_LOG(index < default_args_.size(), "INVALID INDEX INTO DEFAULT ARGS: " << index << " / " << default_args_.size());
					args.push_back(ExpressionPtr(new VariantExpression(default_args_[index])));
				}
			}

			return FormulaFunctionExpressionPtr(new FormulaFunctionExpression(name_, args, formula_, precondition_, args_, variant_types_));
		}

		void FunctionSymbolTable::addFormulaFunction(const std::string& name, ConstFormulaPtr formula, ConstFormulaPtr precondition, const std::vector<std::string>& args, const std::vector<variant>& default_args, const std::vector<variant_type_ptr>& variant_types)
		{
			custom_formulas_[name] = FormulaFunction(name, formula, precondition, args, default_args, variant_types);
		}

		ExpressionPtr FunctionSymbolTable::createFunction(const std::string& fn, const std::vector<ExpressionPtr>& args, ConstFormulaCallableDefinitionPtr callable_def) const
		{

			const std::map<std::string, FormulaFunction>::const_iterator i = custom_formulas_.find(fn);
			if(i != custom_formulas_.end()) {
				return i->second.generateFunctionExpression(args);
			}

			if(backup_) {
				return backup_->createFunction(fn, args, callable_def);
			}

			return ExpressionPtr();
		}

		std::vector<std::string> FunctionSymbolTable::getFunctionNames() const
		{
			std::vector<std::string> res;
			for(std::map<std::string, FormulaFunction>::const_iterator iter = custom_formulas_.begin(); iter != custom_formulas_.end(); iter++ ) {
				res.push_back((*iter).first);
			}
			return res;
		}

		const FormulaFunction* FunctionSymbolTable::getFormulaFunction(const std::string& fn) const
		{
			const std::map<std::string, FormulaFunction>::const_iterator i = custom_formulas_.find(fn);
			if(i == custom_formulas_.end()) {
				return nullptr;
			} else {
				return &i->second;
			}
		}

		RecursiveFunctionSymbolTable::RecursiveFunctionSymbolTable(const std::string& fn, const std::vector<std::string>& args, const std::vector<variant>& default_args, FunctionSymbolTable* backup, ConstFormulaCallableDefinitionPtr closure_definition, const std::vector<variant_type_ptr>& variant_types)
		: name_(fn), stub_(fn, ConstFormulaPtr(), ConstFormulaPtr(), args, default_args, variant_types), backup_(backup), closure_definition_(closure_definition)
		{
		}

		ExpressionPtr RecursiveFunctionSymbolTable::createFunction(
						const std::string& fn,
						const std::vector<ExpressionPtr>& args,
						ConstFormulaCallableDefinitionPtr callable_def) const
		{
			if(fn == name_) {
				FormulaFunctionExpressionPtr expr = stub_.generateFunctionExpression(args);
				if(closure_definition_) {
					expr->set_has_closure(closure_definition_->getNumSlots());
				}
				expr_.push_back(expr);
				return expr;
			} else if(backup_) {
				return backup_->createFunction(fn, args, callable_def);
			}

			return ExpressionPtr();
		}

		void RecursiveFunctionSymbolTable::resolveRecursiveCalls(ConstFormulaPtr f)
		{
			for(FormulaFunctionExpressionPtr& fn : expr_) {
				fn->set_formula(f);
			}
		}

	namespace {

		typedef std::map<std::string, FunctionCreator*> functions_map;

		// Takes ownership of the pointers, deleting them at program termination to
		// suppress valgrind false positives
		struct functions_map_manager {
			functions_map map_;
			~functions_map_manager() {
				for (auto & v : map_) {
					delete(v.second);
				}
			}
		};

		functions_map& get_functions_map() {

			static functions_map_manager map_man;
			functions_map & functions_table = map_man.map_;

			if(functions_table.empty()) {
		#define FUNCTION(name) functions_table[#name] = new SpecificFunctionCreator<name##_function>(FunctionModule);
				FUNCTION(map);
		#undef FUNCTION
			}

			return functions_table;
		}

	}

	ExpressionPtr createFunction(const std::string& fn,
								   const std::vector<ExpressionPtr>& args,
								   const FunctionSymbolTable* symbols,
								   ConstFormulaCallableDefinitionPtr callable_def)
	{
		if(fn == "set") {
			return ExpressionPtr(new set_function(args, callable_def));
		} else if(fn == "add") {
			return ExpressionPtr(new add_function(args, callable_def));
		}

		if(symbols) {
			ExpressionPtr res(symbols->createFunction(fn, args, callable_def));
			if(res) {
				return res;
			}
		}

		const std::map<std::string, FunctionCreator*>& creators = get_function_creators(FunctionModule);
		std::map<std::string, FunctionCreator*>::const_iterator creator_itor = creators.find(fn);
		if(creator_itor != creators.end()) {
			return ExpressionPtr(creator_itor->second->create(args));
		}

		functions_map::const_iterator i = get_functions_map().find(fn);
		if(i == get_functions_map().end()) {
			return ExpressionPtr();
		}

		return ExpressionPtr(i->second->create(args));
	}

	std::vector<std::string> builtin_function_names()
	{
		std::vector<std::string> res;
		const functions_map& m = get_functions_map();
		for(functions_map::const_iterator i = m.begin(); i != m.end(); ++i) {
			res.push_back(i->first);
		}

		return res;
	}

	FunctionExpression::FunctionExpression(
						const std::string& name,
						const args_list& args,
						int min_args, int max_args)
		: FormulaExpression("fn_expr"), name_(name), args_(args), min_args_(min_args), max_args_(max_args)
	{
		setName(name_.c_str());
	}

	void FunctionExpression::setDebugInfo(const variant& parent_formula,
									std::string::const_iterator begin_str,
									std::string::const_iterator end_str)
	{
		FormulaExpression::setDebugInfo(parent_formula, begin_str, end_str);

		if((min_args_ >= 0 && args_.size() < static_cast<size_t>(min_args_)) ||
		   (max_args_ >= 0 && args_.size() > static_cast<size_t>(max_args_))) {
			ASSERT_LOG(false, "ERROR: incorrect number of arguments to function '" << name_ << "': expected between " << min_args_ << " and " << max_args_ << ", found " << args_.size() << "\n" << debugPinpointLocation());
		}
	}

	bool FunctionExpression::canCreateVM() const
	{
		int arg_index = 0;
		for(const ExpressionPtr& a : args()) {
			if(optimizeArgNumToVM(arg_index) && a->canCreateVM() == false) {
				return false;
			}
			++arg_index;
		}

		return true;
	}

	ExpressionPtr FunctionExpression::optimizeToVM()
	{
		bool can_vm = true;
		int arg_index = 0;
		bool can_use_singleton_version = useSingletonVM();
		for(ExpressionPtr& a : args_mutable()) {
			if(optimizeArgNumToVM(arg_index)) {
				optimizeChildToVM(a);
				if(a->canCreateVM() == false) {
					can_vm = false;
				}
			} else {
				can_use_singleton_version = false;
			}

			++arg_index;
		}

		if(can_vm) {
			formula_vm::VirtualMachine vm;
			FunctionExpression* fn = this;
			
			if(can_use_singleton_version) {
				const int index = get_builtin_ffl_function_index(module(), name());
				fn = get_builtin_ffl_function_from_index(index);
				ASSERT_LOG(fn != nullptr, "Could not find function: " << module() << "::" << name());
			}

			vm.addLoadConstantInstruction(variant(fn));
			int arg_index = 0;
			for(ExpressionPtr& a : args_mutable()) {
				if(optimizeArgNumToVM(arg_index)) {
					optimizeChildToVM(a);
					a->emitVM(vm);
				} else {
					vm.addInstruction(OP_PUSH_NULL);
				}

				++arg_index;
			}

			if(dynamicArguments()) {
				vm.addInstruction(OP_CALL_BUILTIN_DYNAMIC);
			} else {
				vm.addInstruction(OP_CALL_BUILTIN);
			}
			vm.addInt(static_cast<int>(args_.size()));

			return createVMExpression(vm, queryVariantType(), *this);
		} else {
			return ExpressionPtr();
		}
	}

	namespace 
	{
		bool point_in_triangle(point p, point t[3]) 
		{
			point v0(t[2].x - t[0].x, t[2].y - t[0].y);
			point v1(t[1].x - t[0].x, t[1].y - t[0].y);
			point v2(p.x - t[0].x, p.y - t[0].y);

			int dot00 = t[0].x * t[0].x + t[0].y * t[0].y;
			int dot01 = t[0].x * t[1].x + t[0].y * t[1].y;
			int dot02 = t[0].x * t[2].x + t[0].y * t[2].y;
			int dot11 = t[1].x * t[1].x + t[1].y * t[1].y;
			int dot12 = t[1].x * t[2].x + t[1].y * t[2].y;
			float invDenom = 1 / float(dot00 * dot11 - dot01 * dot01);
			float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
			float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
			return u >= 0.0f && v >= 0.0f && (u+v) < 1.0f;
		}
	}

	FUNCTION_DEF(sha1, 1, 1, "sha1(string) -> string: Returns the sha1 hash of the given string")
		variant v = EVAL_ARG(0);
		const std::string& s = v.as_string();
		boost::uuids::detail::sha1 hash;
		hash.process_bytes(s.c_str(), s.length());
		unsigned int digest[5];
		hash.get_digest(digest);
		std::stringstream str;
		for(int n = 0; n < 5; ++n) {
			str << std::hex << std::setw(8) << std::setfill('0') << digest[n];
		}
		return variant(str.str());
	END_FUNCTION_DEF(sha1)

	FUNCTION_DEF(get_module_args, 0, 0, "get_module_args() -> callable: Returns the current module callable environment")
		Formula::failIfStaticContext();
		return variant(module::get_module_args().get());
	FUNCTION_TYPE_DEF
		return variant_type::get_type(variant::VARIANT_TYPE_CALLABLE);
	END_FUNCTION_DEF(get_module_args)

	FUNCTION_DEF(seed_rng, 0, 0, "seed_rng() -> none: Seeds the peudo-RNG used.")
		Formula::failIfStaticContext();
		::srand(static_cast<unsigned>(::time(nullptr)));
		return variant();
	FUNCTION_TYPE_DEF
		return variant_type::get_type(variant::VARIANT_TYPE_NULL);
	END_FUNCTION_DEF(seed_rng)

	FUNCTION_DEF(deep_copy, 1, 1, "deep_copy(any) ->any")
		return deep_copy_variant(EVAL_ARG(0));
	FUNCTION_ARGS_DEF
		ARG_TYPE("any");
	FUNCTION_TYPE_DEF
		return args()[0]->queryVariantType();
	END_FUNCTION_DEF(deep_copy)

	FUNCTION_DEF(lower, 1, 1, "lower(s) -> string: lowercase version of string")
		std::string s = EVAL_ARG(0).as_string();
		boost::algorithm::to_lower(s);
		return variant(s);
	FUNCTION_ARGS_DEF
		ARG_TYPE("string");
		RETURN_TYPE("string");
	END_FUNCTION_DEF(lower)

	FUNCTION_DEF(upper, 1, 1, "upper(s) -> string: lowercase version of string")
		std::string s = EVAL_ARG(0).as_string();
		boost::algorithm::to_upper(s);
		return variant(s);
	FUNCTION_ARGS_DEF
		ARG_TYPE("string");
		RETURN_TYPE("string");
	END_FUNCTION_DEF(upper)

	FUNCTION_DEF(rects_intersect, 2, 2, "rects_intersect([int], [int]) ->bool")
		rect a(EVAL_ARG(0));
		rect b(EVAL_ARG(1));

		return variant::from_bool(rects_intersect(a, b));
	FUNCTION_TYPE_DEF
		return variant_type::get_type(variant::VARIANT_TYPE_BOOL);
	END_FUNCTION_DEF(rects_intersect)

	namespace {
	void run_expression_for_edit_and_continue(ExpressionPtr expr, const game_logic::FormulaCallable* variables, bool* success)
	{
		*success = false;
		expr->evaluate(*variables);
		*success = true;
	}
	}

	FUNCTION_DEF(edit_and_continue, 2, 2, "edit_and_continue(expr, filename)")
		if(!preferences::edit_and_continue()) {
			return EVAL_ARG(0);
		}

		const std::string filename = EVAL_ARG(1).as_string();

		try {
			assert_recover_scope scope;
			return EVAL_ARG(0);
		} catch (validation_failure_exception& e) {
			bool success = false;
			std::function<void()> fn(std::bind(run_expression_for_edit_and_continue, args()[0], &variables, &success));

			edit_and_continue_fn(filename, e.msg, fn);
			if(success == false) {
				_exit(0);
			}

			return EVAL_ARG(0);
		}
	END_FUNCTION_DEF(edit_and_continue)

	class console_output_to_screen_command : public game_logic::CommandCallable
	{
		bool value_;
	public:
		explicit console_output_to_screen_command(bool value) : value_(value)
		{}

		virtual void execute(game_logic::FormulaCallable& ob) const override
		{
			debug_console::enable_screen_output(value_);
		}
	};

	FUNCTION_DEF(console_output_to_screen, 1, 1, "console_output_to_screen(bool) -> none: Turns the console output to the screen on and off")
		Formula::failIfStaticContext();
		return variant(new console_output_to_screen_command(EVAL_ARG(0).as_bool()));
	END_FUNCTION_DEF(console_output_to_screen)

	FUNCTION_DEF(user_preferences_path, 0, 0, "user_preferences_path() -> string: Returns the users preferences path")
		return variant(preferences::user_data_path());
	END_FUNCTION_DEF(user_preferences_path)

	class set_user_details_command : public game_logic::CommandCallable
	{
		std::string username_;
		std::string password_;
	public:
		explicit set_user_details_command(const std::string& username, const std::string& password) 
			: username_(username), password_(password)
		{}
		virtual void execute(game_logic::FormulaCallable& ob) const override
		{
			preferences::set_username(username_);
			if(password_.empty() == false) {
				preferences::set_password(password_);
			}
		}
	};

	FUNCTION_DEF(set_user_details, 1, 2, "set_user_details(string username, (opt) string password) -> none: Sets the username and password in the preferences.")
		Formula::failIfStaticContext();
		return variant(new set_user_details_command(EVAL_ARG(0).as_string(),
			NUM_ARGS > 1 ? EVAL_ARG(1).as_string() : ""));
	END_FUNCTION_DEF(set_user_details)

	FUNCTION_DEF(clamp, 3, 3, "clamp(numeric value, numeric min_val, numeric max_val) -> numeric: Clamps the given value inside the given bounds.")
		variant v  = EVAL_ARG(0);
		variant mn = EVAL_ARG(1);
		variant mx = EVAL_ARG(2);
		if(v.is_decimal() || mn.is_decimal() || mx.is_decimal()) {
			return variant(std::min(mx.as_decimal(), std::max(mn.as_decimal(), v.as_decimal())));
		}
		return variant(std::min(mx.as_int(), std::max(mn.as_int(), v.as_int())));
	FUNCTION_ARGS_DEF
		ARG_TYPE("decimal|int")
		ARG_TYPE("decimal|int")
		ARG_TYPE("decimal|int")
		DEFINE_RETURN_TYPE
		std::vector<variant_type_ptr> result_types;
		for(int n = 0; n != NUM_ARGS; ++n) {
			result_types.push_back(args()[n]->queryVariantType());
		}
		return variant_type::get_union(result_types);
	END_FUNCTION_DEF(clamp)

	class set_cookie_command : public game_logic::CommandCallable
	{
		variant cookie_;
	public:
		explicit set_cookie_command(const variant& cookie) 
			: cookie_(cookie)
		{}
		virtual void execute(game_logic::FormulaCallable& ob) const override
		{
			preferences::set_cookie(cookie_);
		}
	};

	FUNCTION_DEF(set_cookie, 1, 1, "set_cookie(data) -> none: Sets the preferences user_data")
		Formula::failIfStaticContext();
		return variant(new set_cookie_command(EVAL_ARG(0)));
	END_FUNCTION_DEF(set_cookie)

	FUNCTION_DEF(get_cookie, 0, 0, "get_cookie() -> none: Returns the preferences user_data")
		Formula::failIfStaticContext();
		return preferences::get_cookie();
	END_FUNCTION_DEF(get_cookie)

	FUNCTION_DEF(types_compatible, 2, 2, "types_compatible(string a, string b) ->bool: returns true if type 'b' is a subset of type 'a'")
		const variant a = EVAL_ARG(0);
		const variant b = EVAL_ARG(1);
		return variant::from_bool(variant_types_compatible(parse_variant_type(a), parse_variant_type(b)));
	END_FUNCTION_DEF(types_compatible)

	FUNCTION_DEF(typeof, 1, 1, "typeof(expression) -> string: yields the statically known type of the given expression")
		variant v = EVAL_ARG(0);
		return variant(get_variant_type_from_value(v)->to_string());
	END_FUNCTION_DEF(typeof)

	FUNCTION_DEF(static_typeof, 1, 1, "static_typeof(expression) -> string: yields the statically known type of the given expression")
		variant_type_ptr type = args()[0]->queryVariantType();
		ASSERT_LOG(type.get() != nullptr, "nullptr VALUE RETURNED FROM TYPE QUERY");
		return variant(type->base_type_no_enum()->to_string());
	END_FUNCTION_DEF(static_typeof)

	FUNCTION_DEF(all_textures, 0, 0, "all_textures()")
		auto s = KRE::Texture::getAllTextures();
		std::vector<KRE::Texture*> seen_textures;
		std::vector<variant> v;
		for(auto t : s) {
			bool already_seen = false;
			for(auto seen_tex : seen_textures) {
				if(*t == *seen_tex) {
					already_seen = true;
					break;
				}
			}

			if(already_seen) {
				continue;
			}

			seen_textures.push_back(t);
			v.push_back(variant(new TextureObject(t->shared_from_this())));
		}

		return variant(&v);

	FUNCTION_TYPE_DEF
		return variant_type::get_list(variant_type::get_type(variant::VARIANT_TYPE_CALLABLE));
	END_FUNCTION_DEF(all_textures)

	class gc_command : public game_logic::CommandCallable
	{
		int gens_;
		bool mandatory_;
	public:
		gc_command(int num_gens, bool mandatory) : gens_(num_gens), mandatory_(mandatory) {}
		virtual void execute(game_logic::FormulaCallable& ob) const override
		{
			//CustomObject::run_garbage_collection();
			int gens = gens_;
			bool mandatory = mandatory_;
			addAsynchronousWorkItem([=]() { runGarbageCollection(gens, mandatory); });
			addAsynchronousWorkItem([=]() { reapGarbageCollection(); });
		}
	};

	FUNCTION_DEF(trigger_garbage_collection, 0, 2, "trigger_garbage_collection(num_gens, mandatory): trigger an FFL garbage collection")
		const int num_gens = NUM_ARGS > 0 ? EVAL_ARG(0).as_int() : -1;
		const bool mandatory = NUM_ARGS > 1 ? EVAL_ARG(1).as_bool() : false;
		return variant(new gc_command(num_gens, mandatory));
	FUNCTION_ARGS_DEF
	ARG_TYPE("null|int")
	RETURN_TYPE("commands")
	END_FUNCTION_DEF(trigger_garbage_collection)

	class debug_gc_command : public game_logic::CommandCallable
	{
		std::string path_;
	public:
		explicit debug_gc_command(const std::string& path) : path_(path) {}
		virtual void execute(game_logic::FormulaCallable& ob) const override
		{
			//CustomObject::run_garbage_collection();
			runGarbageCollectionDebug(path_.c_str());
		}
	};

	FUNCTION_DEF(trigger_debug_garbage_collection, 1, 1, "trigger_debug_garbage_collection(): trigger an FFL garbage collection with additional memory usage information")
		std::string path = EVAL_ARG(0).as_string();
		return variant(new debug_gc_command(path));
	FUNCTION_ARGS_DEF
		ARG_TYPE("string");
	END_FUNCTION_DEF(trigger_debug_garbage_collection)

	FUNCTION_DEF(objects_known_to_gc, 0, 0, "objects_known_to_gc()")
		std::vector<GarbageCollectible*> all_obj;
		GarbageCollectible::getAll(&all_obj);
		std::vector<variant> result;
		for(auto p : all_obj) {
			FormulaCallable* obj = dynamic_cast<FormulaCallable*>(p);
			if(obj) {
				result.push_back(variant(obj));
			}
		}

		return variant(&result);
	FUNCTION_ARGS_DEF
	RETURN_TYPE("[object]")
	END_FUNCTION_DEF(objects_known_to_gc)

	class GarbageCollectorForceDestroyer : public GarbageCollector
	{
	public:
		void surrenderVariant(const variant* v, const char* description) override {
			*const_cast<variant*>(v) = variant();
		}

		void surrenderPtrInternal(ffl::IntrusivePtr<GarbageCollectible>* ptr, const char* description) override {
			ptr->reset();
		}
	private:
	};

	FUNCTION_DEF(force_destroy_object_references, 1, 1, "destroy_object_references(obj)")
		Formula::failIfStaticContext();

		auto p = EVAL_ARG(0).mutable_callable();
		return variant(new FnCommandCallable("force_destroy_object_references", [=]() {
			if(p) {
				GarbageCollectorForceDestroyer destroyer;
				p->surrenderReferences(&destroyer);
			}
		}));

	FUNCTION_ARGS_DEF
		ARG_TYPE("object")
		RETURN_TYPE("commands")
	END_FUNCTION_DEF(force_destroy_object_references)

	FUNCTION_DEF(debug_object_info, 1, 1, "debug_object_info(string) -> give info about the object at the given address")
		std::string obj = EVAL_ARG(0).as_string();
		const intptr_t addr_id = static_cast<intptr_t>(strtoll(obj.c_str(), nullptr, 16));
		void* ptr = reinterpret_cast<void*>(addr_id);
		GarbageCollectible* obj_ptr = GarbageCollectible::debugGetObject(ptr);
		if(obj_ptr == nullptr) {
			return variant("(Invalid object)");
		} else {
			return variant(obj_ptr->debugObjectSpew());
		}
	FUNCTION_ARGS_DEF
		ARG_TYPE("string");
	END_FUNCTION_DEF(debug_object_info)

	FUNCTION_DEF(build_animation, 1, 1, "build_animation(map)")
		variant m = EVAL_ARG(0);

		return variant(new Frame(m));

	FUNCTION_ARGS_DEF
		ARG_TYPE("map")
		RETURN_TYPE("builtin frame")
	END_FUNCTION_DEF(build_animation)


	FUNCTION_DEF(inspect_object, 1, 1, "inspect_object(object obj) -> map: outputs an object's properties")
		variant obj = EVAL_ARG(0);
		variant_type_ptr type = get_variant_type_from_value(obj);

		const game_logic::FormulaCallableDefinition* def = type->getDefinition();
		if(!def) {
			return variant();
		}

		std::map<variant, variant> m;

		const game_logic::FormulaCallable* callable = obj.as_callable();

		for(int slot = 0; slot < def->getNumSlots(); ++slot) {

			variant value;
			try {
				const assert_recover_scope scope;
				if(def->supportsSlotLookups()) {
					value = callable->queryValueBySlot(slot);
				} else {
					value = callable->queryValue(def->getEntry(slot)->id);
				}
			} catch(...) {
				continue;
			}

			m[variant(def->getEntry(slot)->id)] = value;
		}

		return variant(&m);

	FUNCTION_TYPE_DEF
		return variant_type::get_map(variant_type::get_type(variant::VARIANT_TYPE_STRING), variant_type::get_any());
	END_FUNCTION_DEF(inspect_object)

	namespace {
		int g_in_simulation = 0;
		struct SimulationScope {
			SimulationScope() { g_in_simulation++; }
			~SimulationScope() { g_in_simulation--; }
		};
	}

	FUNCTION_DEF(is_simulation, 0, 0, "is_simulation(): returns true iff we are in a 'simulation' such as get_modified_objcts() or eval_with_temp_modifications()")
		return variant::from_bool(g_in_simulation != 0);

	FUNCTION_ARGS_DEF
	RETURN_TYPE("bool")
	END_FUNCTION_DEF(is_simulation)

	FUNCTION_DEF(get_modified_object, 2, 2, "get_modified_object(obj, commands) -> obj: yields a copy of the given object modified by the given commands")

		formula_profiler::Instrument instrument("get_modified_object");
		SimulationScope sim;

		ffl::IntrusivePtr<FormulaObject> obj(EVAL_ARG(0).convert_to<FormulaObject>());

		{
		formula_profiler::Instrument instrument2("deep_clone");
		obj = FormulaObject::deepClone(variant(obj.get())).convert_to<FormulaObject>();
		}

		variant commands_fn = EVAL_ARG(1);

		std::vector<variant> args;
		args.push_back(variant(obj.get()));
		variant commands = commands_fn(args);

		obj->executeCommand(commands);

		return variant(obj.get());

	FUNCTION_TYPE_DEF
		return args()[0]->queryVariantType();
	END_FUNCTION_DEF(get_modified_object)

	FUNCTION_DEF(eval_with_temp_modifications, 4, 4, "")

		SimulationScope sim;
		
		FormulaCallablePtr callable(EVAL_ARG(0).mutable_callable());
		ASSERT_LOG(callable.get(), "Callable invalid");
		variant do_cmd = EVAL_ARG(2);
		variant undo_cmd = EVAL_ARG(3);

		callable->executeCommand(do_cmd);
		variant result = EVAL_ARG(1);
		callable->executeCommand(undo_cmd);

		return result;

	FUNCTION_ARGS_DEF
	ARG_TYPE("object")
	ARG_TYPE("any")
	ARG_TYPE("commands")
	ARG_TYPE("commands")
	FUNCTION_TYPE_DEF
		return args()[1]->queryVariantType();

	END_FUNCTION_DEF(eval_with_temp_modifications)

	FUNCTION_DEF(release_object, 1, 1, "release_object(obj)")
		Formula::failIfStaticContext();
		variant v = EVAL_ARG(0);
		return variant(new game_logic::FnCommandCallable("release_object", [=]() { FormulaObject::deepDestroy(v); }));
	FUNCTION_ARGS_DEF
	ARG_TYPE("any")
	RETURN_TYPE("commands")
	END_FUNCTION_DEF(release_object)

	FUNCTION_DEF(DrawPrimitive, 1, 1, "DrawPrimitive(map): create and return a DrawPrimitive")
		variant v = EVAL_ARG(0);
		return variant(graphics::DrawPrimitive::create(v).get());
	FUNCTION_ARGS_DEF
	ARG_TYPE("map")
	RETURN_TYPE("builtin DrawPrimitive")
	END_FUNCTION_DEF(DrawPrimitive)

	FUNCTION_DEF(auto_update_status, 0, 0, "auto_update_info(): get info on auto update status")
		if(sys::file_exists("./auto-update-status.json")) {
			try {
				return json::parse(sys::read_file("./auto-update-status.json"));
			} catch(...) {
				LOG_ERROR("Could not read auto-update-status.json");
			}
		}

		return g_auto_update_info;
	FUNCTION_ARGS_DEF
	RETURN_TYPE("map")
	END_FUNCTION_DEF(auto_update_status)
}

FUNCTION_DEF(rotate_rect, 4, 4, "rotate_rect(int|decimal center_x, int|decimal center_y, decimal rotation, int|decimal[8] rect) -> int|decimal[8]: rotates rect and returns the result")
	variant center_x = EVAL_ARG(0);
	variant center_y = EVAL_ARG(1);
	float rotate = EVAL_ARG(2).as_float();
	variant v = EVAL_ARG(3);

	ASSERT_LE(v.num_elements(), 8);

	std::vector<variant> res;
	if(center_x.is_decimal() || center_y.is_decimal()) {
		float r[8];
		for(unsigned n = 0; n != v.num_elements(); ++n) {
			r[n] = v[n].as_float();
		}

		for(unsigned n = v.num_elements(); n < 8; ++n) {
			r[n] = 0;
		}

		rotate_rect(center_x.as_float(), center_y.as_float(), rotate, r);

		std::vector<variant> res;
		res.reserve(8);
		for(int n = 0; n != v.num_elements(); ++n) {
			res.push_back(variant(r[n]));
		}
	} else {
		short r[8];
		for(int n = 0; n != v.num_elements(); ++n) {
			r[n] = v[n].as_int();
		}

		for(int n = v.num_elements(); n < 8; ++n) {
			r[n] = 0;
		}

		rotate_rect(center_x.as_int(), center_y.as_int(), rotate, r);

		res.reserve(8);
		for(int n = 0; n != v.num_elements(); ++n) {
			res.push_back(variant(r[n]));
		}
	}
	return variant(&res);

FUNCTION_ARGS_DEF
	ARG_TYPE("int|decimal")
	ARG_TYPE("int|decimal")
	ARG_TYPE("decimal")
	ARG_TYPE("[int]")
//RETURN_TYPE("[int]")
FUNCTION_TYPE_DEF
	if(args()[1]->queryVariantType() == variant_type::get_type(variant::TYPE::VARIANT_TYPE_DECIMAL)) {
		return variant_type::get_list(args()[1]->queryVariantType());
	}
	return variant_type::get_list(args()[0]->queryVariantType());
END_FUNCTION_DEF(rotate_rect)

namespace {
float curve_unit_interval(float p0, float p1, float m0, float m1, float t)
{
    return (2.0*t*t*t - 3.0*t*t + 1.0)*p0 + (t*t*t - 2.0*t*t + t)*m0 + (-2.0*t*t*t + 3.0*t*t)*p1 + (t*t*t - t*t)*m1;
}

}

FUNCTION_DEF(points_along_curve, 1, 2, "points_along_curve([[decimal,decimal]], int) -> [[decimal,decimal]]")
	std::vector<variant> v = EVAL_ARG(0).as_list();
	std::vector<float> points;
	std::vector<float> tangents;
	points.reserve(v.size()*2);
	for(const variant& p : v) {
		points.push_back(p[0].as_float());
		points.push_back(p[1].as_float());

		if(p.num_elements() > 2) {
			tangents.resize(points.size()/2);
			tangents.back() = p[2].as_float();
		}
	}


	std::vector<variant> result;
	if(points.size() < 4) {
		return variant(&result);
	}

	float min_point = points[0];
	float max_point = points[points.size()-2];

	int nout = 100;
	if(NUM_ARGS > 1) {
		nout = EVAL_ARG(1).as_int(nout);
	}

	result.reserve(nout);
	
	float* p = &points[0];

    for(int n = 0; n != nout; ++n) {
        float x = min_point + (float(n)/float(nout-1)) * (max_point-min_point);
        while(x > p[2]) {
            p += 2;
        }

		float x_dist = p[2] - p[0];
        float t = (x - p[0])/x_dist;

        float m0 = 0.0;
        float m1 = 0.0;

		const int tangent_index = (p - &points[0])/2;

		if(tangent_index < tangents.size()) {
			m0 = tangents[tangent_index];
        }

		if(tangent_index+1 < tangents.size()) {
			m1 = tangents[tangent_index+1];
        }

        float y = curve_unit_interval(p[1], p[3], m0*x_dist, m1*x_dist, t);
        result.push_back(variant(y));
    }

    return variant(&result);
	
	
FUNCTION_ARGS_DEF
	ARG_TYPE("[[decimal,decimal]|[decimal,decimal,decimal]|[decimal,decimal,decimal,decimal]]")
	ARG_TYPE("int|null")
RETURN_TYPE("[decimal]")
END_FUNCTION_DEF(points_along_curve)

FUNCTION_DEF(solid, 3, 6, "solid(level, int x, int y, (optional)int w=1, (optional) int h=1, (optional) bool debug=false) -> boolean: returns true iff the level contains solid space within the given (x,y,w,h) rectangle. If 'debug' is set, then the tested area will be displayed on-screen.")
	Level* lvl = EVAL_ARG(0).convert_to<Level>();
	const int x = EVAL_ARG(1).as_int();
	const int y = EVAL_ARG(2).as_int();

	int w = NUM_ARGS >= 4 ? EVAL_ARG(3).as_int() : 1;
	int h = NUM_ARGS >= 5 ? EVAL_ARG(4).as_int() : 1;

	rect r(x, y, w, h);

	if(NUM_ARGS >= 6) {
		//debugging so set the debug rect
		add_debug_rect(r);
	}

	return variant(lvl->solid(r));
FUNCTION_ARGS_DEF
	ARG_TYPE("object")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("bool")
RETURN_TYPE("bool")
END_FUNCTION_DEF(solid)

FUNCTION_DEF(solid_grid, 5, 9, "solid_grid(level, int x, int y, int w, int h, int stride_x=1, int stride_y=1, int stride_w=1, int stride_h=1)")
	Level* lvl = EVAL_ARG(0).convert_to<Level>();
	const int x = EVAL_ARG(1).as_int();
	const int y = EVAL_ARG(2).as_int();
	int w = EVAL_ARG(3).as_int();
	int h = EVAL_ARG(4).as_int();

	int stride_x = NUM_ARGS > 5 ? EVAL_ARG(5).as_int() : 1;
	int stride_y = NUM_ARGS > 6 ? EVAL_ARG(6).as_int() : 1;

	std::vector<variant> res;
	res.reserve(w*h);

	for(int xpos = 0; xpos < w; ++xpos) {
		for(int ypos = 0; ypos < h; ++ypos) {
			//rect r(x + xpos*stride_x, y + ypos*stride_y, stride_w, stride_h);
			res.emplace_back(variant::from_bool(lvl->solid(x + xpos*stride_x, y + ypos*stride_y)));
		}
	}

	return variant(&res);

FUNCTION_ARGS_DEF
	ARG_TYPE("object")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("int")
	ARG_TYPE("int")
RETURN_TYPE("[bool]")
END_FUNCTION_DEF(solid_grid)

/*
FUNCTION_DEF(hsv, 3, 4, "hsv(decimal h, decimal s, decimal v, decimal alpha) -> color_callable")
	float hue = EVAL_ARG(0).as_float() / 360.0f;
	float saturation = EVAL_ARG(1).as_float();
	float value = EVAL_ARG(2).as_float();
	float alpha = NUM_ARGS > 3 ? EVAL_ARG(3).as_float() : 1.0f;
	
	auto color = KRE::Color::from_hsv(hue, saturation, value);

	return variant(new graphics::ColorCallable(color));
FUNCTION_ARGS_DEF
	ARG_TYPE("decimal")
	ARG_TYPE("decimal")
	ARG_TYPE("decimal")
	ARG_TYPE("decimal")
	RETURN_TYPE("builtin color_callable")
END_FUNCTION_DEF
*/


FUNCTION_DEF(format, 1, 2, "format(string, [int|decimal]): Put the numbers in the list into the string. The fractional component of the number will be rounded to the nearest available digit. Example: format('#{01}/#{02}/#{2004}', [20, 5, 2015]) → '20/05/2015'; format('#{02}/#{02}/#{02}', [20, 5, 2015]) → '20/5/2015'; format(#{0.20}, [0.1]) → '0.10'; format(#{0.02}, [0.1]) → '0.1'.")
	std::string input_str = EVAL_ARG(0).as_string();
	//If we can't have the magic string formatting, return early.
	if(input_str.size() < 2) { return EVAL_ARG(0); }
	
	std::vector<variant> values = EVAL_ARG(1).as_list();
	std::string output_str(""); output_str.reserve(input_str.size());
	std::string format_fragment("");
	std::string format_str("");
	
	//Step through the string until we find the magic string - eg, the "#{" of "#{12.34}".
	int char_at = 0;
	int value_at = 0;
	while (char_at < input_str.size()) {
		if(input_str[char_at] == '#' && input_str[char_at+1] == '{') {
			format_fragment.clear();
			format_str.clear();
			char_at+=2;
			
			while (char_at < input_str.size() && input_str[char_at] != '}') {
				format_fragment += input_str[char_at];
				char_at++;
			}
			char_at++;
			
			int decimal_place = format_fragment.find('.');
			
			std::stringstream ss1; 
			if(decimal_place == -1) {
				ss1 << round(values[value_at].as_float());
			} else {
				ss1 << floor(values[value_at].as_float());
			}
			format_str += ss1.str();
			
			int width = decimal_place >= 0 ? decimal_place : format_fragment.length();
			ASSERT_LOG(width <= 100, "Number width probably shouldn't be greater than 100. (In Anura, numbers only get about 20 digits wide.) #{" << format_fragment << "} in " << input_str);
			ASSERT_LOG(width > 0, "Number width must be greater than 0. #{" << format_fragment << "} in " << input_str);
				
			
			if(format_str.length() < width) {
				format_str.insert(0, width - format_str.length(), '0');
			}
			
			output_str += format_str;
			
			//Process the decimal component, if needed.
			if(decimal_place >= 0) {
				format_str.clear();
				
				int width = format_fragment.length() - decimal_place - 1;
				ASSERT_LOG(width <= 100, "Number decimal width probably shouldn't be greater than 100. (In Anura, numbers only get about 20 digits wide.) #{" << format_fragment << "} in " << input_str);
				ASSERT_LOG(width > 0, "Number decimal width must be greater than 0. #{" << format_fragment << "} in " << input_str);
				
				//Round the decimal component, if it needs to be truncated. This is actually somewhat inaccurate due to floating-point approximations, so tends to go either way.
				std::stringstream ss2;
				ss2 << ( round(values[value_at].as_float() * pow(10,width)) / pow(10,width) 
					- floor(values[value_at].as_float()) );
				if(ss2.str().find('.') == 1) {
					format_str += ss2.str().substr(2,20); //Remove the leading 0. from the representation.
				} else {
					format_str += '0';
				}
				
				//Pad decimal with zeros, iff the format string has a trailing 0.
				if(format_fragment[format_fragment.length()-1] == '0') {
					while (format_str.length() < width) {
						format_str += '0';
					}
				}
				
				output_str += '.';
				output_str += format_str;
			}
			
			value_at++;
		} else {
			output_str += input_str[char_at];
			char_at++;
		}
	}
	
	return variant(output_str);
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
	ARG_TYPE("[decimal]");
RETURN_TYPE("string")
END_FUNCTION_DEF(format)

FUNCTION_DEF(sprintf, 1, -1, "sprintf(string, ...): Format the string using standard printf formatting.")
	std::string input_str = EVAL_ARG(0).as_string();

	try {
		boost::format f(input_str);

		for(int i = 1; i < static_cast<int>(NUM_ARGS); ++i) {
			variant v = EVAL_ARG(i);
			if(v.is_decimal()) {
				f % v.as_decimal().as_float();
			} else if(v.is_int()) {
				f % v.as_int();
			} else if(v.is_string()) {
				f % v.as_string();
			} else {
				f % v.write_json();
			}
		}

		std::string res = formatter() << f;
		return variant(res);
	} catch(boost::exception& e) {
		ASSERT_LOG(false, "Error when formatting string: " << boost::diagnostic_information(e) << "\n" << debugPinpointLocation());
	}
FUNCTION_ARGS_DEF
	ARG_TYPE("string");
RETURN_TYPE("string")
END_FUNCTION_DEF(sprintf)


UNIT_TEST(modulo_operation) {
	CHECK(game_logic::Formula(variant("mod(-5, 20)")).execute() == game_logic::Formula(variant("15")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("mod(-25, 20)")).execute() == game_logic::Formula(variant("15")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("mod(15, 20)")).execute() == game_logic::Formula(variant("15")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("mod(35, 20)")).execute() == game_logic::Formula(variant("15")).execute(), "test failed");
}

UNIT_TEST(flatten_function) {
	CHECK(game_logic::Formula(variant("flatten([1,[2,3]])")).execute() == game_logic::Formula(variant("[1,2,3]")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("flatten([1,2,3,[[4,5],6]])")).execute() == game_logic::Formula(variant("[1,2,3,4,5,6]")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("flatten([[1,2,3,4],5,6])")).execute() == game_logic::Formula(variant("[1,2,3,4,5,6]")).execute(), "test failed");
	CHECK(game_logic::Formula(variant("flatten([[[0,2,4],6,8],10,[12,14]])")).execute() == game_logic::Formula(variant("[0,2,4,6,8,10,12,14]")).execute(), "test failed");
}

UNIT_TEST(sqrt_function) {
	CHECK_EQ(game_logic::Formula(variant("sqrt(2147483)")).execute().as_int(), 1465);	

	for(uint64_t n = 0; n < 100000; n += 1000) {
		CHECK_EQ(game_logic::Formula(variant(formatter() << "sqrt(" << n << ".0^2)")).execute().as_decimal(), decimal::from_int(static_cast<int>(n)));
	}
}

UNIT_TEST(map_function) {
	CHECK_EQ(game_logic::Formula(variant("map([2,3,4], value*value)")).execute(), game_logic::Formula(variant("[4,9,16]")).execute());
	CHECK_EQ(game_logic::Formula(variant("map([2,3,4], value+index)")).execute(), game_logic::Formula(variant("[2,4,6]")).execute());
	CHECK_EQ(game_logic::Formula(variant("map([1,2,3,4], range(value))")).execute(), game_logic::Formula(variant("[[0], [0,1], [0,1,2],[0,1,2,3]]")).execute());

	CHECK_EQ(game_logic::Formula(variant("map(flatten(map([1,2,3,4], range(value))), value * value)")).execute(), game_logic::Formula(variant("[0,0,1,0,1,4,0,1,4,9]")).execute());

}

UNIT_TEST(filter_function) {
	CHECK_EQ(game_logic::Formula(variant("filter([2,3,4], value%2 = 0)")).execute(), game_logic::Formula(variant("[2,4]")).execute());
	CHECK_EQ(game_logic::Formula(variant("filter({'a': 2, 'b': 3, 'c': 4}, value%2 = 0)")).execute(), game_logic::Formula(variant("{'a': 2, 'c': 4}")).execute());
	CHECK_EQ(game_logic::Formula(variant("filter({'a': 2, 'b': 3, 'c': 4}, key='a' or key='c')")).execute(), game_logic::Formula(variant("{'a': 2, 'c': 4}")).execute());
}

UNIT_TEST(where_scope_function) {
	CHECK(game_logic::Formula(variant("{'val': num} where num = 5")).execute() == game_logic::Formula(variant("{'val': 5}")).execute(), "map where test failed");
	CHECK(game_logic::Formula(variant("'five: ${five}' where five = 5")).execute() == game_logic::Formula(variant("'five: 5'")).execute(), "string where test failed");
}

UNIT_TEST(binary_search_function) {
	CHECK(game_logic::Formula(variant("binary_search([3,4,7,9,10,24,50], 9)")).execute() == variant::from_bool(true), "binary_search failed");
	CHECK(game_logic::Formula(variant("binary_search([3,4,7,9,10,24,50], 3)")).execute() == variant::from_bool(true), "binary_search failed");
	CHECK(game_logic::Formula(variant("binary_search([3,4,7,9,10,24,50], 50)")).execute() == variant::from_bool(true), "binary_search failed");
	CHECK(game_logic::Formula(variant("binary_search([3,4,7,9,10,24,50], 5)")).execute() == variant::from_bool(false), "binary_search failed");
	CHECK(game_logic::Formula(variant("binary_search([3,4,7,9,10,24,50], 51)")).execute() == variant::from_bool(false), "binary_search failed");
}

UNIT_TEST(format) {
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{70}.', [7])")).execute(), game_logic::Formula(variant("'Hello, 07.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{700}.', [7])")).execute(), game_logic::Formula(variant("'Hello, 007.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{70}.', [700])")).execute(), game_logic::Formula(variant("'Hello, 700.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{700}.', [700])")).execute(), game_logic::Formula(variant("'Hello, 700.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{700}.', [7.4])")).execute(), game_logic::Formula(variant("'Hello, 007.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{700}.', [7.5])")).execute(), game_logic::Formula(variant("'Hello, 008.'")).execute());
	
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.0}.', [7])")).execute(), game_logic::Formula(variant("'Hello, 7.0.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.7}.', [7])")).execute(), game_logic::Formula(variant("'Hello, 7.0.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.7}.', [7.4])")).execute(), game_logic::Formula(variant("'Hello, 7.4.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.07}.', [7.4])")).execute(), game_logic::Formula(variant("'Hello, 7.4.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.07}.', [7.44])")).execute(), game_logic::Formula(variant("'Hello, 7.44.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.07}.', [7.46])")).execute(), game_logic::Formula(variant("'Hello, 7.46.'")).execute()); //7.45 rounds down, probably a floating-point imprecision thing.
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.07}.', [7.446])")).execute(), game_logic::Formula(variant("'Hello, 7.45.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.7}.', [7.44])")).execute(), game_logic::Formula(variant("'Hello, 7.4.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.7}.', [7.46])")).execute(), game_logic::Formula(variant("'Hello, 7.5.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{70.7}.', [7.4])")).execute(), game_logic::Formula(variant("'Hello, 07.4.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Hello, #{7.700}.', [7.46])")).execute(), game_logic::Formula(variant("'Hello, 7.460.'")).execute());
	
	CHECK_EQ(game_logic::Formula(variant("format('Check, #{07.07}, #{007}.', [1.23, 4.56])")).execute(), game_logic::Formula(variant("'Check, 01.23, 005.'")).execute());
	CHECK_EQ(game_logic::Formula(variant("format('Check, #{07.07}, #{${decimals}}.', [1.23, 4.56]) where decimals = '003'")).execute(), game_logic::Formula(variant("'Check, 01.23, 005.'")).execute());
}

BENCHMARK(map_function) {
	using namespace game_logic;

	static MapFormulaCallable* callable = nullptr;
	static variant callable_var;
	static variant main_callable_var;
	static std::vector<variant> v;
	
	if(callable == nullptr) {
		callable = new MapFormulaCallable;
		callable_var = variant(callable);
		callable->add("x", variant(0));
		for(int n = 0; n != 1000; ++n) {
			v.push_back(callable_var);
		}

		callable = new MapFormulaCallable;
		main_callable_var = variant(callable);
		callable->add("items", variant(&v));
	}

	static Formula f(variant("map(items, 'obj', 0)"));
	BENCHMARK_LOOP {
		f.execute(*callable);
	}
}

namespace game_logic 
{
	ConstFormulaCallableDefinitionPtr get_map_callable_definition(ConstFormulaCallableDefinitionPtr base_def, variant_type_ptr key_type, variant_type_ptr value_type, const std::string& value_name)
	{
		return ConstFormulaCallableDefinitionPtr(new MapCallableDefinition(base_def, key_type, value_type, value_name));
	}

	ConstFormulaCallableDefinitionPtr get_variant_comparator_definition(ConstFormulaCallableDefinitionPtr base_def, variant_type_ptr type)
	{
		return ConstFormulaCallableDefinitionPtr(new variant_comparator_definition(base_def, type));
	}

	class formula_FunctionSymbolTable : public FunctionSymbolTable
	{
	public:
		virtual ExpressionPtr createFunction(
								   const std::string& fn,
								   const std::vector<ExpressionPtr>& args,
								   ConstFormulaCallableDefinitionPtr callable_def) const override;
	};

	ExpressionPtr formula_FunctionSymbolTable::createFunction(
							   const std::string& fn,
							   const std::vector<ExpressionPtr>& args,
							   ConstFormulaCallableDefinitionPtr callable_def) const
	{
		const std::map<std::string, FunctionCreator*>& creators = get_function_creators(FunctionModule);
		std::map<std::string, FunctionCreator*>::const_iterator i = creators.find(fn);
		if(i != creators.end()) {
			return ExpressionPtr(i->second->create(args));
		}

		return FunctionSymbolTable::createFunction(fn, args, callable_def);
	}
}

FunctionSymbolTable& get_formula_functions_symbol_table()
{
	static formula_FunctionSymbolTable table;
	return table;
}

UNIT_TEST(split_variant_if_int) {
	const variant non_str_variant(32993);
	const variant split_returned = split_variant_if_str(non_str_variant);
	check::type_is_int(split_returned);
	CHECK_EQ(non_str_variant, split_returned);
}

UNIT_TEST(split_variant_if_str) {
	const variant str_variant("foo");
	LOG_DEBUG(str_variant);
	std::vector<std::string> expected_contents;
	expected_contents.emplace_back("f");
	expected_contents.emplace_back("o");
	expected_contents.emplace_back("o");
	const variant split_returned = split_variant_if_str(str_variant);
	LOG_DEBUG(split_returned);
	check::type_is_list(split_returned);
	const std::vector<variant> variant_list = split_returned.as_list();
	const uint_fast8_t variant_list_size = variant_list.size();
	CHECK_EQ(expected_contents.size(), variant_list_size);
	for (int i = 0; i < variant_list_size; i++) {
		const std::string expected = expected_contents[i];
		const variant actual = variant_list[i];
		check::type_is_string(actual);
		const std::string actual_as_string = actual.as_string();
		CHECK_EQ(expected, actual_as_string);
	}
}

UNIT_TEST(bind_command_return_type) {
	const std::string code = "bind_command(def () -> null null)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant execution_output(formula.execute());
	check::type_is_object(execution_output);
	const game_logic::FormulaCallable * execution_output_as_callable =
			execution_output.as_callable();
	std::string serialized;
	execution_output_as_callable->serialize(serialized);
	LOG_DEBUG(serialized);
	const std::string needle = "(UNSERIALIZABLE_OBJECT ";
	const auto occurrence = serialized.find(needle);
	LOG_DEBUG(occurrence);
	ASSERT_LOG(occurrence != std::string::npos,
			"unexpected serialization form");
// 	const variant slot_zero_value =
// 			execution_output_as_callable->queryValueBySlot(0);
// 	const variant::TYPE slot_zero_value_type = slot_zero_value.type();
// 	const std::string slot_zero_value_type_as_string =
// 			variant::variant_type_to_string(
// 					slot_zero_value_type);
// 	LOG_DEBUG(slot_zero_value_type_as_string);
	const auto needle_len = needle.length();
	const std::string label =
			serialized.substr(
					needle_len,
					serialized.length() - needle_len - 1);
	LOG_DEBUG(label);
	// Observed `N10game_logic12_GLOBAL__N_113bound_commandE` at macOS.
	if ("N10game_logic12_GLOBAL__N_113bound_commandE" != label) {
		LOG_INFO("unexpected return label '" + label + '\'');
		LOG_INFO("this is expected and not a problem");
	}
}

void xml_to_json_demands_quoted_attributes_inner_good(
		const std::string & code) {

	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_list(output);
	const std::vector<variant> output_as_list = output.as_list();
	const uint_fast8_t output_as_list_size = output_as_list.size();
	CHECK_EQ(3, output_as_list_size);
	for (int i = 0; i < output_as_list_size; i++) {
		const variant element = output_as_list[i];
		check::type_is_dictionary(element);
		const std::map<variant, variant> element_as_map =
				element.as_map();
		const uint_fast8_t element_as_map_size = element_as_map.size();
		CHECK_EQ(3, element_as_map_size);
		for (auto const & it : element_as_map) {
			const variant first = it.first;
			check::type_is_string(first);
			const std::string first_as_string = first.as_string();
			const variant second = it.second;
			if (first_as_string == "attr") {
				check::type_is_dictionary(second);
				const std::map<variant, variant> second_as_map =
						second.as_map();
				const uint_fast8_t second_as_map_size =
						second_as_map.size();
				LOG_DEBUG(second_as_map_size);
				if (i == 0) {
					CHECK_EQ(1, second_as_map_size);
					std::map<variant, variant>::const_iterator
					second_as_map_begin =
							second_as_map.begin();
					const variant second_as_map_first =
							second_as_map_begin->first;
					check::type_is_string(second_as_map_first);
					const std::string second_as_map_first_as_string =
							second_as_map_first.as_string();
					CHECK_EQ("b", second_as_map_first_as_string);
					const variant second_as_map_second =
							second_as_map_begin->second;
					check::type_is_string(second_as_map_second);
					const std::string second_as_map_second_as_string =
							second_as_map_second.as_string();
					CHECK_EQ("c", second_as_map_second_as_string);
				} else {
					ASSERT_LOG(i == 1 || i == 2,
							"unexpected list element/s");
					CHECK_EQ(0, second_as_map_size);
				}
			} else if (first_as_string == "data") {
				check::type_is_string(second);
				const std::string second_as_string =
						second.as_string();
				if (i == 0 || i == 2) {
					CHECK_EQ("a", second_as_string);
				} else {
					ASSERT_LOG(i == 1,
							"unexpected list element/s");
					CHECK_EQ("d", second_as_string);
				}
			} else {
				ASSERT_LOG(
						first_as_string == "type",
						"unexpected map key/s");
				check::type_is_enum(second);
				const std::string second_as_enum =
						second.as_enum();
				LOG_DEBUG(second_as_enum);
				if (i == 0) {
					CHECK_EQ("start_element",
							second_as_enum);
				} else if (i == 1) {
					CHECK_EQ("text", second_as_enum);
				} else {
					ASSERT_LOG(i == 2,
							"unexpected list element/s");
					CHECK_EQ("end_element",
							second_as_enum);
				}
			}
		}
	}
}

UNIT_TEST(xml_to_json_demands_quoted_attributes_0) {
	const std::string xml = "<a b=\"c\">d</a>";
	const std::string code = "parse_xml('" + xml + "')";
	xml_to_json_demands_quoted_attributes_inner_good(code);
}

UNIT_TEST(xml_to_json_demands_quoted_attributes_1) {
	const std::string xml = "<a b='c'>d</a>";
	const std::string code = "parse_xml(\"" + xml + "\")";
	xml_to_json_demands_quoted_attributes_inner_good(code);
}

UNIT_TEST(xml_to_json_demands_quoted_attributes_2) {
	const std::string xml = "<a b=c>d</a>";
	const std::string code = "parse_xml('" + xml + "')";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_string(output);
	const std::string output_as_string = output.as_string();
	const std::string expected = "Error parsing XML: <a b=c>d</a>";
	CHECK_EQ(expected, output_as_string);
}

UNIT_TEST(keys_of_map) {
	const std::string code = "keys({0: 'a', 'b': 32993, })";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_list(output);
	const std::vector<variant> output_as_list = output.as_list();
	const uint_fast8_t output_as_list_size = output_as_list.size();
	LOG_DEBUG(output_as_list_size);
	for (uint_fast8_t i = 0; i < output_as_list_size; i++) {
		const variant element = output_as_list[i];
		if (i == 0) {
			check::type_is_int(element);
			const int_fast32_t element_as_int = element.as_int();
			CHECK_EQ(0, element_as_int);
		} else {
			ASSERT_LOG(i == 1, "unexpected list element/s");
			check::type_is_string(element);
			const std::string element_as_string =
					element.as_string();
			CHECK_EQ("b", element_as_string);
		}
	}
}

UNIT_TEST(values_of_map) {
	const std::string code = "values({0: 'a', 'b': 32993, })";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_list(output);
	const std::vector<variant> output_as_list = output.as_list();
	const uint_fast8_t output_as_list_size = output_as_list.size();
	LOG_DEBUG(output_as_list_size);
	for (uint_fast8_t i = 0; i < output_as_list_size; i++) {
		const variant element = output_as_list[i];
		if (i == 0) {
			check::type_is_string(element);
			const std::string element_as_string =
					element.as_string();
			CHECK_EQ("a", element_as_string);
		} else {
			ASSERT_LOG(i == 1, "unexpected list element/s");
			check::type_is_int(element);
			const int_fast32_t element_as_int = element.as_int();
			CHECK_EQ(32993, element_as_int);
		}
	}
}

UNIT_TEST(wave_for_int_0) {
	const std::string code = "wave(0)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_EQ(0, output_as_int);
}

UNIT_TEST(wave_for_int_1) {
	const std::string code = "wave(1)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_GE(output_as_int, 5);
	CHECK_LE(output_as_int, 7);
}

UNIT_TEST(wave_for_int_2) {
	const std::string code = "wave(2)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_GE(output_as_int, 11);
	CHECK_LE(output_as_int, 13);
}

UNIT_TEST(wave_for_int_100) {
	const std::string code = "wave(100)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_GE(output_as_int, 586);
	CHECK_LE(output_as_int, 588);
}

UNIT_TEST(wave_for_int_500) {
	const std::string code = "wave(500)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_EQ(0, output_as_int);
}

UNIT_TEST(wave_for_int_750) {
	const std::string code = "wave(750)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_EQ(-1000, output_as_int);
}

UNIT_TEST(wave_for_int_800) {
	const std::string code = "wave(800)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_GE(output_as_int, -952);
	CHECK_LE(output_as_int, -950);
}

UNIT_TEST(wave_for_int_1000) {
	const std::string code = "wave(1000)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_EQ(0, output_as_int);
}

UNIT_TEST(wave_for_int_1500) {
	const std::string code = "wave(1500)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_EQ(0, output_as_int);
}

UNIT_TEST(decimal_for_parsable_string) {
	const std::string code = "decimal('32993')";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_decimal(output);
	const decimal output_as_decimal = output.as_decimal();
	CHECK_EQ(decimal::from_string("32993.0"), output_as_decimal);
}

// XXX    Code running normally will abort fatally, as it has to, when
// XXX  failing to parse a string to decimal. It would abort fatally also
// XXX  when running this test, that's why it is disabled.
UNIT_TEST(decimal_for_unparsable_string_FAILS) {
	const std::string code = "decimal('foo')";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			formula.execute();
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "expected an exception that did not happen");
}

UNIT_TEST(decimal_for_int) {
	const std::string code = "decimal(32993)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_decimal(output);
	const decimal output_as_decimal = output.as_decimal();
	CHECK_EQ(decimal::from_string("32993.0"), output_as_decimal);
}

UNIT_TEST(int_for_parsable_string) {
	const std::string code = "int('32993')";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_EQ(32993, output_as_int);
}

// XXX    Code running normally will abort fatally, as it has to, when
// XXX  failing to parse a string to decimal. It would abort fatally also
// XXX  when running this test, that's why it is disabled.
UNIT_TEST(int_for_unparsable_string_FAILS) {
	const std::string code = "int('foo')";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	bool excepted = false;
	{
		const assert_recover_scope unit_test_exception_expected;
		try {
			formula.execute();
		} catch (const validation_failure_exception vfe) {
			excepted = true;
		}
	}
	ASSERT_LOG(excepted, "expected an exception that did not happen");
}

UNIT_TEST(int_for_decimal) {
	const std::string code = "int(32993.0)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_int(output);
	const int_fast32_t output_as_int = output.as_int();
	CHECK_EQ(32993, output_as_int);
}

UNIT_TEST(bool_for_expected_string) {
	const std::string code = "bool('true')";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_bool(output);
	const bool output_as_bool = output.as_bool();
	CHECK_EQ(true, output_as_bool);
}

UNIT_TEST(bool_for_unexpected_string) {
	const std::string code = "bool('foo')";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_bool(output);
	const bool output_as_bool = output.as_bool();
	CHECK_EQ(true, output_as_bool);
}

UNIT_TEST(bool_for_number) {
	const std::string code = "bool(32993)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_bool(output);
	const bool output_as_bool = output.as_bool();
	CHECK_EQ(true, output_as_bool);
}


UNIT_TEST(bool_for_zero) {
	const std::string code = "bool(0)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_bool(output);
	const bool output_as_bool = output.as_bool();
	CHECK_EQ(false, output_as_bool);
}

UNIT_TEST(bool_for_nonempty_map) {
	const std::string code = "bool({1: 1, })";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_bool(output);
	const bool output_as_bool = output.as_bool();
	CHECK_EQ(true, output_as_bool);
}

UNIT_TEST(bool_for_empty_map) {
	const std::string code = "bool({})";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_bool(output);
	const bool output_as_bool = output.as_bool();
	CHECK_EQ(false, output_as_bool);
}

UNIT_TEST(sin_zero_rad) {
	const std::string code = "sin(0)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_decimal(output);
	const decimal output_as_decimal = output.as_decimal();
	CHECK_EQ(decimal::from_string("0.0"), output_as_decimal);
}

UNIT_TEST(sin_one_sixth_pi_rad) {
	const std::string code = "sin(30)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_decimal(output);
	const decimal output_as_decimal = output.as_decimal();
	CHECK_EQ(decimal::from_string("0.5"), output_as_decimal);
}

UNIT_TEST(sin_one_quarter_pi_rad) {
	const std::string code = "sin(45)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_decimal(output);
	const decimal output_as_decimal = output.as_decimal();
	CHECK_LE(decimal::from_string("0.707"), output_as_decimal);
	CHECK_GE(decimal::from_string("0.708"), output_as_decimal);
}

UNIT_TEST(sin_one_third_pi_rad) {
	const std::string code = "sin(60)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_decimal(output);
	const decimal output_as_decimal = output.as_decimal();
	CHECK_LE(decimal::from_string("0.86"), output_as_decimal);
	CHECK_GE(decimal::from_string("0.87"), output_as_decimal);
}

UNIT_TEST(sin__half_pi_rad) {
	const std::string code = "sin(90)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_decimal(output);
	const decimal output_as_decimal = output.as_decimal();
	CHECK_LE(decimal::from_string("0.999999"), output_as_decimal);
	CHECK_GE(decimal::from_string("1.000001"), output_as_decimal);
}

UNIT_TEST(sin_pi_rad) {
	const std::string code = "sin(180)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_decimal(output);
	const decimal output_as_decimal = output.as_decimal();
	CHECK_EQ(decimal::from_string("0.0"), output_as_decimal);
}

UNIT_TEST(range_two_args) {
	const std::string code = "range(4, 6)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_list(output);
	const std::vector<variant> output_as_list = output.as_list();
	const uint_fast8_t output_as_list_size = output_as_list.size();
	CHECK_EQ(2, output_as_list_size);
	for (uint_fast8_t i = 0; i < output_as_list_size; i++) {
		const variant element = output_as_list[i];
		check::type_is_int(element);
		const int_fast32_t element_as_int = element.as_int();
		if (i == 0) {
			CHECK_EQ(4, element_as_int);
		} else {
			ASSERT_LOG(i == 1, "unexpected list element/s");
			CHECK_EQ(5, element_as_int);
		}
	}
}

UNIT_TEST(reverse) {
	const std::string code = "reverse([2, 3, 1, ])";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_list(output);
	const std::vector<variant> output_as_list = output.as_list();
	const uint_fast8_t output_as_list_size = output_as_list.size();
	CHECK_EQ(3, output_as_list_size);
	for (uint_fast8_t i = 0; i < output_as_list_size; i++) {
		const variant element = output_as_list[i];
		check::type_is_int(element);
		const int_fast32_t element_as_int = element.as_int();
		if (i == 0) {
			CHECK_EQ(1, element_as_int);
		} else if (i == 1) {
			CHECK_EQ(3, element_as_int);
		} else {
			ASSERT_LOG(i == 2, "unexpected list element/s");
			CHECK_EQ(2, element_as_int);
		}
	}
}

UNIT_TEST(str_for_str) {
	const std::string code = "str('foo')";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_string(output);
	const std::string output_as_string = output.as_string();
	CHECK_EQ("foo", output_as_string);
}

UNIT_TEST(str_for_non_str) {
	const std::string code = "str(42)";
	const variant code_variant(code);
	const game_logic::Formula formula(code_variant);
	const variant output = formula.execute();
	check::type_is_string(output);
	const std::string output_as_string = output.as_string();
	CHECK_EQ("42", output_as_string);
}
