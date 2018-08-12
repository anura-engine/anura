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

#include <GL/glew.h>

#include <algorithm>
#include <iostream>
#include <math.h>

#include "BlendModeScope.hpp"
#include "CameraObject.hpp"
#include "ColorScope.hpp"
#include "Font.hpp"
#include "ModelMatrixScope.hpp"
#include "RenderManager.hpp"
#include "RenderTarget.hpp"
#include "StencilScope.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "collision_utils.hpp"
#include "controls.hpp"
#include "draw_scene.hpp"
#include "editor.hpp"
#include "entity.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula_profiler.hpp"
#include "json_parser.hpp"
#include "hex.hpp"
#include "level.hpp"
#include "level_object.hpp"
#include "level_runner.hpp"
#include "light.hpp"
#include "load_level.hpp"
#include "module.hpp"
#include "multiplayer.hpp"
#include "object_events.hpp"
#include "player_info.hpp"
#include "playable_custom_object.hpp"
#include "preferences.hpp"
#include "preprocessor.hpp"
#include "profile_timer.hpp"
#include "random.hpp"
#include "rect_renderable.hpp"
#include "screen_handling.hpp"
#include "sound.hpp"
#include "stats.hpp"
#include "string_utils.hpp"
#include "surface_palette.hpp"
#include "thread.hpp"
#include "tile_map.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"
#include "wml_formula_callable.hpp"

#if defined(_MSC_VER)
#	define strtoll _strtoi64
#endif

#ifndef NO_EDITOR
std::set<Level*>& get_all_levels_set() 
{
	static std::set<Level*>* all = new std::set<Level*>;
	return *all;
}
#endif

namespace 
{
	PREF_INT(debug_skip_draw_zorder_begin, INT_MIN, "Avoid drawing the given zorder");
	PREF_INT(debug_skip_draw_zorder_end, INT_MIN, "Avoid drawing the given zorder");
	PREF_BOOL(debug_shadows, false, "Show debug visualization of shadow drawing");

	LevelPtr& get_current_level() 
	{
		static LevelPtr current_level;
		return current_level;
	}

	std::map<std::string, Level::Summary> load_level_summaries() 
	{
		std::map<std::string, Level::Summary> result;
		const variant node = json::parse_from_file("data/compiled/level_index.cfg");
	
		for(variant level_node : node["level"].as_list()) {
			Level::Summary& s = result[level_node["level"].as_string()];
			s.music = level_node["music"].as_string();
			s.title = level_node["title"].as_string();
		}
		return result;
	}

	bool level_tile_not_in_rect(const rect& r, const LevelTile& t) 
	{
		return t.x < r.x() || t.y < r.y() || t.x >= r.x2() || t.y >= r.y2();
	}
}

void Level::clearCurrentLevel()
{
	get_current_level().reset();
}

Level::Summary Level::getSummary(const std::string& id)
{
	static const std::map<std::string, Summary> summaries = load_level_summaries();
	std::map<std::string, Summary>::const_iterator i = summaries.find(id);
	if(i != summaries.end()) {
		return i->second;
	}

	return Summary();
}

Level& Level::current()
{
	ASSERT_LOG(get_current_level(), "Tried to query current level when there is none");
	return *get_current_level();
}

Level* Level::getCurrentPtr()
{
	return get_current_level().get();
}

CurrentLevelScope::CurrentLevelScope(Level* lvl) : old_(get_current_level())
{
	lvl->setAsCurrentLevel();
}

CurrentLevelScope::~CurrentLevelScope() 
{
	if(old_) {
		old_->setAsCurrentLevel();
	}
}

void Level::setAsCurrentLevel()
{
	get_current_level() = this;
	Frame::setColorPalette(palettes_used_);
}

namespace 
{
	KRE::ColorTransform default_dark_color() 
	{
		return KRE::ColorTransform(1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	variant_type_ptr g_player_type;
}

Level::SubComponent::SubComponent() : source_area(0, 0, 0, 0), num_variations(1)
{}

Level::SubComponent::SubComponent(variant node) : source_area(node["source_area"]), num_variations(node["num_variations"].as_int(1))
{}

variant Level::SubComponent::write() const
{
	variant_builder res;
	res.add("source_area", source_area.write());
	res.add("num_variations", num_variations);
	return res.build();
}

Level::SubComponentUsage::SubComponentUsage() : dest_area(0, 0, 0, 0), ncomponent(0), ninstance(0)
{}

Level::SubComponentUsage::SubComponentUsage(variant node) : dest_area(node["dest_area"]), ncomponent(node["ncomponent"].as_int(0)), ninstance(node["ninstance"].as_int(0))
{}

const Level::SubComponent& Level::SubComponentUsage::getSubComponent(const Level& lvl) const
{
	assert(ncomponent < lvl.getSubComponents().size());
	return lvl.getSubComponents()[ncomponent];
}

rect Level::SubComponentUsage::getSourceArea(const Level& lvl) const
{
	const auto& sub = getSubComponent(lvl);
	rect res = sub.source_area;
	return rect(res.x() + (res.w() + TileSize*4)*(ninstance%sub.num_variations), res.y(), res.w(), res.h());
}

variant Level::SubComponentUsage::write() const
{
	variant_builder res;
	res.add("dest_area", dest_area.write());
	res.add("ncomponent", ncomponent);
	res.add("ninstance", ninstance);
	return res.build();
}

std::vector<Level::SubComponentUsage> Level::getSubComponentUsagesOrdered() const
{
	//sub component usages all copy their data in.
	//Resolve the sub component usages in the correct order by
	//searching for a sub component usage which doesn't have 
	//any unresolved usages that map into its source.
	std::vector<SubComponentUsage> usages = getSubComponentUsages();
	std::vector<SubComponentUsage> result;

	while(usages.empty() == false) {
		int ntries = 0;
		size_t ncandidate = 0;
		bool new_candidate = true;
		while(new_candidate && ntries < usages.size()) {

			const rect& source_area = usages[ncandidate].getSourceArea(*this);

			new_candidate = false;
			for(size_t n = 0; n != usages.size(); ++n) {
				if(n == ncandidate) {
					continue;
				}

				if(rects_intersect(usages[n].dest_area, source_area)) {
					new_candidate = true;
					ncandidate = n;
					++ntries;
					break;
				}
			}
		}

		result.push_back(usages[ncandidate]);
		usages.erase(usages.begin() + ncandidate);
	}

	return result;
}

void Level::applySubComponents()
{
	std::vector<int> layers;
	for(const SubComponentUsage& usage : getSubComponentUsagesOrdered()) {
		const rect& dst = usage.dest_area;
		const rect& src = usage.getSourceArea(*this);

		const rect tile_src(src.x(), src.y(), src.w()-TileSize, src.h()-TileSize);
		const rect tile_dst(dst.x(), dst.y(), dst.w()-TileSize, dst.h()-TileSize);

		std::map<int, std::vector<std::string> > src_tiles, dst_tiles;
		getAllTilesRect(tile_src.x(), tile_src.y(), tile_src.x2(), tile_src.y2(), src_tiles);
		getAllTilesRect(tile_dst.x(), tile_dst.y(), tile_dst.x2(), tile_dst.y2(), dst_tiles);

		clear_tile_rect(tile_dst.x(), tile_dst.y(), tile_dst.x2(), tile_dst.y2());

		for(auto& p : src_tiles) {
			addTileRectVector(p.first, tile_dst.x(), tile_dst.y(), tile_dst.x2(), tile_dst.y2(), p.second);
			layers.push_back(p.first);
		}
	
		std::vector<EntityPtr> chars = get_chars();
		for(auto c : chars) {
			if(c->x() >= dst.x() && c->x() <= dst.x2() && c->y() >= dst.y() && c->y() <= dst.y2()) {
				remove_character(c);
			}
		}

		chars = get_chars();
		for(auto c : chars) {
			if(c->x() >= src.x() && c->x() <= src.x2() && c->y() >= src.y() && c->y() <= src.y2()) {
				auto clone = c->clone();
				clone->shiftPosition(dst.x() - src.x(), dst.y() - src.y());
				add_character(clone);
			}
		}
	}

	std::sort(layers.begin(), layers.end());

	layers.erase(std::unique(layers.begin(), layers.end()), layers.end());

	if(layers.empty() == false) {
		start_rebuild_tiles_in_background(layers);
		while(complete_rebuild_tiles_in_background() == false) {
		}
	}
}

Level::Level(const std::string& level_cfg, variant node)
	: id_(level_cfg),
	x_resolution_(0),
	y_resolution_(0),
	absolute_object_adjust_x_(0),
	absolute_object_adjust_y_(0),
	set_screen_resolution_on_entry_(false),
	highlight_layer_(std::numeric_limits<int>::min()),
	num_compiled_tiles_(0),
	entered_portal_active_(false),
	save_point_x_(-1),
	save_point_y_(-1),
	editor_(false),
	show_foreground_(true),
	show_background_(true),
	dark_(false),
	dark_color_(KRE::ColorTransform(255, 255, 255, 255, 0, 0, 0, 255)),
	air_resistance_(0),
	water_resistance_(7),
	end_game_(false),
	editor_tile_updates_frozen_(0),
	editor_dragging_objects_(false),
	zoom_level_(1.0f),
	instant_zoom_level_set_(-1),
	  palettes_used_(0),
	  background_palette_(-1),
	  segment_width_(0), 
	  segment_height_(0),
	  mouselook_enabled_(false), 
	  mouselook_inverted_(false),
	  allow_touch_controls_(true),
	  show_builtin_settings_(false),
	  have_render_to_texture_(false),
	  render_to_texture_(false),
	  doing_render_to_texture_(false),
	  scene_graph_(nullptr),
	  last_process_time_(profile::get_tick_time()),
	  hex_map_(nullptr),
	  hex_renderable_(nullptr),
	  hex_masks_(),
	  fb_render_target_()
{
#ifndef NO_EDITOR
	get_all_levels_set().insert(this);
#endif

	scene_graph_ = KRE::SceneGraph::create("level");
	KRE::SceneNodePtr sg_root = scene_graph_->getRootNode();
	sg_root->setNodeName("root_node");
	rmanager_ = KRE::RenderManager::getInstance();
	rmanager_->addQueue(0, "Level::opaques");

	if(KRE::DisplayDevice::checkForFeature(KRE::DisplayDeviceCapabilties::RENDER_TO_TEXTURE)) {
		have_render_to_texture_ = true;
		auto& gs = graphics::GameScreen::get();

		try {
			const assert_recover_scope safe_scope;
			rt_ = KRE::RenderTarget::create(gs.getVirtualWidth(), gs.getVirtualHeight(), 1, false, true);
		} catch(validation_failure_exception& /*e*/) {
			LOG_INFO("Could not create fbo with stencil buffer. Trying without stencil buffer");
			rt_ = KRE::RenderTarget::create(gs.getVirtualWidth(), gs.getVirtualHeight(), 1, false, false);
		}

		if(rt_ != nullptr) {
			if(node.has_key("fb_render_target")) {
				fb_render_target_ = node["fb_render_target"];
				rt_->setFromVariant(fb_render_target_);
			} else {
				rt_->setBlendState(false);
			}
		}

		//rt_->setCamera(std::make_shared<KRE::Camera>("render_target"));
	}

	LOG_INFO("in level constructor...");
	const int start_time = profile::get_tick_time();

	if(node.is_null()) {
		node = load_level_wml(level_cfg);
	}

	variant player_save_node;
	ASSERT_LOG(node.is_null() == false, "LOAD LEVEL WML FOR " << level_cfg << " FAILED");
	if(node.has_key("id")) {

		id_ = node["id"].as_string();

		if(level_cfg.size() > 4 && std::equal(level_cfg.begin(), level_cfg.begin()+4, "save") == false) {
			ASSERT_LOG(level_cfg == id_, "Level file " << level_cfg << " has incorrect id: " << id_);
		}
	}

	for(variant v : node["sub_components"].as_list_optional()) {
		sub_components_.push_back(SubComponent(v));
	}

	for(variant v : node["sub_component_usages"].as_list_optional()) {
		sub_component_usages_.push_back(SubComponentUsage(v));
	}

	if(preferences::load_compiled() && (level_cfg == "save.cfg" || level_cfg == "autosave.cfg")) {
		if(preferences::version() != node["version"].as_string()) {
			LOG_INFO("DIFFERENT VERSION LEVEL");
			for(variant obj_node : node["character"].as_list()) {
				if(obj_node["is_human"].as_bool(false)) {
					player_save_node = obj_node;
					break;
				}
			}

			variant n = node;
			if(node.has_key("id")) {
				n = load_level_wml(node["id"].as_string());
			}

			n = n.add_attr(variant("serialized_objects"), n["serialized_objects"] + node["serialized_objects"]);

			node = n;
		}
	}

	dark_color_ = default_dark_color();
	if(node["dark"].as_bool(false)) {
		dark_ = true;
	}

	if(node.has_key("dark_color")) {
		dark_color_ = KRE::ColorTransform(node["dark_color"]);
	}

	vars_ = node["vars"];
	if(vars_.is_map() == false) {
		std::map<variant,variant> m;
		vars_ = variant(&m);
	}

	segment_width_ = node["segment_width"].as_int();
	ASSERT_LOG(segment_width_%TileSize == 0, "segment_width in " << id_ << " is not divisible by " << TileSize << " (" << segment_width_%TileSize << " wide)");

	segment_height_ = node["segment_height"].as_int();
	ASSERT_LOG(segment_height_%TileSize == 0, "segment_height in " << id_ << " is not divisible by " << TileSize  << " (" << segment_height_%TileSize << " tall)");

	music_ = node["music"].as_string_default();
	replay_data_ = node["replay_data"].as_string_default();
	cycle_ = node["cycle"].as_int();
	paused_ = false;
	time_freeze_ = 0;
	x_resolution_ = node["x_resolution"].as_int();
	y_resolution_ = node["y_resolution"].as_int();
	set_screen_resolution_on_entry_ = node["set_screen_resolution_on_entry"].as_bool(false);
	in_dialog_ = false;
	constrain_camera_ = true;
	title_ = node["title"].as_string_default();
	if(node.has_key("dimensions")) {
		boundaries_ = rect(node["dimensions"]);
	} else {
		boundaries_ = rect(0, 0, node["width"].as_int(799), node["height"].as_int(599));
	}

	if(node.has_key("lock_screen")) {
		lock_screen_.reset(new point(node["lock_screen"].as_string()));
	}

	if(node.has_key("opaque_rects")) {
		const std::vector<std::string> opaque_rects_str = util::split(node["opaque_rects"].as_string(), ':');
		for(const std::string& r : opaque_rects_str) {
			opaque_rects_.push_back(rect(r));
			LOG_INFO("OPAQUE RECT: " << r);
		}
	}

	xscale_ = node["xscale"].as_int(100);
	yscale_ = node["yscale"].as_int(100);
	auto_move_camera_ = point(node["auto_move_camera"]);
	air_resistance_ = node["air_resistance"].as_int(20);
	water_resistance_ = node["water_resistance"].as_int(100);

	camera_rotation_ = game_logic::Formula::createOptionalFormula(node["camera_rotation"]);

	preloads_ = util::split(node["preloads"].as_string_default(""));

	std::string empty_solid_info;
	for(variant rect_node : node["solid_rect"].as_list()) {
		solid_rect r;
		r.r = rect(rect_node["rect"]);
		r.friction = rect_node["friction"].as_int(100);
		r.traction = rect_node["traction"].as_int(100);
		r.damage = rect_node["damage"].as_int();
		solid_rects_.push_back(r);
		add_solid_rect(r.r.x(), r.r.y(), r.r.x2(), r.r.y2(), r.friction, r.traction, r.damage, empty_solid_info);
	}

	LOG_INFO("building..." << profile::get_tick_time());
	widest_tile_ = 0;
	highest_tile_ = 0;
	layers_.insert(0);
	for(variant tile_node : node["tile"].as_list()) {
		const LevelTile t = LevelObject::buildTile(tile_node);
		tiles_.push_back(t);
		layers_.insert(t.zorder);
		add_tile_solid(t);
	}
	LOG_INFO("done building..." << profile::get_tick_time());

	auto begin_tile_index = tiles_.size();
	for(variant tile_node : node["tile_map"].as_list()) {
		variant tiles_value = tile_node["tiles"];
		if(!tiles_value.is_string()) {
			continue;
		}

		const std::string& str = tiles_value.as_string();
		bool contains_data = false;
		for(char c : str) {
			if(c != ',' && !util::c_isspace(c)) {
				contains_data = true;
				break;
			}
		}

		if(!contains_data) {
			continue;
		}

		TileMap m(tile_node);
		ASSERT_LOG(tile_maps_.count(m.zorder()) == 0, "repeated zorder in tile map: " << m.zorder());
		tile_maps_[m.zorder()] = m;
		const auto before = tiles_.size();
		tile_maps_[m.zorder()].buildTiles(&tiles_);
		LOG_INFO("LAYER " << m.zorder() << " BUILT " << (tiles_.size() - before) << " tiles");
	}

	LOG_INFO("done building tile_map..." << profile::get_tick_time());

	num_compiled_tiles_ = node["num_compiled_tiles"].as_int();

	tiles_.resize(tiles_.size() + num_compiled_tiles_);
	std::vector<LevelTile>::iterator compiled_itor = tiles_.end() - num_compiled_tiles_;

	for(variant tile_node : node["compiled_tiles"].as_list()) {
		read_compiled_tiles(tile_node, compiled_itor);
		wml_compiled_tiles_.push_back(tile_node);
	}

	ASSERT_LOG(compiled_itor == tiles_.end(), "INCORRECT NUMBER OF COMPILED TILES");

	for(std::vector<LevelTile>::size_type i = begin_tile_index; i != tiles_.size(); ++i) {
		add_tile_solid(tiles_[i]);
		layers_.insert(tiles_[i].zorder);
	}

	if(std::adjacent_find(tiles_.rbegin(), tiles_.rend(), level_tile_zorder_pos_comparer()) != tiles_.rend()) {
		std::sort(tiles_.begin(), tiles_.end(), level_tile_zorder_pos_comparer());
	}

	if(node.has_key("hex_map")) {
		ASSERT_LOG(scene_graph_ != nullptr, "Couldn't instantiate a HexMap object, scenegraph was nullptr");
		hex_map_ = hex::HexMap::create(node["hex_map"]);
		hex_renderable_ = std::dynamic_pointer_cast<hex::MapNode>(scene_graph_->createNode("hex_map"));
		hex_map_->setRenderable(hex_renderable_);
		scene_graph_->getRootNode()->attachNode(hex_renderable_);
	}

	if(node.has_key("palettes")) {
		std::vector<std::string> v = parse_variant_list_or_csv_string(node["palettes"]);
		for(const std::string& p : v) {
			const int id = graphics::get_palette_id(p);
			palettes_used_ |= (1 << id);
		}
	}

	if(node.has_key("background_palette")) {
		background_palette_ = graphics::get_palette_id(node["background_palette"].as_string());
	}

	prepare_tiles_for_drawing();

	for(variant char_node : node["character"].as_list()) {
		if(player_save_node.is_null() == false && char_node["is_human"].as_bool(false)) {
			continue;
		}

		wml_chars_.push_back(char_node);
		continue;
	}

	if(player_save_node.is_null() == false) {
		wml_chars_.push_back(player_save_node);
	}

	variant serialized_objects = node["serialized_objects"];
	if(serialized_objects.is_null() == false) {
		serialized_objects_.push_back(serialized_objects);
	}

	for(variant portal_node : node["portal"].as_list()) {
		portal p;
		p.area = rect(portal_node["rect"]);
		p.level_dest = portal_node["level"].as_string();
		p.dest = point(portal_node["dest"].as_string());
		p.dest_starting_pos = portal_node["dest_starting_post"].as_bool(false);
		p.automatic = portal_node["automatic"].as_bool(true);
		p.transition = portal_node["transition"].as_string();
		portals_.push_back(p);
	}

	if(node.has_key("next_level")) {
		right_portal_.level_dest = node["next_level"].as_string();
		right_portal_.dest_str = "left";
		right_portal_.dest_starting_pos = false;
		right_portal_.automatic = true;
	}

	if(node.has_key("previous_level")) {
		left_portal_.level_dest = node["previous_level"].as_string();
		left_portal_.dest_str = "right";
		left_portal_.dest_starting_pos = false;
		left_portal_.automatic = true;
	}

	variant bg = node["background"];
	if(bg.is_map()) {
		background_.reset(new Background(bg, background_palette_));
	} else if(node.has_key("background")) {
		background_ = Background::get(node["background"].as_string(), background_palette_);
		background_offset_ = point(node["background_offset"]);
		background_->setOffset(background_offset_);
	}

	if(node.has_key("water")) {
		water_.reset(new Water(node["water"]));
	}

	sub_level_str_ = node["sub_levels"].as_string_default();
	for(const std::string& sub_lvl : util::split(sub_level_str_)) {
		sub_level_data& data = sub_levels_[sub_lvl];
		data.lvl = ffl::IntrusivePtr<Level>(new Level(sub_lvl + ".cfg"));
		for(int layer : data.lvl->layers_) {
			layers_.insert(layer);
		}

		data.active = false;
		data.xoffset = data.yoffset = 0;
		data.xbase = data.ybase = 0;
	}

	allow_touch_controls_ = node["touch_controls"].as_bool(true);

#ifdef USE_BOX2D
	if(node.has_key("bodies") && node["bodies"].is_list()) {
		for(int n = 0; n != node["bodies"].num_elements(); ++n) {
			bodies_.push_back(new box2d::body(node["bodies"][n]));
			LOG_INFO("level create body: " << std::hex << intptr_t(bodies_.back().get()) << " " << intptr_t(bodies_.back()->get_raw_body_ptr()) << std::dec);
		}
	}
#endif

	if(node.has_key("shader")) {
		if(node["shader"].is_string()) {
			shader_.reset(new graphics::AnuraShader(node["shader"].as_string()));
		} else {
			shader_.reset(new graphics::AnuraShader(node["shader"]["name"].as_string(), node["shader"]));
		}
	}

	const int time_taken_ms = (profile::get_tick_time() - start_time);
	stats::Entry("load", id()).set("time", variant(time_taken_ms));
	LOG_INFO("done level constructor: " << time_taken_ms);


}

Level::~Level()
{
#ifndef NO_EDITOR
	get_all_levels_set().erase(this);
#endif

	for(auto i : backups_) {
		for(const EntityPtr& e : i->chars) {
			//kill off any references this entity holds, to workaround
			//circular references causing things to stick around.
			e->cleanup_references();
		}
	}

	if(before_pause_controls_backup_) {
		before_pause_controls_backup_->cancel();
	}
}

void Level::setRenderToTexture(int width, int height)
{
	render_to_texture_ = true;
	doing_render_to_texture_ = false;

		try {
			const assert_recover_scope safe_scope;
			rt_ = KRE::RenderTarget::create(width, height, 1, false, true);
			rt_->setBlendState(false);
		} catch(validation_failure_exception& /*e*/) {
			LOG_INFO("Could not create fbo with stencil buffer. Trying without stencil buffer");
			rt_ = KRE::RenderTarget::create(width, height, 1, false, false);
			rt_->setBlendState(false);
		}
}

namespace {
	int g_num_level_transition_frames = 0;
	decimal g_level_transition_ratio;
}

int Level::setup_level_transition(const std::string& transition_type)
{
	g_num_level_transition_frames = 0;

	game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable());
	callable->add("transition", variant(transition_type));
	std::vector<EntityPtr> active_chars = get_active_chars();
	for(const auto& c : active_chars) {
		c->handleEvent(OBJECT_EVENT_BEGIN_TRANSITION_LEVEL, callable.get());
	}

	return g_num_level_transition_frames;
}

void Level::set_level_transition_ratio(decimal d)
{
	g_level_transition_ratio = d;
}

void Level::read_compiled_tiles(variant node, std::vector<LevelTile>::iterator& out)
{
	const int xbase = node["x"].as_int();
	const int ybase = node["y"].as_int();
	const int zorder = parse_zorder(node["zorder"]);

	int x = xbase;
	int y = ybase;
	const std::string& tiles = node["tiles"].as_string();
	const char* i = tiles.c_str();
	const char* end = tiles.c_str() + tiles.size();
	while(i != end) {
		if(*i == '|') {
			++i;
		} else if(*i == ',') {
			x += TileSize;
			++i;
		} else if(*i == '\n') {
			x = xbase;
			y += TileSize;
			++i;
		} else {
			ASSERT_LOG(out != tiles_.end(), "NOT ENOUGH COMPILED TILES REPORTED");

			out->x = x;
			out->y = y;
			out->zorder = zorder;
			out->face_right = false;
			out->draw_disabled = false;
			if(*i == '~') {
				out->face_right = true;
				++i;
			}

			ASSERT_LOG(end - i >= 3, "ILLEGAL TILE FOUND");

			out->object = LevelObject::getCompiled(i).get();
			++out;
			i += 3;
		}
	}
}

void Level::load_character(variant c)
{
	chars_.push_back(Entity::build(c));
	layers_.insert(chars_.back()->zorder());
	if(!chars_.back()->isHuman()) {
		chars_.back()->setId(static_cast<int>(chars_.size()));
	}
	if(chars_.back()->isHuman()) {
		if(players_.size() == multiplayer::slot()) {
			last_touched_player_ = player_ = chars_.back();
		}
		ASSERT_LOG(!g_player_type || g_player_type->match(variant(chars_.back().get())), "Player object being added to level does not match required player type. " << chars_.back()->getDebugDescription() << " is not a " << g_player_type->to_string());

		players_.push_back(chars_.back());
		players_.back()->getPlayerInfo()->setPlayerSlot(static_cast<int>(players_.size() - 1));
	}

	const int group = chars_.back()->group();
	if(group >= 0) {
		if(static_cast<unsigned>(group) >= groups_.size()) {
			groups_.resize(group + 1);
		}

		groups_[group].push_back(chars_.back());
	}

	if(chars_.back()->label().empty() == false) {
		EntityPtr& ptr = chars_by_label_[chars_.back()->label()];
		ASSERT_LOG(ptr.get() == nullptr, "Loading object with duplicate label: " << chars_.back()->label());
		ptr = chars_.back();
	}

	solid_chars_.clear();
}

PREF_BOOL(respect_difficulty, false, "");

void Level::finishLoading()
{
	if(sub_component_usages_.empty() == false) {
		if(!editor_) {
			for(SubComponentUsage& usage : sub_component_usages_) {
				const SubComponent& sub = usage.getSubComponent(*this);
				usage.ninstance = rng::generate()%sub.num_variations;
			}
		}

		applySubComponents();
	}

	assert(refcount() > 0);
	CurrentLevelScope level_scope(this);

	std::vector<sub_level_data> sub_levels;
	if((segment_width_ > 0 || segment_height_ > 0) && !editor_ && !preferences::compiling_tiles) {

		const int seg_width = segment_width_ > 0 ? segment_width_ : boundaries_.w();
		const int seg_height = segment_height_ > 0 ? segment_height_ : boundaries_.h();

		for(int y = boundaries_.y(); y < boundaries_.y2(); y += seg_height) {
			for(int x = boundaries_.x(); x < boundaries_.x2(); x += seg_width) {
				Level* sub_level = new Level(*this);
				const rect bounds(x, y, seg_width, seg_height);

				sub_level->boundaries_ = bounds;
				sub_level->tiles_.erase(std::remove_if(sub_level->tiles_.begin(), sub_level->tiles_.end(), std::bind(level_tile_not_in_rect, bounds, std::placeholders::_1)), sub_level->tiles_.end());
				sub_level->solid_.clear();
				sub_level->standable_.clear();
				for(const LevelTile& t : sub_level->tiles_) {
					sub_level->add_tile_solid(t);
				}
				sub_level->prepare_tiles_for_drawing();

				sub_level_data data;
				data.lvl.reset(sub_level);
				data.xbase = x;
				data.ybase = y;
				data.xoffset = data.yoffset = 0;
				data.active = false;
				sub_levels.push_back(data);
			}
		}

		const std::vector<EntityPtr> objects = get_chars();
		for(const EntityPtr& obj : objects) {
			if(!obj->isHuman()) {
				remove_character(obj);
			}
		}

		solid_.clear();
		standable_.clear();
		tiles_.clear();
		prepare_tiles_for_drawing();

		int index = 0;
		for(const sub_level_data& data : sub_levels) {
			sub_levels_[formatter() << index] = data;
			++index;
		}
	}

	if(sub_levels_.empty() == false) {
		solid_base_ = solid_;
		standable_base_ = standable_;
	}

	// XXX
	//graphics::texture::build_textures_from_worker_threads();

	if (editor_ || preferences::compiling_tiles) {
		//game_logic::set_verbatim_string_expressions (true);
	}

	std::vector<EntityPtr> objects_not_in_level;

	{
	game_logic::wmlFormulaCallableReadScope read_scope;
	for(variant node : serialized_objects_) {
		for(variant obj_node : node["character"].as_list()) {
			game_logic::WmlSerializableFormulaCallablePtr obj;

			boost::uuids::uuid obj_uuid;

			if(obj_node.is_map()) {

				EntityPtr e(Entity::build(obj_node));
				objects_not_in_level.push_back(e);
				obj = e;

				if(obj_node.has_key("_addr")) {
					//convert old style _addr to uuid.
					obj_uuid = addr_to_uuid(obj_node["_addr"].as_string());
				} else {
					obj_uuid = obj->uuid();
				}
			} else {
				obj = obj_node.try_convert<game_logic::WmlSerializableFormulaCallable>();
				obj_uuid = obj->uuid();
			}

			game_logic::wmlFormulaCallableReadScope::registerSerializedObject(obj_uuid, obj);
		}
	}

	for(variant node : wml_chars_) {
		load_character(node);

		boost::uuids::uuid obj_uuid;
		if(node.has_key("_addr")) {
			obj_uuid = addr_to_uuid(node["_addr"].as_string());
		} else {
			obj_uuid = read_uuid(node["_uuid"].as_string());
		}

		game_logic::wmlFormulaCallableReadScope::registerSerializedObject(obj_uuid, chars_.back());

		if(node.has_key("attached_objects")) {
			LOG_INFO("LOADING ATTACHED: " << node["attached_objects"].as_string());
			std::vector<EntityPtr> attached;
			std::vector<std::string> v = util::split(node["attached_objects"].as_string());
			for(const std::string& s : v) {
				LOG_INFO("ATTACHED: " << s);
				boost::uuids::uuid attached_uuid = addr_to_uuid(s);
				game_logic::WmlSerializableFormulaCallablePtr obj = game_logic::wmlFormulaCallableReadScope::getSerializedObject(attached_uuid);
				Entity* e = dynamic_cast<Entity*>(obj.get());
				if(e) {
					LOG_INFO("GOT ATTACHED\n");
					attached.push_back(EntityPtr(e));
				}
			}

			chars_.back()->setAttachedObjects(attached);
		}
	}

	game_logic::set_verbatim_string_expressions (false);

	wml_chars_.clear();
	serialized_objects_.clear();

	controls::new_level(cycle_, players_.empty() ? 1 : static_cast<int>(players_.size()), multiplayer::slot());

	//start loading FML for previous and next level
	if(!previous_level().empty()) {
		preload_level_wml(previous_level());
	}

	if(!next_level().empty()) {
		preload_level_wml(next_level());
	}

	if(!sub_levels.empty()) {
		const int seg_width = segment_width_ > 0 ? segment_width_ : boundaries_.w();
		const int seg_height = segment_height_ > 0 ? segment_height_ : boundaries_.h();
		int segment_number = 0;
		for(int y = boundaries_.y(); y < boundaries_.y2(); y += seg_height) {
			for(int x = boundaries_.x(); x < boundaries_.x2(); x += seg_width) {
				const std::vector<EntityPtr> objects = get_chars();
				for(const EntityPtr& obj : objects) {
					if(!obj->isHuman() && obj->getMidpoint().x >= x && obj->getMidpoint().x < x + seg_width && obj->getMidpoint().y >= y && obj->getMidpoint().y < y + seg_height) {
						ASSERT_INDEX_INTO_VECTOR(segment_number, sub_levels);
						sub_levels[segment_number].lvl->add_character(obj);
						remove_character(obj);
					}
				}

				++segment_number;
			}
		}
	}

	} //end serialization read scope. Now all objects should be fully resolved.

	if((g_respect_difficulty || preferences::force_difficulty() != std::numeric_limits<int>::min()) && !editor_) {
		const int difficulty = current_difficulty();
		for(int n = 0; n != chars_.size(); ++n) {
			if(chars_[n].get() != nullptr && !chars_[n]->appearsAtDifficulty(difficulty)) {
				chars_[n] = EntityPtr();
			}
		}

		chars_.erase(std::remove(chars_.begin(), chars_.end(), EntityPtr()), chars_.end());
	}

#if defined(USE_BOX2D)
	for(auto it : bodies_) {
		it->finishLoading();
		LOG_INFO("level body finish loading: " << std::hex << intptr_t(it.get()) << " " << intptr_t(it->get_raw_body_ptr()) << std::dec);
	}
#endif

	//iterate over all our objects and let them do any final loading actions.
	for(EntityPtr e : objects_not_in_level) {
		if(e) {
			e->finishLoading(this);
		}
	}

	for(EntityPtr e : chars_) {
		if(e) {
			e->finishLoading(this);
		}
	}
/*  Removed firing createObject() for now since create relies on things
    that might not be around yet.
	const std::vector<EntityPtr> chars = chars_;
	for(const EntityPtr& e : chars) {
		const bool res = e->createObject();
		if(!res) {
			e->validate_properties();
		}
	}
	*/
}

void Level::setMultiplayerSlot(int slot)
{
	ASSERT_INDEX_INTO_VECTOR(slot, players_);
	last_touched_player_ = player_ = players_[slot];
	controls::new_level(cycle_, players_.empty() ? 1 : static_cast<int>(players_.size()), slot);
}

void Level::load_save_point(const Level& lvl)
{
	if(lvl.save_point_x_ < 0) {
		return;
	}

	save_point_x_ = lvl.save_point_x_;
	save_point_y_ = lvl.save_point_y_;
	if(player_) {
		player_->setPos(save_point_x_, save_point_y_);
	}
}

namespace 
{
	//we allow rebuilding tiles in the background. We only rebuild the tiles
	//one at a time, if more requests for rebuilds come in while we are
	//rebuilding, then queue the requests up.

