/*
	Copyright (C) 2003-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#pragma once
/*
#include <unordered_map>

#include <vector>
#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "draw_primitive_fwd.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "geometry.hpp"
#include "isochunk.hpp"
#include "nocopy.hpp"
#include "pathfinding.hpp"
#include "skybox.hpp"
#include "variant.hpp"
#include "wml_formula_callable.hpp"

namespace voxel
{
	class user_voxel_object;
	typedef ffl::IntrusivePtr<user_voxel_object> UserVoxelObjectPtr;

	template <class T>
	inline void hash_combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	struct pair_hash
	{
		inline size_t operator()(const std::pair<int, int> & v) const
		{
			size_t seed = 0;
			voxel::hash_combine(seed, v.first);
			voxel::hash_combine(seed, v.second);
			return seed;
		}
	};

	class LogicalWorld : public game_logic::WmlSerializableFormulaCallable
	{
	public:
		explicit LogicalWorld(const variant& node);
		virtual ~LogicalWorld();

		pathfinding::directed_graph_ptr createDirectedGraph(bool allow_diagonals=false) const;

		bool isSolid(int x, int y, int z) const;

		bool isXEdge(int x) const;
		bool isyEedge(int y) const;
		bool isZEdge(int z) const;
		
		size_t sizeX() const { return size_x_; }
		size_t sizeY() const { return size_y_; }
		size_t sizeZ() const { return size_z_; }

		size_t scaleX() const { return scale_x_; }
		size_t scaleY() const { return scale_y_; }
		size_t scaleZ() const { return scale_z_; }

		glm::ivec3 worldspaceToLogical(const glm::vec3& wsp) const;
	private:
		DECLARE_CALLABLE(LogicalWorld);

		variant serializeToWml() const;

		std::unordered_map<std::pair<int,int>, int, pair_hash> heightmap_;
		// Only valid for fixed size worlds
		int size_x_;
		int size_y_;
		int size_z_;

		variant chunks_;

		// Voxels per game logic sqaure
		size_t scale_x_;
		size_t scale_y_;
		size_t scale_z_;

		LogicalWorld();
		LogicalWorld(const LogicalWorld&);
	};

	typedef ffl::IntrusivePtr<LogicalWorld> LogicalWorldPtr;

	class World : public game_logic::FormulaCallable
	{
	public:
		explicit World(const variant& node);
		virtual ~World();

		void setTile(int x, int y, int z, const variant& type);
		void delTile(int x, int y, int z);
		variant getTileType(int x, int y, int z) const;

		void buildInfinite();
		void buildFixed(const variant& node);
		void draw() const;
		variant write();
		void process();

		void addObject(UserVoxelObjectPtr obj);
		void removeObject(UserVoxelObjectPtr obj);

		void getObjectsAtPoint(const glm::vec3& pt, std::vector<UserVoxelObjectPtr>& obj);
		std::set<UserVoxelObjectPtr>& getObjects() { return objects_; }
	private:
		DISALLOW_COPY_ASSIGN_AND_DEFAULT(World);
		DECLARE_CALLABLE(World);

		graphics::skybox_ptr skybox_;

		int view_distance_;

		uint32_t seed_;

		std::vector<ChunkPtr> active_chunks_;
		std::unordered_map<ChunkPosition, ChunkPtr, chunk_hasher> chunks_;

		std::set<UserVoxelObjectPtr> objects_;

		std::vector<graphics::DrawPrimitivePtr> DrawPrimitives_;

		LogicalWorldPtr logic_;
		
		void get_active_chunks();
	};

	typedef ffl::IntrusivePtr<World> WorldPtr;
}
*/
