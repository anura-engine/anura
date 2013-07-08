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
#include "profile_timer.hpp"
#include "variant_utils.hpp"
#include "voxel_object.hpp"

namespace voxel
{
	voxel_object::voxel_object(const std::string& type, float x, float y, float z)
		: type_(type), translation_(x,y,z), rotation_(0.0f,0.0f,0.0f), scale_(1.0f,1.0f,1.0f),
		cycle_(0), paused_(false)
	{
		a_normal_ = shader_->get_fixed_attribute("normal");
		mvp_matrix_ = shader_->get_fixed_uniform("mvp_matrix");
	}

	voxel_object::voxel_object(const variant& node)
		: translation_(0.0f), rotation_(0.0f), scale_(1.0f,1.0f,1.0f),
		cycle_(0), paused_(false), type_(node["type"].as_string())
	{
		shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();
		ASSERT_LOG(node.has_key("model"), "Must have 'model' attribute");
		std::map<variant,variant> m;
		m[variant("model")] = variant(node["model"].as_string());
		model_.reset(new voxel_model(variant(&m)));
		model_ = model_->build_instance();
		model_->set_animation("stand");

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

	void voxel_object::draw(camera_callable_ptr camera) const
	{
		if(model_) {
			//profile::manager pman("voxel_object::draw");
			std::vector<GLfloat> varray;
			std::vector<GLfloat> carray;
			std::vector<GLfloat> narray;
			
			model_->process_animation();
			model_->generate_geometry(&varray, &narray, &carray);

			glm::mat4 model_mat = glm::scale(glm::mat4(1.0f), scale_);
			//model_mat = glm::rotate(model_mat, rotation_.x, glm::vec3(1,0,0));
			//model_mat = glm::rotate(model_mat, rotation_.y, glm::vec3(0,1,0));
			//model_mat = glm::rotate(model_mat, rotation_.z, glm::vec3(0,0,1));
			model_mat = glm::translate(model_mat, translation_);
			glm::mat4 mvp = camera->projection_mat() * camera->view_mat() * model_mat;
			//glUniformMatrix4fv(mvp_matrix_, 1, GL_FALSE, glm::value_ptr(mvp));
			GLuint u_mvp = -1;
			if(u_mvp == -1) {
				GLint active_program;
				glGetIntegerv(GL_CURRENT_PROGRAM, &active_program);
				u_mvp = glGetUniformLocation(active_program, "mvp_matrix");
			}
			glUniformMatrix4fv(u_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

			if(!varray.empty()) {
				//glUseProgram(shader_->get());
				assert(varray.size() == narray.size());
				shader_->vertex_array(3, GL_FLOAT, GL_FALSE, 0, &varray[0]);
				shader_->color_array(4, GL_FLOAT, GL_FALSE, 0, &carray[0]);
				//shader_->vertex_attrib_array(a_normal_, 3, GL_FLOAT, GL_FALSE, 0, &narray[0]);
				glDrawArrays(GL_TRIANGLES, 0, varray.size()/3);
			}
		}

		for(auto w : widgets_) {
		}
	}

	void voxel_object::process(level& lvl)
	{
		if(paused_) {
			return;
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
		return voxel_object_ptr(new voxel_object(node));
	}
}

#endif
