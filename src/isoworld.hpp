#pragma once

#if defined(USE_ISOMAP)
#if !defined(USE_GLES2)
#error in order to build with Iso tiles you need to be building with shaders (USE_GLES2)
#endif

#include <boost/intrusive_ptr.hpp>
#include <boost/unordered_map.hpp>

#include <vector>
#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "draw_primitive.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "geometry.hpp"
#include "graphics.hpp"
#include "isochunk.hpp"
#include "lighting.hpp"
#include "raster.hpp"
#include "shaders.hpp"
#include "skybox.hpp"
#include "variant.hpp"
#include "wml_formula_callable.hpp"

namespace voxel
{
	class user_voxel_object;
	typedef boost::intrusive_ptr<user_voxel_object> user_voxel_object_ptr;

	class logical_world : public game_logic::wml_serializable_formula_callable
	{
	public:
		explicit logical_world(const variant& node);
		virtual ~logical_world();

		pathfinding::directed_graph_ptr create_directed_graph(bool allow_diagonals=false) const;

		bool is_solid(int x, int y, int z) const;

		bool is_xedge(int x) const;
		bool is_yedge(int y) const;
		bool is_zedge(int z) const;
		
		size_t size_x() const { return size_x_; }
		size_t size_y() const { return size_y_; }
		size_t size_z() const { return size_z_; }

		size_t scale_x() const { return scale_x_; }
		size_t scale_y() const { return scale_y_; }
		size_t scale_z() const { return scale_z_; }

		glm::ivec3 worldspace_to_logical(const glm::vec3& wsp) const;
	private:
		DECLARE_CALLABLE(logical_world);

		variant serialize_to_wml() const;

		boost::unordered_map<std::pair<int,int>, int> heightmap_;
		// Only valid for fixed size worlds
		int size_x_;
		int size_y_;
		int size_z_;

		variant chunks_;

		// Voxels per game logic sqaure
		size_t scale_x_;
		size_t scale_y_;
		size_t scale_z_;

		logical_world();
		logical_world(const logical_world&);
	};

	typedef boost::intrusive_ptr<logical_world> logical_world_ptr;

	class world : public game_logic::formula_callable
	{
	public:
		explicit world(const variant& node);
		virtual ~world();
		
		gles2::program_ptr shader() { return shader_; }

		void set_tile(int x, int y, int z, const variant& type);
		void del_tile(int x, int y, int z);
		variant get_tile_type(int x, int y, int z) const;

		void build_infinite();
		void build_fixed(const variant& node);
		void draw(const camera_callable_ptr& camera) const;
		variant write();
		void process();

		void add_object(user_voxel_object_ptr obj);
		void remove_object(user_voxel_object_ptr obj);

		void get_objects_at_point(const glm::vec3& pt, std::vector<user_voxel_object_ptr>& obj);
		std::set<user_voxel_object_ptr>& get_objects() { return objects_; }
	protected:
	private:
		DECLARE_CALLABLE(world);
		gles2::program_ptr shader_;

		graphics::lighting_ptr lighting_;

		graphics::skybox_ptr skybox_;

		int view_distance_;

		uint32_t seed_;

		std::vector<chunk_ptr> active_chunks_;
		boost::unordered_map<position, chunk_ptr> chunks_;

		std::set<user_voxel_object_ptr> objects_;

		std::vector<graphics::draw_primitive_ptr> draw_primitives_;

		logical_world_ptr logic_;
		
		void get_active_chunks();

		world();
		world(const world&);
	};

	typedef boost::intrusive_ptr<world> world_ptr;
}

#endif
