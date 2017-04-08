/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	round
#endif
/*
#include <vector>
#include "asserts.hpp"
#include "isoworld.hpp"
#include "level.hpp"
#include "profile_timer.hpp"
#include "user_voxel_object.hpp"
#include "variant_utils.hpp"
#include "voxel_object.hpp"
#include "wml_formula_callable.hpp"

namespace voxel
{
	const int chunk_size = 32;
	const int initial_chunks = 16;

	const int default_view_distance = 5;

	LogicalWorld::LogicalWorld(const variant& node)
		:size_x_(0), size_y_(0), size_z_(0), 
		scale_x_(node["scale_x"].as_int(1)), scale_y_(node["scale_y"].as_int(1)), scale_z_(node["scale_z"].as_int(1)),
		chunks_(node)
	{
		//this code wouldn't work anymore. Change it if we ever resurrect this.
		//READ_SERIALIZABLE_CALLABLE(node);
		ASSERT_LOG(node.has_key("chunks"), "To create a logic world must have 'chunks' attribute");

		int min_x, min_y, min_z;
		int max_x, max_y, max_z;
		min_x = min_y = min_z = std::numeric_limits<int>::max();
		max_x = max_y = max_z = std::numeric_limits<int>::min();

		for(int n = 0; n != node["chunks"].num_elements(); ++n) {
			const int wpx = node["chunks"][n]["getWorldspacePosition"][0].as_int();
			const int wpy = node["chunks"][n]["getWorldspacePosition"][1].as_int();
			const int wpz = node["chunks"][n]["getWorldspacePosition"][2].as_int();

			for(auto& p : node["chunks"][n]["voxels"].as_map()) {
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

	LogicalWorld::~LogicalWorld()
	{
	}

	glm::ivec3 LogicalWorld::worldspaceToLogical(const glm::vec3& wsp) const
	{
		// XXX fixme
		glm::ivec3 voxel_coord = glm::ivec3(
			abs(wsp[0]-bmround(wsp[0])) < 0.05f ? int(bmround(wsp[0])) : int(floor(wsp[0])),
			abs(wsp[1]-bmround(wsp[1])) < 0.05f ? int(bmround(wsp[1])) : int(floor(wsp[1])),
			abs(wsp[2]-bmround(wsp[2])) < 0.05f ? int(bmround(wsp[2])) : int(floor(wsp[2])));
		glm::ivec3 facing = Level::current().camera()->get_facing(wsp);
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
		return glm::ivec3();
	}


	World::World(const variant& node)
		: view_distance_(node["view_distance"].as_int(default_view_distance)), 
		seed_(node["seed"].as_int(0))
	{
		if(node.has_key("objects")) {
			for(int n = 0; n != node["objects"].num_elements(); ++n) {
				add_object(new user_voxel_object(node["objects"][n]));
			}
		}

		if(node.has_key("skybox")) {
			skybox_.reset(new graphics::skybox(node["skybox"]));
		}

		if(node.has_key("chunks")) {
			logic_.reset(new LogicalWorld(node));
			build_fixed(node["chunks"]);
		} else {
			buildInfinite();
		}
	}

	world::~world()
	{
	}

	void world::setTile(int x, int y, int z, const variant& type)
	{
		int fx = int(floor(x));
		int fy = int(floor(y));
		int fz = int(floor(z));
		auto it = chunks_.find(position(fx, fy, fz));
		if(it != chunks_.end()) {
			it->second->setTile(x-fx, y-fy, z-fz, type);
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

	variant world::get_TileType(int x, int y, int z) const
	{
		int fx = int(floor(x));
		int fy = int(floor(y));
		int fz = int(floor(z));
		auto it = chunks_.find(position(fx, fy, fz));
		if(it != chunks_.end()) {
			return it->second->get_TileType(x-fx, y-fy, z-fz);
		}
		return variant();
	}

	void world::add_object(UserVoxelObjectPtr obj)
	{
		objects_.insert(obj);
	}

	void world::remove_object(UserVoxelObjectPtr obj)
	{
		auto it = objects_.find(obj);
		ASSERT_LOG(it != objects_.end(), "Unable to remove object '" << obj->type() << "' from level");
		objects_.erase(it);
	}

	void world::get_objects_at_point(const glm::vec3& pt, std::vector<UserVoxelObjectPtr>& obj_list)
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
			int wpx = node[n]["getWorldspacePosition"][0].as_int() * logic_->scale_x();
			int wpy = node[n]["getWorldspacePosition"][1].as_int() * logic_->scale_y();
			int wpz = node[n]["getWorldspacePosition"][2].as_int() * logic_->scale_z();
			chunks_[position(wpx,wpy,wpz)] = cp;
		}
	}

	void world::buildInfinite()
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
					m[variant("getWorldspacePosition")] = variant(&v);
					m[variant("random")] = rnd.build();

					chunk_ptr cp = voxel::chunk_factory::create(shader_, LogicalWorldPtr(), variant(&m));
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

		// skybox should be drawn last
		if(skybox_) {
			skybox_->draw(lighting_, camera);
		}

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

		for(auto prim : DrawPrimitives_) {
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
			wsp.add("getWorldspacePosition", chnk.first.x);
			wsp.add("getWorldspacePosition", chnk.first.y);
			wsp.add("getWorldspacePosition", chnk.first.z);
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

	REGISTER_SERIALIZABLE_CALLABLE(LogicalWorld, "@LogicalWorld");

	variant LogicalWorld::serializeToWml() const
	{
		variant v(chunks_);
		v.add_attr(variant("@LogicalWorld"), variant("LogicalWorld"));
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

	bool LogicalWorld::is_xedge(int x) const
	{
		if(x >= 0 && x < size_x()) {
			return false;
		}
		return true;
	}

	bool LogicalWorld::is_yedge(int y) const
	{
		if(y >= 0 && y < size_y()) {
			return false;
		}
		return true;
	}

	bool LogicalWorld::is_zedge(int z) const
	{
		if(z >= 0 && z < size_z()) {
			return false;
		}
		return true;
	}

	bool LogicalWorld::isSolid(int x, int y, int z) const
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

	pathfinding::directed_graph_ptr LogicalWorld::createDirectedGraph(bool allow_diagonals) const
	{
		profile::manager pman("LogicalWorld::createDirectedGraph");

		std::vector<variant> vertex_list;
		std::map<std::pair<int,int>, int> vlist;

		for(auto p : heightmap_) {
			int x = p.first.first;
			int y = p.second;
			int z = p.first.second;

			if(y < size_y() - 1) {
				if(isSolid(x, y+1, z) == false) {
					vertex_list.push_back(variant_list_from_position(x,y+1,z));
					vlist[std::make_pair(x,z)] = y+1;
				}
			} else {
				vertex_list.push_back(variant_list_from_position(x,y+1,z));
				vlist[std::make_pair(x,z)] = y+1;
			}
		}
	
		pathfinding::graph_edge_list edges;
		for(auto p : vertex_list) {
			const int x = p[0].as_int();
			const int y = p[1].as_int();
			const int z = p[2].as_int();

			std::vector<variant> current_edges;
			
			auto it = vlist.find(std::make_pair(x+1,z));
			if(it != vlist.end() && !is_xedge(x+1) && !isSolid(x+1,it->second,z)) {
				current_edges.push_back(variant_list_from_position(x+1,it->second,z));
			}
			it = vlist.find(std::make_pair(x-1,z));
			if(it != vlist.end() && !is_xedge(x-1) && !isSolid(x-1,it->second,z)) {
				current_edges.push_back(variant_list_from_position(x-1,it->second,z));
			}
			it = vlist.find(std::make_pair(x,z+1));
			if(it != vlist.end() && !is_zedge(z+1) && !isSolid(x,it->second,z+1)) {
				current_edges.push_back(variant_list_from_position(x,it->second,z+1));
			}
			it = vlist.find(std::make_pair(x,z-1));
			if(it != vlist.end() && !is_zedge(z-1) && !isSolid(x,it->second,z-1)) {
				current_edges.push_back(variant_list_from_position(x,it->second,z-1));
			}
			if(allow_diagonals) {
				it = vlist.find(std::make_pair(x+1,z+1));
				if(it != vlist.end() && !is_xedge(x+1) && !is_zedge(z+1) && !isSolid(x+1,it->second,z+1)) {
					current_edges.push_back(variant_list_from_position(x+1,it->second,z+1));
				}
				it = vlist.find(std::make_pair(x+1,z-1));
				if(it != vlist.end() && !is_xedge(x+1) && !is_zedge(z-1) && !isSolid(x+1,it->second,z-1)) {
					current_edges.push_back(variant_list_from_position(x+1,it->second,z-1));
				}
				it = vlist.find(std::make_pair(x-1,z+1));
				if(it != vlist.end() && !is_xedge(x-1) && !is_zedge(z+1) && !isSolid(x-1,it->second,z+1)) {
					current_edges.push_back(variant_list_from_position(x-1,it->second,z+1));
				}
				it = vlist.find(std::make_pair(x-1,z-1));
				if(it != vlist.end() && !is_xedge(x-1) && !is_zedge(z-1) && !isSolid(x-1,it->second,z-1)) {
					current_edges.push_back(variant_list_from_position(x-1,it->second,z-1));
				}
			}
			edges[variant_list_from_position(x, y, z)] = current_edges;
		}
		return pathfinding::directed_graph_ptr(new pathfinding::directed_graph(&vertex_list, &edges));
	}

	class create_world_callable : public game_logic::FormulaCallable 
	{
		variant world_;
		variant getValue(const std::string& key) const {
			return variant();
		}
		virtual void execute(game_logic::FormulaCallable& ob) const {
			level::current().iso_world().reset(new world(world_));
		}
	public:
		explicit create_world_callable(const variant& world) 
			: world_(world)
		{}
	};

	// Start definitions for voxel::LogicalWorld
	BEGIN_DEFINE_CALLABLE_NOBASE(LogicalWorld)
	BEGIN_DEFINE_FN(createDirectedGraph, "(bool=false) ->builtin directed_graph")
		bool allow_diagonals = NUM_FN_ARGS ? FN_ARG(0).as_bool() : false;
		return variant(obj.createDirectedGraph(allow_diagonals).get());
	END_DEFINE_FN

	BEGIN_DEFINE_FN(point_convert, "([decimal,decimal,decimal]) -> [int,int,int]")
		glm::ivec3 iv = obj.worldspaceToLogical(variant_to_vec3(FN_ARG(0)));
		std::vector<variant> v;
		v.push_back(variant(iv.x)); v.push_back(variant(iv.y)); v.push_back(variant(iv.z));
		return variant(&v);
	END_DEFINE_FN

	BEGIN_DEFINE_FN(get_height_at_point, "(int,int) -> int|null")
		int xx = FN_ARG(0).as_int();
		int yy = FN_ARG(1).as_int();
		auto it = obj.heightmap_.find(std::make_pair(xx,yy));
		if(it == obj.heightmap_.end()) {
			return variant();
		}
		return variant(it->second);
	END_DEFINE_FN

	BEGIN_DEFINE_FN(create_world, "() -> commands")
		return variant(new create_world_callable(obj.chunks_));
	END_DEFINE_FN

	DEFINE_FIELD(scale, "[int,int,int]")
		return ivec3_to_variant(glm::ivec3(obj.scale_x(), obj.scale_y(), obj.scale_z()));

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
	END_DEFINE_CALLABLE(LogicalWorld)

	//////////////////////////////////////////////////
	// Start definitions for voxel::world
	//////////////////////////////////////////////////
	BEGIN_DEFINE_CALLABLE_NOBASE(world)
	DEFINE_FIELD(lighting, "builtin lighting")
		return variant(obj.lighting_.get());
	DEFINE_SET_FIELD_TYPE("map")
		obj.lighting_.reset(new graphics::lighting(obj.shader_, value));

	DEFINE_FIELD(lighting, "builtin skybox")
		return variant(obj.skybox_.get());
	DEFINE_SET_FIELD_TYPE("map")
		obj.skybox_.reset(new graphics::skybox(value));

	DEFINE_FIELD(objects, "[builtin voxel_object]")
		std::vector<variant> v;
		for(auto o : obj.objects_) {
			v.push_back(variant(o.get()));
		}
		return variant(&v);	
	DEFINE_SET_FIELD_TYPE("[builtin voxel_object|map]")
		obj.objects_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_callable()) {
				UserVoxelObjectPtr o = value.try_convert<user_voxel_object>();
				ASSERT_LOG(o != nullptr, "Couldn't convert value to user_voxel_object.");
				obj.objects_.insert(o.get());
			} else {				
				obj.objects_.insert(new user_voxel_object(value[n]));
			}
		}
	DEFINE_FIELD(logical, "builtin logical_world")
		return variant(obj.logic_.get());

	DEFINE_FIELD(draw_primitive, "[builtin draw_primitive]")
		std::vector<variant> v;
		for(auto prim : obj.DrawPrimitives_) {
			v.push_back(variant(prim.get()));
		}
		return variant(&v);
	DEFINE_SET_FIELD_TYPE("[map|builtin draw_primitive]")
		obj.DrawPrimitives_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			obj.DrawPrimitives_.push_back(graphics::DrawPrimitive::create(value[n]));
		}
	END_DEFINE_CALLABLE(world)
}
*/
