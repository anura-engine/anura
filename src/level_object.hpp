/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <vector>

#include "Canvas.hpp"
#include "Texture.hpp"

#include "Color.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "level_object_fwd.hpp"
#include "variant.hpp"

struct LevelTile 
{
	LevelTile() : x(0), y(0), layer_from(0), zorder(0), object(nullptr), face_right(false), draw_disabled(false) {}
	bool isSolid(int x, int y) const;
	int x, y;
	int layer_from; //which zorder layer causes this tile to be built?
	int zorder;
	const LevelObject* object;
	bool face_right;
	bool draw_disabled;
};

struct level_tile_zorder_comparer 
{
	bool operator()(const LevelTile& a, const LevelTile& b) const {
		return a.zorder < b.zorder;
	}

	bool operator()(const LevelTile& a, int b) const {
		return a.zorder < b;
	}

	bool operator()(int a, const LevelTile& b) const {
		return a < b.zorder;
	}
};

struct level_tile_pos_comparer 
{
	bool operator()(const LevelTile& a, const LevelTile& b) const {
		return a.y < b.y || (a.y == b.y && a.x < b.x);
	}

	bool operator()(const LevelTile& a, const std::pair<int, int>& b) const {
		return a.y < b.second || (a.y == b.second && a.x < b.first);
	}

	bool operator()(const std::pair<int, int>& a, const LevelTile& b) const {
		return a.second < b.y || (a.second == b.y && a.first < b.x);
	}
};

struct level_tile_zorder_pos_comparer 
{
	bool operator()(const LevelTile& a, const LevelTile& b) const {
		return a.zorder < b.zorder || (a.zorder == b.zorder && a.y < b.y) || (a.zorder == b.zorder && a.y == b.y && a.x < b.x);
	}
};

struct level_tile_y_pos_comparer 
{
        bool operator()(const LevelTile& a, int b) const {
                return a.y < b;
        }
 
        bool operator()(int a, const LevelTile& b) const {
                return a < b.y;
        }
 
        bool operator()(const LevelTile& a, const LevelTile& b) const {
                return a.y < b.y;
        }
};

//utility which sets the palette for objects loaded within a scope
struct palette_scope 
{
	explicit palette_scope(const std::vector<std::string>& v);
	~palette_scope();	

	unsigned int original_value;
};

class LevelObject : public game_logic::FormulaCallable 
{
public:
	static std::vector<ConstLevelObjectPtr> all();
	static LevelTile buildTile(variant node);
	static void writeCompiled();

	static void setCurrentPalette(unsigned int palette);
	explicit LevelObject(variant node, const char* id=nullptr);
	~LevelObject();

	int width() const;
	int height() const;
	bool isPassthrough() const { return passthrough_; }
	bool isSolid(int x, int y) const;
	bool flipped() const { return flip_; }
	bool hasSolid() const { return !solid_.empty(); }
	bool allSolid() const { return all_solid_; }
	const std::string& id() const { return id_; }
	const std::string& info() const { return info_; }
	int friction() const { return friction_; }
	int traction() const { return traction_; }
	int damage() const { return damage_; }
	const KRE::TexturePtr& texture() const { return t_; }
	static void queueDraw(KRE::CanvasPtr canvas, const LevelTile& t, const rect* dst_area=nullptr);
	static int calculateTileCorners(std::vector<tile_corner>* result, const LevelTile& t);

	bool isOpaque() const { return opaque_; }
	bool calculateOpaque() const;
	bool calculateUsesAlphaChannel() const;
	bool calculateIsSolidColor(KRE::Color& col) const;

	bool calculateDrawArea();

	const KRE::Color* getSolidColor() const { return solid_color_.get(); }

	//write the compiled index of this object. buf MUST point to a buf
	//of at least 4 chars.
	void writeCompiledIndex(char* buf) const;

	//reads an object from its compiled index. buf MUST point to a buf of
	//at least 3 chars.
	static ConstLevelObjectPtr getCompiled(const char* buf);

	//only used when compiling: notifies the object it is used at the
	//given zorder.
	LevelObjectPtr recordZorder(int zorder) const;

private:
	DECLARE_CALLABLE(LevelObject);

	std::string id_;
	std::string image_;
	std::string info_;
	KRE::TexturePtr t_;
	std::vector<int> tiles_;
	std::vector<bool> solid_;
	bool all_solid_;
	bool passthrough_;
	bool flip_;
	int damage_;
	int friction_;
	int traction_;

	bool opaque_;

	rect draw_area_;

	KRE::ColorPtr solid_color_;

	int tile_index_;

	//only used when compiling: records all possible zorders for the object.
	mutable std::vector<int> zorders_;

	unsigned int palettes_recognized_;
	unsigned int current_palettes_;

	void setPalette(unsigned int palette);

	void getPalettesUsed(std::vector<int>& v) const;
};
