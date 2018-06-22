/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
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

#include <deque>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include "ColorTransform.hpp"
#include "SceneFwd.hpp"
#include "geometry.hpp"

#include "anura_shader.hpp"
#if defined(USE_BOX2D)
#include "b2d_ffl.hpp"
#endif
#include "background.hpp"
#include "entity.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition_fwd.hpp"
#include "hex_fwd.hpp"
#include "hex_renderable_fwd.hpp"
#include "LayerBlitInfo.hpp"
#include "level_object.hpp"
#include "level_solid_map.hpp"
#include "random.hpp"
#include "speech_dialog.hpp"
#include "tile_map.hpp"
#include "variant.hpp"
#include "water.hpp"

class Level;
typedef ffl::IntrusivePtr<Level> LevelPtr;

class CurrentLevelScope 
{
	LevelPtr old_;
public:
	explicit CurrentLevelScope(Level* ptr);
	~CurrentLevelScope();
};

class Level : public game_logic::FormulaCallable
{
public:

	struct Summary {
		std::string music, title;
	};

	static Summary getSummary(const std::string& id);

	static Level& current();
	static Level* getCurrentPtr();
	void setAsCurrentLevel();
	static void clearCurrentLevel();

	static int tileRebuildStateId();

	static void setPlayerVariantType(variant type);

	explicit Level(const std::string& level_cfg, variant node=variant());
	virtual ~Level();

	//function to do anything which loads the level and must be done
	//in the main thread.
	void finishLoading();

	virtual game_logic::FormulaPtr createFormula(const variant& v) override;
	bool executeCommand(const variant& var) override;

	//function which sets which player we're controlling on this machine.
	void setMultiplayerSlot(int slot);

	const std::string& replay_data() const { return replay_data_; }
	void load_save_point(const Level& lvl);
	void set_save_point(int x, int y) { save_point_x_ = x; save_point_y_ = y; }

	const std::string& id() const { return id_; }
	void setId(const std::string& s) { id_ = s; }
	const std::string& music() const { return music_; }

	std::string package() const;

	variant write() const;
	void draw(int x, int y, int w, int h) const;
	void drawLater(int x, int y, int w, int h) const;
	void draw_status() const;
	void draw_debug_solid(int x, int y, int w, int h) const;
	void draw_background(int x, int y, int rotation, float xdelta, float ydelta) const;
	void process();
	void set_active_chars();
	void process_draw();
	bool standable(const rect& r, const SurfaceInfo** info=nullptr) const;
	bool standable(int x, int y, const SurfaceInfo** info=nullptr) const;
	bool standable_tile(int x, int y, const SurfaceInfo** info=nullptr) const;
	bool solid(int x, int y, const SurfaceInfo** info=nullptr) const;
	bool solid(const Entity& e, const std::vector<point>& points, const SurfaceInfo** info=nullptr) const;
	bool solid(const rect& r, const SurfaceInfo** info=nullptr) const;
	bool solid(int xbegin, int ybegin, int w, int h, const SurfaceInfo** info=nullptr) const;
	bool may_be_solid_in_rect(const rect& r) const;
	void set_solid_area(const rect& r, bool solid);
	EntityPtr board(int x, int y) const;
	const rect& boundaries() const { return boundaries_; }
	void set_boundaries(const rect& bounds) { boundaries_ = bounds; }
	bool constrain_camera() const { return constrain_camera_; }
	void add_tile(const LevelTile& t);
	bool add_tile_rect(int zorder, int x1, int y1, int x2, int y2, const std::string& tile);
	bool addTileRectVector(int zorder, int x1, int y1, int x2, int y2, const std::vector<std::string>& tiles);
	void set_tile_layer_speed(int zorder, int x_speed, int y_speed);
	void refresh_tile_rect(int x1, int y1, int x2, int y2);
	void get_tile_rect(int zorder, int x1, int y1, int x2, int y2, std::vector<std::string>& tiles) const;
	void getAllTilesRect(int x1, int y1, int x2, int y2, std::map<int, std::vector<std::string> >& tiles) const;
	bool clear_tile_rect(int x1, int y1, int x2, int y2);
	bool remove_tiles_at(int x, int y);

