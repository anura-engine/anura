#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	round
#endif

#include <vector>
#include "asserts.hpp"
#include "isoworld.hpp"
#include "level.hpp"
#include "profile_timer.hpp"
#include "user_voxel_object.hpp"
#include "variant_utils.hpp"
#include "voxel_object.hpp"

namespace voxel
{
	const int chunk_size = 32;
	const int initial_chunks = 16;

	const int default_view_distance = 5;

	logical_world::logical_world(const variant& node)
		:size_x_(0), size_y_(0), size_z_(0), 
		scale_x_(node["scale_x"].as_int(1)), scale_y_(node["scale_y"].as_int(1)), scale_z_(node["scale_z"].as_int(1)),
		chunks_(node)
	{
		ASSERT_LOG(node.has_key("chunks"), "To create a logic world must have 'chunks' attribute");

		int min_x, min_y, min_z;
		int max_x, max_y, max_z;
		min_x = min_y = min_z = std::numeric_limits<int>::max();
		max_x = max_y = max_z = std::numeric_limits<int>::min();

		for(int n = 0; n != node["chunks"].num_elements(); ++n) {
			const int wpx = node["chunks"][n]["worldspace_position"][0].as_int();
			const int wpy = node["chunks"][n]["worldspace_position"][1].as_int();
			const int wpz = node["chunks"][n]["worldspace_position"][2].as_int();

			foreach(const variant_pair& p, node["chunks"][n]["voxels"].as_map()) {
				const int gpx = p.first.as_list()[0].as_int() + wpx;
				const int gpy = p.first.as_list()[1].as_int() + wpy;
				const int gpz = p.first.as_list()[2].as_int() + wpz;
				if(min_x > gpx) { min_x = gpx; }
				if(max_x < gpx) { max_x = gpx; }
				if(min_y > gpy) { min_y = gpy; }
				if(max_y < gpy) { max_y = gpy; }
				if(min_z > gpz) { min_z = gpz; }
				if(max_z < gpz) { max_z = gpz; }

				heightmap_[std::make_pair(gpx,gpz)] = gpy;
			}
		}
		size_x_ = max_x - min_x + 1;
		size_y_ = max_y - min_y + 1;
		size_z_ = max_z - min_z + 1;
	}

	logical_world::~logical_world()
	{
	}

	glm::ivec3 logical_world::worldspace_to_logical(const glm::vec3& wsp) const
	{
		glm::ivec3 voxel_coord = glm::ivec3(
			abs(wsp[0]-bmround(wsp[0])) < 0.05f ? int(bmround(wsp[0])) : int(floor(wsp[0])),
			abs(wsp[1]-bmround(wsp[1])) < 0.05f ? int(bmround(wsp[1])) : int(floor(wsp[1])),
			abs(wsp[2]-bmround(wsp[2])) < 0.05f ? int(bmround(wsp[2])) : int(floor(wsp[2])));
		glm::ivec3 facing = level::current().camera()->get_facing(wsp);
		if(facing.x > 0) {
			--voxel_coord.x; 
		}
		if(facing.y > 0) {
			--voxel_coord.y; 
		}
		if(facing.z > 0) {
			--voxel_coord.z; 
		}
		voxel_coord /= glm::ivec3(scale_x_, scale_y_, scale_z_);
		return voxel_coord;
	}


