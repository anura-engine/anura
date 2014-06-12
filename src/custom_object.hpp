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

#include <memory>
#include <set>

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>
#include <stack>

#include "blur.hpp"
#include "custom_object_type.hpp"
#include "decimal.hpp"
#include "draw_primitive.hpp"
#include "draw_scene.hpp"
#include "entity.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_visitor.hpp"
#include "formula_variable_storage.hpp"
#include "light.hpp"
#include "particle_system.hpp"
#include "variant.hpp"
#include "vector_text.hpp"
#include "widget.hpp"

struct collision_info;
class level;

struct custom_object_text;

class custom_object : public entity
{
public:
	static const std::string* current_debug_error();
	static void reset_current_debug_error();

	static std::set<custom_object*>& get_all();
	static std::set<custom_object*>& get_all(const std::string& type);
	static void init();

	static void run_garbage_collection();

	explicit custom_object(variant node);
	custom_object(const std::string& type, int x, int y, bool face_right);
	custom_object(const custom_object& o);
	virtual ~custom_object();

	void validate_properties();

	bool is_a(const std::string& type) const;

	//finish_loading(): called when a level finishes loading all objects,
	//and allows us to do any final setup such as finding our parent.
	void finish_loading(level* lvl);
	virtual variant write() const;
	virtual void setup_drawing() const;
	virtual void draw(int x, int y) const;
	virtual void draw_later(int x, int y) const;
	virtual void draw_group() const;
	virtual void process(level& lvl);
	virtual void construct();
	virtual bool create_object();
	void set_level(level& lvl) { }

	void check_initialized();

	int parallax_scale_millis_x() const {
		if(parallax_scale_millis_.get() == NULL){
			return type_->parallax_scale_millis_x();
		}else{
			return parallax_scale_millis_->first;
		}
	}
	int parallax_scale_millis_y() const {
		if(parallax_scale_millis_.get() == NULL){
			return type_->parallax_scale_millis_y();
		}else{
			return parallax_scale_millis_->second;
		}
	}

	
	virtual int zorder() const;
	virtual int zsub_order() const;

	virtual int velocity_x() const;
	virtual int velocity_y() const;
	virtual int mass() const { return type_->mass(); }

	int teleport_offset_x() const { return type_->teleport_offset_x(); }
	int teleport_offset_y() const { return type_->teleport_offset_y(); }
	bool no_move_to_standing() const { return type_->no_move_to_standing(); };
	bool reverse_global_vertical_zordering() const { return type_->reverse_global_vertical_zordering(); };

	bool has_feet() const;

	
	virtual bool is_standable(int x, int y, int* friction=NULL, int* traction=NULL, int* adjust_y=NULL) const;

	virtual bool destroyed() const;
	virtual bool point_collides(int x, int y) const;
	virtual bool rect_collides(const rect& r) const;

	virtual const frame& current_frame() const { return *frame_; }

	void set_frame(const std::string& name);
	void set_frame(const frame& new_frame);

	virtual rect draw_rect() const;

	//bare setting of the frame without adjusting position/checking solidity
	//etc etc.
	void set_frame_no_adjustments(const std::string& name);
	void set_frame_no_adjustments(const frame& new_frame);
	void die();
	void die_with_no_event();
	virtual bool isActive(const rect& screen_area) const;
	bool dies_on_inactive() const;
	bool always_active() const;
	bool move_to_standing(level& lvl, int max_displace=10000);

	bool body_harmful() const;
	bool body_passthrough() const;

	int time_in_frame() const { return time_in_frame_; }

	FormulaCallable* vars() { return vars_.get(); }
	const FormulaCallable* vars() const { return vars_.get(); }

	int cycle() const { return cycle_; }

	int surface_friction() const;
	int surface_traction() const;

	variant get_child(const std::string& key) const {
		return type_->get_child(key);
	}

	const frame& icon_frame() const;

	virtual entity_ptr clone() const;
	virtual entity_ptr backup() const;

	game_logic::const_formula_ptr get_event_handler(int key) const;
	void set_event_handler(int, game_logic::const_formula_ptr f);