	bool is_mouselook_enabled() const { return mouselook_enabled_; }
	void set_mouselook(bool ml=true) { mouselook_enabled_ = ml; }
	bool is_mouselook_inverted() const { return mouselook_inverted_; }
	void set_mouselook_inverted(bool mli=true) { mouselook_inverted_ = true; }
	std::vector<EntityPtr> get_characters_at_world_point(const glm::vec3& pt);

	//function to do 'magic wand' selection -- given an x/y pixel position,
	//will return all the solid tiles connected
	std::vector<point> get_solid_contiguous_region(int xpos, int ypos) const;

	const LevelTile* getTileAt(int x, int y) const;
	void remove_character(EntityPtr e);
	std::vector<EntityPtr> get_characters_in_rect(const rect& r, int screen_xpos, int screen_ypos) const;
	std::vector<EntityPtr> get_characters_at_point(int x, int y, int screen_xpos, int screen_ypos) const;
	EntityPtr get_next_character_at_point(int x, int y, int screen_xpos, int screen_ypos, const void* currently_selected=nullptr) const;
	const PlayerInfo* player() const { return player_ ? player_->getPlayerInfo() : nullptr; }
	PlayerInfo* player() { return player_ ? player_->getPlayerInfo() : nullptr; }
	std::vector<EntityPtr>& players() { return players_; }
	const std::vector<EntityPtr>& players() const { return players_; }
	void add_multi_player(EntityPtr p);
	void add_player(EntityPtr p);
	void add_character(EntityPtr p);

	//add a character that will be drawn on the scene. It will be removed
	//from the level next time set_active_chars() is called.
	void add_draw_character(EntityPtr p);

	//schedule a character for removal at the end of the current cycle.
	void schedule_character_removal(EntityPtr p);

	//sets the last 'touched' player. This is the player found in the level when
	//using WML, so it works reasonably well in multiplayer.
	void set_touched_player(EntityPtr p) { last_touched_player_ = p; }

	struct portal {
		portal() : dest_starting_pos(false), automatic(false), saved_game(false), no_move_to_standing(false)
		{}
		rect area;
		LevelPtr level_dest_obj;
		std::string level_dest;
		std::string dest_label;
		std::string dest_str;
		point dest;
		bool dest_starting_pos;
		bool automatic;
		std::string transition;
		bool saved_game;
		EntityPtr new_playable;
		bool no_move_to_standing;
	};

	//function which will make it so the next call to get_portal() will return
	//a pointer to a copy of the given portal. i.e. this makes the character immediately
	//enter a portal.
	void force_enter_portal(const portal& p);

	//the portal the character has entered (if any)
	const portal* get_portal() const;

	int xscale() const { return xscale_; }
	int yscale() const { return yscale_; }

	int group_size(int group) const;
	void set_character_group(EntityPtr c, int group_num);
	int add_group();

#ifndef NO_EDITOR
	void set_editor(bool value=true) { editor_ = value; if(editor_) { prepare_tiles_for_drawing(); } }
#else
	void set_editor(bool value=true) {}
#endif // !NO_EDITOR
	void set_editor_highlight(EntityPtr c) { editor_highlight_ = c; }
	EntityPtr editor_highlight() const { return editor_highlight_; }

	void editor_select_object(EntityPtr c);
	void editor_deselect_object(EntityPtr c);
	void editor_clear_selection();

	const std::vector<EntityPtr>& editor_selection() const { return editor_selection_; }

	bool show_foreground() const { return show_foreground_; }
	void setShowForeground(bool value) { show_foreground_ = value; }

	bool show_background() const { return show_background_; }
	void setShowBackground(bool value) { show_background_ = value; }

	const std::string& get_background_id() const;
	void set_background_by_id(const std::string& id);

	//a function to start rebuilding tiles in a background thread.
	void start_rebuild_tiles_in_background(const std::vector<int>& layers);

	//a function which, if rebuilding tiles has been completed, will update
	//with the new tiles. Returns true iff there is no longer a tile build going on.
	bool complete_rebuild_tiles_in_background();

