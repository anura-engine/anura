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
#include <vector>

#include "geometry.hpp"
#include "Texture.hpp"

#include "Color.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "SceneObjectCallable.hpp"
#include "variant.hpp"

namespace voxel
{
	struct ChunkPosition
	{
		ChunkPosition(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}
		int x, y, z;
	};
	bool operator==(ChunkPosition const& p1, ChunkPosition const& p2);
	std::size_t hash_value(ChunkPosition const& p);
	struct chunk_hasher
	{
		size_t operator()(ChunkPosition const& p) {
			return hash_value(p);
		}
	};

	struct TexturedTileEditorInfo
	{
		std::string name;
		std::string group;
		variant id;
		KRE::TexturePtr tex;
		rect area;
	};

	struct ColoredTileEditorInfo
	{
		std::string name;
		std::string group;
		variant id;
		KRE::Color color;
	};

	class LogicalWorld;
	typedef ffl::IntrusivePtr<LogicalWorld> LogicalWorldPtr;

	class Chunk : public graphics::SceneObjectCallable
	{
	public:
		enum {
			FRONT	= 1,
			RIGHT	= 2,
			TOP		= 4,
			BACK	= 8,
			LEFT	= 16,
			BOTTOM	= 32,
		};

		Chunk();
		explicit Chunk(LogicalWorldPtr logic, const variant& node);
		virtual ~Chunk();
		
		void init();
		void build();
		void draw() const;
		variant write();

		virtual bool isSolid(int x, int y, int z) const = 0;
		virtual variant getTileType(int x, int y, int z) const = 0;

		void setTile(int x, int y, int z, const variant& type);
		void delTile(int x, int y, int z);

		virtual bool isTextured() const = 0;
		int sizeX() const { return size_x_; }
		int sizeY() const { return size_y_; }
		int sizeZ() const { return size_z_; }
		void setSize(int mx, int my, int mz);

		size_t scaleX() const { return scale_x_; }
		size_t scaleY() const { return scale_y_; }
		size_t scaleZ() const { return scale_z_; }

		static variant getTileInfo(const std::string& type);
		static const std::vector<TexturedTileEditorInfo>& getTexturedEditorTiles();
		static const std::vector<ColoredTileEditorInfo>& getColoredEditorTiles();
	protected:
		enum {
			FRONT_FACE,
			RIGHT_FACE,
			TOP_FACE,
			BACK_FACE,
			LEFT_FACE,
			BOTTOM_FACE,
			MAX_FACES,
		};

		virtual void handleBuild() = 0;
		virtual void handleDraw() const = 0;
		virtual void handleSetTile(int x, int y, int z, const variant& type) = 0;
		virtual void handleDelTile(int x, int y, int z) = 0;
		virtual variant handleWrite() = 0;

		const glm::vec3& getWorldspacePosition() const {return getWorldspacePosition_; }
	private:
		DECLARE_CALLABLE(Chunk);

		// Vertex array data for the chunk
		std::vector<std::vector<float>> varray_;
		// Vertex attribute offsets
		std::vector<size_t> vattrib_offsets_;
		// Number of vertices to be drawn.
		std::vector<size_t> num_vertices_;

		int size_x_;
		int size_y_;
		int size_z_;

		size_t scale_x_;
		size_t scale_y_;
		size_t scale_z_;

		glm::vec3 getWorldspacePosition_;
	};

	class ChunkColored : public Chunk
	{
	public:
		ChunkColored();
		explicit ChunkColored(LogicalWorldPtr logic, const variant& node);
		virtual ~ChunkColored();
		bool isSolid(int x, int y, int z) const override;
		variant getTileType(int x, int y, int z) const override;

		virtual bool isTextured() const override { return false; }
	private:
		void handleBuild() override;
		void handleDraw() const override;
		variant handleWrite() override;
		void handleSetTile(int x, int y, int z, const variant& type) override;
		void handleDelTile(int x, int y, int z) override;

		void addFaceLeft(float x, float y, float z, float size, const variant& col);
		void addFaceRight(float x, float y, float z, float size, const variant& col);
		void addFaceFront(float x, float y, float z, float size, const variant& col);
		void addFaceBack(float x, float y, float z, float size, const variant& col);
		void addFaceTop(float x, float y, float z, float size, const variant& col);
		void addFaceBottom(float x, float y, float z, float size, const variant& col);

		void addColorAarrayData(int face, const KRE::Color& color, std::vector<uint8_t>& carray);

		std::vector<std::vector<uint8_t> > carray_;
		std::vector<size_t> cattrib_offsets_;
		std::unordered_map<ChunkPosition, variant, chunk_hasher> tiles_;
	};

	class ChunkTextured : public Chunk
	{
	public:
		ChunkTextured();
		explicit ChunkTextured(LogicalWorldPtr logic, const variant& node);
		virtual ~ChunkTextured();
		bool isSolid(int x, int y, int z) const;
		variant getTileType(int x, int y, int z) const;
		virtual bool isTextured() const override { return true; }
	private:
		void handleBuild() override;
		void handleDraw() const override;
		variant handleWrite() override;
		void handleSetTile(int x, int y, int z, const variant& type) override;
		void handleDelTile(int x, int y, int z) override;

		void addFaceLeft(float x, float y, float z, float size, const std::string& bid);
		void addFaceRight(float x, float y, float z, float size, const std::string& bid);
		void addFaceFront(float x, float y, float z, float size, const std::string& bid);
		void addFaceBack(float x, float y, float z, float size, const std::string& bid);
		void addFaceTop(float x, float y, float z, float size, const std::string& bid);
		void addFaceBottom(float x, float y, float z, float size, const std::string& bid);

		void addTextureArrayData(int face, const rectf& area, std::vector<float>& tarray);

		std::vector<std::vector<float>> tarray_;
		std::vector<size_t> tattrib_offsets_;
		std::unordered_map<ChunkPosition, std::string, chunk_hasher> tiles_;
	};

	typedef ffl::IntrusivePtr<Chunk> ChunkPtr;
	typedef ffl::IntrusivePtr<const Chunk> ConstChunkPtr;

	namespace chunk_factory 
	{
		ChunkPtr create(LogicalWorldPtr logic, const variant& v);
	}
}
*/