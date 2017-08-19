/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <set>
#include <string>

#include "geometry.hpp"
#include "hex_fwd.hpp"
#include "hex_renderable_fwd.hpp"
#include "variant.hpp"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace hex
{
	struct ImageHolder 
	{
		std::string name;
		int layer;
		point base;
		point center;
		point offset;
		rect crop;
		float opacity;
		bool is_animated;
		std::vector<std::string> animation_frames;
		int animation_timing;
	};

	// Realisation of a HexTile
	class HexObject
	{
	public:
		HexObject(int x, int y, const HexTilePtr& tile, const HexMap* parent);
		void setTypeStr(const std::string& full_type, const std::string& type, const std::string& mods=std::string()) {
			full_type_str_ = full_type;
			type_str_ = type;
			mod_str_ = mods;
		}
		const point& getPosition() const { return pos_; }
		int getX() const { return pos_.x; }
		int getY() const { return pos_.y; }
		const std::string& getTypeString() const { return type_str_; }
		const std::string& getModString() const { return mod_str_; }
		const std::string& getFullTypeString() const { return full_type_str_; }
		const HexObject* getTileAt(int x, int y) const;
		const HexObject* getTileAt(const point& p) const; 
		bool hasFlag(const std::string& flag) const { return flags_.find(flag) != flags_.end() || temp_flags_.find(flag) != temp_flags_.end(); }
		void addFlag(const std::string& flag) { flags_.emplace(flag); }
		void addTempFlag(const std::string& flag) const { temp_flags_.emplace(flag); }
		void clearTempFlags() const { temp_flags_.clear(); }
		void setTempFlags() const;
		void clear();
		void addImage(const ImageHolder& holder);
		const std::vector<ImageHolder>& getImages() const { return images_; }

		const HexTilePtr& getTileType() const { return tile_; }

		const HexMap* getParent() const { return parent_; }
	private:
		const HexMap* parent_;
		point pos_;
		HexTilePtr tile_;
		std::string type_str_;
		std::string mod_str_;
		std::string full_type_str_;
		mutable std::set<std::string> flags_;
		mutable std::set<std::string> temp_flags_;
		std::vector<ImageHolder> images_;
	};

	class HexMap : public game_logic::FormulaCallable
	{
	public:
		explicit HexMap(const std::string& filename);
		explicit HexMap(const variant& v);
		~HexMap();

		void build();
		void build_single(HexObject*  obj);

		const HexObject* getTileAt(int x, int y) const ;
		const HexObject* getTileAt(const point& p) const ;
		const std::vector<HexObject>& getTiles() const { return tiles_; }
		std::vector<HexObject>& getTilesMutable() { return tiles_; }

		int getWidth() const { return width_; }
		int getHeight() const { return height_; }

		static HexMapPtr create(const std::string& filename);
		static HexMapPtr create(const variant& v);

		void setRenderable(MapNodePtr renderable) { 
			renderable_ = renderable; 
			changed_ = true; 
		}
		void process();
		void surrenderReferences(GarbageCollector* collector) override;

		variant write() const;

		void setChanged() { changed_ = true; }
		void setChangedRebuild() { rebuild_ = true; }
	private:
		DECLARE_CALLABLE(HexMap);
		void process_type_string(int x, int y, const std::string& type);
		std::string parse_type_string(const std::string& type, std::string* full_type, std::string* type_str, std::string* mod_str) const;

		HexObject* getNeighbour(point hex, int direction);

		std::vector<HexObject> tiles_;
		int x_;
		int y_;
		int width_;
		int height_;
		struct StartingPosition {
			StartingPosition(const point& p, const std::string& r) : pos(p), ref(r) {}
			point pos;
			std::string ref;
		};
		std::vector<StartingPosition> starting_positions_;
		bool changed_;
		bool rebuild_;
		MapNodePtr renderable_;
		int rx_;
		int ry_;
		std::set<int> tiles_changed_;
	};
}
