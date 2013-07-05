/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
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