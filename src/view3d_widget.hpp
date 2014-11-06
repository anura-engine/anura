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

#if defined(USE_ISOMAP)

#include <vector>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "button.hpp"
#include "camera.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "gles2.hpp"
#include "graphics.hpp"
#include "grid_widget.hpp"
#include "shaders.hpp"
#include "variant.hpp"
#include "widget.hpp"

namespace gui
{
	class view3d_widget : public widget
	{
	public:
		view3d_widget(int x, int y, int width, int height);
		view3d_widget(const variant& v, game_logic::formula_callable* e);
		virtual ~view3d_widget();
	protected:
		void init();
	private:
		DECLARE_CALLABLE(view3d_widget);

		void reset_contents(const variant& v);

		boost::shared_ptr<GLuint> fbo_;
		boost::shared_ptr<GLuint> texture_;
		boost::shared_ptr<GLuint> depth_id_;

		camera_callable_ptr camera_;
		float camera_distance_;

		size_t tex_width_;
		size_t tex_height_;

		void handle_process();
		void handle_draw() const;
		bool handle_event(const SDL_Event& event, bool claimed);
		void render_fbo() const;
		void render_texture() const;
		glm::mat4 proj_2d_;

		std::vector<widget_ptr> children_;

		view3d_widget();
		view3d_widget(const view3d_widget&);
	};

	typedef boost::intrusive_ptr<view3d_widget> view3d_widget_ptr;
	typedef boost::intrusive_ptr<const view3d_widget> const_view3d_widget_ptr;
}

#endif
