#include <vector>
#include "asserts.hpp"
#include "isoworld.hpp"
#include "level.hpp"
#include "profile_timer.hpp"
#include "variant_utils.hpp"
#include "voxel_object.hpp"

namespace voxel
{
	const int chunk_size = 32;
	const int initial_chunks = 2;

	world::world(const variant& node)
		: view_distance_(node["view_distance"].as_int(5)), seed_(node["seed"].as_int(rand()))
	{
		ASSERT_LOG(node.has_key("shader"), "Must have 'shader' attribute");
		ASSERT_LOG(node["shader"].is_string(), "'shader' attribute must be a string");
		shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();

		if(node.has_key("lighting")) {
			lighting_.reset(new graphics::lighting(shader_, node["lighting"]));
		}

		if(node.has_key("objects")) {
			for(int n = 0; n != node["objects"].num_elements(); ++n) {
				add_object(voxel_object_factory::create(node["objects"][n]));
			}
		}

		build();
	}

	world::~world()
	{
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

	variant world::get_tile_type(int x, int y, int z) const
	{
		int fx = int(floor(x));
		int fy = int(floor(y));
		int fz = int(floor(z));
		auto it = chunks_.find(position(fx, fy, fz));
		if(it != chunks_.end()) {
			return it->second->get_tile_type(x-fx, y-fy, z-fz);
		}
		return variant();
	}

	void world::add_object(voxel_object_ptr obj)
	{
		objects_.insert(obj);
	}

	void world::remove_object(voxel_object_ptr obj)
	{
		auto it = objects_.find(obj);
		ASSERT_LOG(it != objects_.end(), "Unable to remove object '" << obj->type() << "' from level");
		objects_.erase(it);
	}

	void world::build()
	{
		profile::manager pman("Built voxel::world in");
		// Generates initial chunks
		for(int x = 0; x != initial_chunks; ++x) {
			for(int y = 0; y < 3; y++) {
				for(int z = 0; z != initial_chunks; ++z) {
					//std::cerr << "Generating chunk: " << x << ",0," << z << std::endl;
					std::map<variant,variant> m;
					variant_builder rnd;
				
					rnd.add("width", chunk_size);
					rnd.add("height", chunk_size);
					rnd.add("depth", chunk_size);
					rnd.add("noise_height", rand() % chunk_size);
					rnd.add("type", graphics::color("medium_sea_green").write());
					rnd.add("seed", seed_);

					m[variant("type")] = variant("colored");
					m[variant("shader")] = variant(shader_->name());
					std::vector<variant> v;
					v.push_back(variant(x*chunk_size));
					v.push_back(variant(0));
					v.push_back(variant(z*chunk_size));
					m[variant("worldspace_position")] = variant(&v);
					m[variant("random")] = rnd.build();


					chunks_[position(x*chunk_size,0,z*chunk_size)] = voxel::chunk_factory::create(variant(&m));
					active_chunks_.push_back(chunks_[position(x*chunk_size,0,z*chunk_size)]);
				}
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

		for(auto chunks : active_chunks_) {
			chunks->do_draw(lighting_, camera);
		}

		for(auto obj : objects_) {
			obj->draw(lighting_, camera);
		}

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

		level::current().camera()->frustum().draw();
	}

	variant world::write()
	{
		variant_builder res;

		return res.build();
	}

	void world::process()
	{
		get_active_chunks();
		for(auto obj : objects_) {
			obj->process(level::current());
		}
	}

	void world::get_active_chunks()
	{
		//profile::manager pman("get_active_chunks");
		const graphics::frustum& frustum = level::current().camera()->frustum();
		active_chunks_.clear();
		for(auto chnk : chunks_) {
			if(frustum.cube_inside(glm::vec3(chnk.first.x, chnk.first.y, chnk.first.z), float(chunk_size),float(chunk_size), float(chunk_size))) {
			//if(frustum.point_inside(glm::vec3(chnk.first.x + 16.5f, chnk.first.y + 16.5f, chnk.first.z + 16.5f))) {
				active_chunks_.push_back(chnk.second);
			}
		}
		//std::cerr << "WORLD: " << active_chunks_.size() << "/" << chunks_.size() << " in viewing volume" << std::endl;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(world)
	DEFINE_FIELD(lighting, "builtin lighting")
		return variant(obj.lighting_.get());
	DEFINE_SET_FIELD_TYPE("map")
		obj.lighting_.reset(new graphics::lighting(obj.shader_, value));
	DEFINE_FIELD(objects, "[builtin voxel_object]")
		std::vector<variant> v;
		for(auto o : obj.objects_) {
			v.push_back(variant(o.get()));
		}
		return variant(&v);
	END_DEFINE_CALLABLE(world)
}