	world::world(const variant& node)
		: view_distance_(node["view_distance"].as_int(default_view_distance)), 
		seed_(node["seed"].as_int(0))
	{
		ASSERT_LOG(node.has_key("shader"), "Must have 'shader' attribute");
		ASSERT_LOG(node["shader"].is_string(), "'shader' attribute must be a string");
		shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();

		if(node.has_key("lighting")) {
			lighting_.reset(new graphics::lighting(shader_, node["lighting"]));
		}

		if(node.has_key("objects")) {
			for(int n = 0; n != node["objects"].num_elements(); ++n) {
				add_object(new user_voxel_object(node["objects"][n]));
			}
		}

		if(node.has_key("chunks")) {
			logic_.reset(new logical_world(node));
			build_fixed(node["chunks"]);
		} else {
			build_infinite();
		}
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

	void world::add_object(user_voxel_object_ptr obj)
	{
		objects_.insert(obj);
	}

	void world::remove_object(user_voxel_object_ptr obj)
	{
		auto it = objects_.find(obj);
		ASSERT_LOG(it != objects_.end(), "Unable to remove object '" << obj->type() << "' from level");
		objects_.erase(it);
	}

	void world::get_objects_at_point(const glm::vec3& pt, std::vector<user_voxel_object_ptr>& obj_list)
	{
		for(auto obj : objects_) {
			if(obj->pt_in_object(pt)) {
				obj_list.push_back(obj);
			}
		}
	}

	void world::build_fixed(const variant& node)
	{
		for(int n = 0; n != node.num_elements(); ++n) {
			chunk_ptr cp = voxel::chunk_factory::create(shader_, logic_, node[n]);
			int wpx = node[n]["worldspace_position"][0].as_int() * logic_->scale_x();
			int wpy = node[n]["worldspace_position"][1].as_int() * logic_->scale_y();
			int wpz = node[n]["worldspace_position"][2].as_int() * logic_->scale_z();
			chunks_[position(wpx,wpy,wpz)] = cp;
		}
	}

	void world::build_infinite()
	{
		profile::manager pman("Built voxel::world in");

		int x_smoothness = rand() % 480 + 32;
		int z_smoothness = rand() % 480 + 32;

		// Generates initial chunks
		for(int x = 0; x != initial_chunks; ++x) {
			for(int y = 0; y != 4; y++) {
				for(int z = 0; z != initial_chunks; ++z) {
					std::cerr << "Generating chunk: " << x << "," << y << "," << z << std::endl;
					std::map<variant,variant> m;
					variant_builder rnd;

					glm::ivec3 worldspace_pos(x * chunk_size, y * chunk_size, z * chunk_size);
				
					rnd.add("width", chunk_size);
					rnd.add("height", chunk_size);
					rnd.add("depth", chunk_size);
					//rnd.add("noise_height", rand() % chunk_size*2);
					rnd.add("noise_height", 128);
					rnd.add("type", graphics::color("medium_sea_green").write());
					rnd.add("seed", seed_);
					rnd.add("x_smoothness", x_smoothness);		// 32 is very spiky, 512 is very flat
					rnd.add("z_smoothness", z_smoothness);

					m[variant("type")] = variant("colored");
					m[variant("shader")] = variant(shader_->name());
					std::vector<variant> v;
					v.push_back(variant(worldspace_pos.x));
					v.push_back(variant(worldspace_pos.y));
					v.push_back(variant(worldspace_pos.z));
					m[variant("worldspace_position")] = variant(&v);
					m[variant("random")] = rnd.build();

					chunk_ptr cp = voxel::chunk_factory::create(shader_, logical_world_ptr(), variant(&m));
					chunks_[position(worldspace_pos.x,worldspace_pos.y,worldspace_pos.z)] = cp;
					active_chunks_.push_back(cp);
				}
			}
		}
		//get_active_chunks();
	}

	void world::draw(const camera_callable_ptr& camera) const
	{
		//profile::manager pman("world::draw");
		glUseProgram(shader_->get());
		glClear(GL_DEPTH_BUFFER_BIT);
		// Cull triangles which normal is not towards the camera
		glEnable(GL_CULL_FACE);
		// Enable depth test
		glEnable(GL_DEPTH_TEST);

		for(auto chunks : active_chunks_) {
			chunks->draw(lighting_, camera);
		}

		for(auto obj : objects_) {
			obj->draw(lighting_, camera);
		}

		for(auto prim : draw_primitives_) {
			prim->draw(lighting_, camera);
		}

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

		level::current().camera()->frustum().draw();
	}

	variant world::write()
	{
		variant_builder res;

		res.add("shader", shader_->name());

		if(lighting_) {
			res.add("lighting", lighting_->write());
		}

		if(view_distance_ != default_view_distance) {
			res.add("view_distance", view_distance_);
		}

		if(seed_ != 0) {
			res.add("seed", seed_);
		}

		for(auto chnk : chunks_) {
			variant_builder wsp;
			wsp.add("worldspace_position", chnk.first.x);
			wsp.add("worldspace_position", chnk.first.y);
			wsp.add("worldspace_position", chnk.first.z);
			wsp.add("data", chnk.second->write());

			res.add("chunks", wsp.build());
		}		

		// XXX
		//for(auto obj : objects_) {
		//	res.add("objects", obj->write());
		//}

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
				active_chunks_.push_back(chnk.second);
			}
		}
	}

	REGISTER_SERIALIZABLE_CALLABLE(logical_world, "@logical_world");

	variant logical_world::serialize_to_wml() const
	{
		variant v(chunks_);
		v.add_attr(variant("@logical_world"), variant("logical_world"));
		return v;
	}

	namespace
	{
		variant variant_list_from_position(int x, int y, int z)
		{
			std::vector<variant> v;
			v.push_back(variant(x)); v.push_back(variant(y)); v.push_back(variant(z));
			return variant(&v);
		}
	}

	bool logical_world::is_xedge(int x) const
	{
		if(x >= 0 && x < size_x()) {
			return false;
		}
		return true;
	}

	bool logical_world::is_yedge(int y) const
	{
		if(y >= 0 && y < size_y()) {
			return false;
		}
		return true;
	}

	bool logical_world::is_zedge(int z) const
	{
		if(z >= 0 && z < size_z()) {
			return false;
		}
		return true;
	}

	bool logical_world::is_solid(int x, int y, int z) const
	{
		auto it = heightmap_.find(std::make_pair(x,z));
		if(it == heightmap_.end()) {
			return false;
		}
		if(y <= it->second) {
			return true;
		} else {
			return false;
		}
	}

	pathfinding::directed_graph_ptr logical_world::create_directed_graph(bool allow_diagonals) const
	{
		profile::manager pman("logical_world::create_directed_graph");

		std::vector<variant> vertex_list;
		std::map<std::pair<int,int>, int> vlist;

		for(auto p : heightmap_) {
			int x = p.first.first;
			int y = p.second;
			int z = p.first.second;

			if(y < size_y() - 1) {
				if(is_solid(x, y+1, z) == false) {
					vertex_list.push_back(variant_list_from_position(x,y+1,z));
					vlist[std::make_pair(x,z)] = y+1;
				}
			} else {
				vertex_list.push_back(variant_list_from_position(x,y+1,z));
				vlist[std::make_pair(x,z)] = y+1;
			}
		}
	
		pathfinding::graph_edge_list edges;
		for(auto p : heightmap_) {
			const int x = p.first.first;
			const int y = p.second;
			const int z = p.first.second;

			std::vector<variant> current_edges;
			
			auto it = vlist.find(std::make_pair(x+1,z));
			if(it != vlist.end() && !is_xedge(x+1) && !is_solid(x+1,it->second,z)) {
				current_edges.push_back(variant_list_from_position(x+1,it->second,z));
			}
			it = vlist.find(std::make_pair(x-1,z));
			if(it != vlist.end() && !is_xedge(x-1) && !is_solid(x-1,it->second,z)) {
				current_edges.push_back(variant_list_from_position(x-1,it->second,z));
			}
			it = vlist.find(std::make_pair(x,z+1));
			if(it != vlist.end() && !is_zedge(z+1) && !is_solid(x,it->second,z+1)) {
				current_edges.push_back(variant_list_from_position(x,it->second,z+1));
			}
			it = vlist.find(std::make_pair(x,z-1));
			if(it != vlist.end() && !is_zedge(z-1) && !is_solid(x,it->second,z-1)) {
				current_edges.push_back(variant_list_from_position(x,it->second,z-1));
			}
			if(allow_diagonals) {
				it = vlist.find(std::make_pair(x+1,z+1));
				if(it != vlist.end() && !is_xedge(x+1) && !is_zedge(z+1) && !is_solid(x+1,it->second,z+1)) {
					current_edges.push_back(variant_list_from_position(x+1,it->second,z+1));
				}
				it = vlist.find(std::make_pair(x+1,z-1));
				if(it != vlist.end() && !is_xedge(x+1) && !is_zedge(z-1) && !is_solid(x+1,it->second,z-1)) {
					current_edges.push_back(variant_list_from_position(x+1,it->second,z-1));
				}
				it = vlist.find(std::make_pair(x-1,z+1));
				if(it != vlist.end() && !is_xedge(x-1) && !is_zedge(z+1) && !is_solid(x-1,it->second,z+1)) {
					current_edges.push_back(variant_list_from_position(x-1,it->second,z+1));
				}
				it = vlist.find(std::make_pair(x-1,z-1));
				if(it != vlist.end() && !is_xedge(x-1) && !is_zedge(z-1) && !is_solid(x-1,it->second,z-1)) {
					current_edges.push_back(variant_list_from_position(x-1,it->second,z-1));
				}
			}
			edges[variant_list_from_position(p.first.first, p.second, p.first.second)] = current_edges;
		}
		return pathfinding::directed_graph_ptr(new pathfinding::directed_graph(&vertex_list, &edges));
	}

	// Start definitions for voxel::logical_world
	BEGIN_DEFINE_CALLABLE_NOBASE(logical_world)
	BEGIN_DEFINE_FN(create_directed_graph, "(bool=false) ->builtin directed_graph")
		bool allow_diagonals = NUM_FN_ARGS ? FN_ARG(0).as_bool() : false;
		return variant(obj.create_directed_graph(allow_diagonals).get());
	END_DEFINE_FN

	BEGIN_DEFINE_FN(point_convert, "([decimal,decimal,decimal]) -> [int,int,int]")
		glm::ivec3 iv = obj.worldspace_to_logical(variant_to_vec3(FN_ARG(0)));
		std::vector<variant> v;
		v.push_back(variant(iv.x)); v.push_back(variant(iv.y)); v.push_back(variant(iv.z));
		return variant(&v);
	END_DEFINE_FN

	DEFINE_FIELD(x_scale, "int")
		return variant(obj.scale_x());

	DEFINE_FIELD(y_scale, "int")
		return variant(obj.scale_y());

	DEFINE_FIELD(z_scale, "int")
		return variant(obj.scale_z());

	DEFINE_FIELD(size, "[int,int,int]")
		return vec3_to_variant(glm::vec3(obj.size_x(), obj.size_y(), obj.size_z()));

	DEFINE_FIELD(size_x, "int")
		return variant(obj.size_x());

	DEFINE_FIELD(size_y, "int")
		return variant(obj.size_y());

	DEFINE_FIELD(size_z, "int")
		return variant(obj.size_z());
	END_DEFINE_CALLABLE(logical_world)

	//////////////////////////////////////////////////
	// Start definitions for voxel::world
	//////////////////////////////////////////////////
	BEGIN_DEFINE_CALLABLE_NOBASE(world)
	DEFINE_FIELD(lighting, "builtin lighting")
		return variant(obj.lighting_.get());
	DEFINE_SET_FIELD_TYPE("map")
		obj.lighting_.reset(new graphics::lighting(obj.shader_, value));

	DEFINE_FIELD(objects, "[builtin user_voxel_object]")
		std::vector<variant> v;
		for(auto o : obj.objects_) {
			v.push_back(variant(o.get()));
		}
		return variant(&v);	
	DEFINE_SET_FIELD_TYPE("[builtin user_voxel_object|map]")
		obj.objects_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_callable()) {
				user_voxel_object_ptr o = value.try_convert<user_voxel_object>();
				ASSERT_LOG(o != NULL, "Couldn't convert value to user_voxel_object.");
				obj.objects_.insert(o.get());
			} else {				
				obj.objects_.insert(new user_voxel_object(value[n]));
			}
		}
	DEFINE_FIELD(logical, "builtin logical_world")
		return variant(obj.logic_.get());

	DEFINE_FIELD(draw_primitive, "[builtin draw_primitive]")
		std::vector<variant> v;
		for(auto prim : obj.draw_primitives_) {
			v.push_back(variant(prim.get()));
		}
		return variant(&v);
	DEFINE_SET_FIELD_TYPE("[map|builtin draw_primitive]")
		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_callable()) {
				graphics::draw_primitive_ptr dp = value.try_convert<graphics::draw_primitive>();
				ASSERT_LOG(dp != NULL, "Unable to convert from callable to graphics::draw_primitive");
				obj.draw_primitives_.push_back(dp);
			} else {
				obj.draw_primitives_.push_back(graphics::draw_primitive::create(value[n]));
			}
		}
	END_DEFINE_CALLABLE(world)
}