	bool can_interact_with() const;

	std::string debug_description() const;

	void map_entities(const std::map<entity_ptr, entity_ptr>& m);
	void cleanup_references();

	void add_particle_system(const std::string& key, const std::string& type);
	void remove_particle_system(const std::string& key);

	void setText(const std::string& text, const std::string& font, int size, int align);
	void add_vector_text(const gui::vector_text_ptr& txtp) {
		vector_text_.push_back(txtp);
	}
	void clear_vector_text() { vector_text_.clear(); }

	virtual int hitpoints() const { return hitpoints_; }

	virtual bool boardable_vehicle() const;

	virtual void boarded(level& lvl, const entity_ptr& player);
	virtual void unboarded(level& lvl);

	virtual void board_vehicle();
	virtual void unboard_vehicle();

	void set_driver_position();

	virtual bool use_absolute_screen_coordinates() const { return use_absolute_screen_coordinates_; }

	virtual int current_animation_id() const { return current_animation_id_; }

	virtual bool handle_sdl_event(const SDL_Event& event, bool claimed);
#ifndef NO_EDITOR
	virtual const_editor_entity_info_ptr EditorInfo() const;
#endif // !NO_EDITOR

	virtual bool handleEvent(const std::string& event, const FormulaCallable* context=NULL);
	virtual bool handleEvent(int event, const FormulaCallable* context=NULL);
	virtual bool handleEvent_delay(int event, const FormulaCallable* context=NULL);

	virtual void resolve_delayed_events();

	virtual bool serializable() const;

	void set_blur(const BlurInfo* blur);
	void set_sound_volume(const int volume);
	void set_zsub_order(const int zsub_order) {zsub_order_ = zsub_order;}
	
	bool executeCommand(const variant& var);

	virtual game_logic::formula_ptr createFormula(const variant& v);

	bool allow_level_collisions() const;

	//statistic on how many FFL events are handled every second.
	static int events_handled_per_second;

	const std::vector<light_ptr>& lights() const { return lights_; }
	void swap_lights(std::vector<light_ptr>& lights) { lights_.swap(lights); }

	void shift_position(int x, int y);

	bool appears_at_difficulty(int difficulty) const;

	int min_difficulty() const { return min_difficulty_; }
	int max_difficulty() const { return max_difficulty_; }

	void set_difficulty(int min, int max) { min_difficulty_ = min; max_difficulty_ = max; }

	void update_type(const_custom_object_type_ptr old_type,
	                 const_custom_object_type_ptr new_type);

	bool mouse_event_swallowed() const {return swallow_mouse_event_;}
	void reset_mouse_event() {swallow_mouse_event_ = false;}
	void addWidget(const gui::WidgetPtr& w);
	void add_widgets(std::vector<gui::WidgetPtr>* widgets);
	void clear_widgets();
	void remove_widget(gui::WidgetPtr w);
	gui::WidgetPtr getWidgetById(const std::string& id);
	gui::ConstWidgetPtr getWidgetById(const std::string& id) const;
	std::vector<variant> get_variant_widget_list() const;
	bool get_clipArea(rect* clipArea) {
		if(clip_area_ && clipArea) {
			*clipArea = *clip_area_.get();
			return true;
		}
		return false;
	}

	struct AnimatedMovement {
		std::string name;

		//animation_values is a multiple of animation_slots size.
		//animation_slots represents the values being set for each frame
		//in the animation. animation_values contains all the data for
		//all the frames.
		std::vector<int> animation_slots;
		std::vector<variant> animation_values;

		int pos;

		variant on_process, on_complete;

		std::vector<std::pair<variant,variant> > follow_on;

		AnimatedMovement() : pos(0)
		{}

		int animation_frames() const { return animation_values.size()/animation_slots.size(); }
	};

	void add_animated_movement(variant attr, variant options);
	void set_animated_schedule(std::shared_ptr<AnimatedMovement> movement);
	void cancel_animated_schedule(const std::string& name);

protected:
	//components of per-cycle process() that can be done even on
	//static objects.
	void static_process(level& lvl);

