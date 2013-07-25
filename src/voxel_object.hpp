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

#ifdef USE_GLES2

#include <vector>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "camera.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "graphics.hpp"
#include "level.hpp"
#include "lighting.hpp"
#include "variant.hpp"
#include "voxel_model.hpp"
#include "widget_factory.hpp"

namespace voxel
{
class voxel_object : public game_logic::formula_callable
{
public:
	voxel_object(const std::string& type, float x, float y, float z);
	explicit voxel_object(const variant& node);
	virtual ~voxel_object();

	const std::string& type() const { return type_; }
	bool is_a(const std::string& type) const { return type_ == type; }

	void draw(const graphics::lighting_ptr lighting, camera_callable_ptr camera) const;
	virtual void process(level& lvl);
	bool handle_sdl_event(const SDL_Event& event, bool claimed);

	const gles2::program_ptr& shader() const { return shader_; }

	const glm::vec3& translation() const { return translation_; } 
	const glm::vec3& rotation() const { return rotation_; } 
	const glm::vec3& scale() const { return scale_; } 

	float x() const { return translation_.x; }
	float y() const { return translation_.y; }
	float z() const { return translation_.z; }

	float rotation_x() const { return rotation_.x; }
	float rotation_y() const { return rotation_.y; }
	float rotation_z() const { return rotation_.z; }

	float scale_x() const { return scale_.x; }
	float scale_y() const { return scale_.y; }
	float scale_z() const { return scale_.z; }

	voxel_model_ptr& model() { return model_; }
	const voxel_model_ptr& model_const() { return model_; }

	void add_widget(const gui::widget_ptr& w);
	void add_widgets(std::vector<gui::widget_ptr>* widgets);
	void clear_widgets();
	void remove_widget(gui::widget_ptr w);
	gui::widget_ptr get_widget_by_id(const std::string& id);
	gui::const_widget_ptr get_widget_by_id(const std::string& id) const;
	std::vector<variant> get_variant_widget_list() const;

	bool paused() const { return paused_; }
	void set_paused(bool p=true);

	size_t cycle() const { return cycle_; }

	bool pt_in_object(const glm::vec3& pt);

	void set_event_arg(variant v);

	bool is_mouseover_object() const { return is_mouseover_; }
	void set_mouseover_object(bool mo=true) { is_mouseover_ = mo; }

	void add_scheduled_command(int cycle, variant cmd);
	std::vector<variant> pop_scheduled_commands();

protected:
private:
	DECLARE_CALLABLE(voxel_object);

	std::string type_;

	bool paused_;

	size_t cycle_;

	glm::vec3 translation_;
	glm::vec3 rotation_;
	glm::vec3 scale_;

	voxel_model_ptr model_;

	gles2::program_ptr shader_;

	typedef std::set<gui::widget_ptr, gui::widget_sort_zorder> widget_list;
	widget_list widgets_;	

	GLuint a_normal_;
	GLuint mvp_matrix_;


	mutable glm::mat4 model_matrix_;
	
	bool is_mouseover_;

	typedef std::pair<int, variant> ScheduledCommand;
	std::vector<ScheduledCommand> scheduled_commands_;

	// XXX hack
	variant event_arg_;

	voxel_object();
};

typedef boost::intrusive_ptr<voxel_object> voxel_object_ptr;
typedef boost::intrusive_ptr<const voxel_object> const_voxel_object_ptr;
}

namespace voxel_object_factory
{
	voxel::voxel_object_ptr create(const variant& node);
}

#endif