	//the level we're currently building tiles for.
	const Level* level_building = nullptr;

	struct level_tile_rebuild_info 
	{
		level_tile_rebuild_info() : tile_rebuild_in_progress(false),
									tile_rebuild_queued(false),
									rebuild_tile_thread(nullptr),
									tile_rebuild_complete(false)
		{}

		//record whether we are currently rebuilding tiles, and if we have had
		//another request come in during the current building of tiles.
		bool tile_rebuild_in_progress;
		bool tile_rebuild_queued;

		threading::thread* rebuild_tile_thread;

		//an unsynchronized buffer only accessed by the main thread with layers
		//that will be rebuilt.
		std::vector<int> rebuild_tile_layers_buffer;

		//buffer accessed by the worker thread which contains layers that will
		//be rebuilt.
		std::vector<int> rebuild_tile_layers_worker_buffer;

		//a locked flag which is polled to see if tile rebuilding has been completed.
		bool tile_rebuild_complete;

		threading::mutex tile_rebuild_complete_mutex;

		//the tiles where the thread will store the new tiles.
		std::vector<LevelTile> task_tiles;
	};

	std::map<const Level*, level_tile_rebuild_info> tile_rebuild_map;

	void build_tiles_thread_function(level_tile_rebuild_info* info, std::map<int, TileMap> tile_maps, threading::mutex& sync) {
		std::lock_guard<std::mutex> lock(GarbageCollector::getGlobalMutex());

		info->task_tiles.clear();

		if(info->rebuild_tile_layers_worker_buffer.empty()) {
			for(auto& i : tile_maps) {
				i.second.buildTiles(&info->task_tiles);
			}
		} else {
			for(int layer : info->rebuild_tile_layers_worker_buffer) {
				auto itor = tile_maps.find(layer);
				if(itor != tile_maps.end()) {
					itor->second.buildTiles(&info->task_tiles);
				}
			}
		}

		threading::lock l(info->tile_rebuild_complete_mutex);
		info->tile_rebuild_complete = true;
	}
}

void Level::start_rebuild_tiles_in_background(const std::vector<int>& layers)
{
	level_tile_rebuild_info& info = tile_rebuild_map[this];

	//merge the new layers with any layers we already have queued up.
	if(layers.empty() == false && (!info.tile_rebuild_queued || info.rebuild_tile_layers_buffer.empty() == false)) {
		//add the layers we want to rebuild to those already requested.
		info.rebuild_tile_layers_buffer.insert(info.rebuild_tile_layers_buffer.end(), layers.begin(), layers.end());
		std::sort(info.rebuild_tile_layers_buffer.begin(), info.rebuild_tile_layers_buffer.end());
		info.rebuild_tile_layers_buffer.erase(std::unique(info.rebuild_tile_layers_buffer.begin(), info.rebuild_tile_layers_buffer.end()), info.rebuild_tile_layers_buffer.end());
	} else if(layers.empty()) {
		info.rebuild_tile_layers_buffer.clear();
	}

	if(info.tile_rebuild_in_progress) {
		info.tile_rebuild_queued = true;
		return;
	}

	info.tile_rebuild_in_progress = true;
	info.tile_rebuild_complete = false;

	info.rebuild_tile_layers_worker_buffer = info.rebuild_tile_layers_buffer;
	info.rebuild_tile_layers_buffer.clear();

	std::map<int, TileMap> worker_tile_maps = tile_maps_;
	for(auto& i : worker_tile_maps) {
		//make the tile maps safe to go into a worker thread.
		i.second.prepareForCopyToWorkerThread();
	}

	static threading::mutex* sync = new threading::mutex;

	info.rebuild_tile_thread = new threading::thread("rebuild_tiles", std::bind(build_tiles_thread_function, &info, worker_tile_maps, *sync), threading::THREAD_ALLOCATES_COLLECTIBLE_OBJECTS);
}

void Level::freeze_rebuild_tiles_in_background()
{
	level_tile_rebuild_info& info = tile_rebuild_map[this];
	info.tile_rebuild_in_progress = true;
}

void Level::unfreeze_rebuild_tiles_in_background()
{
	level_tile_rebuild_info& info = tile_rebuild_map[this];
	if(info.rebuild_tile_thread != nullptr) {
		//a thread is actually in flight calculating tiles, so any requests
		//would have been queued up anyway.
		return;
	}

	info.tile_rebuild_in_progress = false;
	start_rebuild_tiles_in_background(info.rebuild_tile_layers_buffer);
}

namespace 
{
	bool level_tile_from_layer(const LevelTile& t, int zorder) 
	{
		return t.layer_from == zorder;
	}

	int g_tile_rebuild_state_id;
}

int Level::tileRebuildStateId()
{
	return g_tile_rebuild_state_id;
}

void Level::setPlayerVariantType(variant type_str)
{
	if(type_str.is_null()) {
		type_str = variant("custom_obj");
	}

	using namespace game_logic;

	g_player_type = parse_variant_type(type_str);

	ConstFormulaCallableDefinitionPtr def = game_logic::get_formula_callable_definition("level");
	assert(def.get());

	FormulaCallableDefinition* mutable_def = const_cast<FormulaCallableDefinition*>(def.get());
	FormulaCallableDefinition::Entry* entry = mutable_def->getEntryById("player");
	assert(entry);
	entry->setVariantType(g_player_type);
}

namespace {
struct TileBackupScope {
	std::vector<LevelTile>& level_tiles;
	std::vector<LevelTile> tiles;
	bool cancelled;
	TileBackupScope(std::vector<LevelTile>& t) : level_tiles(t), tiles(t), cancelled(false) {
	}

	~TileBackupScope() {
		if(!cancelled) {
			level_tiles.swap(tiles);
		}
	}

