/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <array>
#include <cstdint>
#include <memory>

#include "AttributeSet.hpp"
#include "Color.hpp"
#include "DisplayDeviceFwd.hpp"
#include "SceneNode.hpp"
#include "SceneObject.hpp"
#include "Texture.hpp"

namespace tiled
{
	enum class Orientation {
		ORTHOGONAL,
		ISOMETRIC,
		STAGGERED,
		HEXAGONAL
	};

	enum class RenderOrder {
		RIGHT_DOWN,
		RIGHT_UP,
		LEFT_DOWN,
		LEFT_UP,
	};

	enum class StaggerIndex {
		EVEN,
		ODD,
	};

	enum class StaggerDirection {
		ROWS,
		COLS,
	};

	enum class ImageFormat {
		NONE,
		PNG,
		GIF,
		BMP,
		JPEG
	};

	class Map;
	typedef std::shared_ptr<Map> MapPtr;
	class TileDefinition;
	class TileSet;

	struct Property
	{
		explicit Property(const std::string& n, const std::string& v) : name(n), value(v) {}
		std::string name;
		std::string value;
	};
	
	struct Terrain
	{
		explicit Terrain(const std::string& n, uint32_t id) : name(n), tile_id(id) {}
		std::string name;
		uint32_t tile_id;
	};

	class ObjectGroup
	{
	public:
		ObjectGroup();
	private:
	};

	class Tile
	{
	public:
		Tile(int gid, KRE::TexturePtr tex);
		
		void setFlipFlags(bool h, bool v, bool d) { 
			flipped_horizontally_ = h; 
			flipped_vertically_ = v; 
			flipped_diagonally_ = d; 
		}

		void setDestRect(const rect& dst) { dest_rect_ = dst; }
		void setSrcRect(const rect& src) { src_rect_ = src; }

		const rect& getSrcRect() const { return src_rect_; }
		const rect& getDestRect() const { return dest_rect_; }

		KRE::TexturePtr getTexture() const { return texture_; }

		int gid() const { return global_id_; }

		void draw(std::vector<KRE::vertex_texcoord>* tiles) const;
	private:
		int global_id_;
		rect dest_rect_;
		KRE::TexturePtr texture_;
		rect src_rect_;
		bool flipped_horizontally_;
		bool flipped_vertically_;
		bool flipped_diagonally_;
	};
	typedef std::shared_ptr<Tile> TilePtr;
	typedef std::weak_ptr<Tile> WeakTilePtr;

	class Layer : public KRE::SceneObject
	{
	public:
		explicit Layer(MapPtr parent, const std::string& name);
		void setProperties(std::vector<Property>* props) { properties_.swap(*props); }
		void setOpacity(float o) { opacity_ = o; }
		void setVisibility(bool visible) { is_visible_ = visible; }
		void addTile(TilePtr t);
		void preRender(const KRE::WindowPtr& wnd) override;
		MapPtr getParentMap() const;
	private:
		void drawIsometic(RenderOrder render_order, std::vector<KRE::vertex_texcoord>* tiles) const;
		void drawStaggered(RenderOrder render_order, std::vector<KRE::vertex_texcoord>* tiles) const;
		void drawOrthogonal(RenderOrder render_order, std::vector<KRE::vertex_texcoord>* tiles) const;
		void drawHexagonal(RenderOrder render_order, std::vector<KRE::vertex_texcoord>* tiles) const;
		std::string name_;
		int width_;
		int height_;
		std::vector<Property> properties_;
		std::vector<std::vector<TilePtr>> tiles_;
		float opacity_;
		bool is_visible_;
		int add_x_;
		int add_y_;
		bool tiles_changed_;
		
		std::weak_ptr<Map> parent_map_;

		std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> attr_;
	};

	class TileImage
	{
	public:
		TileImage();
		void setSource(const std::string& source) { source_ = source; }
		void setImageData(ImageFormat fmt, const std::vector<char>& data) { format_ = fmt; data_ = data; }
		explicit TileImage(const std::string& source);
		void setTransparentColor(const KRE::Color& color) { transparent_color_ = color; has_transparent_color_set_ = true; }
		void setWidth(int w) { width_ = w; }
		void setHeight(int h) { height_ = h; }
		KRE::TexturePtr getTexture() const;
		int getWidth() const { return width_; }
		int getHeight() const { return height_; }
	private:
		ImageFormat format_;
		std::vector<char> data_;
		std::string source_;
		bool has_transparent_color_set_;
		KRE::Color transparent_color_;
		int width_;
		int height_;
	};

