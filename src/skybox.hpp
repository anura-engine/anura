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

#pragma once
#if defined(USE_ISOMAP)

#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>

#include "camera.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "lighting.hpp"
#include "raster.hpp"
#include "shaders.hpp"
#include "texture.hpp"
#include "variant.hpp"

namespace graphics
{
	class skybox : public game_logic::FormulaCallable
	{
	public:
		explicit skybox(const variant& node);
		virtual ~skybox();
		void draw(const lighting_ptr lighting, const camera_callable_ptr& camera) const;
	private:
		DECLARE_CALLABLE(skybox);

		gles2::program_ptr shader_;

		boost::shared_ptr<GLuint> tex_id_;
		
		GLuint u_texture_id_;
		GLuint u_mv_inverse_matrix_;
		GLuint u_p_inverse_matrix_;
		GLuint u_color_;
		GLuint a_position_;

		graphics::color color_;

		skybox();
		skybox(const skybox&);
	};

	typedef boost::intrusive_ptr<skybox> skybox_ptr;
}

#endif