	void cancel() {
		cancelled = true;
	}
};
}

bool Level::complete_rebuild_tiles_in_background()
{
	level_tile_rebuild_info& info = tile_rebuild_map[this];
	if(!info.tile_rebuild_in_progress) {
		return true;
	}

	{
		threading::lock l(info.tile_rebuild_complete_mutex);
		if(!info.tile_rebuild_complete) {
			return false;
		}
	}

	const int begin_time = profile::get_tick_time();

//	ASSERT_LOG(rebuild_tile_thread, "REBUILD TILE THREAD IS nullptr");
	delete info.rebuild_tile_thread;
	info.rebuild_tile_thread = nullptr;

	TileBackupScope backup(tiles_);

	if(info.rebuild_tile_layers_worker_buffer.empty()) {
		tiles_.clear();
	} else {
		for(int layer : info.rebuild_tile_layers_worker_buffer) {
			using namespace std::placeholders;
			tiles_.erase(std::remove_if(tiles_.begin(), tiles_.end(), std::bind(level_tile_from_layer, std::placeholders::_1, layer)), tiles_.end());
		}
	}

	tiles_.insert(tiles_.end(), info.task_tiles.begin(), info.task_tiles.end());
	info.task_tiles.clear();

	LOG_INFO("COMPLETE TILE REBUILD: " << (profile::get_tick_time() - begin_time));

	info.rebuild_tile_layers_worker_buffer.clear();

	info.tile_rebuild_in_progress = false;

	++g_tile_rebuild_state_id;

	complete_tiles_refresh();

	backup.cancel();

	if(info.tile_rebuild_queued) {
		info.tile_rebuild_queued = false;
		start_rebuild_tiles_in_background(info.rebuild_tile_layers_buffer);
	}

	return true;
}

void Level::rebuildTiles()
{
	if(editor_tile_updates_frozen_) {
		return;
	}

	tiles_.clear();
	for(auto& i : tile_maps_) {
		i.second.buildTiles(&tiles_);
	}

	complete_tiles_refresh();
}

void Level::complete_tiles_refresh()
{
	const int start = profile::get_tick_time();
	//LOG_INFO("adding solids... " << (profile::get_tick_time() - start));
	solid_.clear();
	standable_.clear();

	for(LevelTile& t : tiles_) {
		add_tile_solid(t);
		layers_.insert(t.zorder);
	}

	//LOG_INFO("sorting... " << (profile::get_tick_time() - start));

	if(std::adjacent_find(tiles_.rbegin(), tiles_.rend(), level_tile_zorder_pos_comparer()) != tiles_.rend()) {
		std::sort(tiles_.begin(), tiles_.end(), level_tile_zorder_pos_comparer());
	}
	prepare_tiles_for_drawing();
	//LOG_INFO("done... " << (profile::get_tick_time() - start));

	const std::vector<EntityPtr> chars = chars_;
	for(const EntityPtr& e : chars) {
		e->handleEvent("level_tiles_refreshed");
	}
}

int Level::variations(int xtile, int ytile) const
{
	for(auto& i : tile_maps_) {
		const int var = i.second.getVariations(xtile, ytile);
		if(var > 1) {
			return var;
		}
	}

	return 1;
}

void Level::flip_variations(int xtile, int ytile, int delta)
{
	for(auto& i : tile_maps_) {
		LOG_INFO("get_variations zorder: " << i.first);
		if(i.second.getVariations(xtile, ytile) > 1) {
			i.second.flipVariation(xtile, ytile, delta);
		}
	}

	rebuild_tiles_rect(rect(xtile*TileSize, ytile*TileSize, TileSize, TileSize));
}

namespace 
{
	struct TileInRect {
		explicit TileInRect(const rect& r) : rect_(r)
		{}

		bool operator()(const LevelTile& t) const {
			return pointInRect(point(t.x, t.y), rect_);
		}

		rect rect_;
	};
}

void Level::rebuild_tiles_rect(const rect& r)
{
	if(editor_tile_updates_frozen_) {
		return;
	}

	for(int x = r.x(); x < r.x2(); x += TileSize) {
		for(int y = r.y(); y < r.y2(); y += TileSize) {
			tile_pos pos(x/TileSize, y/TileSize);
			solid_.erase(pos);
			standable_.erase(pos);
		}
	}

	tiles_.erase(std::remove_if(tiles_.begin(), tiles_.end(), TileInRect(r)), tiles_.end());

	std::vector<LevelTile> tiles;
	for(auto& i : tile_maps_) {
		i.second.buildTiles(&tiles, &r);
	}

	for(LevelTile& t : tiles) {
		add_tile_solid(t);
		tiles_.push_back(t);
		layers_.insert(t.zorder);
	}

	if(std::adjacent_find(tiles_.rbegin(), tiles_.rend(), level_tile_zorder_pos_comparer()) != tiles_.rend()) {
		std::sort(tiles_.begin(), tiles_.end(), level_tile_zorder_pos_comparer());
	}
	prepare_tiles_for_drawing();
}

std::string Level::package() const
{
	std::string::const_iterator i = std::find(id_.begin(), id_.end(), '/');
	if(i == id_.end()) {
		return "";
	}

	return std::string(id_.begin(), i);
}

variant Level::write() const
{
	std::sort(tiles_.begin(), tiles_.end(), level_tile_zorder_pos_comparer());
	game_logic::wmlFormulaCallableSerializationScope serialization_scope;

	variant_builder res;
	res.add("id", id_);
	res.add("version", preferences::version());
	res.add("title", title_);
	res.add("music", music_);
	res.add("segment_width", segment_width_);
	res.add("segment_height", segment_height_);

	if(sub_components_.empty() == false) {
		std::vector<variant> sub;
		for(const auto& c : sub_components_) {
			sub.push_back(c.write());
		}

		res.add("sub_components", variant(&sub));
	}

	if(sub_component_usages_.empty() == false) {
		std::vector<variant> sub;
		for(const auto& c : sub_component_usages_) {
			sub.push_back(c.write());
		}

		res.add("sub_component_usages", variant(&sub));
	}

	if(x_resolution_ || y_resolution_) {
		res.add("x_resolution", x_resolution_);
		res.add("y_resolution", y_resolution_);
	}

	res.add("set_screen_resolution_on_entry", set_screen_resolution_on_entry_);

	if(dark_) {
		res.add("dark", true);
	}

	if(dark_color_ != default_dark_color()) {
		res.add("dark_color", dark_color_.write());
	}

	if(cycle_) {
		res.add("cycle", cycle_);
	}

	if(!sub_level_str_.empty()) {
		res.add("sub_levels", sub_level_str_);
	}

	res.add("dimensions", boundaries().write());

	res.add("xscale", xscale_);
	res.add("yscale", yscale_);
	res.add("auto_move_camera", auto_move_camera_.write());
	res.add("air_resistance", air_resistance_);
	res.add("water_resistance", water_resistance_);

	res.add("touch_controls", allow_touch_controls_);

	res.add("preloads", util::join(preloads_));

	if(lock_screen_) {
		res.add("lock_screen", lock_screen_->write());
	}

	if(water_) {
		res.add("water", water_->write());
	}

	if(camera_rotation_) {
		res.add("camera_rotation", camera_rotation_->str());
	}

	for(const solid_rect& r : solid_rects_) {
		variant_builder node;
		node.add("rect", r.r.write());
		node.add("friction", r.friction);
		node.add("traction", r.traction);
		node.add("damage", r.damage);

		res.add("solid_rect", node.build());
	}

	for(auto& i : tile_maps_) {
		variant node = i.second.write();
		if(preferences::compiling_tiles) {
			node.add_attr(variant("tiles"), variant(""));
			node.add_attr(variant("unique_tiles"), variant(""));
		}
		res.add("tile_map", node);
	}

	if(preferences::compiling_tiles && !tiles_.empty()) {
		LevelObject::setCurrentPalette(palettes_used_);

		int num_tiles = 0;
		int last_zorder = std::numeric_limits<int>::min();
		int basex = 0, basey = 0;
		int last_x = 0, last_y = 0;
		std::string tiles_str;
		for(unsigned n = 0; n <= tiles_.size(); ++n) {
			if(n != tiles_.size() && tiles_[n].draw_disabled && tiles_[n].object->hasSolid() == false) {
				continue;
			}

			if(n == tiles_.size() || tiles_[n].zorder != last_zorder) {
				if(!tiles_str.empty()) {
					variant_builder node;
					node.add("zorder", write_zorder(last_zorder));
					node.add("x", basex);
					node.add("y", basey);
					node.add("tiles", tiles_str);
					res.add("compiled_tiles", node.build());
				}

				if(n == tiles_.size()) {
					break;
				}

				tiles_str.clear();

				last_zorder = tiles_[n].zorder;

				basex = basey = std::numeric_limits<int>::max();
				for(int m = n; m != tiles_.size() && tiles_[m].zorder == tiles_[n].zorder; ++m) {
					if(tiles_[m].x < basex) {
						basex = tiles_[m].x;
					}

					if(tiles_[m].y < basey) {
						basey = tiles_[m].y;
					}
				}

				last_x = basex;
				last_y = basey;
			}

			while(last_y < tiles_[n].y) {
				tiles_str += "\n";
				last_y += TileSize;
				last_x = basex;
			}

			while(last_x < tiles_[n].x) {
				tiles_str += ",";
				last_x += TileSize;
			}

			ASSERT_EQ(last_x, tiles_[n].x);
			ASSERT_EQ(last_y, tiles_[n].y);

			if(tiles_[n].face_right) {
				tiles_str += "~";
			}

			const int xpos = tiles_[n].x;
			const int ypos = tiles_[n].y;
			const int zpos = tiles_[n].zorder;
			const int start_n = n;

			while(n != tiles_.size() && tiles_[n].x == xpos && tiles_[n].y == ypos && tiles_[n].zorder == zpos) {
				char buf[4];
				tiles_[n].object->writeCompiledIndex(buf);
				if(n != start_n) {
					tiles_str += "|";
				}
				tiles_str += buf;
				++n;
				++num_tiles;
			}

			--n;

			tiles_str += ",";

			last_x += TileSize;
		}

		res.add("num_compiled_tiles", num_tiles);

		//calculate rectangular opaque areas of tiles that allow us
		//to avoid drawing the background. Start by calculating the set
		//of tiles that are opaque.
		typedef std::pair<int,int> OpaqueLoc;
		std::set<OpaqueLoc> opaque;
		for(const LevelTile& t : tiles_) {
			if(t.object->isOpaque() == false) {
				continue;
			}

			auto tile_itor = tile_maps_.find(t.zorder);
			ASSERT_LOG(tile_itor != tile_maps_.end(), "COULD NOT FIND TILE LAYER IN MAP");
			if(tile_itor->second.getXSpeed() != 100 || tile_itor->second.getYSpeed() != 100) {
				//we only consider the layer that moves at 100% speed,
				//since calculating obscured areas at other layers is too
				//complicated.
				continue;
			}

			opaque.insert(std::pair<int,int>(t.x,t.y));
		}

		LOG_INFO("BUILDING RECTS...");

		std::vector<rect> opaque_rects;

		//keep iterating, finding the largest rectangle we can make of
		//available opaque locations, then removing all those opaque
		//locations from our set, until we have all areas covered.
		while(!opaque.empty()) {
			rect largest_rect;

			//iterate over every opaque location, treating each one
			//as a possible upper-left corner of our rectangle.
			for(std::set<OpaqueLoc>::const_iterator loc_itor = opaque.begin();
			    loc_itor != opaque.end(); ++loc_itor) {
				const OpaqueLoc& loc = *loc_itor;

				std::vector<OpaqueLoc> v;
				v.push_back(loc);

				std::set<OpaqueLoc>::const_iterator find_itor = opaque.end();

				int prev_rows = 0;

				//try to build a top row of a rectangle. After adding each
				//cell, we will try to expand the rectangle downwards, as
				//far as it will go.
				while((find_itor = opaque.find(OpaqueLoc(v.back().first + TileSize, v.back().second))) != opaque.end()) {
					v.push_back(OpaqueLoc(v.back().first + TileSize, v.back().second));

					int rows = 1;

					bool found_non_opaque = false;
					while(found_non_opaque == false) {
						for(int n = rows < prev_rows ? static_cast<int>(v.size())-1 : 0; n != static_cast<int>(v.size()); ++n) {
							if(!opaque.count(OpaqueLoc(v[n].first, v[n].second + rows*TileSize))) {
								found_non_opaque = true;
								break;
							}
						}

						if(found_non_opaque == false) {
							++rows;
						}
					}

					prev_rows = rows;

					rect r(v.front().first, v.front().second, static_cast<int>(v.size()*TileSize), rows*TileSize);
					if(r.w()*r.h() > largest_rect.w()*largest_rect.h()) {
						largest_rect = r;
					}
				} //end while expand rectangle to the right.
			} //end for iterating over all possible rectangle upper-left positions

			LOG_INFO("LARGEST_RECT: " << largest_rect.w() << " x " << largest_rect.h());

			//have a minimum size for rectangles. If we fail to reach
			//the minimum size then just stop. It's not worth bothering 
			//with lots of small little rectangles.
			if(largest_rect.w()*largest_rect.h() < TileSize*TileSize*32) {
				break;
			}

			opaque_rects.push_back(largest_rect);

			for(std::set<OpaqueLoc>::iterator i = opaque.begin();
			    i != opaque.end(); ) {
				if(i->first >= largest_rect.x() && i->second >= largest_rect.y() && i->first < largest_rect.x2() && i->second < largest_rect.y2()) {
					opaque.erase(i++);
				} else {
					++i;
				}
			}
		} //end searching for rectangles to add.
		LOG_INFO("DONE BUILDING RECTS...\n");

		if(!opaque_rects.empty()) {
			std::ostringstream opaque_rects_str;
			for(const rect& r : opaque_rects) {
				opaque_rects_str << r.toString() << ":";
			}

			res.add("opaque_rects", opaque_rects_str.str());

			LOG_INFO("RECTS: " << id_ << ": " << opaque_rects.size());
		}
	} //end if preferences::compiling

	for(EntityPtr ch : chars_) {
		if(!ch->serializable()) {
			continue;
		}

		variant node(ch->write());
		game_logic::wmlFormulaCallableSerializationScope::registerSerializedObject(ch);
		res.add("character", node);
	}

	for(const portal& p : portals_) {
		variant_builder node;
		node.add("rect", p.area.write());
		node.add("level", p.level_dest);
		node.add("dest_starting_pos", p.dest_starting_pos);
		node.add("dest", p.dest.write());
		node.add("automatic", p.automatic);
		node.add("transition", p.transition);
		res.add("portal", node.build());
	}

	if(right_portal_.level_dest.empty() == false) {
		res.add("next_level", right_portal_.level_dest);
	}

	LOG_INFO("PREVIOUS LEVEL: " << left_portal_.level_dest);
	if(left_portal_.level_dest.empty() == false) {
		res.add("previous_level", left_portal_.level_dest);
	}

	if(background_) {
		if(background_->id().empty()) {
			res.add("background", background_->write());
		} else {
			res.add("background", background_->id());
			res.add("background_offset", background_offset_.write());
		}
	}

	if(num_compiled_tiles_ > 0) {
		res.add("num_compiled_tiles", num_compiled_tiles_);
		for(variant compiled_node : wml_compiled_tiles_) {
			res.add("compiled_tiles", compiled_node);
		}
	}

	if(palettes_used_) {
		std::vector<variant> out;
		unsigned int p = palettes_used_;
		int id = 0;
		while(p) {
			if(p&1) {
				out.push_back(variant(graphics::get_palette_name(id)));
			}

			p >>= 1;
			++id;
		}

		res.add("palettes", variant(&out));
	}

	if(background_palette_ != -1) {
		res.add("background_palette", graphics::get_palette_name(background_palette_));
	}

	res.add("vars", vars_);

#if defined(USE_BOX2D)
	for(std::vector<box2d::body_ptr>::const_iterator it = bodies_.begin(); 
		it != bodies_.end();
		++it) {
		res.add("bodies", (*it)->write());
	}
#endif

	variant result = res.build();
	result.add_attr(variant("serialized_objects"), serialization_scope.writeObjects(result));
	return result;
}

point Level::get_dest_from_str(const std::string& key) const
{
	int ypos = 0;
	if(player()) {
		ypos = player()->getEntity().y();
	}
	if(key == "left") {
		return point(boundaries().x() + 32, ypos);
	} else if(key == "right") {
		return point(boundaries().x2() - 128, ypos);
	} else {
		return point();
	}
}

const std::string& Level::previous_level() const
{
	return left_portal_.level_dest;
}

const std::string& Level::next_level() const
{
	return right_portal_.level_dest;
}

void Level::set_previous_level(const std::string& name)
{
	left_portal_.level_dest = name;
	left_portal_.dest_str = "right";
	left_portal_.dest_starting_pos = false;
	left_portal_.automatic = true;
}

void Level::set_next_level(const std::string& name)
{
	right_portal_.level_dest = name;
	right_portal_.dest_str = "left";
	right_portal_.dest_starting_pos = false;
	right_portal_.automatic = true;
}

namespace 
{
	//counter incremented every time the level is drawn.
	int draw_count = 0;
}

void Level::draw_layer(int layer, int x, int y, int w, int h) const
{
	if(layer >= 1000 && editor_ && show_foreground_ == false) {
		return;
	}

	for(auto& i : sub_levels_) {
		if(i.second.active) {
			KRE::ModelManager2D matrix_scope(i.second.xoffset, i.second.yoffset);
			i.second.lvl->draw_layer(layer, x - i.second.xoffset, y - i.second.yoffset - TileSize, w, h + TileSize);
		}
	}

	KRE::Color color = KRE::Color::colorWhite();
	point position;

	if(editor_ && layer == highlight_layer_) {
		const float alpha = static_cast<float>(0.3f + (1.0f+sin(draw_count/5.0f))*0.35f);
		color.setAlpha(alpha);

	} else if(editor_ && hidden_layers_.count(layer)) {
		color.setAlpha(0.3f);
	}
	KRE::ColorScope color_scope(color);

	// parallax scrolling for tiles.
	auto tile_map_iterator = tile_maps_.find(layer);
	if(tile_map_iterator != tile_maps_.end()) {
		int scrollx = tile_map_iterator->second.getXSpeed();
		int scrolly = tile_map_iterator->second.getYSpeed();

		const int diffx = ((scrollx - 100)*x)/100;
		const int diffy = ((scrolly - 100)*y)/100;

		position.x = diffx;
		position.y = diffy;
		
		//here, we adjust the screen bounds (they're a first order optimization) to account for the parallax shift
		x -= diffx;
		y -= diffy;
	} 

	typedef std::vector<LevelTile>::const_iterator itor;
	std::pair<itor,itor> range = std::equal_range(tiles_.begin(), tiles_.end(), layer, level_tile_zorder_comparer());
	itor tile_itor = std::lower_bound(range.first, range.second, y, level_tile_y_pos_comparer());
	if(tile_itor == range.second) {
		return;
	}

	auto layer_itor = blit_cache_.find(layer);
	if(layer_itor == blit_cache_.end()) {
		return;
	}

	draw_layer_solid(layer, x, y, w, h);
	
	auto& blit_cache_info = *layer_itor->second;
	KRE::ModelManager2D model_matrix_scope(position.x, position.y);
	KRE::WindowManager::getMainWindow()->render(&blit_cache_info);
}

void Level::draw_layer_solid(int layer, int x, int y, int w, int h) const
{
	solid_color_rect arg;
	arg.layer = layer;
	typedef std::vector<solid_color_rect>::const_iterator SolidItor;
	std::pair<SolidItor, SolidItor> solid = std::equal_range(solid_color_rects_.begin(), solid_color_rects_.end(), arg, solid_color_rect_cmp());
	if(solid.first != solid.second) {
		const rect viewport(x, y, w, h);

		RectRenderable rr;

		while(solid.first != solid.second) {
			rect area = solid.first->area;
			if(!rects_intersect(area, viewport)) {
				++solid.first;
				continue;
			}

			area = intersection_rect(area, viewport);

			rr.update(solid.first->area, solid.first->color);
			KRE::WindowManager::getMainWindow()->render(&rr);
			
			++solid.first;
		}
	}
}

void Level::prepare_tiles_for_drawing()
{
	auto main_wnd = KRE::WindowManager::getMainWindow();
	LevelObject::setCurrentPalette(palettes_used_);

	solid_color_rects_.clear();
	blit_cache_.clear();

	for(int n = 0; n != tiles_.size(); ++n) {
		if(!is_arcade_level() && tiles_[n].object->getSolidColor()) {
			continue;
		}

		//in the editor we want to draw the whole level, so don't exclude
		//things outside the level bounds. Also if the camera is unconstrained
//		if(!editor_ && (tiles_[n].x <= boundaries().x() - TileSize || tiles_[n].y <= boundaries().y() - TileSize || tiles_[n].x >= boundaries().x2() || tiles_[n].y >= boundaries().y2())) {
//			continue;
//		}

		std::shared_ptr<LayerBlitInfo>& blit_cache_info_ptr = blit_cache_[tiles_[n].zorder];
		if(blit_cache_info_ptr == nullptr) {
			blit_cache_info_ptr = std::make_shared<LayerBlitInfo>();
		}

		if(!blit_cache_info_ptr->isInitialised()) {
			//LOG_DEBUG("zorder: " << tiles_[n].zorder << ", set texture with id: "<< tiles_[n].object->texture()->id());
			blit_cache_info_ptr->setTexture(tiles_[n].object->texture());
			blit_cache_info_ptr->setBase(tiles_[n].x, tiles_[n].y);
		}

		if(tiles_[n].x < blit_cache_info_ptr->xbase()) {
			blit_cache_info_ptr->setXbase(tiles_[n].x);
		}
		if(tiles_[n].y < blit_cache_info_ptr->ybase()) {
			blit_cache_info_ptr->setYbase(tiles_[n].y);
		}
	}

	std::map<int, std::pair<std::vector<tile_corner>, std::vector<tile_corner>>> vertices_ot;

	for(int n = 0; n != tiles_.size(); ++n) {
//		if(!editor_ && (tiles_[n].x <= boundaries().x() - TileSize || tiles_[n].y <= boundaries().y() - TileSize || tiles_[n].x >= boundaries().x2() || tiles_[n].y >= boundaries().y2())) {
//			continue;
//		}

		if(!is_arcade_level() && tiles_[n].object->getSolidColor()) {
			tiles_[n].draw_disabled = true;
			if(!solid_color_rects_.empty()) {
				solid_color_rect& r = solid_color_rects_.back();
				if(r.layer == tiles_[n].zorder && r.color == *tiles_[n].object->getSolidColor() && r.area.y() == tiles_[n].y && r.area.x() + r.area.w() == tiles_[n].x) {
					r.area = rect(r.area.x(), r.area.y(), r.area.w() + TileSize, r.area.h());
					continue;
				}
			}
				
			solid_color_rect r;
			r.color = *tiles_[n].object->getSolidColor();
			r.area = rect(tiles_[n].x, tiles_[n].y, TileSize, TileSize);
			r.layer = tiles_[n].zorder;
			solid_color_rects_.push_back(r);
			continue;
		}

		auto blit_cache_info_ptr = blit_cache_[tiles_[n].zorder];

		tiles_[n].draw_disabled = false;

		const int npoints = LevelObject::calculateTileCorners(tiles_[n].object->isOpaque() ? &vertices_ot[tiles_[n].zorder].first : &vertices_ot[tiles_[n].zorder].second, tiles_[n]);
		if(npoints > 0) {
			if(*tiles_[n].object->texture() != *blit_cache_info_ptr->getTexture()) {
				ASSERT_LOG(false, "Multiple tile textures per level per zorder are unsupported. level: '" 
					<< this->id() << "' zorder: " << tiles_[n].zorder
					<< " ; " << tiles_[n].object->texture()->isPaletteized() << " " << blit_cache_info_ptr->getTexture()->isPaletteized());
			}
		}
	}

	for(auto& v_ot : vertices_ot) {
		auto blit_cache_info_ptr = blit_cache_[v_ot.first];
		blit_cache_info_ptr->setVertices(&v_ot.second.first, &v_ot.second.second);
	}

	for(int n = 1; n < static_cast<int>(solid_color_rects_.size()); ++n) {
		solid_color_rect& a = solid_color_rects_[n-1];
		solid_color_rect& b = solid_color_rects_[n];
		if(a.area.x() == b.area.x() && a.area.x2() == b.area.x2() && a.area.y() + a.area.h() == b.area.y() && a.layer == b.layer) {
			a.area = rect(a.area.x(), a.area.y(), a.area.w(), a.area.h() + b.area.h());
			b.area = rect(0,0,0,0);
		}
	}

	solid_color_rects_.erase(std::remove_if(solid_color_rects_.begin(), solid_color_rects_.end(), solid_color_rect_empty()), solid_color_rects_.end());

	//remove tiles that are obscured by other tiles.
	std::set<std::pair<int, int> > opaque;
	for(auto n = tiles_.size(); n > 0; --n) {
		LevelTile& t = tiles_[n-1];
		ASSERT_LOG(t.object != nullptr, "Tile object is null.");
		const TileMap& map = tile_maps_[t.zorder];
		if(map.getXSpeed() != 100 || map.getYSpeed() != 100) {
			while(n > 1 && tiles_[n-1].zorder == t.zorder) {
				--n;
			}
			continue;
		}

		if(!t.draw_disabled && opaque.count(std::pair<int,int>(t.x, t.y))) {
			t.draw_disabled = true;
			continue;
		}

		if(t.object->isOpaque()) {
			opaque.insert(std::pair<int,int>(t.x, t.y));
		}
	}

}

void Level::draw_status() const
{
	if(current_speech_dialog()) {
		current_speech_dialog()->draw();
	}
}

namespace 
{
	void draw_entity(const Entity& obj, int x, int y, bool editor) 
	{
		const std::pair<int,int>* scroll_speed = obj.parallaxScaleMillis();

		int diffx = 0;
		int diffy = 0;
		if(scroll_speed) {
			const int scrollx = scroll_speed->first;
			const int scrolly = scroll_speed->second;

			diffx = ((scrollx - 1000)*x)/1000;
			diffy = ((scrolly - 1000)*y)/1000;
		}

		KRE::ModelManager2D model_scope(diffx, diffy);
		obj.draw(x, y);
		if(editor) {
			obj.drawGroup();
		}
	}