	class TileDefinition
	{
	public:
		explicit TileDefinition(uint32_t local_id);
		void addImage(const TileImage& image);
		void setProperties(std::vector<Property>* props) { properties_.swap(*props); }
		void setProbability(float p) { probability_ = p; }
		void setTerrain(const std::array<int, 4>& t) { terrain_ = t; }
		
		int getLocalId() const { return local_id_; }
		KRE::TexturePtr getTexture() const { return texture_; }
		void setTexture(KRE::TexturePtr tex) { texture_ = tex; }
	private:
		TileDefinition() = delete;
		uint32_t local_id_;
		std::array<int, 4> terrain_;
		float probability_;
		std::vector<Property> properties_;
		std::vector<ObjectGroup> object_group_;
		KRE::TexturePtr texture_;
	};

	class TileSet
	{
	public:
		explicit TileSet(int first_gid);
		
		void setName(const std::string& name) { name_ = name; }
		void setTileDimensions(int width, int height) { tile_width_ = width; tile_height_ = height; }
		void setSpacing(int spacing) { spacing_ = spacing; }
		void setMargin(int margin) { margin_ = margin; }
		void setTileOffset(int x, int y) { tile_offset_x_ = x; tile_offset_y_ = y; }
		void setImage(const TileImage& image);
		void setTerrainTypes(const std::vector<Terrain>& tt) { terrain_types_ = tt; }
		void setProperties(std::vector<Property>* props) { properties_.swap(*props); }
		void addTile(const TileDefinition& t) { tiles_.emplace_back(t); }

		int getFirstId() const { return first_gid_; }
		const TileDefinition* getTileDefinition(int local_id) const;

		int getTileWidth() const { return tile_width_; }
		int getTileHeight() const { return tile_height_; }
		int getTileOffsetX() const { return tile_offset_x_; }
		int getTileOffsetY() const { return tile_offset_y_; }

		rect getImageRect(int local_id) const;
		KRE::TexturePtr getTexture() const { return texture_; }
	private:
		TileSet() = delete;

		int first_gid_;
		std::string name_;
		int tile_width_;
		int tile_height_;
		int spacing_;
		int margin_;
		int tile_offset_x_;
		int tile_offset_y_;
		std::vector<Property> properties_;
		std::vector<Terrain> terrain_types_;
		std::vector<TileDefinition> tiles_;
		KRE::TexturePtr texture_;
		int image_width_;
		int image_height_;
	};

	class Map : public KRE::SceneNode
	{
	public:
		explicit Map(std::weak_ptr<KRE::SceneGraph> sg, const variant& node);
		static MapPtr create(std::weak_ptr<KRE::SceneGraph> sg, const variant& node);

		void setDimensions(int w, int h) { width_ = w; height_ = h; }
		void setTileDimensions(int w, int h) { tile_width_ = w; tile_height_ = h; }
		void setOrientation(Orientation o) { orientation_ = o; }
		void setRenderOrder(RenderOrder ro) { render_order_ = ro; }
		void setStaggerIndex(StaggerIndex si) { stagger_index_ = si; }
		void setStaggerDirection(StaggerDirection sd) { stagger_direction_ = sd; }
		void setHexsideLength(int length) { hexside_length_ = length; }
		void setBackgroundColor(const KRE::Color& color) { background_color_ = color; }
		void setProperties(std::vector<Property>* props) { properties_.swap(*props); }
		void addLayer(std::shared_ptr<Layer> layer) { layers_.emplace_back(layer); }
		void addTileSet(const TileSet& ts) { tile_sets_.emplace_back(ts); }

		Orientation getOrientation() const { return orientation_; }
		RenderOrder getRenderOrder() const { return render_order_; }

		int getTileWidth() const { return tile_width_; }
		int getTileHeight() const { return tile_height_; }

		int getWidth() const { return width_; }
		int getHeight() const { return height_; }

		point getPixelPos(int x, int y) const;
		TilePtr createTileInstance(int x, int y, int tile_gid);
	private:
		int width_;
		int height_;
		int tile_width_;
		int tile_height_;
		Orientation orientation_;
		RenderOrder render_order_;
		StaggerIndex stagger_index_;
		StaggerDirection stagger_direction_;
		int hexside_length_;
		KRE::Color background_color_;

		std::vector<TileSet> tile_sets_;
		std::vector<Property> properties_;
		std::vector<std::shared_ptr<Layer>> layers_;

		MapPtr get_this_pointer();
		void init(const variant& node);
		void notifyNodeAttached(std::weak_ptr<SceneNode> parent) override;
	};
}
