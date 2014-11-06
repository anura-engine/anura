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
     in a product, an acknowledgement in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

     3. This notice may not be removed or altered from any source
     distribution.
*/
#pragma once
#ifndef ARRAY_CALLABLE_HPP_INCLUDED
#define ARRAY_CALLABLE_HPP_INCLUDED

#include <vector>

#include "asserts.hpp"
#include "formula_callable.hpp"
#include "graphics.hpp"
#include "variant.hpp"

namespace game_logic {

	class float_array_callable : public formula_callable
	{
	public:
		explicit float_array_callable(std::vector<GLfloat>* f, int ne = 1)
			: ne_(ne)
		{
			f_.swap(*f);
		}
		virtual void set_value(const std::string& key, const variant& value) 
		{
			if(key == "floats" || key == "value") {
				ASSERT_LOG(value.is_list(), "Must be a list type");
				f_.resize(value.num_elements());
				for(size_t n = 0; n < value.num_elements(); ++n) {
					f_[n] = GLfloat(value[n].as_decimal().as_float());
				}
			}
		}
		virtual variant get_value(const std::string& key) const
		{
			if(key == "floats" || key == "value") {
				std::vector<variant> v;
				for(size_t n = 0; n < f_.size(); ++n) {
					v.push_back(variant(f_[n]));
				}
				return variant(&v);
			} else if(key == "size") {
				return variant(f_.size());
			}
			return variant();
		}
		const std::vector<GLfloat>& floats() { return f_; }
		int num_elements() const { return ne_; }
	private:
		int ne_;
		std::vector<GLfloat> f_;
	};

	class short_array_callable : public formula_callable
	{
	public:
		explicit short_array_callable(std::vector<GLshort>* s, int ne = 1)
			: ne_(ne)
		{
			s_.swap(*s);
		}
		virtual void set_value(const std::string& key, const variant& value) 
		{
			if(key == "shorts" || key == "value") {
				ASSERT_LOG(value.is_list(), "Must be a list type");
				s_.resize(value.num_elements());
				for(size_t n = 0; n < value.num_elements(); ++n) {
					s_[n] = GLshort(value[n].as_int());
				}
			}
		}
		variant get_value(const std::string& key) const
		{
			if(key == "shorts" || key == "value") {
				std::vector<variant> v;
				for(size_t n = 0; n < s_.size(); ++n) {
					v.push_back(variant(s_[n]));
				}
				return variant(&v);
			} else if(key == "size") {
				return variant(s_.size());
			}
			return variant();
		}
		const std::vector<GLshort>& shorts() { return s_; }
		int num_elements() const { return ne_; }
	private:
		int ne_;
		std::vector<GLshort> s_;
	};

}

#endif