	void draw_entity_later(const Entity& obj, int x, int y, bool editor) 
	{
		const std::pair<int,int>* scroll_speed = obj.parallaxScaleMillis();

		int diffx = 0;
		int diffy = 0;
		if(scroll_speed) {
			const int scrollx = scroll_speed->first;
			const int scrolly = scroll_speed->second;

			diffx = ((scrollx - 1000)*x)/1000;
			diffy = ((scrolly - 1000)*y)/1000;
		}

		KRE::ModelManager2D model_scope(diffx, diffy);
		obj.drawLater(x, y);
	}
}

extern std::vector<rect> background_rects_drawn;

void Level::drawLater(int x, int y, int w, int h) const
{
	if(shader_) {
		ASSERT_LOG(false, "apply shader_ here");
	}
	// Delayed drawing for some elements.
	for(auto e : active_chars_) {
		draw_entity_later(*e, x, y, editor_);
	}
}

//The amount the drawing goes outside of the actual camera position.
//Used for adjustments with absolute screen position.
int g_camera_extend_x, g_camera_extend_y;

void Level::draw(int x, int y, int w, int h) const
{
	formula_profiler::Instrument instrument_prepare("LEVEL_PREPARE_DRAW");

	auto wnd = KRE::WindowManager::getMainWindow();
	if(shader_) {
		ASSERT_LOG(false, "apply shader_ here");
	}
	++draw_count;

	const int start_x = x;
	const int start_y = y;
	const int start_w = w;
	const int start_h = h;

	const int ticks = profile::get_tick_time();

	g_camera_extend_x = widest_tile_;
	g_camera_extend_y = highest_tile_;
	
	x -= widest_tile_;
	y -= highest_tile_;
	w += widest_tile_;
	h += highest_tile_;
	
	{
		{
		formula_profiler::Instrument instrument_sort("LEVEL_SORT");
		std::sort(active_chars_.begin(), active_chars_.end(), EntityZOrderCompare());
		}

		const std::vector<EntityPtr>* chars_ptr = &active_chars_;
		std::vector<EntityPtr> editor_chars_buf;

		if(editor_) {
			editor_chars_buf = active_chars_;
			rect screen_area(x, y, w, h);

			//in the editor draw all characters that are on screen as well
			//as active ones.
			for(const EntityPtr& c : chars_) {
				if(std::find(editor_chars_buf.begin(), editor_chars_buf.end(), c) != editor_chars_buf.end()) {
					continue;
				}

				if(std::find(active_chars_.begin(), active_chars_.end(), c) != active_chars_.end() || rects_intersect(c->getDrawRect(), screen_area)) {
					editor_chars_buf.push_back(c);
				}
			}

			std::sort(editor_chars_buf.begin(), editor_chars_buf.end(), zorder_compare);
			chars_ptr = &editor_chars_buf;
		}

		const std::vector<EntityPtr>& chars = *chars_ptr;
		std::vector<EntityPtr>::const_iterator entity_itor = chars.begin();

		bool water_drawn = true;
		int water_zorder = 0;
		if(water_) {
			water_drawn = false;
			water_zorder = water_->zorder();
		}

		auto& gs = graphics::GameScreen::get();

		for(auto mask : hex_masks_) {
			KRE::RenderTargetPtr rt = mask->getRenderTarget();
			if(rt.get() == nullptr) {
				rt = KRE::RenderTarget::create(gs.getVirtualWidth(), gs.getVirtualHeight());
				mask->setRenderTarget(rt);
			}

			{
				KRE::RenderTarget::RenderScope scope(rt, rect(0, 0, gs.getVirtualWidth(), gs.getVirtualHeight()));
				rt->setClearColor(KRE::Color(0,0,0,0));
				rt->clear();

				mask->preRender(wnd);
				wnd->render(mask.get());
			}
		}

		auto stencil = KRE::StencilScope::create(KRE::StencilSettings(true, 
			KRE::StencilFace::FRONT_AND_BACK,
			KRE::StencilFunc::ALWAYS,
			0xff,
			0x02,
			0x02,
			KRE::StencilOperation::KEEP,
			KRE::StencilOperation::KEEP,
			KRE::StencilOperation::REPLACE));
		wnd->clear(KRE::ClearFlags::STENCIL);


		frameBufferEnterZorder(-100000);
		const int begin_alpha_test = get_named_zorder("anura_begin_shadow_casting");
		const int end_alpha_test = get_named_zorder("shadows");

		if(scene_graph_ != nullptr) {
			scene_graph_->renderScene(rmanager_);
			rmanager_->render(KRE::WindowManager::getMainWindow());
		}

		instrument_prepare.finish();

		std::set<int>::const_iterator layer = layers_.begin();

		for(; layer != layers_.end(); ++layer) {
			if (*layer >= g_debug_skip_draw_zorder_begin && *layer < g_debug_skip_draw_zorder_end) {
				continue;
			}

			formula_profiler::Instrument instrument(formula_profiler::Instrument::generate_id("ZORDER", *layer));

			frameBufferEnterZorder(*layer);
			const bool alpha_test = *layer >= begin_alpha_test && *layer < end_alpha_test;
			graphics::set_alpha_test(alpha_test);
			stencil->updateMask(alpha_test ? 0x02 : 0x0);
			
			if(!water_drawn && *layer > water_zorder) {
				get_water()->preRender(wnd);
				wnd->render(get_water());
				water_drawn = true;
			}

			{

			CustomObjectDrawZOrderManager draw_manager;

			while(entity_itor != chars.end() && (*entity_itor)->zorder() <= *layer) {
				draw_entity(**entity_itor, x, y, editor_);
				++entity_itor;
			}

			}

			draw_layer(*layer, x, y, w, h);
		}

		if(!water_drawn) {
			get_water()->preRender(wnd);
			wnd->render(get_water());
			water_drawn = true;
		}

		int last_zorder = -1000000;
		while(entity_itor != chars.end()) {
			if((*entity_itor)->zorder() != last_zorder) {
				last_zorder = (*entity_itor)->zorder();
				frameBufferEnterZorder(last_zorder);
				const bool alpha_test = last_zorder >= begin_alpha_test && last_zorder < end_alpha_test;
				graphics::set_alpha_test(alpha_test);
				stencil->updateMask(alpha_test ? 0x02 : 0x0);
			}

			draw_entity(**entity_itor, x, y, editor_);
			++entity_itor;
		}

		graphics::set_alpha_test(false);
		frameBufferEnterZorder(1000000);

		if(editor_) {
			for(const EntityPtr& obj : chars_) {
				if(!obj->allowLevelCollisions() && entity_collides_with_level(*this, *obj, MOVE_DIRECTION::NONE)) {
					//if the entity is colliding with the level, then draw
					//it in red to mark as 'bad'.
					KRE::BlendModeScope blend_scope(KRE::BlendModeConstants::BM_SRC_ALPHA, KRE::BlendModeConstants::BM_ONE);
					const float alpha = 0.5f + std::sin(draw_count / 5.0f) * 0.5f;
					KRE::ColorScope color_scope(KRE::Color(1.0f, 0.0f, 0.0f, alpha));
					obj->draw(x, y);
				}
			}
		}

		if(editor_highlight_ || !editor_selection_.empty()) {
			if(editor_highlight_ && std::count(chars_.begin(), chars_.end(), editor_highlight_)) {
				draw_entity(*editor_highlight_, x, y, true);
			}

			for(const EntityPtr& e : editor_selection_) {
				if(std::count(chars_.begin(), chars_.end(), e)) {
					draw_entity(*e, x, y, true);
				}
			}

			KRE::BlendModeScope blend_scope(KRE::BlendModeConstants::BM_SRC_ALPHA, KRE::BlendModeConstants::BM_ONE);
			const float alpha = 0.5f + sin(draw_count / 5.0f) * 0.5f;
			KRE::ColorScope color_scope(KRE::Color(1.0f, 0.0f, 1.0f, alpha));

			if(editor_highlight_ && std::count(chars_.begin(), chars_.end(), editor_highlight_)) {
				auto color = KRE::Color(1.0f, 1.0f, 1.0f, alpha);
				if(editor_highlight_->wasSpawnedBy().empty() == false) {
					color.setBlue(0.0f);
				}
				KRE::ColorScope color_scope(color);
				draw_entity(*editor_highlight_, x, y, true);
			}

			for(const EntityPtr& e : editor_selection_) {
				if(std::count(chars_.begin(), chars_.end(), e)) {
					draw_entity(*e, x, y, true);
				}
			}
		}

		draw_debug_solid(x, y, w, h);

		if(background_) {
			background_->drawForeground(start_x, start_y, 0.0f, cycle());
		}

	}

	calculateLighting(start_x, start_y, start_w, start_h);

	if(g_debug_shadows) {
		auto stencil = KRE::StencilScope::create(KRE::StencilSettings(true, 
			KRE::StencilFace::FRONT_AND_BACK,
			KRE::StencilFunc::EQUAL,
			0xff,
			0x02,
			0x00,
			KRE::StencilOperation::KEEP,
			KRE::StencilOperation::KEEP,
			KRE::StencilOperation::KEEP));
		RectRenderable rr;
		rr.update(rect(x,y,w,h), KRE::Color(255, 255, 255, 196 + static_cast<int>(std::sin(profile::get_tick_time() / 100.0f) * 8.0f)));
		wnd->render(&rr);
	}
}

void Level::frameBufferEnterZorder(int zorder) const
{
	if(!have_render_to_texture_) {
		return;
	}

	std::vector<graphics::AnuraShaderPtr> shaders;
	for(const FrameBufferShaderEntry& e : fb_shaders_) {
		if(zorder >= e.begin_zorder && zorder <= e.end_zorder) {
			if(!e.shader) {
				if(e.shader_node.is_string()) {
					e.shader = graphics::AnuraShaderPtr(new graphics::AnuraShader(e.shader_node.as_string()));
				} else {
					e.shader = graphics::AnuraShaderPtr(new graphics::AnuraShader(e.shader_node["name"].as_string(), e.shader_node));
				}
				e.shader->setParent(nullptr);
			}
			shaders.emplace_back(e.shader);
		}
	}

	if(shaders != active_fb_shaders_ || (render_to_texture_ && !doing_render_to_texture_)) {

		bool need_flush_to_screen = true, need_new_virtual_area = true;

		if(active_fb_shaders_.empty()) {
			need_flush_to_screen = false;
		} else if(shaders.empty() && !render_to_texture_) {
			need_new_virtual_area = false;
		}

		if(need_flush_to_screen) {
			flushFrameBufferShadersToScreen();
		}

		if(need_new_virtual_area) {
			auto& gs = graphics::GameScreen::get();
			rt_->renderToThis(gs.getVirtualArea());
			rt_->setClearColor(KRE::Color(0,0,0,0));
			rt_->clear();
		}

		active_fb_shaders_ = shaders;

		doing_render_to_texture_ = render_to_texture_;
	}
}

void Level::flushFrameBufferShadersToScreen() const
{
	for(int n = 0; n != active_fb_shaders_.size(); ++n) {
		KRE::RenderTargetPtr& fb = applyShaderToFrameBufferTexture(active_fb_shaders_[n], n == active_fb_shaders_.size()-1);

		const FrameBufferShaderEntry* entry = nullptr;
		for(const FrameBufferShaderEntry& e : fb_shaders_) {
			if(e.shader == active_fb_shaders_[n]) {
				entry = &e;
				break;
			}
		}
	}
}

KRE::RenderTargetPtr& Level::applyShaderToFrameBufferTexture(graphics::AnuraShaderPtr shader, bool render_to_screen) const
{
	if(render_to_screen) {
		rt_->renderToPrevious();
	} else {
		rt_->renderToPrevious();
		auto& gs = graphics::GameScreen::get();

		if(!backup_rt_) {
			try {
				const assert_recover_scope safe_scope;
				backup_rt_ = KRE::RenderTarget::create(gs.getVirtualWidth(), gs.getVirtualHeight(), 1, false, true);			
			} catch(validation_failure_exception& /*e*/) {
				LOG_INFO("Could not create fbo with stencil buffer. Trying without stencil buffer");
				backup_rt_ = KRE::RenderTarget::create(gs.getVirtualWidth(), gs.getVirtualHeight(), 1, false, false);
			}
			ASSERT_LOG(backup_rt_ != nullptr, "Backup render target was null.");
			if(fb_render_target_.is_null()) {
				backup_rt_->setBlendState(false);
			} else {
				backup_rt_->setFromVariant(fb_render_target_);
			}
		}
		backup_rt_->renderToThis(gs.getVirtualArea());
		backup_rt_->setClearColor(KRE::Color(0,0,0,0));
		backup_rt_->clear();
	}

	KRE::ModelManager2D model_scope;
	model_scope.setIdentity();

	auto wnd = KRE::WindowManager::getMainWindow();

	auto& gs = graphics::GameScreen::get();

	rt_->setShader(shader->getShader());
	shader->setDrawArea(rect(0, 0, gs.getVirtualWidth(), gs.getVirtualHeight()));
	shader->setCycle(cycle());

	if(preferences::screen_rotated()) {
		rt_->setRotation(0, glm::vec3(0.0f, 0.0f, 1.0f));
	}

	rt_->clearBlendState();
	KRE::BlendModeScope blend_scope(KRE::BlendModeConstants::BM_SRC_ALPHA, KRE::BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);
	rt_->preRender(wnd);
	wnd->render(rt_.get());

	if(!render_to_screen) {
		std::swap(rt_, backup_rt_);
		return backup_rt_;
	} else {
		return rt_;
	}
}

void Level::shadersUpdated()
{
	for(FrameBufferShaderEntry& e : fb_shaders_) {
		e.shader.reset();
	}
}

void Level::calculateLighting(int x, int y, int w, int h) const
{
	bool fbo = KRE::DisplayDevice::checkForFeature(KRE::DisplayDeviceCapabilties::RENDER_TO_TEXTURE);
	if(!dark_ || editor_ || !fbo) {
		return;
	}
	auto wnd = KRE::WindowManager::getMainWindow();

	//find all the lights in the level
	static std::vector<Light*> lights;
	lights.clear();
	for(const EntityPtr& c : active_chars_) {
		for(const auto& lt : c->lights()) {
			lights.push_back(lt.get());
		}
	}

	auto& gs = graphics::GameScreen::get();
	static KRE::RenderTargetPtr rt;
	static int rt_width = -1;
	static int rt_height = -1;

	if (gs.getVirtualWidth() != rt_width || gs.getVirtualHeight() != rt_height) {
		rt = KRE::RenderTarget::create(gs.getVirtualWidth(), gs.getVirtualHeight());
		rt_width = gs.getVirtualWidth();
		rt_height = gs.getVirtualHeight();
	}

	{
		KRE::BlendModeScope blend_scope(KRE::BlendModeConstants::BM_ONE, KRE::BlendModeConstants::BM_ONE);
		//rect screen_area(x, y, w, h);
		
		rt->setClearColor(dark_color_.applyBlack());
		KRE::RenderTarget::RenderScope scope(rt, rect(0, 0, gs.getVirtualWidth(), gs.getVirtualHeight()));

		rt->clear();

		KRE::ColorScope color_scope(dark_color_.applyBlack());
		for(auto& lt : lights) {
			lt->preRender(wnd);
			wnd->render(lt);
		}
	}

	KRE::BlendModeScope blend_scope(KRE::BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA, KRE::BlendModeConstants::BM_SRC_ALPHA);
	rt->setPosition(x, y);
	rt->preRender(wnd);
	wnd->render(rt.get());
}

void Level::draw_debug_solid(int x, int y, int w, int h) const
{
	if(preferences::show_debug_hitboxes() == false) {
		return;
	}

	const int tile_x = x/TileSize - 2;
	const int tile_y = y/TileSize - 2;

	for(int xpos = 0; xpos < w/TileSize + 4; ++xpos) {
		for(int ypos = 0; ypos < h/TileSize + 4; ++ypos) {
			const tile_pos pos(tile_x + xpos, tile_y + ypos);
			const TileSolidInfo* info = solid_.find(pos);
			if(info == nullptr) {
				continue;
			}

			const int xpixel = (tile_x + xpos)*TileSize;
			const int ypixel = (tile_y + ypos)*TileSize;

			RectRenderable rr;
			if(info->all_solid) {
				rr.update(rect(xpixel, ypixel, TileSize, TileSize),
					info->info.damage ? KRE::Color(255, 0, 0, 196) : KRE::Color(255, 255, 255, 196));
			} else {
				std::vector<glm::u16vec2> v;

				for(int suby = 0; suby != TileSize; ++suby) {
					for(int subx = 0; subx != TileSize; ++subx) {
						if(info->bitmap.test(suby*TileSize + subx)) {
							v.emplace_back(xpixel + subx + 1, ypixel + suby + 1);
						}
					}
				}

				if(!v.empty()) {
					rr.update(&v, info->info.damage ? KRE::Color(255, 0, 0, 196) : KRE::Color(255, 255, 255, 196));
				}

			}
			KRE::WindowManager::getMainWindow()->render(&rr);
		}
	}
}

void Level::draw_background(int x, int y, int rotation, float xdelta, float ydelta) const
{
	if(show_background_ == false) {
		return;
	}
	auto wnd = KRE::WindowManager::getMainWindow();
	if(shader_) {
		ASSERT_LOG(false, "apply shader_ here");
	}

	if(background_) {
		if(rt_) {
			active_fb_shaders_.clear();
			frameBufferEnterZorder(-1000000);
		}

		static std::vector<rect> opaque_areas;
		opaque_areas.clear();

		auto& gs = graphics::GameScreen::get();
		int screen_width = gs.getVirtualWidth();
		int screen_height = gs.getVirtualHeight();
		if(last_draw_position().zoom < 1.0) {
			screen_width = static_cast<int>(screen_width / last_draw_position().zoom);
			screen_height = static_cast<int>(screen_height / last_draw_position().zoom);
		}

		rect screen_area(x - xdelta, y - ydelta, screen_width + xdelta*2, screen_height + ydelta*2);
		for(const rect& r : opaque_rects_) {
			if(rects_intersect(r, screen_area)) {

				rect intersection = intersection_rect(r, screen_area);

				if(intersection.w() == screen_area.w() || intersection.h() == screen_area.h()) {
					rect result[2];
					const auto nrects = geometry::rect_difference(screen_area, intersection, result);
					ASSERT_LOG(nrects <= 2, "TOO MANY RESULTS " << nrects << " IN " << screen_area << " - " << intersection);
					if(nrects < 1) {
						//background is completely obscured, so return
						return;
					} else if(nrects == 1) {
						screen_area = result[0];
					} else {
						opaque_areas.push_back(intersection);
					}
				} else if(intersection.w()*intersection.h() >= TileSize*TileSize*8) {
					opaque_areas.push_back(intersection);
				}
			}
		}
		background_->draw(x, y, screen_area, opaque_areas, static_cast<float>(rotation), xdelta, ydelta, cycle());
	} else {
		wnd->setClearColor(KRE::Color(0.0f, 0.0f, 0.0f, 0.0f));
		wnd->clear(KRE::ClearFlags::COLOR);
	}
}

void Level::process()
{
	formula_profiler::Instrument instrumentation("LEVEL_PROCESS");

	if(hex_map_) {
		hex_map_->process();
	}

	for(auto m : hex_masks_) {
		m->process();
	}

	if(scene_graph_ != nullptr) {
		auto current_time = profile::get_tick_time();
		const float delta_time = (current_time - last_process_time_) / 1000.0f;
		scene_graph_->process(delta_time);
		last_process_time_ = current_time;
	}

	const int LevelPreloadFrequency = 500; //10 seconds
	//see if we have levels to pre-load. Load one periodically.
	if((cycle_%LevelPreloadFrequency) == 0) {
		const int index = cycle_/LevelPreloadFrequency;
		if(index < static_cast<int>(preloads_.size())) {
			preload_level(preloads_[index]);
		}
	}

	controls::read_local_controls();

	multiplayer::send_and_receive();

	do_processing();

	if(speech_dialogs_.empty() == false) {
		if(speech_dialogs_.top()->process()) {
			speech_dialogs_.pop();
		}
	}

	editor_dragging_objects_ = false;

	//if(iso_world_) {
	//	iso_world_->process();
	//}

	sound::process();
	
	auto& gs = graphics::GameScreen::get();
	if(rt_ && rt_->needsRebuild()) {
		rt_->rebuild(gs.getVirtualWidth(), gs.getVirtualHeight());
	}
	if(backup_rt_ && backup_rt_->needsRebuild()) {
		backup_rt_->rebuild(gs.getVirtualWidth(), gs.getVirtualHeight());
	}
	for(auto mask : hex_masks_) {
		KRE::RenderTargetPtr rt = mask->getRenderTarget();
		if(rt && rt->needsRebuild()) {
			auto& gs = graphics::GameScreen::get();
			rt->rebuild(gs.getVirtualWidth(), gs.getVirtualHeight());
		}
	}

	if(shader_) {
		shader_->process();
	}
}

void Level::process_draw()
{
	for(auto& fb : fb_shaders_) {
		if(fb.shader) {
			fb.shader->process();
		}
	}

	std::vector<EntityPtr> chars = active_chars_;
	for(const EntityPtr& e : chars) {
		e->handleEvent(OBJECT_EVENT_DRAW);
	}
}

namespace 
{
	bool compare_entity_num_parents(const EntityPtr& a, const EntityPtr& b) 
	{
		bool a_human = false, b_human = false;
		const int deptha = a->parentDepth(&a_human);
		const int depthb = b->parentDepth(&b_human);
		if(a_human != b_human) {
			return b_human;
		}

		const bool standa = a->standingOn().get() ? true : false;
		const bool standb = b->standingOn().get() ? true : false;
		return deptha < depthb || (deptha == depthb && standa < standb) ||
			 (deptha == depthb && standa == standb && a->isHuman() < b->isHuman());
	}
}

void Level::set_active_chars()
{
	int screen_width = graphics::GameScreen::get().getVirtualWidth();
	int screen_height = graphics::GameScreen::get().getVirtualHeight();
	
	const float inverse_zoom_level = std::abs(zoom_level_) > FLT_EPSILON ? (1.0f / zoom_level_) : 0.0f;
	// pad the screen if we're zoomed out so stuff now-visible becomes active  
	const int zoom_buffer = static_cast<int>(std::max(0.0f, (inverse_zoom_level - 1.0f)) * screen_width);
	const int screen_left = last_draw_position().x/100 - zoom_buffer;
	const int screen_right = last_draw_position().x/100 + screen_width + zoom_buffer;
	const int screen_top = last_draw_position().y/100 - zoom_buffer;
	const int screen_bottom = last_draw_position().y/100 + screen_height + zoom_buffer;

	const rect screen_area(screen_left, screen_top, screen_right - screen_left, screen_bottom - screen_top);
	active_chars_.clear();
	std::vector<EntityPtr> objects_to_remove;
	for(EntityPtr& c : chars_) {
		const bool isActive = c->isActive(screen_area) || c->useAbsoluteScreenCoordinates();

		if(isActive) {
			if(c->group() >= 0) {
				assert(c->group() < static_cast<int>(groups_.size()));
				const entity_group& group = groups_[c->group()];
				active_chars_.insert(active_chars_.end(), group.begin(), group.end());
			} else {
				active_chars_.push_back(c);
			}
		} else { //char is inactive
			if(c->diesOnInactive()) {
				objects_to_remove.push_back(c);
			}
		}
	}

	for(auto& e : objects_to_remove) {
		remove_character(e);
	}

	std::sort(active_chars_.begin(), active_chars_.end());
	active_chars_.erase(std::unique(active_chars_.begin(), active_chars_.end()), active_chars_.end());
	std::sort(active_chars_.begin(), active_chars_.end(), zorder_compare);
}

void Level::do_processing()
{
	if(cycle_ == 0) {
		const std::vector<EntityPtr> chars = chars_;
		for(const EntityPtr& e : chars) {
			e->handleEvent(OBJECT_EVENT_START_LEVEL);
			e->createObject();
		}
	}

	if(!paused_) {
		++cycle_;
	}
/*
	if(!player_) {
		return;
	}
*/
	const int ticks = profile::get_tick_time();
	set_active_chars();
	detect_user_collisions(*this);

	int checksum = 0;
	for(const EntityPtr& e : chars_) {
		checksum += e->x() + e->y();
	}

	controls::set_checksum(cycle_, checksum);

	const int ActivationDistance = 700;

	std::vector<EntityPtr> active_chars = active_chars_;
	std::sort(active_chars.begin(), active_chars.end(), compare_entity_num_parents);
	if(time_freeze_ >= 1000) {
		time_freeze_ -= 1000;
		active_chars = chars_immune_from_time_freeze_;
	}

	{
	formula_profiler::Instrument instrumentation("CHARS_PROCESS");
	while(!active_chars.empty()) {
		new_chars_.clear();
		for(const EntityPtr& c : active_chars) {
			if(!c->destroyed()) {
				c->process(*this);
			}
	
			if(c->destroyed() && !c->isHuman()) {
				if(player_ && !c->respawn() && c->getId() != -1) {
					player_->isHuman()->objectDestroyed(id(), c->getId());
				}
	
				erase_char(c);
			}
		}

		active_chars = new_chars_;
		active_chars_.insert(active_chars_.end(), new_chars_.begin(), new_chars_.end());
	}
	}

	if(water_) {
		water_->process(*this);
	}

	solid_chars_.clear();
}

void Level::erase_char(EntityPtr c)
{
	c->beingRemoved();

	if(c->label().empty() == false) {
		chars_by_label_.erase(c->label());
	}
	chars_.erase(std::remove(chars_.begin(), chars_.end(), c), chars_.end());
	if(c->group() >= 0) {
		assert(c->group() < static_cast<int>(groups_.size()));
		entity_group& group = groups_[c->group()];
		group.erase(std::remove(group.begin(), group.end(), c), group.end());
	}

	solid_chars_.clear();
}

bool Level::isSolid(const LevelSolidMap& map, const Entity& e, const std::vector<point>& points, const SurfaceInfo** surf_info) const
{
	const TileSolidInfo* info = nullptr;
	int prev_x = std::numeric_limits<int>::min(), prev_y = std::numeric_limits<int>::min();

	const Frame& current_frame = e.getCurrentFrame();
	
	for(std::vector<point>::const_iterator p = points.begin(); p != points.end(); ++p) {
		int x, y;
		if(prev_x != std::numeric_limits<int>::min()) {
			const int diff_x = (p->x - (p-1)->x) * (e.isFacingRight() ? 1 : -1);
			const int diff_y = p->y - (p-1)->y;

			x = prev_x + diff_x;
			y = prev_y + diff_y;
			
			if(x < 0 || y < 0 || x >= TileSize || y >= TileSize) {
				//we need to recalculate the info, since we've stepped into
				//another tile.
				prev_x = std::numeric_limits<int>::min();
			}
		}
		
		if(prev_x == std::numeric_limits<int>::min()) {
			x = e.x() + (e.isFacingRight() ? p->x : (current_frame.width() - 1 - p->x));
			y = e.y() + p->y;

			tile_pos pos(x/TileSize, y/TileSize);
			x = x%TileSize;
			y = y%TileSize;
			if(x < 0) {
				pos.first--;
				x += TileSize;
			}

			if(y < 0) {
				pos.second--;
				y += TileSize;
			}

			info = map.find(pos);
		}

		if(info != nullptr) {
			if(info->all_solid) {
				if(surf_info) {
					*surf_info = &info->info;
				}

				return true;
			}
		
			const int index = y*TileSize + x;
			if(info->bitmap.test(index)) {
				if(surf_info) {
					*surf_info = &info->info;
				}

				return true;
			}
		}

		prev_x = x;
		prev_y = y;
	}

	return false;
}

bool Level::isSolid(const LevelSolidMap& map, int x, int y, const SurfaceInfo** surf_info) const
{
	tile_pos pos(x/TileSize, y/TileSize);
	x = x%TileSize;
	y = y%TileSize;
	if(x < 0) {
		pos.first--;
		x += TileSize;
	}

	if(y < 0) {
		pos.second--;
		y += TileSize;
	}

	const TileSolidInfo* info = map.find(pos);
	if(info != nullptr) {
		if(info->all_solid) {
			if(surf_info) {
				*surf_info = &info->info;
			}

			return true;
		}
		
		const int index = y*TileSize + x;
		if(info->bitmap.test(index)) {
			if(surf_info) {
				*surf_info = &info->info;
			}

			return true;
		} else {
			return false;
		}
	}

	return false;
}

bool Level::standable(const rect& r, const SurfaceInfo** info) const
{
	const int ybegin = r.y();
	const int yend = r.y2();
	const int xbegin = r.x();
	const int xend = r.x2();

	for(int y = ybegin; y != yend; ++y) {
		for(int x = xbegin; x != xend; ++x) {
			if(standable(x, y, info)) {
				return true;
			}
		}
	}

	return false;
}

bool Level::standable(int x, int y, const SurfaceInfo** info) const
{
	if(isSolid(solid_, x, y, info) || isSolid(standable_, x, y, info)) {
	   return true;
	}

	return false;
}

bool Level::standable_tile(int x, int y, const SurfaceInfo** info) const
{
	if(isSolid(solid_, x, y, info) || isSolid(standable_, x, y, info)) {
		return true;
	}
	
	return false;
}


bool Level::solid(int x, int y, const SurfaceInfo** info) const
{
	return isSolid(solid_, x, y, info);
}

bool Level::solid(const Entity& e, const std::vector<point>& points, const SurfaceInfo** info) const
{
	return isSolid(solid_, e, points, info);
}

bool Level::solid(int xbegin, int ybegin, int w, int h, const SurfaceInfo** info) const
{
	const int xend = xbegin + w;
	const int yend = ybegin + h;

	for(int y = ybegin; y != yend; ++y) {
		for(int x = xbegin; x != xend; ++x) {
			if(solid(x, y, info)) {
				return true;
			}
		}
	}

	return false;
}

bool Level::solid(const rect& r, const SurfaceInfo** info) const
{
	//TODO: consider optimizing this function.
	const int ybegin = r.y();
	const int yend = r.y2();
	const int xbegin = r.x();
	const int xend = r.x2();

	for(int y = ybegin; y != yend; ++y) {
		for(int x = xbegin; x != xend; ++x) {
			if(solid(x, y, info)) {
				return true;
			}
		}
	}

	return false;
}

bool Level::may_be_solid_in_rect(const rect& r) const
{
	int x = r.x();
	int y = r.y();
	tile_pos pos(x/TileSize, y/TileSize);
	x = x%TileSize;
	y = y%TileSize;
	if(x < 0) {
		pos.first--;
		x += TileSize;
	}

	if(y < 0) {
		pos.second--;
		y += TileSize;
	}

	const int x2 = (x + r.w())/TileSize + ((x + r.w())%TileSize ? 1 : 0);
	const int y2 = (y + r.h())/TileSize + ((y + r.h())%TileSize ? 1 : 0);

	for(int ypos = 0; ypos < y2; ++ypos) {
		for(int xpos = 0; xpos < x2; ++xpos) {
			if(solid_.find(tile_pos(pos.first + xpos, pos.second + ypos))) {
				return true;
			}
		}
	}

	return false;
}

void Level::set_solid_area(const rect& r, bool solid)
{
	std::string empty_info;
	for(int y = r.y(); y < r.y2(); ++y) {
		for(int x = r.x(); x < r.x2(); ++x) {
			setSolid(solid_, x, y, 100, 100, 0, empty_info, solid);
		}
	}
}

EntityPtr Level::board(int x, int y) const
{
	for(std::vector<EntityPtr>::const_iterator i = active_chars_.begin();
	    i != active_chars_.end(); ++i) {
		const EntityPtr& c = *i;
		if(c->boardableVehicle() && c->pointCollides(x, y)) {
			return c;
		}
	}

	return EntityPtr();
}

void Level::add_tile(const LevelTile& t)
{
	auto itor = std::lower_bound(tiles_.begin(), tiles_.end(), t, level_tile_zorder_comparer());
	tiles_.insert(itor, t);
	add_tile_solid(t);
	layers_.insert(t.zorder);
	prepare_tiles_for_drawing();
}

bool Level::add_tile_rect(int zorder, int x1, int y1, int x2, int y2, const std::string& str)
{
	return addTileRectVector(zorder, x1, y1, x2, y2, std::vector<std::string>(1, str));
}

bool Level::addTileRectVector(int zorder, int x1, int y1, int x2, int y2, const std::vector<std::string>& tiles)
{
	if(x1 > x2) {
		std::swap(x1, x2);
	}

	if(y1 > y2) {
		std::swap(y1, y2);
	}
	return add_tile_rect_vector_internal(zorder, x1, y1, x2, y2, tiles);
}

void Level::set_tile_layer_speed(int zorder, int x_speed, int y_speed)
{
	TileMap& m = tile_maps_[zorder];
	m.setZOrder(zorder);
	m.setSpeed(x_speed, y_speed);
}

void Level::refresh_tile_rect(int x1, int y1, int x2, int y2)
{
	rebuild_tiles_rect(rect(x1-128, y1-128, (x2 - x1) + 256, (y2 - y1) + 256));
}

namespace 
{
	int round_tile_size(int n)
	{
		if(n >= 0) {
			return n - n%TileSize;
		} else {
			n = -n + TileSize;
			return -(n - n%TileSize);
		}
	}
}

bool Level::add_tile_rect_vector_internal(int zorder, int x1, int y1, int x2, int y2, const std::vector<std::string>& tiles)
{
	if(tiles.empty()) {
		return false;
	}

	if(x1 > x2) {
		std::swap(x1, x2);
	}

	if(y1 > y2) {
		std::swap(y1, y2);
	}

	x1 = round_tile_size(x1);
	y1 = round_tile_size(y1);
	x2 = round_tile_size(x2 + TileSize);
	y2 = round_tile_size(y2 + TileSize);

	TileMap& m = tile_maps_[zorder];
	m.setZOrder(zorder);

	bool changed = false;

	int index = 0;
	for(int x = x1; x < x2; x += TileSize) {
		for(int y = y1; y < y2; y += TileSize) {
			changed = m.setTile(x, y, tiles[index]) || changed;
			if(index+1 < static_cast<int>(tiles.size())) {
				++index;
			}
		}
	}

	return changed;
}

void Level::get_tile_rect(int zorder, int x1, int y1, int x2, int y2, std::vector<std::string>& tiles) const
{
	if(x1 > x2) {
		std::swap(x1, x2);
	}

	if(y1 > y2) {
		std::swap(y1, y2);
	}

	x1 = round_tile_size(x1);
	y1 = round_tile_size(y1);
	x2 = round_tile_size(x2 + TileSize);
	y2 = round_tile_size(y2 + TileSize);

	auto map_iterator = tile_maps_.find(zorder);
	if(map_iterator == tile_maps_.end()) {
		tiles.push_back("");
		return;
	}
	const TileMap& m = map_iterator->second;

	for(int x = x1; x < x2; x += TileSize) {
		for(int y = y1; y < y2; y += TileSize) {
			tiles.push_back(m.getTileFromPixelPos(x, y));
		}
	}
}

void Level::getAllTilesRect(int x1, int y1, int x2, int y2, std::map<int, std::vector<std::string> >& tiles) const
{
	for(std::set<int>::const_iterator i = layers_.begin(); i != layers_.end(); ++i) {
		if(hidden_layers_.count(*i)) {
			continue;
		}

		std::vector<std::string> cleared;
		get_tile_rect(*i, x1, y1, x2, y2, cleared);
		if(std::count(cleared.begin(), cleared.end(), "") != cleared.size()) {
			tiles[*i].swap(cleared);
		}
	}
}

bool Level::clear_tile_rect(int x1, int y1, int x2, int y2)
{
	if(x1 > x2) {
		std::swap(x1, x2);
	}

	if(y1 > y2) {
		std::swap(y1, y2);
	}

	bool changed = false;
	std::vector<std::string> v(1, "");
	for(std::set<int>::const_iterator i = layers_.begin(); i != layers_.end(); ++i) {
		if(hidden_layers_.count(*i)) {
			continue;
		}

		if(add_tile_rect_vector_internal(*i, x1, y1, x2, y2, v)) {
			changed = true;
		}
	}
	
	return changed;
}

void Level::add_tile_solid(const LevelTile& t)
{
	//zorders greater than 1000 are considered in the foreground and so
	//have no solids.
	if(t.zorder >= 1000) {
		return;
	}

	if(t.object->width() > widest_tile_) {
		widest_tile_ = t.object->width();
	}

	if(t.object->height() > highest_tile_) {
		highest_tile_ = t.object->height();
	}

	const ConstLevelObjectPtr& obj = t.object;
	if(obj->allSolid()) {
		add_solid_rect(t.x, t.y, t.x + obj->width(), t.y + obj->height(), obj->friction(), obj->traction(), obj->damage(), obj->info());
		return;
	}

	if(obj->hasSolid()) {
		for(int y = 0; y != obj->height(); ++y) {
			for(int x = 0; x != obj->width(); ++x) {
				int xpos = x;
				if(!t.face_right) {
					xpos = obj->width() - x - 1;
				}
				if(obj->isSolid(xpos, y)) {
					if(obj->isPassthrough()) {
						add_standable(t.x + x, t.y + y, obj->friction(), obj->traction(), obj->damage(), obj->info());
					} else {
						add_solid(t.x + x, t.y + y, obj->friction(), obj->traction(), obj->damage(), obj->info());
					}
				}
			}
		}
	}
}

struct tile_on_point {
	int x_, y_;
	tile_on_point(int x, int y) : x_(x), y_(y)
	{}

