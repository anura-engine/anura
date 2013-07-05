#include <vector>
#include "asserts.hpp"
#include "isoworld.hpp"
#include "level.hpp"
#include "variant_utils.hpp"

namespace isometric
{
	world::world(const variant& node)
		: gamma_(1.0f), light_power_(15000.0f), light_position_(glm::vec3(100.f, 100.0f, 100.0f)),
		view_distance_(node["view_distance"].as_int(5)), seed_(node["seed"].as_int(rand()))
	{
		ASSERT_LOG(node.has_key("shader"), "Must have 'shader' attribute");
		ASSERT_LOG(node["shader"].is_string(), "'shader' attribute must be a string");
		shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();
		u_lightposition_ = shader_->get_fixed_uniform("light_position");
		u_lightpower_ = shader_->get_fixed_uniform("light_power");
		u_gamma_ = shader_->get_fixed_uniform("gamma");

		lighting_enabled_ = u_lightposition_ != -1 && u_lightpower_ != -1 && u_gamma_ != -1;
		std::cerr << "isometric::world: Lighting is " << (lighting_enabled_ ? "enabled" : "disable") << std::endl;

		build();
	}

	world::~world()
	{
	}

	void world::set_gamma(float g)
	{
		gamma_ = std::max(0.001f, std::min(g, 100.0f)); 
	}

	void world::set_light_power(float lp)
	{
		light_power_ = lp > 0.0f ? lp : 0.1f;
	}

	void world::set_light_position(const glm::vec3& lp)
	{
		light_position_ = lp;
	}

	void world::set_light_position(const variant& lp)
	{
		ASSERT_LOG(lp.is_list() && lp.num_elements() == 3, "light_position must be a list of 3 elements");
		light_position_ = glm::vec3(float(lp[0].as_decimal().as_float()),
			float(lp[1].as_decimal().as_float()),
			float(lp[2].as_decimal().as_float()));
	}

	void world::set_tile(int x, int y, int z, const variant& type)
	{
		int fx = int(floor(x));
		int fy = int(floor(y));
		int fz = int(floor(z));
		auto it = chunks_.find(position(fx, fy, fz));
		if(it != chunks_.end()) {
			it->second->set_tile(x-fx, y-fy, z-fz, type);
		}
	}
	
	void world::del_tile(int x, int y, int z)
	{
		int fx = int(floor(x));
		int fy = int(floor(y));
		int fz = int(floor(z));
		auto it = chunks_.find(position(fx, fy, fz));
		if(it != chunks_.end()) {
			it->second->del_tile(x-fx, y-fy, z-fz);
		}
	}

	void world::build()
	{
		// Generates initial chunks
		for(int x = 0; x != 10; ++x) {
			for(int z = 0; z != 10; ++z) {
				std::cerr << "Generating chunk: " << x << ",0," << z << std::endl;
				std::map<variant,variant> m;
				variant_builder rnd;
				
				rnd.add("width", 32);
				rnd.add("height", 32);
				rnd.add("depth", 32);
				rnd.add("noise_height", rand() % 32);
				rnd.add("type", graphics::color("lawn_green").write());
				rnd.add("seed", seed_);

				m[variant("type")] = variant("colored");
				m[variant("shader")] = variant(shader_->name());
				m[variant("skip_lighting_uniforms")] = variant::from_bool(true);
				std::vector<variant> v;
				v.push_back(variant(x*32));
				v.push_back(variant(0));
				v.push_back(variant(z*32));
				m[variant("worldspace_position")] = variant(&v);
				m[variant("random")] = rnd.build();


				chunks_[position(x,0,z)] = isometric::chunk_factory::create(variant(&m));
				active_chunks_.push_back(chunks_[position(x,0,z)]);
			}
		}
		//get_active_chunks();
	}

	void world::draw(const camera_callable_ptr& camera) const
	{
		glUseProgram(shader_->get());
		glClear(GL_DEPTH_BUFFER_BIT);
		// Cull triangles which normal is not towards the camera
		glEnable(GL_CULL_FACE);
		// Enable depth test
		glEnable(GL_DEPTH_TEST);

		if(lighting_enabled_) {
			glUniform3fv(u_lightposition_, 1, glm::value_ptr(light_position_));
			glUniform1f(u_lightpower_, light_power_);
			glUniform1f(u_gamma_, gamma_);
		}

		for(auto chunks : active_chunks_) {
			chunks->do_draw(camera);
		}

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
	}

	variant world::write()
	{
		variant_builder res;

		return res.build();
	}

	void world::process()
	{
		//get_active_chunks();
	}

	void world::get_active_chunks()
	{
		const glm::vec3& pos = level::current().camera()->position();
		active_chunks_.clear();
		for(auto chnk : chunks_) {
			glm::vec3 cpos(chnk.first.x, chnk.first.y, chnk.first.z);
			if(glm::length(cpos - pos) < float(view_distance_)) {
				active_chunks_.push_back(chnk.second);
			}
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(world)
	DEFINE_FIELD(gamma, "decimal")
		return variant(obj.gamma());
	DEFINE_SET_FIELD
		obj.set_gamma(float(value.as_decimal().as_float()));
	DEFINE_FIELD(view_distance, "int")
		return variant(obj.view_distance_);
	DEFINE_SET_FIELD
		obj.view_distance_ = std::max(0, std::min(value.as_int(), 10));
	DEFINE_FIELD(light_power, "decimal")
		return variant(obj.light_power());
	DEFINE_SET_FIELD
		obj.set_light_power(float(value.as_decimal().as_float()));
	DEFINE_FIELD(light_position, "[decimal,decimal,decimal]")
		std::vector<variant> v;
		v.push_back(variant(obj.light_position().x));
		v.push_back(variant(obj.light_position().y));
		v.push_back(variant(obj.light_position().z));
		return variant(&v);
	DEFINE_SET_FIELD
		obj.set_light_position(value);
	END_DEFINE_CALLABLE(world)
}