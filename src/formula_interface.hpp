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

#pragma once

#include <map>
#include <vector>

#include "intrusive_ptr.hpp"
#include <boost/scoped_ptr.hpp>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"
#include "variant_type.hpp"

namespace game_logic
{
	struct FormulaInterfaceImpl;

	class FormulaInterfaceInstanceFactory : public FormulaCallable
	{
	public:
		virtual ~FormulaInterfaceInstanceFactory();

		virtual bool all_static_lookups() const = 0;
		virtual variant create(const variant& v) const = 0;
		virtual int getId() const = 0;

		variant getValue(const std::string& key) const override { return variant(); }
	};

	class FormulaInterface : public FormulaCallable
	{
	public:
		struct interface_mismatch_error {
			explicit interface_mismatch_error(const std::string& msg) : msg(msg) {}
			std::string msg;
		};

		explicit FormulaInterface(const std::map<std::string, variant_type_ptr>& types);
		const std::map<std::string, variant_type_ptr>& get_types() const { return types_; }

		FormulaInterfaceInstanceFactory* createFactory(variant_type_ptr type) const; //throw interface_mismatch_error
		FormulaInterfaceInstanceFactory* getDynamicFactory() const;

		ConstFormulaCallableDefinitionPtr getDefinition() const;

		bool match(const variant& v) const;

		std::string to_string() const;

	private:
		variant getValue(const std::string& key) const override;

		std::map<std::string, variant_type_ptr> types_;
		std::unique_ptr<FormulaInterfaceImpl> impl_;
	};
}

typedef ffl::IntrusivePtr<game_logic::FormulaInterface> FormulaInterfacePtr;
typedef ffl::IntrusivePtr<const game_logic::FormulaInterface> ConstFormulaInterfacePtr;