	bool operator()(const LevelTile& t) const {
		return x_ >= t.x && y_ >= t.y && x_ < t.x + t.object->width() && y_ < t.y + t.object->height();
	}
};

bool Level::remove_tiles_at(int x, int y)
{
	const auto nitems = tiles_.size();
	tiles_.erase(std::remove_if(tiles_.begin(), tiles_.end(), tile_on_point(x,y)), tiles_.end());
	const bool result = nitems != tiles_.size();
	prepare_tiles_for_drawing();
	return result;
}

std::vector<point> Level::get_solid_contiguous_region(int xpos, int ypos) const
{
	std::vector<point> result;

	xpos = round_tile_size(xpos);
	ypos = round_tile_size(ypos);

	tile_pos base(xpos/TileSize, ypos/TileSize);
	const TileSolidInfo* info = solid_.find(base);
	if(info == nullptr || (info->all_solid == false && info->bitmap.any() == false)) {
		return result;
	}

	std::set<tile_pos> positions;
	positions.insert(base);

	std::set<std::pair<int,int>>::size_type last_count = -1;
	while(positions.size() != last_count) {
		last_count = positions.size();

		std::vector<tile_pos> new_positions;
		for(const tile_pos& pos : positions) {
			new_positions.push_back(std::make_pair(pos.first-1, pos.second));
			new_positions.push_back(std::make_pair(pos.first+1, pos.second));
			new_positions.push_back(std::make_pair(pos.first, pos.second-1));
			new_positions.push_back(std::make_pair(pos.first, pos.second+1));
		}

		for(const tile_pos& pos : new_positions) {
			if(positions.count(pos)) {
				continue;
			}

			const TileSolidInfo* info = solid_.find(pos);
			if(info == nullptr || (info->all_solid == false && info->bitmap.any() == false)) {
				continue;
			}

			positions.insert(pos);
		}
	}

	for(const tile_pos& pos : positions) {
		result.push_back(point(pos.first, pos.second));
	}

	return result;
}

const LevelTile* Level::getTileAt(int x, int y) const
{
	auto i = std::find_if(tiles_.begin(), tiles_.end(), tile_on_point(x,y));
	if(i != tiles_.end()) {
		return &*i;
	} else {
		return nullptr;
	}
}

void Level::remove_character(EntityPtr e)
{
	e->beingRemoved();
	if(e->label().empty() == false) {
		chars_by_label_.erase(e->label());
	}
	chars_.erase(std::remove(chars_.begin(), chars_.end(), e), chars_.end());
	solid_chars_.erase(std::remove(solid_chars_.begin(), solid_chars_.end(), e), solid_chars_.end());
	active_chars_.erase(std::remove(active_chars_.begin(), active_chars_.end(), e), active_chars_.end());
	new_chars_.erase(std::remove(new_chars_.begin(), new_chars_.end(), e), new_chars_.end());
}

std::vector<EntityPtr> Level::get_characters_in_rect(const rect& r, int screen_xpos, int screen_ypos) const
{
	std::vector<EntityPtr> res;
	for(EntityPtr c : chars_) {
		if(object_classification_hidden(*c)) {
			continue;
		}
		CustomObject* obj = dynamic_cast<CustomObject*>(c.get());

		const int xP = c->getMidpoint().x + ((c->parallaxScaleMillisX() - 1000)*screen_xpos)/1000 
			+ (obj->useAbsoluteScreenCoordinates() ? screen_xpos + absolute_object_adjust_x() : 0);
		const int yP = c->getMidpoint().y + ((c->parallaxScaleMillisY() - 1000)*screen_ypos)/1000 
			+ (obj->useAbsoluteScreenCoordinates() ? screen_ypos + absolute_object_adjust_y() : 0);
		if(pointInRect(point(xP, yP), r)) {
			res.push_back(c);
		}
	}

	return res;
}

std::vector<EntityPtr> Level::get_characters_at_point(int x, int y, int screen_xpos, int screen_ypos) const
{
	std::vector<EntityPtr> result;
	for(EntityPtr c : chars_) {
		if(object_classification_hidden(*c)) {
			continue;
		}

		const int xP = x + ((1000 - (c->parallaxScaleMillisX()))* screen_xpos )/1000
			- (c->useAbsoluteScreenCoordinates() ? screen_xpos + absolute_object_adjust_x() : 0);
		const int yP = y + ((1000 - (c->parallaxScaleMillisY()))* screen_ypos )/1000
			- (c->useAbsoluteScreenCoordinates() ? screen_ypos + absolute_object_adjust_y() : 0);

		if(!c->isAlpha(xP, yP)) {
			result.push_back(c);
		}
	}
	
	return result;
}

namespace 
{
	bool compare_entities_by_spawned(EntityPtr a, EntityPtr b)
	{
		return a->wasSpawnedBy().size() < b->wasSpawnedBy().size();
	}
}

EntityPtr Level::get_next_character_at_point(int x, int y, int screen_xpos, int screen_ypos, const void* currently_selected) const
{
	std::vector<EntityPtr> v = get_characters_at_point(x, y, screen_xpos, screen_ypos);
	if(v.empty()) {
		return EntityPtr();
	}

	std::sort(v.begin(), v.end(), compare_entities_by_spawned);

	if(currently_selected == nullptr && editor_selection_.empty() == false) {
		currently_selected = editor_selection_.back().get();
	}

	if(currently_selected == nullptr) {
		return v.front();
	}

	std::vector<EntityPtr>::iterator itor = std::find(v.begin(), v.end(), currently_selected);
	if(itor == v.end()) {
		return v.front();
	}

	++itor;
	if(itor == v.end()) {
		itor = v.begin();
	}

	return *itor;
}

void Level::add_solid_rect(int x1, int y1, int x2, int y2, int friction, int traction, int damage, const std::string& info_str)
{
	if((x1%TileSize) != 0 || (y1%TileSize) != 0 ||
	   (x2%TileSize) != 0 || (y2%TileSize) != 0) {
		for(int y = y1; y < y2; ++y) {
			for(int x = x1; x < x2; ++x) {
				add_solid(x, y, friction, traction, damage, info_str);
			}
		}

		return;
	}

	for(int y = y1; y < y2; y += TileSize) {
		for(int x = x1; x < x2; x += TileSize) {
			tile_pos pos(x/TileSize, y/TileSize);
			TileSolidInfo& s = solid_.insertOrFind(pos);
			s.all_solid = true;
			s.info.friction = friction;
			s.info.traction = traction;

			if(s.info.damage >= 0) {
				s.info.damage = std::min(s.info.damage, damage);
			} else {
				s.info.damage = damage;
			}

			if(info_str.empty() == false) {
				s.info.info = SurfaceInfo::get_info_str(info_str);
			}
		}
	}
}

void Level::add_solid(int x, int y, int friction, int traction, int damage, const std::string& info)
{
	setSolid(solid_, x, y, friction, traction, damage, info);
}

void Level::add_standable(int x, int y, int friction, int traction, int damage, const std::string& info)
{
	setSolid(standable_, x, y, friction, traction, damage, info);
}

void Level::setSolid(LevelSolidMap& map, int x, int y, int friction, int traction, int damage, const std::string& info_str, bool solid)
{
	tile_pos pos(x/TileSize, y/TileSize);
	x = x%TileSize;
	y = y%TileSize;
	if(x < 0) {
		pos.first--;
		x += TileSize;
	}

	if(y < 0) {
		pos.second--;
		y += TileSize;
	}
	const int index = y*TileSize + x;
	TileSolidInfo& info = map.insertOrFind(pos);

	if(info.info.damage >= 0) {
		info.info.damage = std::min(info.info.damage, damage);
	} else {
		info.info.damage = damage;
	}

	if(solid) {
		info.info.friction = friction;
		info.info.traction = traction;
		info.bitmap.set(index);
	} else {
		if(info.all_solid) {
			info.all_solid = false;
			info.bitmap.set();
		}

		info.bitmap.reset(index);
	}

	if(info_str.empty() == false) {
		info.info.info = SurfaceInfo::get_info_str(info_str);
	}
}

void Level::add_multi_player(EntityPtr p)
{
	last_touched_player_ = p;
	p->getPlayerInfo()->setPlayerSlot(static_cast<int>(players_.size()));
	ASSERT_LOG(!g_player_type || g_player_type->match(variant(p.get())), "Player object being added to level does not match required player type. " << p->getDebugDescription() << " is not a " << g_player_type->to_string());
	players_.push_back(p);
	chars_.push_back(p);
	if(p->label().empty() == false) {
		chars_by_label_[p->label()] = p;
	}
	layers_.insert(p->zorder());
}

void Level::add_player(EntityPtr p)
{
	const int nslot = p->getPlayerInfo()->getPlayerSlot();

	if(players_.size() > nslot && players_[nslot]) {
		if(players_[nslot] != p) {
			players_[nslot]->beingRemoved();
		}

		if(players_[nslot]->label().empty() == false) {
			chars_by_label_.erase(players_[nslot]->label());
		}

		chars_.erase(std::remove(chars_.begin(), chars_.end(), players_[nslot]), chars_.end());
	}

	if(LevelRunner::getCurrent()) {
		LevelRunner::getCurrent()->on_player_set(p);
	}

	last_touched_player_ = player_ = p;
	ASSERT_LOG(!g_player_type || g_player_type->match(variant(p.get())), "Player object being added to level does not match required player type. " << p->getDebugDescription() << " is not a " << g_player_type->to_string());
	if(players_.size() <= nslot) {
		player_->getPlayerInfo()->setPlayerSlot(static_cast<int>(players_.size()));
		players_.push_back(player_);
	} else {
		ASSERT_LOG(player_->isHuman(), "Level::add_player(): Tried to add player to the level that isn't human.");
		players_[nslot] = player_;
	}

	p->addToLevel();

	assert(player_);
	chars_.push_back(p);

	//remove objects that have already been destroyed
	const std::vector<int>& destroyed_objects = player_->getPlayerInfo()->getObjectsDestroyed(id());
	for(int n = 0; n != chars_.size(); ++n) {
		if(chars_[n]->respawn() == false && std::binary_search(destroyed_objects.begin(), destroyed_objects.end(), chars_[n]->getId())) {
			if(chars_[n]->label().empty() == false) {
				chars_by_label_.erase(chars_[n]->label());
			}
			chars_[n] = EntityPtr();
		}
	}

	if(!editor_) {
		const int difficulty = current_difficulty();
		for(int n = 0; n != chars_.size(); ++n) {
			if(chars_[n].get() != nullptr && !chars_[n]->appearsAtDifficulty(difficulty)) {
				chars_[n] = EntityPtr();
			}
		}
	}

	chars_.erase(std::remove(chars_.begin(), chars_.end(), EntityPtr()), chars_.end());
}

void Level::add_character(EntityPtr p)
{

	ASSERT_LOG(p->label().empty() == false, "Entity has no label");

	if(p->label().empty() == false) {
		EntityPtr& target = chars_by_label_[p->label()];
		if(!target) {
			target = p;
		} else if(target == p) {
			return;
		} else {
			while(chars_by_label_[p->label()]) {
				p->setLabel(formatter() << p->label() << rand());
			}

			chars_by_label_[p->label()] = p;
		}
	}

	if(solid_chars_.empty() == false && p->solid()) {
		solid_chars_.push_back(p);
	}

	if(p->isHuman()) {
		add_player(p);
	} else {
		chars_.push_back(p);
	}

	p->addToLevel();

	layers_.insert(p->zorder());

	const int screen_left = last_draw_position().x/100;
	const int screen_right = last_draw_position().x/100 + KRE::WindowManager::getMainWindow()->width();
	const int screen_top = last_draw_position().y/100;
	const int screen_bottom = last_draw_position().y/100 + KRE::WindowManager::getMainWindow()->height();

	const rect screen_area(screen_left, screen_top, screen_right - screen_left, screen_bottom - screen_top);
	if(!active_chars_.empty() && (p->isActive(screen_area) || p->useAbsoluteScreenCoordinates())) {
		new_chars_.push_back(p);
	}
	p->beingAdded();
}

void Level::add_draw_character(EntityPtr p)
{
	active_chars_.push_back(p);
}

void Level::force_enter_portal(const portal& p)
{
	entered_portal_active_ = true;
	entered_portal_ = p;
}

const Level::portal* Level::get_portal() const
{
	if(entered_portal_active_) {
		entered_portal_active_ = false;
		return &entered_portal_;
	}

	if(!player_) {
		return nullptr;
	}

	const rect& r = player_->getBodyRect();
	if(r.x() < boundaries().x() && left_portal_.level_dest.empty() == false) {
		return &left_portal_;
	}

	if(r.x2() > boundaries().x2() && right_portal_.level_dest.empty() == false) {
		return &right_portal_;
	}
	for(const portal& p : portals_) {
		if(rects_intersect(r, p.area) && (p.automatic || player_->enter())) {
			return &p;
		}
	}

	return nullptr;
}

int Level::group_size(int group) const
{
	int res = 0;
	for(const EntityPtr& c : active_chars_) {
		if(c->group() == group) {
			++res;
		}
	}

	return res;
}

void Level::set_character_group(EntityPtr c, int group_num)
{
	assert(group_num < static_cast<int>(groups_.size()));

	//remove any current grouping
	if(c->group() >= 0) {
		assert(c->group() < static_cast<int>(groups_.size()));
		entity_group& group = groups_[c->group()];
		group.erase(std::remove(group.begin(), group.end(), c), group.end());
	}

	c->setGroup(group_num);

	if(group_num >= 0) {
		entity_group& group = groups_[group_num];
		group.push_back(c);
	}
}

int Level::add_group()
{
	groups_.resize(groups_.size() + 1);
	return static_cast<int>(groups_.size() - 1);
}

void Level::editor_select_object(EntityPtr c)
{
	if(!c) {
		return;
	}
	editor_selection_.push_back(c);
}

void Level::editor_deselect_object(EntityPtr c)
{
	editor_selection_.erase(std::remove(editor_selection_.begin(), editor_selection_.end(), c), editor_selection_.end());
}

void Level::editor_clear_selection()
{
	editor_selection_.clear();
}

const std::string& Level::get_background_id() const
{
	if(background_) {
		return background_->id();
	} else {
		static const std::string empty_string;
		return empty_string;
	}
}

void Level::set_background_by_id(const std::string& id)
{
	background_ = Background::get(id, background_palette_);
}

BEGIN_DEFINE_CALLABLE_NOBASE(Level)
DEFINE_FIELD(title, "string")
	return variant(obj.title());
DEFINE_FIELD(music, "string")
	return variant(obj.music());
DEFINE_FIELD(cycle, "int")
	return variant(obj.cycle_);
DEFINE_SET_FIELD
	obj.cycle_ = value.as_int();
	controls::new_level(obj.cycle_, obj.players_.empty() ? 1 : static_cast<int>(obj.players_.size()), multiplayer::slot());
DEFINE_FIELD(player, "custom_obj")
	ASSERT_LOG(obj.last_touched_player_, "No player found in level");
	return variant(obj.last_touched_player_.get());
DEFINE_SET_FIELD
	obj.last_touched_player_ = obj.player_ = EntityPtr(value.convert_to<Entity>());
DEFINE_FIELD(player_info, "object")
	ASSERT_LOG(obj.last_touched_player_, "No player found in level");
	return variant(obj.last_touched_player_.get());
DEFINE_FIELD(in_dialog, "bool")
	//ffl::IntrusivePtr<const game_logic::FormulaCallableDefinition> def(variant_type::get_builtin("level")->getDefinition());
	return variant::from_bool(obj.in_dialog_);
DEFINE_FIELD(local_player, "null|custom_obj")
	ASSERT_LOG(obj.player_, "No player found in level");
	return variant(obj.player_.get());
DEFINE_FIELD(num_active, "int")
	return variant(static_cast<int>(obj.active_chars_.size()));
DEFINE_FIELD(active_chars, "[custom_obj]")
	std::vector<variant> v;
	for(const EntityPtr& e : obj.active_chars_) {
		v.push_back(variant(e.get()));
	}
	return variant(&v);
DEFINE_FIELD(chars, "[custom_obj]")
	std::vector<variant> v;
	for(const EntityPtr& e : obj.chars_) {
		v.push_back(variant(e.get()));
	}
	return variant(&v);
DEFINE_FIELD(players, "[custom_obj]")
	std::vector<variant> v;
	for(const EntityPtr& e : obj.players()) {
		v.push_back(variant(e.get()));
	}
	return variant(&v);
DEFINE_SET_FIELD
	std::vector<variant> list = value.as_list();
	int nslot = 0;
	for(variant p : list) {
		EntityPtr pl(p.convert_to<Entity>());
		pl->getPlayerInfo()->setPlayerSlot(nslot);
		obj.add_character(pl);
		++nslot;
	}

DEFINE_FIELD(in_editor, "bool")
	return variant::from_bool(obj.editor_);
DEFINE_FIELD(editor, "null|builtin editor")
	if(LevelRunner::getCurrent()) {
		return variant(LevelRunner::getCurrent()->get_editor().get());
	}