	//stop calls to start_rebuild_tiles_in_background from proceeding
	//until unfreeze_rebuild_tiles_in_background() is called.
	void freeze_rebuild_tiles_in_background();

	void unfreeze_rebuild_tiles_in_background();

	void rebuildTiles();

	const std::string& title() const { return title_; }
	void set_title(const std::string& t) { title_ = t; }

	int variations(int xtile, int ytile) const;
	void flip_variations(int xtile, int ytile, int delta=1);

	int auto_move_camera_x() const { return auto_move_camera_.x; }
	int auto_move_camera_y() const { return auto_move_camera_.y; }

	int air_resistance() const { return air_resistance_; }
	int water_resistance() const { return water_resistance_; }

	int camera_rotation() const;

	void set_end_game() { end_game_ = true; }
	bool end_game() const { return end_game_; }

	//Function used when the player is entering the level at a certain point.
	//Will take the name of a destination within a level and return the point
	//at that location. For now only takes "left" and "right".
	point get_dest_from_str(const std::string& key) const;

	//levels ended up at by exiting this level to the left or right.
	const std::string& previous_level() const;
	const std::string& next_level() const;

	void set_previous_level(const std::string& name);
	void set_next_level(const std::string& name);

	int cycle() const { return cycle_; }
	bool in_dialog() const { return in_dialog_; }
	void set_in_dialog(bool value) { in_dialog_ = value; }
	bool isUnderwater(const rect& r, rect* res_water_area=nullptr, variant* v=nullptr) const;

	void getCurrent(const Entity& e, int* velocity_x, int* velocity_y) const;

	Water* get_water() { return water_.get(); }
	const Water* get_water() const { return water_.get(); }

	Water& get_or_create_water();

	EntityPtr get_entity_by_label(const std::string& label);
	ConstEntityPtr get_entity_by_label(const std::string& label) const;

	void getAllLabels(std::vector<std::string>& labels) const;

	const std::vector<EntityPtr>& get_active_chars() const { return active_chars_; }
	const std::vector<EntityPtr>& get_chars() const { return chars_; }
	const std::vector<EntityPtr>& get_solid_chars() const;
	void swap_chars(std::vector<EntityPtr>& v) { chars_.swap(v); solid_chars_.clear(); }
	int num_active_chars() const { return static_cast<int>(active_chars_.size()); }

	//function which, given the rect of the player's body will return true iff
	//the player can currently "interact" with a portal or object. i.e. if
	//pressing up will talk to someone or enter a door etc.
	bool can_interact(const rect& body) const;

	int earliest_backup_cycle() const;
	void replay_from_cycle(int ncycle);
	void backup(bool force=false);
	void reverse_one_cycle();
	void reverse_to_cycle(int ncycle);

	void transfer_state_to(Level& lvl);

	//gets historical 'shadows' of a given object back to the given cycle
	std::vector<EntityPtr> trace_past(EntityPtr e, int ncycle);

	std::vector<EntityPtr> predict_future(EntityPtr e, int ncycles);

	bool is_multiplayer() const { return players_.size() > 1; }

	void get_tile_layers(std::set<int>* all_layers, std::set<int>* hidden_layers=nullptr);
	void hide_tile_layer(int layer, bool is_hidden);

	void highlight_tile_layer(int layer) { highlight_layer_ = layer; }

	void hide_object_classification(const std::string& classification, bool hidden);
	const std::set<std::string>& hidden_object_classifications() const { return hidden_classifications_; }

	bool object_classification_hidden(const Entity& e) const;

	const point* lock_screen() const { return lock_screen_.get(); }

	void editor_freeze_tile_updates(bool value);

	float zoom_level() const;
	bool instant_zoom_level_set() const;

	void add_speech_dialog(std::shared_ptr<SpeechDialog> d);
	void remove_speech_dialog();
	std::shared_ptr<const SpeechDialog> current_speech_dialog() const;

	const std::vector<EntityPtr>& focus_override() const { return focus_override_; }

#ifndef NO_EDITOR
	bool in_editor() const {return editor_;}
#else
	bool in_editor() const {return false;}
#endif // !NO_EDITOR