	virtual void control(const level& lvl);
	variant getValue(const std::string& key) const;
	variant getValue_by_slot(int slot) const;
	void setValue(const std::string& key, const variant& value);
	void setValue_by_slot(int slot, const variant& value);

	virtual variant get_player_value_by_slot(int slot) const;
	virtual void set_player_value_by_slot(int slot, const variant& value);

	//function which indicates if the object wants to walk up or down stairs.
	//-1 = up stairs, 0 = no change, 1 = down stairs
	virtual int walk_up_or_down_stairs() const { return 0; }

	bool is_underwater() const {
		return was_underwater_;
	}

	const std::pair<int,int>* parallax_scale_millis() const { return parallax_scale_millis_.get(); }

	enum STANDING_STATUS { NOT_STANDING, STANDING_BACK_FOOT, STANDING_FRONT_FOOT };
	STANDING_STATUS is_standing(const level& lvl, collision_info* info=NULL) const;

	void set_parent(entity_ptr e, const std::string& pivot_point);

	virtual int parent_depth(bool* has_human_parent=NULL, int cur_depth=0) const;

	virtual bool editor_force_standing() const;

	virtual game_logic::const_FormulaCallable_definition_ptr get_definition() const;

	entity_ptr standing_on() const { return standing_on_; }
	virtual void add_to_level();

	virtual rect platform_rect_at(int xpos) const;
	virtual int platform_slope_at(int xpos) const;

	virtual bool solid_platform() const;

	virtual void being_removed();
	virtual void being_added();

	//set up an animation schedule. values.size() should be a multiple of
	//slots.size().

private:
	void init_properties();
	custom_object& operator=(const custom_object& o);
	struct Accessor;

	struct gc_object_reference {
		entity* owner;
		entity* target;
		variant* from_variant;
		entity_ptr* from_ptr;
		std::shared_ptr<game_logic::FormulaCallableVisitor> visitor;
	};

	void extract_gc_object_references(std::vector<gc_object_reference>& v);
	void extract_gc_object_references(entity_ptr& e, std::vector<gc_object_reference>& v);
	void extract_gc_object_references(variant& var, std::vector<gc_object_reference>& v);
	static void restore_gc_object_reference(gc_object_reference ref);

	bool move_to_standing_internal(level& lvl, int max_displace);

	void process_frame();

	const_solid_info_ptr calculate_solid() const;
	const_solid_info_ptr calculate_platform() const;

	virtual void get_inputs(std::vector<game_logic::formula_input>* inputs) const;

	int slope_standing_on(int range) const;

	int previous_y_;

	variant custom_type_;
	const_custom_object_type_ptr type_; //the type after variations are applied
	const_custom_object_type_ptr base_type_; //the type without any variation
	std::vector<std::string> current_variation_;
	boost::intrusive_ptr<const frame> frame_;
	std::string frame_name_;
	int time_in_frame_;
	int time_in_frame_delta_;

	int velocity_x_, velocity_y_;
	int accel_x_, accel_y_;
	int gravity_shift_;
	decimal rotate_x_;
	decimal rotate_y_;
	decimal rotate_z_;

    void set_mid_x(int new_mid_x) {
        const int current_x = x() + current_frame().width()/2;
		const int xdiff = current_x - x();
		set_pos(new_mid_x - xdiff, y());
    }
    void set_mid_y(int new_mid_y) {
		const int current_y = y() + current_frame().height()/2;
		const int ydiff = current_y - y();
		set_pos(x(), new_mid_y - ydiff);
    }
    
	std::unique_ptr<std::pair<int, int> > parallax_scale_millis_;

	int zorder_;
	int zsub_order_;
	
	int hitpoints_, max_hitpoints_;
	bool was_underwater_;

	bool has_feet_;

	int invincible_;

	bool use_absolute_screen_coordinates_;
	
	int sound_volume_;	//see sound.cpp; valid values are 0-128, note that this affects all sounds spawned by this object

	game_logic::const_formula_ptr next_animation_formula_;

