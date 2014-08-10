/*
	Copyright (C) 2003-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "ColorTransform.hpp"
#include "variant_utils.hpp"

namespace KRE
{
	namespace 
	{
		template<typename T>
		T clamp(T value, T minval, T maxval)
		{
			return std::min<T>(maxval, std::max(value, minval));
		}
	}

	ColorTransform::ColorTransform()
//		: add_rgba_()
	{
		mul_rgba_[0] = mul_rgba_[1] = mul_rgba_[2] = mul_rgba_[3] = 1.0;
		add_rgba_[0] = add_rgba_[2] = add_rgba_[2] = add_rgba_[3] = 0.0;
	}

	ColorTransform::ColorTransform(const Color& color)
	{
		mul_rgba_[0] = mul_rgba_[1] = mul_rgba_[2] = mul_rgba_[3] = 1.0;
		add_rgba_[0] = color.r();
		add_rgba_[1] = color.g();
		add_rgba_[2] = color.b();
		add_rgba_[3] = color.a();
	}

	ColorTransform::ColorTransform(const variant& v)
	{
		mul_rgba_[0] = mul_rgba_[1] = mul_rgba_[2] = mul_rgba_[3] = 1.0;
		add_rgba_[0] = add_rgba_[1] = add_rgba_[2] = add_rgba_[3] = 0.0;

		if(v.is_list()) {
			for(unsigned n = 0; n != 4; ++n) {
				if(n < v.num_elements()) {
					add_rgba_[n] = v[n].as_int() / 255.0;
				} else {
					add_rgba_[n] = 1.0;
				}
			}
		} else if(v.is_map()) {
			if(v.has_key("add")) {
				const variant& a = v["add"];
				for(unsigned n = 0; n != 4; ++n) {
					if(n < a.num_elements()) {
						if(a.is_int()) {
							add_rgba_[n] = a[n].as_int() / 255.0;
						} else {
							add_rgba_[n] = a[n].as_double();
						}
					}
				}
			}
			if(v.has_key("mul")) {
				const variant& m = v["mul"];
				for(unsigned n = 0; n != 4; ++n) {
					if(n < m.num_elements()) {
						if(m.is_int()) {
							add_rgba_[n] = m[n].as_int() / 255.0;
						} else {
							add_rgba_[n] = m[n].as_double();
						}
					}
				}
			}
		}
	}

	ColorTransform::ColorTransform(double mr, double mg, double mb, double ma, double ar, double ag, double ab, double aa)
	{
		mul_rgba_[0] = mr;
		mul_rgba_[1] = mg;
		mul_rgba_[2] = mb;
		mul_rgba_[3] = ma;
		add_rgba_[0] = ar;
		add_rgba_[1] = ag;
		add_rgba_[2] = ab;
		add_rgba_[3] = aa;
	}

	ColorTransform::ColorTransform(int mr, int mg, int mb, int ma, int ar, int ag, int ab, int aa)
	{
		mul_rgba_[0] = mr/255.0;
		mul_rgba_[1] = mg/255.0;
		mul_rgba_[2] = mb/255.0;
		mul_rgba_[3] = ma/255.0;
		add_rgba_[0] = ar/255.0;
		add_rgba_[1] = ag/255.0;
		add_rgba_[2] = ab/255.0;
		add_rgba_[3] = aa/255.0;
	}

	ColorTransform::ColorTransform(int ar, int ag, int ab, int aa)
	{
		mul_rgba_[0] = 0;
		mul_rgba_[1] = 0;
		mul_rgba_[2] = 0;
		mul_rgba_[3] = 0;
		add_rgba_[0] = ar/255.0;
		add_rgba_[1] = ag/255.0;
		add_rgba_[2] = ab/255.0;
		add_rgba_[3] = aa/255.0;
	}

	ColorTransform::~ColorTransform()
	{
	}

	ColorTransform operator+(const ColorTransform& a, const ColorTransform& b)
	{
		return ColorTransform(a.mulRed() * b.mulRed(),
			a.mulGreen() * b.mulGreen(),
			a.mulBlue() * b.mulBlue(),
			a.mulAlpha() * b.mulAlpha(),
			a.addRed() + b.addRed(),
			a.addGreen() + b.addGreen(),
			a.addBlue() + b.addBlue(),
			a.addAlpha() + b.addAlpha());
	}

	ColorTransform operator-(const ColorTransform& a, const ColorTransform& b)
	{
		return ColorTransform(a.mulRed() * b.mulRed(),
			a.mulGreen() * b.mulGreen(),
			a.mulBlue() * b.mulBlue(),
			a.mulAlpha() * b.mulAlpha(),
			a.addRed() - b.addRed(),
			a.addGreen() - b.addGreen(),
			a.addBlue() - b.addBlue(),
			a.addAlpha() - b.addAlpha());
	}

	Color ColorTransform::apply(const Color& color) const
	{
		return Color(color.r() * mul_rgba_[0] + add_rgba_[0],
			color.g() * mul_rgba_[1] + add_rgba_[1],
			color.b() * mul_rgba_[2] + add_rgba_[2],
			color.a() * mul_rgba_[3] + add_rgba_[3]);
	}

	Color ColorTransform::applyWhite() const
	{
		return Color(mul_rgba_[0] + add_rgba_[0], 
			mul_rgba_[1] + add_rgba_[1], 
			mul_rgba_[2] + add_rgba_[2], 
			mul_rgba_[3] + add_rgba_[3]);
	}

	Color ColorTransform::applyBlack() const
	{
		return Color(add_rgba_[0], add_rgba_[1], add_rgba_[2], add_rgba_[3]);
	}

	variant ColorTransform::write() const
	{
		variant_builder res;
		for(int n = 0; n != 4; ++n) {
			res.add("add", add_rgba_[n]);
		}
		for(int n = 0; n != 4; ++n) {
			res.add("mul", mul_rgba_[n]);
		}
		return res.build();
	}

	// legacy function
	std::string ColorTransform::toString() const
	{
		std::stringstream s;
		s << add_rgba_[0] << "," << add_rgba_[1] << "," << add_rgba_[2] << "," << add_rgba_[3];
		return s.str();
	}

	// legacy function
	bool ColorTransform::fits_in_color() const
	{
		for(int n = 0; n != 4; ++n) {
			if(add_rgba_[n] > 1.0) {
				return false;
			}
		}
		return true;
	}

	// legacy function
	Color ColorTransform::toColor() const
	{
		return applyBlack();
	}

	void ColorTransform::setAddRed(int ar)
	{
		add_rgba_[0] = clamp(ar/255.0f, 0.0f, 1.0f);
	}

	void ColorTransform::setAddGreen(int ag)
	{
		add_rgba_[1] = clamp(ag/255.0f, 0.0f, 1.0f);
	}

	void ColorTransform::setAddBlue(int ab)
	{
		add_rgba_[2] = clamp(ab/255.0f, 0.0f, 1.0f);
	}

	void ColorTransform::setAddAlpha(int aa)
	{
		add_rgba_[3] = clamp(aa/255.0f, 0.0f, 1.0f);
	}


	bool operator==(const ColorTransform& a, const ColorTransform& b)
	{
		return std::abs(a.addRed()-b.addRed())<DBL_EPSILON 
			&& std::abs(a.addGreen()-b.addGreen())<DBL_EPSILON
			&& std::abs(a.addBlue()-b.addBlue())<DBL_EPSILON
			&& std::abs(a.addAlpha()-b.addAlpha())<DBL_EPSILON
			&& std::abs(a.mulRed()-b.mulRed())<DBL_EPSILON
			&& std::abs(a.mulGreen()-b.mulGreen())<DBL_EPSILON
			&& std::abs(a.mulBlue()-b.mulBlue())<DBL_EPSILON
			&& std::abs(a.mulAlpha()-b.mulAlpha())<DBL_EPSILON;
	}

	bool operator!=(const ColorTransform& a, const ColorTransform& b)
	{
		return !operator==(a,b);
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(ColorTransform)
		DEFINE_FIELD(r, "int")
			return variant(obj.add_rgba_[0]);
		DEFINE_FIELD(g, "int")
			return variant(obj.add_rgba_[1]);
		DEFINE_FIELD(b, "int")
			return variant(obj.add_rgba_[2]);
		DEFINE_FIELD(a, "int")
			return variant(obj.add_rgba_[3]);
	END_DEFINE_CALLABLE(ColorTransform)
}