	void set_editor_dragging_objects() { editor_dragging_objects_ = true; }
	bool is_editor_dragging_objects() const { return editor_dragging_objects_; }

	void add_sub_level(const std::string& lvl, int xoffset, int yoffset, bool add_objects=true);
	void remove_sub_level(const std::string& lvl);
	void adjust_level_offset(int xoffset, int yoffset);

	bool relocate_object(EntityPtr e, int x, int y);

	int segment_width() const { return segment_width_; }
	void set_segment_width(int width) { segment_width_ = width; }

	int segment_height() const { return segment_height_; }
	void set_segment_height(int height) { segment_height_ = height; }

	bool is_arcade_level() const { return segment_height_ != 0 || segment_width_ != 0; }

	variant get_var(const std::string& str) const {
		return vars_[str];
	}
	void set_var(const std::string& str, variant value) { vars_ = vars_.add_attr(variant(str), value); }

	bool set_dark(bool value) { bool res = dark_; dark_ = value; return res; }

	void record_zorders();

	int current_difficulty() const;

	int x_resolution() const { return x_resolution_; }
	int y_resolution() const { return y_resolution_; }

	int absolute_object_adjust_x() const { return absolute_object_adjust_x_; }
	int absolute_object_adjust_y() const { return absolute_object_adjust_y_; }

	void launch_new_module(const std::string& module_id, game_logic::ConstFormulaCallablePtr callable = nullptr);

	typedef std::vector<LevelTile>::const_iterator TileItor;
	std::pair<TileItor, TileItor> tiles_at_loc(int x, int y) const;

	const std::vector<std::string>& debug_properties() const { return debug_properties_; }

	bool allow_touch_controls() const { return allow_touch_controls_; }

	LevelPtr suspended_level() const { return suspended_level_; }
	void set_suspended_level(const LevelPtr& lvl) { suspended_level_ = lvl; }

	void set_show_builtin_settingsDialog(bool value) { show_builtin_settings_ = value; }

	bool show_builtin_settingsDialog() const { return show_builtin_settings_; }

	KRE::SceneGraphPtr getSceneGraph() const { return scene_graph_; }

	void setRenderToTexture(int width, int height);
	KRE::RenderTargetPtr getRenderTarget() const { return rt_; }

	//sends events to all objects telling them about the transition to another level. They can
	//set transition frames and otherwise set things up for a transition.
	int setup_level_transition(const std::string& transition_type);

	static void set_level_transition_ratio(decimal value);

	struct SubComponent {
		rect source_area;
		int num_variations;

		SubComponent();
		explicit SubComponent(variant node);
		variant write() const;
	};

	struct SubComponentUsage {
		rect dest_area;
		int ncomponent;
		int ninstance;

		SubComponentUsage();
		explicit SubComponentUsage(variant node);
		const SubComponent& getSubComponent(const Level& lvl) const;
		rect getSourceArea(const Level& lvl) const;
		variant write() const;
	};

	const std::vector<SubComponent>& getSubComponents() const { return sub_components_; }
	const std::vector<SubComponentUsage>& getSubComponentUsages() const { return sub_component_usages_; }

	void setSubComponentUsages(const std::vector<SubComponentUsage>& u) { sub_component_usages_ = u; }

	int addSubComponent(int w, int h);
	void removeSubComponent(int nindex=-1);

	void addSubComponentVariations(int nsub, int ndelta);
	void setSubComponentArea(int nsub, const rect& area);

	void addSubComponentUsage(int nsub, const rect& area);

	void updateSubComponentFromUsage(const SubComponentUsage& usage);

	std::vector<SubComponentUsage> getSubComponentUsagesOrdered() const;
	void applySubComponents();


private:
	DECLARE_CALLABLE(Level);

	void read_compiled_tiles(variant node, std::vector<LevelTile>::iterator& out);

	void complete_tiles_refresh();
	void prepare_tiles_for_drawing();

	void do_processing();

	void calculateLighting(int x, int y, int w, int h) const;

