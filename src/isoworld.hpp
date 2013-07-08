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

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "geometry.hpp"
#include "graphics.hpp"
#include "isochunk.hpp"
#include "lighting.hpp"
#include "raster.hpp"
#include "shaders.hpp"
#include "variant.hpp"

namespace voxel
{
	class voxel_object;
	typedef boost::intrusive_ptr<voxel_object> voxel_object_ptr;
	typedef boost::intrusive_ptr<const voxel_object> const_voxel_object_ptr;

	class world : public game_logic::formula_callable
	{
	public:
		explicit world(const variant& node);
		virtual ~world();
		
		gles2::program_ptr shader() { return shader_; }

		void set_tile(int x, int y, int z, const variant& type);
		void del_tile(int x, int y, int z);
		variant get_tile_type(int x, int y, int z) const;

		void build();
		void draw(const camera_callable_ptr& camera) const;
		variant write();
		void process();

		void add_object(voxel::voxel_object_ptr obj);
		void remove_object(voxel::voxel_object_ptr obj);
	protected:
	private:
		DECLARE_CALLABLE(world);
		gles2::program_ptr shader_;

		graphics::lighting_ptr lighting_;

		int view_distance_;

		uint32_t seed_;

		std::vector<chunk_ptr> active_chunks_;
		boost::unordered_map<position, chunk_ptr> chunks_;

		std::set<voxel_object_ptr> objects_;
		
		void get_active_chunks();

		world();
		world(const world&);
	};

	typedef boost::intrusive_ptr<world> world_ptr;
}

#endif