	game_logic::formula_variable_storage_ptr vars_, tmp_vars_;
	game_logic::MapFormulaCallablePtr tags_;

	variant& get_property_data(int slot) { if(property_data_.size() <= size_t(slot)) { property_data_.resize(slot+1); } return property_data_[slot]; }
	variant get_property_data(int slot) const { if(property_data_.size() <= size_t(slot)) { return variant(); } return property_data_[slot]; }
	std::vector<variant> property_data_;
	mutable int active_property_;

	//a stack of items that serve as the 'value' parameter, used in
	//property setters.
	mutable std::stack<variant> value_stack_;

	friend class active_property_scope;

	entity_ptr last_hit_by_;
	int last_hit_by_anim_;
	int current_animation_id_;

	int cycle_;

	bool created_;

	//variable which is always set to false on construction, and then the
	//first time process is called will fire the on_load event and set to false
	bool loaded_;

	std::vector<game_logic::const_formula_ptr> event_handlers_;

	entity_ptr standing_on_;

	int standing_on_prev_x_, standing_on_prev_y_;

	void make_draw_color();
	const graphics::color_transform& draw_color() const;
	std::shared_ptr<graphics::color_transform> draw_color_;

	std::shared_ptr<decimal> draw_scale_;
	std::shared_ptr<rect> draw_area_, activation_area_, clip_area_;
	int activation_border_;
	
	bool can_interact_with_;

	std::map<std::string, particle_system_ptr> particle_systems_;

	typedef std::shared_ptr<custom_object_text> custom_object_text_ptr;
	custom_object_text_ptr text_;

	std::vector<gui::vector_text_ptr> vector_text_;

	entity_ptr driver_;

	std::shared_ptr<BlurInfo> blur_;

	//set if we should fall through platforms. This is decremented automatically
	//at the end of every cycle.
	int fall_through_platforms_;

#ifdef USE_BOX2D
	box2d::body_ptr body_;
#endif

	bool always_active_;

	mutable std::stack<const FormulaCallable*> backup_callable_stack_;

	int last_cycle_active_;

	struct PositionSchedule {
		PositionSchedule() : speed(1), base_cycle(0), expires(false) {}
		int speed, base_cycle;
		bool expires;
		std::vector<int> x_pos;
		std::vector<int> y_pos;
		std::vector<decimal> rotation;
	};

	std::unique_ptr<PositionSchedule> position_schedule_;

	std::vector<std::shared_ptr<AnimatedMovement> > animated_movement_;

	std::vector<light_ptr> lights_;

	std::unique_ptr<rect> platform_area_;
	const_solid_info_ptr platform_solid_info_;

	point parent_position() const;

	//storage of the parent object while we're loading the object still.
	variant parent_loading_;

	entity_ptr parent_;
	std::string parent_pivot_;
	int parent_prev_x_, parent_prev_y_;
	bool parent_prev_facing_;
    int relative_x_, relative_y_;

	int min_difficulty_, max_difficulty_;

	std::shared_ptr<const std::vector<frame::CustomPoint> > custom_draw_;

	std::vector<float> custom_draw_xy_;
	std::vector<float> custom_draw_uv_;

	void set_platform_area(const rect& area);

	std::vector<int> platform_offsets_;

	bool swallow_mouse_event_;

	bool handleEvent_internal(int event, const FormulaCallable* context, bool executeCommands_now=true);
	std::vector<variant> delayed_commands_;

	int currently_handling_die_event_;

	typedef std::set<gui::WidgetPtr, gui::WidgetSortZOrder> widget_list;
	widget_list widgets_;

	rect previous_water_bounds_;

	mutable screen_position adjusted_draw_position_;

#if defined(USE_SHADERS)
	std::vector<graphics::DrawPrimitivePtr> DrawPrimitives_;
#endif

	bool paused_;

	std::vector<int> properties_requiring_dynamic_initialization_;

	// for lua integration
#if defined(USE_LUA)
	void init_lua();
	std::unique_ptr<lua::lua_context> lua_ptr_;
	std::unique_ptr<lua::compiled_chunk> lua_chunk_;
#endif
};