	bool add_tile_rect_vector_internal(int zorder, int x1, int y1, int x2, int y2, const std::vector<std::string>& tiles);

	void draw_layer(int layer, int x, int y, int w, int h) const;
	void draw_layer_solid(int layer, int x, int y, int w, int h) const;

	void rebuild_tiles_rect(const rect& r);
	void add_tile_solid(const LevelTile& t);
	void add_solid_rect(int x1, int y1, int x2, int y2, int friction, int traction, int damage, const std::string& info);
	void add_solid(int x, int y, int friction, int traction, int damage, const std::string& info);
	void add_standable(int x, int y, int friction, int traction, int damage, const std::string& info);
	typedef std::pair<int,int> tile_pos;

	std::string id_;
	std::string music_;
	std::string replay_data_;
	int cycle_;

	int time_freeze_;

	bool paused_;
	std::shared_ptr<controls::control_backup_scope> before_pause_controls_backup_;

	bool in_dialog_;

	//preferred screen dimensions to play the level on.
	int x_resolution_, y_resolution_;

	int absolute_object_adjust_x_, absolute_object_adjust_y_;

	bool set_screen_resolution_on_entry_;

	variant vars_;
	
	LevelSolidMap solid_;
	LevelSolidMap standable_;

	LevelSolidMap solid_base_;
	LevelSolidMap standable_base_;

	bool isSolid(const LevelSolidMap& map, int x, int y, const SurfaceInfo** surf_info) const;
	bool isSolid(const LevelSolidMap& map, const Entity& e, const std::vector<point>& points, const SurfaceInfo** surf_info) const;

	void setSolid(LevelSolidMap& map, int x, int y, int friction, int traction, int damage, const std::string& info, bool solid=true);

	std::string title_;

	rect boundaries_;

	bool constrain_camera_;

	struct solid_rect {
		rect r;
		int friction;
		int traction;
		int damage;
	};
	std::vector<solid_rect> solid_rects_;
	mutable std::vector<LevelTile> tiles_;

	//tiles sorted by position rather than zorder.
	mutable std::vector<LevelTile> tiles_by_position_;
	std::set<int> layers_;
	std::set<int> hidden_layers_; //layers hidden in the editor.
	int highlight_layer_;

	struct solid_color_rect {
		KRE::Color color;
		rect area;
		int layer;
	};

	struct solid_color_rect_empty {
		bool operator()(const solid_color_rect& r) const { return r.area.w() == 0; }
	};

	struct solid_color_rect_cmp {
		bool operator()(const solid_color_rect& r, int zorder) const { return r.layer < zorder; }
		bool operator()(int zorder, const solid_color_rect& r) const { return zorder < r.layer; }
		bool operator()(const solid_color_rect& a, const solid_color_rect& b) const { return a.layer < b.layer; }
	};

	std::vector<solid_color_rect> solid_color_rects_;

	std::vector<rect> opaque_rects_;

	void erase_char(EntityPtr c);
	std::vector<EntityPtr> chars_;
	mutable std::vector<EntityPtr> active_chars_;
	std::vector<EntityPtr> new_chars_;
	mutable std::vector<EntityPtr> solid_chars_;

	std::vector<EntityPtr> chars_immune_from_time_freeze_;

	std::map<std::string, EntityPtr> chars_by_label_;
	EntityPtr player_;
	EntityPtr last_touched_player_;

	std::vector<EntityPtr> players_;

	//characters stored in wml format; they can't be loaded in a separate thread
	//they will be loaded when complete_load_level() is called.
	std::vector<variant> wml_chars_;
	std::vector<variant> serialized_objects_;

	std::vector<variant> wml_compiled_tiles_;
	int num_compiled_tiles_;

	void load_character(variant c);

	typedef std::vector<EntityPtr> entity_group;
	std::vector<entity_group> groups_;

	portal left_portal_, right_portal_;
	std::vector<portal> portals_;

	mutable bool entered_portal_active_;
	portal entered_portal_;

	std::shared_ptr<Background> background_;
	point background_offset_;
	int widest_tile_, highest_tile_;

