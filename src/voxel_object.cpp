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
#ifdef USE_GLES2

#include "json_parser.hpp"
#include "object_events.hpp"
#include "profile_timer.hpp"
#include "user_voxel_object.hpp"
#include "variant_utils.hpp"
#include "voxel_object.hpp"

namespace voxel
{
	voxel_object::voxel_object(const std::string& type, float x, float y, float z)
		: type_(type), translation_(x,y,z), rotation_(0.0f), scale_(1.0f),
		cycle_(0), paused_(false)
	{
		a_normal_ = shader_->get_fixed_attribute("normal");
		mvp_matrix_ = shader_->get_fixed_uniform("mvp_matrix");
	}

	voxel_object::voxel_object(const variant& node)
		: translation_(0.0f), rotation_(0.0f), scale_(1.0f),
		cycle_(0), paused_(false), type_(node["type"].as_string())
	{
		shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();
		ASSERT_LOG(node.has_key("model"), "Must have 'model' attribute");
 		model_.reset(new voxel_model(node));
		model_->set_animation("stand");

		std::map<variant,variant> items;
		items[variant("model")] = variant("data/voxel_models/sword.cfg");
		boost::intrusive_ptr<voxel_model> weapon(new voxel_model(variant(&items)));
		weapon->set_animation("stand");

		model_->attach_child(weapon, "handle", "melee_weapon");

		if(node.has_key("translation")) {
			translation_ = variant_to_vec3(node["translation"]);
		}
		if(node.has_key("rotation")) {
			rotation_ = variant_to_vec3(node["rotation"]);
		}
		if(node.has_key("scale")) {
			if(node["scale"].is_decimal()) {
				float scale = float(node["scale"].as_decimal().as_float());
				scale_ = glm::vec3(scale, scale, scale);
			} else {
				scale_ = variant_to_vec3(node["scale"]);
			}
		}
		if(node.has_key("widgets")) {
			if(node["widgets"].is_list()) {
				for(int n = 0; n != node["widgets"].num_elements(); ++n) {
					widgets_.insert(widget_factory::create(node["widgets"][n], this));
				}
			}
		}

		a_normal_ = shader_->get_fixed_attribute("normal");
		mvp_matrix_ = shader_->get_fixed_uniform("mvp_matrix");
	}

	voxel_object::~voxel_object()
	{
	}

	void voxel_object::draw(graphics::lighting_ptr lighting, camera_callable_ptr camera) const
	{
		//profile::manager pman("voxel_object::draw");
		if(model_) {
			glm::mat4 model_mat = 
				  glm::translate(glm::mat4(1.0f), translation_)
				* glm::scale(glm::mat4(1.0f), scale_)
				* glm::rotate(glm::mat4(1.0f), rotation_.x, glm::vec3(1,0,0))
				* glm::rotate(glm::mat4(1.0f), rotation_.z, glm::vec3(0,0,1))
				* glm::rotate(glm::mat4(1.0f), rotation_.y, glm::vec3(0,1,0));
			model_->draw(lighting, camera, model_mat);
		}

		for(auto w : widgets_) {
		}
	}

	void voxel_object::process(level& lvl)
	{
		if(paused_) {
			return;
		}

		if(model_) {
			model_->process_animation();
		}

		for(auto w : widgets_) {
			w->process();
		}
	}

	bool voxel_object::handle_sdl_event(const SDL_Event& event, bool claimed)
	{
		// XXX
		return claimed;
	}

	void voxel_object::set_paused(bool p)
	{
		paused_ = p;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(voxel_object)
	DEFINE_FIELD(widgets, "[builtin widget]")
		std::vector<variant> v;
		for(auto w : obj.widgets_) {
			v.push_back(variant(w.get()));
		}
		return variant(&v);
	DEFINE_FIELD(x, "decimal")
		return variant(obj.x());
	DEFINE_SET_FIELD
		obj.translation_.x = float(value.as_decimal().as_float());
	DEFINE_FIELD(y, "decimal")
		return variant(obj.y());
	DEFINE_SET_FIELD
		obj.translation_.y = float(value.as_decimal().as_float());
	DEFINE_FIELD(z, "decimal")
		return variant(obj.z());
	DEFINE_SET_FIELD
		obj.translation_.z = float(value.as_decimal().as_float());
	DEFINE_FIELD(translation, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.translation_);
	DEFINE_SET_FIELD
		obj.translation_ = variant_to_vec3(value);
	DEFINE_FIELD(rotation, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.rotation_);
	DEFINE_SET_FIELD
		obj.rotation_ = variant_to_vec3(value);
	DEFINE_FIELD(scale, "[decimal,decimal,decimal]")
		return vec3_to_variant(obj.scale_);
	DEFINE_SET_FIELD_TYPE("[decimal,decimal,decimal]|decimal")
		if(value.is_list()) {
			obj.scale_ = variant_to_vec3(value);
		} else {
			float scale = float(value.as_decimal().as_float());
			obj.scale_ = glm::vec3(scale, scale, scale);
		}
	DEFINE_FIELD(paused, "bool")
		return variant::from_bool(obj.paused());
	DEFINE_SET_FIELD
		obj.set_paused(value.as_bool());
	DEFINE_FIELD(animation, "string")
		return variant(obj.model_ ? obj.model_->current_animation() : "");
	DEFINE_SET_FIELD
		if(obj.model_) {
			obj.model_->set_animation(value.as_string());
		}
	END_DEFINE_CALLABLE(voxel_object)
}

namespace voxel_object_factory
{
	using namespace voxel;
	voxel_object_ptr create(const variant& node)
	{
		if(node.is_callable()) {
			voxel_object_ptr c = node.try_convert<voxel_object>();
			ASSERT_LOG(c != NULL, "Error converting voxel_object from callable.");
			return c;
		}
		ASSERT_LOG(node.has_key("type"), "No 'type' attribute found in definition.");
		const std::string& type = node["type"].as_string();
		return voxel_object_ptr(new user_voxel_object(node));
	}
}

#endif