	return variant();
	
DEFINE_FIELD(zoom, "decimal")
	return variant(obj.zoom_level_);
DEFINE_SET_FIELD
	obj.zoom_level_ = value.as_float();
DEFINE_FIELD(instant_zoom, "decimal")
return variant(obj.zoom_level_);
DEFINE_SET_FIELD
obj.zoom_level_ = value.as_float();
obj.instant_zoom_level_set_ = obj.cycle_;

DEFINE_FIELD(focus, "[custom_obj]")
	std::vector<variant> v;
	for(const EntityPtr& e : obj.focus_override_) {
		v.push_back(variant(e.get()));
	}
	return variant(&v);
DEFINE_SET_FIELD
	obj.focus_override_.clear();
	for(int n = 0; n != value.num_elements(); ++n) {
		Entity* e = value[n].try_convert<Entity>();
		if(e) {
			obj.focus_override_.push_back(EntityPtr(e));
			LOG_DEBUG("entity '" << e->label() << "' added as focus override");
		}
	}

DEFINE_FIELD(id, "string")
	return variant(obj.id_);

DEFINE_FIELD(dimensions, "[int,int,int,int]")
	std::vector<variant> v;
	v.push_back(variant(obj.boundaries_.x()));
	v.push_back(variant(obj.boundaries_.y()));
	v.push_back(variant(obj.boundaries_.x2()));
	v.push_back(variant(obj.boundaries_.y2()));
	return variant(&v);
DEFINE_SET_FIELD
	ASSERT_EQ(value.num_elements(), 4);
	obj.boundaries_ = rect(value[0].as_int(), value[1].as_int(), value[2].as_int() - value[0].as_int(), value[3].as_int() - value[1].as_int());

DEFINE_FIELD(constrain_camera, "bool")
	return variant::from_bool(obj.constrain_camera());
DEFINE_SET_FIELD
	obj.constrain_camera_ = value.as_bool();
DEFINE_FIELD(music_volume, "decimal")
	return variant(sound::get_engine_music_volume());
DEFINE_SET_FIELD
	sound::set_engine_music_volume(value.as_float());
DEFINE_FIELD(paused, "bool")
	return variant::from_bool(obj.paused_);
DEFINE_SET_FIELD
	const bool new_value = value.as_bool();
	if(new_value != obj.paused_) {
		obj.paused_ = new_value;
		if(obj.paused_) {
			obj.before_pause_controls_backup_.reset(new controls::control_backup_scope);
		} else {
			if(&obj != getCurrentPtr()) {
				obj.before_pause_controls_backup_->cancel();
			}
			obj.before_pause_controls_backup_.reset();
		}
		for(EntityPtr e : obj.chars_) {
			e->mutateValue("paused", value);
		}
	}

DEFINE_FIELD(module_args, "object")
	return variant(module::get_module_args().get());

#if defined(USE_BOX2D)
DEFINE_FIELD(world, "object")
	return variant(box2d::world::our_world_ptr().get());
#else
DEFINE_FIELD(world, "null")
	return variant();
#endif

DEFINE_FIELD(time_freeze, "int")
	return variant(obj.time_freeze_);
DEFINE_SET_FIELD
	obj.time_freeze_ = value.as_int();
DEFINE_FIELD(chars_immune_from_time_freeze, "[custom_obj]")
	std::vector<variant> v;
	for(const EntityPtr& e : obj.chars_immune_from_time_freeze_) {
		v.push_back(variant(e.get()));
	}
	return variant(&v);
DEFINE_SET_FIELD
	obj.chars_immune_from_time_freeze_.clear();
	for(int n = 0; n != value.num_elements(); ++n) {
		EntityPtr e(value[n].try_convert<Entity>());
		if(e) {
			obj.chars_immune_from_time_freeze_.push_back(e);
		}
	}

DEFINE_FIELD(segment_width, "int")
	return variant(obj.segment_width_);
DEFINE_FIELD(segment_height, "int")
	return variant(obj.segment_height_);
DEFINE_FIELD(num_segments, "int")
	return variant(unsigned(obj.sub_levels_.size()));

DEFINE_FIELD(camera_position, "[int, int, int, int]")
	std::vector<variant> pos;
	pos.reserve(4);
	pos.push_back(variant(last_draw_position().x/100));
	pos.push_back(variant(last_draw_position().y/100));