	std::map<int, TileMap> tile_maps_;
	int xscale_, yscale_;

	graphics::AnuraShaderPtr shader_;

	struct FrameBufferShaderEntry {
		std::string label;
		int begin_zorder, end_zorder;
		variant shader_node;
		mutable graphics::AnuraShaderPtr shader;

		mutable KRE::RenderTargetPtr rt;
	};
	std::vector<FrameBufferShaderEntry> fb_shaders_;
	mutable std::vector<graphics::AnuraShaderPtr> active_fb_shaders_;
	mutable variant fb_shaders_variant_;

	void flushFrameBufferShadersToScreen() const;
	KRE::RenderTargetPtr& applyShaderToFrameBufferTexture(graphics::AnuraShaderPtr shader, bool render_to_screen) const;
	void frameBufferEnterZorder(int zorder) const;

public:
	void shadersUpdated();

private:
	int save_point_x_, save_point_y_;
	bool editor_;
	EntityPtr editor_highlight_;

	std::vector<EntityPtr> editor_selection_;

	bool show_foreground_, show_background_;

	bool dark_;
	KRE::ColorTransform dark_color_;

	point auto_move_camera_;
	int air_resistance_;
	int water_resistance_;

	game_logic::ConstFormulaPtr camera_rotation_;
	bool end_game_;

	std::vector<std::string> preloads_; //future levels to preload

	std::shared_ptr<Water> water_;

	std::shared_ptr<point> lock_screen_;

	struct backup_snapshot {
		rng::Seed rng_seed;
		int cycle;
		std::vector<EntityPtr> chars;
		std::vector<EntityPtr> players;
		std::vector<entity_group> groups;
		EntityPtr player, last_touched_player;
	};

	void restore_from_backup(backup_snapshot& snapshot);

	typedef std::shared_ptr<backup_snapshot> backup_snapshot_ptr;

	std::deque<backup_snapshot_ptr> backups_;

	int editor_tile_updates_frozen_;
	bool editor_dragging_objects_;

	float zoom_level_;
	int instant_zoom_level_set_;
	std::vector<EntityPtr> focus_override_;

	std::stack<std::shared_ptr<SpeechDialog> > speech_dialogs_;

	std::set<std::string> hidden_classifications_;

	//color palettes that the level has set.
	unsigned int palettes_used_;

	int background_palette_;

	int segment_width_, segment_height_;

	struct sub_level_data {
		LevelPtr lvl;
		int xbase, ybase;
		int xoffset, yoffset;
		bool active;
		std::vector<EntityPtr> objects;
	};

	void build_solid_data_from_sub_levels();

	std::string sub_level_str_;
	std::map<std::string, sub_level_data> sub_levels_;

	//A list of properties that each object in the level should display
	//for debugging purposes.
	std::vector<std::string> debug_properties_;

#if defined(USE_BOX2D)
	// List of static bodies present in the level.
	std::vector<box2d::body_ptr> bodies_;
#endif

	bool mouselook_enabled_;
	bool mouselook_inverted_;

	// Hack to disable the touchscreen controls for the current level -- replace for 1.4
	bool allow_touch_controls_;

	//determines if we should be using the builtin/Anura settings dialog
	//or if the level will show its own settings.
	bool show_builtin_settings_;

	LevelPtr suspended_level_;

	mutable std::map<int, std::shared_ptr<LayerBlitInfo>> blit_cache_;

	mutable KRE::RenderTargetPtr rt_, backup_rt_;
	bool have_render_to_texture_;

	bool render_to_texture_;
	mutable bool doing_render_to_texture_;

	void surrenderReferences(GarbageCollector* gc) override;

	KRE::SceneGraphPtr scene_graph_;
	KRE::RenderManagerPtr rmanager_;
	int last_process_time_;

	hex::HexMapPtr hex_map_;
	hex::MapNodePtr hex_renderable_;
	std::vector<hex::MaskNodePtr> hex_masks_;

	variant fb_render_target_;

	std::vector<SubComponent> sub_components_;
	std::vector<SubComponentUsage> sub_component_usages_;
};

bool entity_in_current_level(const Entity* e);
