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
#if defined(USE_SHADERS) && defined(USE_ISOMAP)

#include "json_parser.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "profile_timer.hpp"
#include "user_voxel_object.hpp"
#include "variant_utils.hpp"
#include "voxel_object.hpp"
#include "voxel_object_type.hpp"
#include "voxel_object_functions.hpp"

namespace voxel
{
	namespace
	{
		typedef std::map<std::string,std::string> model_path_type;

		model_path_type model_cache_populate()
		{
			model_path_type res;
			module::get_unique_filenames_under_dir("data/voxel_models", &res, module::MODULE_NO_PREFIX);
			return res;
		}

		model_path_type& model_path_cache()
		{
			static model_path_type res = model_cache_populate();
			return res;
		}

		std::string model_path_get_or_die(const std::string& model_name)
		{
			auto it = model_path_cache().find(model_name);
			if(it ==  model_path_cache().end()) {
				it = model_path_cache().find(model_name + ".cfg");
				ASSERT_LOG(it != model_path_cache().end(), "Unable to find the file '" << model_name << "' in the list of models.");
			}				
			return it->second;
		}
	}

	voxel_object::voxel_object(const variant& node)
		// The initializer list should NOT read from 'node'. It should only
		// set up default values. Read from node in the body.
		: translation_(0.0f), rotation_(0.0f), scale_(1.0f),
		cycle_(0), paused_(false), is_mouseover_(false)
	{
		if(node.has_key("type")) {
			const std::string type = node["type"].as_string();

			const voxel_object* prototype = voxel_object_type::get(type)->prototype();
			if(prototype) {
				*this = *prototype;
			}

			type_ = type;
		}

		if(!shader_ || node.has_key("shader")) {
			shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();
		}
		
		if(!model_ || node.has_key("model")) {
			std::map<variant,variant> m;
			m[variant("model")] = variant(model_path_get_or_die(node["model"].as_string()));
 			model_.reset(new voxel_model(variant(&m)));
			model_->set_animation("stand");
		}

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
			model_matrix_ = 
				  glm::translate(glm::mat4(1.0f), translation_)
				* glm::scale(glm::mat4(1.0f), scale_)
				* glm::rotate(glm::mat4(1.0f), rotation_.x, glm::vec3(1,0,0))
				* glm::rotate(glm::mat4(1.0f), rotation_.z, glm::vec3(0,0,1))
				* glm::rotate(glm::mat4(1.0f), rotation_.y, glm::vec3(0,1,0));
			model_->draw(lighting, camera, model_matrix_);
		}

		for(auto w : widgets_) {
		}
	}

	bool voxel_object::pt_in_object(const glm::vec3& pt)
	{
		if(model_) {
			glm::vec3 b1;
			glm::vec3 b2;
			model_->get_bounding_box(b1, b2);
			glm::vec4 bb1 = model_matrix_ * glm::vec4(b1,1.0f);
			glm::vec4 bb2 = model_matrix_ * glm::vec4(b2,1.0f);
			//std::cerr << "pt_in_object: " << pt.x << "," << pt.y << "," << pt.z << " : " << b1.x << "," << b1.y << "," << b1.z << ":" << b2.x << "," << b2.y << "," << b2.z << " : " << std::endl;
			//std::cerr << "pt_in_object: " << bb1.x << "," << bb1.y << "," << bb1.z << ":" << bb2.x << "," << bb2.y << "," << bb2.z << std::endl;
			if(pt.x >= bb1.x && pt.x <= bb2.x && pt.y >= bb1.y && pt.y <= bb2.y && pt.z >= bb1.z && pt.z <= bb2.z) {
				return true;
			}
		} 
		return false;
	}

	void voxel_object::process(level& lvl)
	{
		if(paused_) {
			return;
		}

		std::vector<variant> scheduled_commands = pop_scheduled_commands();
		foreach(const variant& cmd, scheduled_commands) {
			executeCommand(cmd);
		}

		if(model_) {
			model_->process_animation();
		}

		for(auto w : widgets_) {
			w->process();
		}

		++cycle_;
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

	// XXX hack
	void voxel_object::set_event_arg(variant v)
	{
		event_arg_ = v;
	}

	void voxel_object::add_scheduled_command(int cycle, variant cmd)
	{
		scheduled_commands_.push_back(ScheduledCommand(cycle, cmd));
	}

	std::vector<variant> voxel_object::pop_scheduled_commands()
	{
		std::vector<variant> result;
		std::vector<ScheduledCommand>::iterator i = scheduled_commands_.begin();
		while(i != scheduled_commands_.end()) {
			if(--(i->first) <= 0) {
				result.push_back(i->second);
				i = scheduled_commands_.erase(i);
			} else {
				++i;
			}
		}

		return result;
	}


	BEGIN_DEFINE_CALLABLE_NOBASE(voxel_object)
	BEGIN_DEFINE_FN(attach_model, "(builtin voxel_model,string,string) ->commands")
		variant model_var = FN_ARG(0);
		variant child_point = FN_ARG(1);
		variant parent_point = FN_ARG(2);

		boost::intrusive_ptr<voxel_model> model(model_var.convert_to<voxel_model>());

		std::function<void()> fn = [=]() { obj.model_->attach_child(model, child_point.as_string(), parent_point.as_string()); };
		return variant(new game_logic::fn_command_callable(fn));
	END_DEFINE_FN

	DEFINE_FIELD(world, "builtin world")
		return variant(level::current().iso_world().get());
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
	DEFINE_FIELD(cycle, "int")
		return variant(obj.cycle_);
	DEFINE_FIELD(event_arg, "any")
		return obj.event_arg_;
	DEFINE_FIELD(type, "string")
		return variant(obj.type_);
	DEFINE_FIELD(model, "builtin voxel_model")
		return variant(obj.model_.get());
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
