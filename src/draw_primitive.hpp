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
#if defined(USE_SHADERS)

#include <boost/intrusive_ptr.hpp>

#include "camera.hpp"
#include "lighting.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace graphics
{

class draw_primitive : public game_logic::formula_callable
{
public:
	static boost::intrusive_ptr<draw_primitive> create(const variant& v);

	explicit draw_primitive(const variant& v);


	void draw() const;
#if defined(USE_ISOMAP)
	void draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const;
#endif
private:
	DECLARE_CALLABLE(draw_primitive);

	virtual void handle_draw() const = 0;
#if defined(USE_ISOMAP)
	virtual void handle_draw(const lighting_ptr& lighting, const camera_callable_ptr& camera) const = 0;
#endif
	GLenum src_factor_, dst_factor_;
};

typedef boost::intrusive_ptr<draw_primitive> draw_primitive_ptr;
typedef boost::intrusive_ptr<const draw_primitive> const_draw_primitive_ptr;

}

#endif