	auto& gs = graphics::GameScreen::get();
	pos.push_back(variant(gs.getVirtualWidth()));
	pos.push_back(variant(gs.getVirtualHeight()));
	return variant(&pos);
DEFINE_SET_FIELD_TYPE("[decimal,decimal]")

	ASSERT_EQ(value.num_elements(), 2);
	last_draw_position().x_pos = last_draw_position().x = (value[0].as_decimal()*100).as_int();
	last_draw_position().y_pos = last_draw_position().y = (value[1].as_decimal()*100).as_int();

DEFINE_FIELD(camera_target, "[int,int]")
	std::vector<variant> pos;
	pos.reserve(2);

	pos.push_back(variant(last_draw_position().target_xpos));
	pos.push_back(variant(last_draw_position().target_ypos));

	return variant(&pos);
	
DEFINE_FIELD(zoom_current, "decimal")
	return variant(last_draw_position().zoom);
	

DEFINE_FIELD(debug_properties, "[string]")
	return vector_to_variant(obj.debug_properties_);
DEFINE_SET_FIELD
	if(value.is_null()) {
		obj.debug_properties_.clear();
	} else if(value.is_string()) {
		obj.debug_properties_.clear();
		obj.debug_properties_.push_back(value.as_string());
	} else {
		obj.debug_properties_ = value.as_list_string();
	}

DEFINE_FIELD(is_paused, "bool")
	if(LevelRunner::getCurrent()) {
		return variant::from_bool(LevelRunner::getCurrent()->is_paused());
	}

	return variant(false);

DEFINE_FIELD(editor_selection, "[custom_obj]")
	std::vector<variant> result;
	for(EntityPtr s : obj.editor_selection_) {
		result.push_back(variant(s.get()));
	}

	return variant(&result);

DEFINE_FIELD(frame_buffer_shaders, "[{begin_zorder: int, end_zorder: int, shader: object|null, shader_info: map|string, label: string|null}]")
	std::vector<variant> v;
	for(const FrameBufferShaderEntry& e : obj.fb_shaders_) {
		std::map<variant,variant> m;
		m[variant("label")] = variant(e.label);
		m[variant("begin_zorder")] = variant(e.begin_zorder);
		m[variant("end_zorder")] = variant(e.end_zorder);
		m[variant("shader_info")] = e.shader_node;

		m[variant("shader")] = variant(e.shader.get());
		v.push_back(variant(&m));
	}

	obj.fb_shaders_variant_ = variant(&v);
	return obj.fb_shaders_variant_;

DEFINE_SET_FIELD
	obj.fb_shaders_variant_ = variant();
	obj.fb_shaders_.clear();
	for(const variant& v : value.as_list()) {
		FrameBufferShaderEntry e;
		if(v.has_key("label")) {
			e.label = v["label"].as_string();
		}

		e.begin_zorder = v["begin_zorder"].as_int();
		e.end_zorder = v["end_zorder"].as_int();
		e.shader_node = v["shader_info"];

		if(v.has_key("shader")) {
			e.shader.reset(v["shader"].try_convert<graphics::AnuraShader>());
		}

		if(!e.shader) {
			if(e.shader_node.is_string()) {
				e.shader = graphics::AnuraShaderPtr(new graphics::AnuraShader(e.shader_node.as_string()));
			} else {
				e.shader = graphics::AnuraShaderPtr(new graphics::AnuraShader(e.shader_node["name"].as_string(), e.shader_node));
			}
			e.shader->setParent(nullptr);
		}

		obj.fb_shaders_.push_back(e);
	}

DEFINE_FIELD(preferences, "object")
	return variant(preferences::get_settings_obj());
DEFINE_FIELD(lock_screen, "null|[int]")
	if(obj.lock_screen_.get()) {
		std::vector<variant> v;
		v.push_back(variant(obj.lock_screen_->x));
		v.push_back(variant(obj.lock_screen_->y));
		return variant(&v);
	} else {
		return variant();
	}
DEFINE_SET_FIELD
	if(value.is_list()) {
		obj.lock_screen_.reset(new point(value[0].as_int(), value[1].as_int()));
	} else {
		obj.lock_screen_.reset();
	}

DEFINE_FIELD(shader, "builtin anura_shader")
	return variant(obj.shader_.get());
DEFINE_SET_FIELD_TYPE("string|map|builtin anura_shader")
	if(value.is_string()) {
		obj.shader_ = graphics::AnuraShaderPtr(new graphics::AnuraShader(value.as_string()));
	} else if(value.is_map()) {
		obj.shader_ = graphics::AnuraShaderPtr(new graphics::AnuraShader(value["name"].as_string(), value));
	} else {
		graphics::AnuraShaderPtr shader_ptr = value.try_convert<graphics::AnuraShader>();
		ASSERT_LOG(shader_ptr != nullptr, "shader wasn't valid to set: " << value.to_debug_string());
		obj.shader_ = shader_ptr;
	}

//DEFINE_FIELD(isoworld, "builtin world")
//	ASSERT_LOG(obj.iso_world_, "No world present in level");
//	return variant(obj.iso_world_.get());
//DEFINE_SET_FIELD_TYPE("builtin world|map|null")
//	if(value.is_null()) {
//		obj.iso_world_.reset(); 
//	} else {
//		obj.iso_world_.reset(new voxel::World(value));
//	}

DEFINE_FIELD(mouselook, "bool")
	return variant::from_bool(obj.is_mouselook_enabled());
DEFINE_SET_FIELD
	obj.set_mouselook(value.as_bool());

DEFINE_FIELD(mouselook_invert, "bool")
#if defined(USE_ISOMAP)
	return variant::from_bool(obj.is_mouselook_inverted());
#else
	return variant::from_bool(false);
#endif
DEFINE_SET_FIELD
#if defined(USE_ISOMAP)
	obj.set_mouselook_inverted(value.as_bool());
#endif

DEFINE_FIELD(suspended_level, "builtin level")
	ASSERT_LOG(obj.suspended_level_, "Query of suspended_level when there is no suspended level");
	return variant(obj.suspended_level_.get());

DEFINE_FIELD(show_builtin_settings_dialog, "bool")
	return variant::from_bool(obj.show_builtin_settings_);
DEFINE_SET_FIELD
	obj.show_builtin_settings_ = value.as_bool();

DEFINE_FIELD(hex_map, "null|builtin hex_map") // builtin hex_map
	return variant(obj.hex_map_.get());
DEFINE_SET_FIELD_TYPE("null|map")
	if(obj.hex_renderable_) {
		obj.scene_graph_->getRootNode()->removeNode(obj.hex_renderable_);
	}

	if(value.is_map()) {
		obj.hex_map_ = hex::HexMap::create(value);
		obj.hex_renderable_ = std::dynamic_pointer_cast<hex::MapNode>(obj.scene_graph_->createNode("hex_map"));
		obj.hex_map_->setRenderable(obj.hex_renderable_);
		obj.scene_graph_->getRootNode()->attachNode(obj.hex_renderable_);
	} else {
		obj.hex_map_.reset();
		obj.hex_renderable_.reset();
	}

DEFINE_FIELD(hex_masks, "[builtin mask_node]")
	std::vector<variant> result;
	for(auto mask : obj.hex_masks_) {
		result.emplace_back(variant(mask.get()));
	}

	return variant(&result);
DEFINE_SET_FIELD_TYPE("[map|builtin mask_node]")
	std::vector<variant> items = value.as_list();
	obj.hex_masks_.clear();
	for(auto v : items) {
		if(v.is_map()) {
			obj.hex_masks_.emplace_back(hex::MaskNodePtr(new hex::MaskNode(v)));
		} else {
			obj.hex_masks_.emplace_back(hex::MaskNodePtr(v.convert_to<hex::MaskNode>()));
		}

		ASSERT_LOG(obj.hex_masks_.back().get() != nullptr, "null hex mask");
	}

DEFINE_FIELD(fb_render_target, "map")
	return obj.fb_render_target_;
DEFINE_SET_FIELD_TYPE("map")
	if(obj.rt_ != nullptr) {
		obj.fb_render_target_ = value;
		if(!value.is_null()) {
			obj.rt_->setFromVariant(obj.fb_render_target_);
		}
	}
	if(obj.backup_rt_ != nullptr && !value.is_null()) {
		obj.backup_rt_->setFromVariant(obj.fb_render_target_);
	}

DEFINE_FIELD(absolute_object_adjust_x, "int")
	return variant(obj.absolute_object_adjust_x_);
DEFINE_SET_FIELD_TYPE("int")
	obj.absolute_object_adjust_x_ = value.as_int();

DEFINE_FIELD(absolute_object_adjust_y, "int")
	return variant(obj.absolute_object_adjust_y_);
DEFINE_SET_FIELD_TYPE("int")
	obj.absolute_object_adjust_y_ = value.as_int();

DEFINE_FIELD(quitting_game, "bool")
	if(LevelRunner::getCurrent()) {
		return variant::from_bool(LevelRunner::getCurrent()->is_quitting());
	}

	return variant::from_bool(false);

DEFINE_SET_FIELD_TYPE("bool")
	if(LevelRunner::getCurrent()) {
		LevelRunner::getCurrent()->set_quitting(value.as_bool());
	}

DEFINE_FIELD(num_transition_frames, "int")
	return variant(g_num_level_transition_frames);
DEFINE_SET_FIELD_TYPE("int")
	g_num_level_transition_frames = value.as_int();

DEFINE_FIELD(transition_ratio, "decimal")
	return variant(g_level_transition_ratio);

DEFINE_FIELD(is_building_tiles, "bool")
	level_tile_rebuild_info& info = tile_rebuild_map[&obj];
	if(!info.tile_rebuild_in_progress) {
		return variant::from_bool(false);
	} else {
		return variant::from_bool(true);
	}

END_DEFINE_CALLABLE(Level)

int Level::camera_rotation() const
{
	if(!camera_rotation_) {
		return 0;
	}

	return camera_rotation_->execute(*this).as_int();
}

bool Level::isUnderwater(const rect& r, rect* res_water_area, variant* v) const
{
	return water_ && water_->isUnderwater(r, res_water_area, v);
}

void Level::getCurrent(const Entity& e, int* velocity_x, int* velocity_y) const
{
	if(e.mass() == 0) {
		return;
	}

	int delta_x = 0, delta_y = 0;
	if(isUnderwater(e.getBodyRect())) {
		delta_x += *velocity_x;
		delta_y += *velocity_y;
		water_->getCurrent(e, &delta_x, &delta_y);
		delta_x -= *velocity_x;
		delta_y -= *velocity_y;
	}

	delta_x /= e.mass();
	delta_y /= e.mass();

	for(const EntityPtr& c : active_chars_) {
		if(c.get() != &e) {
			delta_x += *velocity_x;
			delta_y += *velocity_y;
			c->generateCurrent(e, &delta_x, &delta_y);
			delta_x -= *velocity_x;
			delta_y -= *velocity_y;
		}
	}

	*velocity_x += delta_x;
	*velocity_y += delta_y;
}

Water& Level::get_or_create_water()
{
	if(!water_) {
		water_.reset(new Water());
	}

	return *water_;
}

EntityPtr Level::get_entity_by_label(const std::string& label)
{
	std::map<std::string, EntityPtr>::iterator itor = chars_by_label_.find(label);
	if(itor != chars_by_label_.end()) {
		return itor->second;
	}

	return EntityPtr();
}

ConstEntityPtr Level::get_entity_by_label(const std::string& label) const
{
	std::map<std::string, EntityPtr>::const_iterator itor = chars_by_label_.find(label);
	if(itor != chars_by_label_.end()) {
		return itor->second;
	}

	return ConstEntityPtr();
}

void Level::getAllLabels(std::vector<std::string>& labels) const
{
	for(std::map<std::string, EntityPtr>::const_iterator i = chars_by_label_.begin(); i != chars_by_label_.end(); ++i) {
		labels.push_back(i->first);
	}
}

const std::vector<EntityPtr>& Level::get_solid_chars() const
{
	if(solid_chars_.empty()) {
		for(const EntityPtr& e : chars_) {
			if(e->solid() || e->platform()) {
				solid_chars_.push_back(e);
			}
		}
	}

	return solid_chars_;
}

bool Level::can_interact(const rect& body) const
{
	for(const portal& p : portals_) {
		if(p.automatic == false && rects_intersect(body, p.area)) {
			return true;
		}
	}

	for(const EntityPtr& c : active_chars_) {
		if(c->canInteractWith() && rects_intersect(body, c->getBodyRect()) &&
		   intersection_rect(body, c->getBodyRect()).w() >= std::min(body.w(), c->getBodyRect().w())/2) {
			return true;
		}
	}

	return false;
}

void Level::replay_from_cycle(int ncycle)
{
	const int cycles_ago = cycle_ - ncycle;
	if(cycles_ago <= 0) {
		return;
	}

	int index = static_cast<int>(backups_.size()) - cycles_ago;
	ASSERT_GE(index, 0);

	const int cycle_to_play_until = cycle_;
	restore_from_backup(*backups_[index]);
	ASSERT_EQ(cycle_, ncycle);
	backups_.erase(backups_.begin() + index, backups_.end());
	while(cycle_ < cycle_to_play_until) {
		backup();
		do_processing();
	}
}

PREF_BOOL(enable_history, true, "Allow editor history features");

void Level::backup(bool force)
{
	if((!g_enable_history && !force) || (backups_.empty() == false && backups_.back()->cycle == cycle_)) {
		return;
	}

	std::map<EntityPtr, EntityPtr> entity_map;

	backup_snapshot_ptr snapshot(new backup_snapshot);
	snapshot->rng_seed = rng::get_seed();
	snapshot->cycle = cycle_;
	snapshot->chars.reserve(chars_.size());


	for(const EntityPtr& e : chars_) {
		snapshot->chars.push_back(e->backup());
		entity_map[e] = snapshot->chars.back();

		if(snapshot->chars.back()->isHuman()) {
			snapshot->players.push_back(snapshot->chars.back());
			if(e == player_) {
				snapshot->player = snapshot->players.back();
			}
		}
	}

	for(entity_group& g : groups_) {
		snapshot->groups.push_back(entity_group());

		for(EntityPtr e : g) {
			std::map<EntityPtr, EntityPtr>::iterator i = entity_map.find(e);
			if(i != entity_map.end()) {
				snapshot->groups.back().push_back(i->second);
			}
		}
	}

	for(const EntityPtr& e : snapshot->chars) {
		e->mapEntities(entity_map);
	}

	snapshot->last_touched_player = last_touched_player_;

	backups_.push_back(snapshot);
	if(backups_.size() > 250) {

		for(std::deque<backup_snapshot_ptr>::iterator i = backups_.begin();
		    i != backups_.begin() + 1; ++i) {
			for(const EntityPtr& e : (*i)->chars) {
				//kill off any references this entity holds, to workaround
				//circular references causing things to stick around.
				e->cleanup_references();
			}
		}
		backups_.erase(backups_.begin(), backups_.begin() + 1);
	}
}

int Level::earliest_backup_cycle() const
{
	if(backups_.empty()) {
		return cycle_;
	} else {
		return backups_.front()->cycle;
	}
}

void Level::reverse_one_cycle()
{
	if(backups_.empty()) {
		return;
	}

	restore_from_backup(*backups_.back());
	backups_.pop_back();
}

void Level::reverse_to_cycle(int ncycle)
{
	if(backups_.empty()) {
		return;
	}

	LOG_INFO("REVERSING FROM " << cycle_ << " TO " << ncycle << "...");

	while(backups_.size() > 1 && backups_.back()->cycle > ncycle) {
		LOG_INFO("REVERSING PAST " << backups_.back()->cycle << "...");
		backups_.pop_back();
	}

	LOG_INFO("GOT TO CYCLE: " << backups_.back()->cycle);

	reverse_one_cycle();
}

void Level::restore_from_backup(backup_snapshot& snapshot)
{
	rng::set_seed(snapshot.rng_seed);
	cycle_ = snapshot.cycle;
	chars_ = snapshot.chars;
	players_ = snapshot.players;
	player_ = snapshot.player;
	groups_ = snapshot.groups;
	last_touched_player_ = snapshot.last_touched_player;
	active_chars_.clear();

	solid_chars_.clear();

	chars_by_label_.clear();
	for(const EntityPtr& e : chars_) {
		if(e->label().empty() == false) {
			chars_by_label_[e->label()] = e;
		}
	}

	for(const EntityPtr& ch : snapshot.chars) {
		ch->handleEvent(OBJECT_EVENT_LOAD);
	}
}

std::vector<EntityPtr> Level::trace_past(EntityPtr e, int ncycle)
{
	backup();
	int prev_cycle = -1;
	std::vector<EntityPtr> result;
	std::deque<backup_snapshot_ptr>::reverse_iterator i = backups_.rbegin();
	while(i != backups_.rend() && (*i)->cycle >= ncycle) {
		const backup_snapshot& snapshot = **i;
		if(prev_cycle != -1 && snapshot.cycle == prev_cycle) {
			++i;
			continue;
		}

		prev_cycle = snapshot.cycle;

		for(const EntityPtr& ghost : snapshot.chars) {
			if(ghost->label() == e->label()) {
				result.push_back(ghost);
				break;
			}
		}
		++i;
	}

	return result;
}

std::vector<EntityPtr> Level::predict_future(EntityPtr e, int ncycles)
{
	disable_flashes_scope flashes_disabled_scope;
	const controls::control_backup_scope ctrl_backup_scope;

	backup();
	backup_snapshot_ptr snapshot = backups_.back();
	backups_.pop_back();

	const size_t starting_backups = backups_.size();

	int begin_time = profile::get_tick_time();
	int nframes = 0;

	const int controls_end = controls::local_controls_end();
	LOG_INFO("PREDICT FUTURE: " << cycle_ << "/" << controls_end);
	while(cycle_ < controls_end) {
		try {
			const assert_recover_scope safe_scope;
			process();
			backup();
			++nframes;
		} catch(validation_failure_exception&) {
			LOG_INFO("ERROR WHILE PREDICTING FUTURE...");
			break;
		}
	}

	LOG_INFO("TOOK " << (profile::get_tick_time() - begin_time) << "ms TO MOVE FORWARD " << nframes << " frames");

	begin_time = profile::get_tick_time();

	std::vector<EntityPtr> result = trace_past(e, -1);

	LOG_INFO("TOOK " << (profile::get_tick_time() - begin_time) << "ms to TRACE PAST OF " << result.size() << " FRAMES");

	backups_.resize(starting_backups);
	restore_from_backup(*snapshot);

	return result;
}

void Level::transfer_state_to(Level& lvl)
{
	backup(true);
	lvl.restore_from_backup(*backups_.back());
	backups_.pop_back();
}

void Level::get_tile_layers(std::set<int>* all_layers, std::set<int>* hidden_layers)
{
	if(all_layers) {
		for(const LevelTile& t : tiles_) {
			all_layers->insert(t.zorder);
		}
	}

	if(hidden_layers) {
		*hidden_layers = hidden_layers_;
	}
}

void Level::hide_tile_layer(int layer, bool is_hidden)
{
	if(is_hidden) {
		hidden_layers_.insert(layer);
	} else {
		hidden_layers_.erase(layer);
	}
}

void Level::hide_object_classification(const std::string& classification, bool hidden)
{
	if(hidden) {
		hidden_classifications_.insert(classification);
	} else {
		hidden_classifications_.erase(classification);
	}
}

bool Level::object_classification_hidden(const Entity& e) const
{
#ifndef NO_EDITOR
	return e.getEditorInfo() && hidden_object_classifications().count(e.getEditorInfo()->getClassification());
#else
	return false;
#endif
}

void Level::editor_freeze_tile_updates(bool value)
{
	if(value) {
		++editor_tile_updates_frozen_;
	} else {
		--editor_tile_updates_frozen_;
		if(editor_tile_updates_frozen_ == 0) {
			rebuildTiles();
		}
	}
}

float Level::zoom_level() const
{
	return zoom_level_;
}

bool Level::instant_zoom_level_set() const
{
	return instant_zoom_level_set_ >= cycle_-1;
}

void Level::add_speech_dialog(std::shared_ptr<SpeechDialog> d)
{
	speech_dialogs_.push(d);
}

void Level::remove_speech_dialog()
{
	if(speech_dialogs_.empty() == false) {
		speech_dialogs_.pop();
	}
}

std::shared_ptr<const SpeechDialog> Level::current_speech_dialog() const
{
	if(speech_dialogs_.empty()) {
		return std::shared_ptr<const SpeechDialog>();
	}

	return speech_dialogs_.top();
}

bool entity_in_current_level(const Entity* e)
{
	const Level& lvl = Level::current();
	return std::find(lvl.get_chars().begin(), lvl.get_chars().end(), e) != lvl.get_chars().end();
}

void Level::add_sub_level(const std::string& lvl, int xoffset, int yoffset, bool add_objects)
{

	const std::map<std::string, sub_level_data>::iterator itor = sub_levels_.find(lvl);
	ASSERT_LOG(itor != sub_levels_.end(), "SUB LEVEL NOT FOUND: " << lvl);

	if(itor->second.active && add_objects) {
		remove_sub_level(lvl);
	}

	const int xdiff = xoffset - itor->second.xoffset;
	const int ydiff = yoffset - itor->second.yoffset;

	itor->second.xoffset = xoffset - itor->second.xbase;
	itor->second.yoffset = yoffset - itor->second.ybase;

	LOG_INFO("ADDING SUB LEVEL: " << lvl << "(" << itor->second.lvl->boundaries() << ") " << itor->second.xbase << ", " << itor->second.ybase << " -> " << itor->second.xoffset << ", " << itor->second.yoffset);

	itor->second.active = true;
	Level& sub = *itor->second.lvl;

	if(add_objects) {
		const int difficulty = current_difficulty();
		for(EntityPtr e : sub.chars_) {
			if(e->isHuman()) {
				continue;
			}
	
			EntityPtr c = e->clone();
			if(!c) {
				continue;
			}

			relocate_object(c, c->x() + itor->second.xoffset, c->y() + itor->second.yoffset);
			if(c->appearsAtDifficulty(difficulty)) {
				add_character(c);
				c->handleEvent(OBJECT_EVENT_START_LEVEL);

				itor->second.objects.push_back(c);
			}
		}
	}

	for(solid_color_rect& r : sub.solid_color_rects_) {
		r.area = rect(r.area.x() + xdiff, r.area.y() + ydiff, r.area.w(), r.area.h());
	}

	build_solid_data_from_sub_levels();
}

void Level::remove_sub_level(const std::string& lvl)
{
	const std::map<std::string, sub_level_data>::iterator itor = sub_levels_.find(lvl);
	ASSERT_LOG(itor != sub_levels_.end(), "SUB LEVEL NOT FOUND: " << lvl);

	if(itor->second.active) {
		for(EntityPtr& e : itor->second.objects) {
			if(std::find(active_chars_.begin(), active_chars_.end(), e) == active_chars_.end()) {
				remove_character(e);
			}
		}

		itor->second.objects.clear();
	}

	itor->second.active = false;
}

void Level::build_solid_data_from_sub_levels()
{
	solid_ = solid_base_;
	standable_ = standable_base_;
	solid_.clear();
	standable_.clear();

	for(auto i : sub_levels_) {
		if(!i.second.active) {
			continue;
		}

		const int xoffset = i.second.xoffset/TileSize;
		const int yoffset = i.second.yoffset/TileSize;
		solid_.merge(i.second.lvl->solid_, xoffset, yoffset);
		standable_.merge(i.second.lvl->standable_, xoffset, yoffset);
	}
}

void Level::adjust_level_offset(int xoffset, int yoffset)
{
	game_logic::MapFormulaCallable* callable(new game_logic::MapFormulaCallable);
	variant holder(callable);
	callable->add("xshift", variant(xoffset));
	callable->add("yshift", variant(yoffset));
	for(EntityPtr e : chars_) {
		e->shiftPosition(xoffset, yoffset);
		e->handleEvent(OBJECT_EVENT_COSMIC_SHIFT, callable);
	}

	boundaries_ = rect(boundaries_.x() + xoffset, boundaries_.y() + yoffset, boundaries_.w(), boundaries_.h());

	for(std::map<std::string, sub_level_data>::iterator i = sub_levels_.begin();
	    i != sub_levels_.end(); ++i) {
		if(i->second.active) {
			add_sub_level(i->first, i->second.xoffset + xoffset + i->second.xbase, i->second.yoffset + yoffset + i->second.ybase, false);
		}
	}

	last_draw_position().x += xoffset*100;
	last_draw_position().y += yoffset*100;
	last_draw_position().focus_x += xoffset;
	last_draw_position().focus_y += yoffset;
}

bool Level::relocate_object(EntityPtr e, int new_x, int new_y)
{
	const int orig_x = e->x();
	const int orig_y = e->y();

	const int delta_x = new_x - orig_x;
	const int delta_y = new_y - orig_y;

	e->setPos(new_x, new_y);

	if(!place_entity_in_level(*this, *e)) {
		//if we can't place the object due to solidity, then cancel
		//the movement.
		e->setPos(orig_x, orig_y);
		return false;
	}


#ifndef NO_EDITOR
	//update any x/y co-ordinates to be the same relative to the object's
	//new position.
	if(e->getEditorInfo()) {
		for(const auto& var : e->getEditorInfo()->getVarsAndProperties()) {
			const variant value = e->queryValue(var.getVariableName());
			switch(var.getType()) {
			case VARIABLE_TYPE::XPOSITION:
				if(value.is_int()) {
					e->handleEvent("editor_changing_variable");
					e->mutateValue(var.getVariableName(), variant(value.as_int() + delta_x));
					e->handleEvent("editor_changed_variable");
				}
				break;
			case VARIABLE_TYPE::YPOSITION:
				if(value.is_int()) {
					e->handleEvent("editor_changing_variable");
					e->mutateValue(var.getVariableName(), variant(value.as_int() + delta_y));
					e->handleEvent("editor_changed_variable");
				}
				break;
			case VARIABLE_TYPE::POINTS:
				if(value.is_list()) {
					std::vector<variant> new_value;
					for(variant point : value.as_list()) {
						std::vector<variant> p = point.as_list();
						if(p.size() == 2) {
							p[0] = variant(p[0].as_int() + delta_x);
							p[1] = variant(p[1].as_int() + delta_y);
							new_value.push_back(variant(&p));
						}
					}
					e->handleEvent("editor_changing_variable");
					e->mutateValue(var.getVariableName(), variant(&new_value));
					e->handleEvent("editor_changed_variable");
				}
			default:
				break;
			}
		}
	}
#endif // !NO_EDITOR

	return true;
}

void Level::record_zorders()
{
	for(const LevelTile& t : tiles_) {
		t.object->recordZorder(t.zorder);
	}
}

std::vector<EntityPtr> Level::get_characters_at_world_point(const glm::vec3& pt)
{
	std::vector<EntityPtr> result;
	/*
	const double tolerance = 0.25;
	for(EntityPtr c : chars_) {
		if(object_classification_hidden(*c)) {
			continue;
		}

		if(abs(pt.x - c->tx()) < tolerance 
			&& abs(pt.y - c->ty()) < tolerance 
			&& abs(pt.z - c->tz()) < tolerance) {
			result.push_back(c);
		}
	}
	*/
	return result;	
}

int Level::current_difficulty() const
{
	if(!editor_ && preferences::force_difficulty() != std::numeric_limits<int>::min()) {
		return preferences::force_difficulty();
	}

	if(!last_touched_player_) {
		return 0;
	}

	PlayableCustomObject* p = dynamic_cast<PlayableCustomObject*>(last_touched_player_.get());
	if(!p) {
		return 0;
	}

	return p->difficulty();
}

void Level::launch_new_module(const std::string& module_id, game_logic::ConstFormulaCallablePtr callable)
{
	module::reload(module_id);
	reload_level_paths();
	CustomObjectType::ReloadFilePaths();

	std::map<std::string,std::string> font_paths;
	module::get_unique_filenames_under_dir("data/fonts/", &font_paths);
	KRE::Font::setAvailableFonts(font_paths);

	const std::vector<EntityPtr> players = this->players();
	for(EntityPtr e : players) {
		this->remove_character(e);
	}

	if(callable) {
		module::set_module_args(callable);
	}

	Level::portal p;
	p.level_dest = "titlescreen.cfg";
	p.dest_starting_pos = true;
	p.automatic = true;
	p.transition = "instant";
	p.saved_game = true; //makes it use the player in there.
	force_enter_portal(p);
}

std::pair<std::vector<LevelTile>::const_iterator, std::vector<LevelTile>::const_iterator> Level::tiles_at_loc(int x, int y) const
{
	x = round_tile_size(x);
	y = round_tile_size(y);

	if(tiles_by_position_.size() != tiles_.size()) {
		tiles_by_position_ = tiles_;
		std::sort(tiles_by_position_.begin(), tiles_by_position_.end(), level_tile_pos_comparer());
	}

	std::pair<int, int> loc(x, y);
	return std::equal_range(tiles_by_position_.begin(), tiles_by_position_.end(), loc, level_tile_pos_comparer());
}

int Level::addSubComponent(int w, int h)
{
	int xpos = 0;
	int ypos = boundaries_.y2() + TileSize*4;

	if(sub_components_.empty() == false) {
		ypos = sub_components_.back().source_area.y2() + TileSize*4;
	}

	SubComponent sub;
	sub.source_area = rect(xpos, ypos, w, h);
	sub.num_variations = 1;
	sub_components_.push_back(sub);
	return sub_components_.size() - 1;
}

void Level::removeSubComponent(int nindex)
{
	if(nindex < 0) {
		nindex = sub_components_.size() - 1;
	}

	if(nindex >= 0 && nindex < sub_components_.size()) {
		sub_components_.erase(sub_components_.begin() + nindex);
	}
}

void Level::addSubComponentVariations(int nindex, int ndelta)
{
	if(nindex >= 0 && nindex < sub_components_.size()) {
		sub_components_[nindex].num_variations = std::max<int>(1, sub_components_[nindex].num_variations+ndelta);
	}
}

void Level::setSubComponentArea(int nindex, const rect& area)
{
	if(nindex >= 0 && nindex < sub_components_.size()) {
		sub_components_[nindex].source_area = area;
	}
}

void Level::addSubComponentUsage(int nsub, const rect& area)
{
	SubComponentUsage usage;
	usage.dest_area = area;
	usage.ncomponent = nsub;
	sub_component_usages_.push_back(usage);
}

void Level::updateSubComponentFromUsage(const SubComponentUsage& usage)
{
	ASSERT_LOG(usage.ncomponent >= 0 && usage.ncomponent < sub_components_.size(), "Illegal sub component usage: " << usage.ncomponent);

	rect source_area = sub_components_[usage.ncomponent].source_area;

	std::map<int, std::vector<std::string> > tiles;

	getAllTilesRect(usage.dest_area.x(), usage.dest_area.y(), usage.dest_area.x2(), usage.dest_area.y2(), tiles);

}

game_logic::FormulaPtr Level::createFormula(const variant& v)
{
	// XXX Add symbol table here?
	return game_logic::FormulaPtr(new game_logic::Formula(v));
}

bool Level::executeCommand(const variant& var)
{
	bool result = true;
	if(var.is_null()) {
		return result;
	}

	if(var.is_list()) {
		const int num_elements = var.num_elements();
		for(int n = 0; n != num_elements; ++n) {
			if(var[n].is_null() == false) {
				result = executeCommand(var[n]) && result;
			}
		}
	} else {
		game_logic::CommandCallable* cmd = var.try_convert<game_logic::CommandCallable>();
		if(cmd != nullptr) {
			cmd->runCommand(*this);
		}
	}
	return result;
}

void Level::surrenderReferences(GarbageCollector* gc)
{
	for(std::pair<const std::string, EntityPtr>& p : chars_by_label_) {
		gc->surrenderPtr(&p.second, "chars_by_label");
	}

	gc->surrenderVariant(&vars_, "vars");

	gc->surrenderPtr(&suspended_level_, "suspended_level");

	gc->surrenderPtr(&editor_highlight_, "editor_high");
	gc->surrenderPtr(&player_, "player");
	gc->surrenderPtr(&last_touched_player_, "last_touched_player");
	for(EntityPtr& e : chars_) {
		gc->surrenderPtr(&e, "chars");
	}
	for(EntityPtr& e : new_chars_) {
		gc->surrenderPtr(&e, "new_chars");
	}
	for(EntityPtr& e : active_chars_) {
		gc->surrenderPtr(&e, "active_chars");
	}
	for(EntityPtr& e : solid_chars_) {
		gc->surrenderPtr(&e, "solid_chars");
	}
	for(EntityPtr& e : chars_immune_from_time_freeze_) {
		gc->surrenderPtr(&e, "chars_immune");
	}
	for(EntityPtr& e : players_) {
		gc->surrenderPtr(&e, "players");
	}
	for(EntityPtr& e : editor_selection_) {
		gc->surrenderPtr(&e, "editor_selection");
	}

	for(entity_group& group : groups_) {
		for(EntityPtr& e : group) {
			gc->surrenderPtr(&e, "groups");
		}
	}

	gc->surrenderPtr(&shader_, "SHADER");
	gc->surrenderVariant(&fb_shaders_variant_, "FB_SHADERS_VARIANT");

	for(graphics::AnuraShaderPtr& ptr : active_fb_shaders_) {
		gc->surrenderPtr(&ptr, "ACTIVE_FB_SHADERS");
	}

	for(FrameBufferShaderEntry& entry : fb_shaders_) {
		gc->surrenderPtr(&entry.shader, "FB_SHADER");
		gc->surrenderVariant(&entry.shader_node, "FB_SHADER_NODE");
	}
}

/*
UTILITY(correct_solidity)
{
	std::vector<std::string> files;
	sys::get_files_in_dir(preferences::level_path(), &files);
	for(const std::string& file : files) {
		if(file.size() <= 4 || std::string(file.end()-4, file.end()) != ".cfg") {
			continue;
		}

		ffl::IntrusivePtr<Level> lvl(new Level(file));
		lvl->finishLoading();
		lvl->setAsCurrentLevel();

		for(EntityPtr c : lvl->get_chars()) {
			if(entity_collides_with_level(*lvl, *c, MOVE_DIRECTION::NONE)) {
				if(place_entity_in_level_with_large_displacement(*lvl, *c)) {
					LOG_INFO("LEVEL: " << lvl->id() << " CORRECTED " << c->getDebugDescription());
				} else {
					LOG_INFO("LEVEL: " << lvl->id() << " FAILED TO CORRECT " << c->getDebugDescription());
				}
			}

			c->handleEvent("editor_removed");
			c->handleEvent("editor_added");
		}

		sys::write_file(preferences::level_path() + file, lvl->write().write_json(true));
	}
}

UTILITY(compile_levels)
{
	preferences::compiling_tiles = true;

	LOG_INFO("COMPILING LEVELS...");

	std::map<std::string, std::string> file_paths;
	module::get_unique_filenames_under_dir(preferences::level_path(), &file_paths);

	variant_builder index_node;

	for(std::map<std::string, std::string>::const_iterator i = file_paths.begin(); i != file_paths.end(); ++i) {
		if(strstr(i->second.c_str(), "/Unused")) {
			continue;
		}

		const std::string& file = module::get_id(i->first);
		LOG_INFO("LOADING LEVEL '" << file << "'");
		ffl::IntrusivePtr<Level> lvl(new Level(file));
		lvl->finishLoading();
		lvl->record_zorders();
		module::write_file("data/compiled/level/" + file, lvl->write().write_json(true));
		LOG_INFO("SAVING LEVEL TO MODULE: data/compiled/level/" << file);

		variant_builder level_summary;
		level_summary.add("level", lvl->id());
		level_summary.add("title", lvl->title());
		level_summary.add("music", lvl->music());
		index_node.add("level", level_summary.build());
	}

	module::write_file("data/compiled/level_index.cfg", index_node.build().write_json(true));

	LevelObject::writeCompiled();
}
*/
BENCHMARK(level_solid)
{
	//benchmark which tells us how long Level::solid takes.
	static Level* lvl = new Level("stairway-to-heaven.cfg");
	BENCHMARK_LOOP {
		lvl->solid(rng::generate()%1000, rng::generate()%1000);
	}
}

BENCHMARK(load_nene)
{
	BENCHMARK_LOOP {
		Level lvl("to-nenes-house.cfg");
	}
}

/*
BENCHMARK(load_all_levels)
{
	std::vector<std::string> files;
	module::get_files_in_dir(preferences::level_path(), &files);
	BENCHMARK_LOOP {
		for(const std::string& file : files) {
			ffl::IntrusivePtr<Level> lvl(new Level(file));
		}
	}
}

UTILITY(load_and_save_all_levels)
{
	std::map<std::string, std::string> files;
	module::get_unique_filenames_under_dir(preferences::level_path(), &files);
	for(auto i : files) {
		const std::string& file = i.first;
		LOG_INFO("LOAD_LEVEL '" << file << "'");
		ffl::IntrusivePtr<Level> lvl(new Level(file));
		lvl->finishLoading();

		const std::string path = get_level_path(file);

		LOG_INFO("WRITE_LEVEL: '" << file << "' TO " << path);
		sys::write_file(path, lvl->write().write_json(true));
	}
}
*/
