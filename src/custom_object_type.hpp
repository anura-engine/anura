/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CUSTOM_OBJECT_TYPE_HPP_INCLUDED
#define CUSTOM_OBJECT_TYPE_HPP_INCLUDED

#include <map>
#include <string>

#include "boost/shared_ptr.hpp"

#include "custom_object_callable.hpp"
#include "editor_variable_info.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula_function.hpp"
#include "frame.hpp"
#include "lua_iface.hpp"
#include "particle_system.hpp"
#include "raster.hpp"
#include "solid_map_fwd.hpp"
#include "variant.hpp"
#include "variant_type.hpp"
#ifdef USE_BOX2D
#include "b2d_ffl.hpp"
#endif

class custom_object_type;

typedef boost::shared_ptr<custom_object_type> custom_object_type_ptr;
typedef boost::shared_ptr<const custom_object_type> const_custom_object_type_ptr;

std::map<std::string, std::string>& prototype_file_paths();

namespace wml {
class modifier;
typedef boost::shared_ptr<const modifier> const_modifier_ptr;
}

class custom_object_type
{
public:
	static void set_player_variant_type(variant type_str);

	static game_logic::formula_callable_definition_ptr get_definition(const std::string& id);
	static bool is_derived_from(const std::string& base, const std::string& derived);
	static variant merge_prototype(variant node, std::vector<std::string>* proto_paths=NULL);
	static const std::string* get_object_path(const std::string& id);
	static const_custom_object_type_ptr get(const std::string& id);
	static const_custom_object_type_ptr get_or_die(const std::string& id);
	static custom_object_type_ptr create(const std::string& id);
	static void invalidate_object(const std::string& id);
	static void invalidate_all_objects();
	static std::vector<const_custom_object_type_ptr> get_all();
	static std::vector<std::string> get_all_ids();

	//a function which returns all objects that have an editor category
	//mapped to the category they are in.
	struct EditorSummary {
		std::string category, help;
		variant first_frame;
	};

	static std::map<std::string,EditorSummary> get_editor_categories();

	static void set_file_contents(const std::string& path, const std::string& contents);

	static int reload_modified_code();
	static void reload_object(const std::string& type);

	static int num_object_reloads();

	typedef std::vector<game_logic::const_formula_ptr> event_handler_map;

	void init_event_handlers(variant node,
	                         event_handler_map& handlers,
							 game_logic::function_symbol_table* symbols=0,
							 const event_handler_map* base_handlers=NULL) const;

	custom_object_type(const std::string& id, variant node, const custom_object_type* base_type=NULL, const custom_object_type* old_type=NULL);
	~custom_object_type();

	const_custom_object_type_ptr get_sub_object(const std::string& id) const;

	const_custom_object_callable_ptr callable_definition() const { return callable_definition_; }

	const std::string& id() const { return id_; }
	int hitpoints() const { return hitpoints_; }

	int timer_frequency() const { return timer_frequency_; }

	const frame& default_frame() const;
	const frame& get_frame(const std::string& key) const;

	const game_logic::const_formula_ptr& next_animation_formula() const { return next_animation_formula_; }

	game_logic::const_formula_ptr get_event_handler(int event) const;
	int parallax_scale_millis_x() const {
		if(parallax_scale_millis_.get() == NULL){
			return 1000;
		}else{
			return parallax_scale_millis_->first;
		}
	}
	int parallax_scale_millis_y() const {
		if(parallax_scale_millis_.get() == NULL){
			return 1000;
		}else{
			return parallax_scale_millis_->second;
		}
	}
	
	int zorder() const { return zorder_; }
	int zsub_order() const { return zsub_order_; }
	bool is_human() const { return is_human_;}
	bool goes_inactive_only_when_standing() const { return goes_inactive_only_when_standing_; }
	bool dies_on_inactive() const { return dies_on_inactive_;}
	bool always_active() const { return always_active_;}
	bool body_harmful() const { return body_harmful_; }
	bool body_passthrough() const { return body_passthrough_; }
	bool ignore_collide() const { return ignore_collide_; }

#ifdef USE_BOX2D
	box2d::body_ptr body() { return body_; }
	box2d::const_body_ptr body() const { return body_; }
#endif

	int get_mouseover_delay() const { return mouseover_delay_; }
	const rect& mouse_over_area() const { return mouse_over_area_; }

	bool object_level_collisions() const { return object_level_collisions_; }

	int surface_friction() const { return surface_friction_; }
	int surface_traction() const { return surface_traction_; }
	int mass() const { return mass_; }

	//amount of friction we experience.
	int friction() const { return friction_; }
	int traction() const { return traction_; }
	int traction_in_air() const { return traction_in_air_; }
	int traction_in_water() const { return traction_in_water_; }

	bool respawns() const { return respawns_; }

	bool affected_by_currents() const { return affected_by_currents_; }

	variant get_child(const std::string& key) const {
		if(children_.count(key)) {
			return children_.find(key)->second;
		}

		return variant();
	}

	const_particle_system_factory_ptr get_particle_system_factory(const std::string& id) const;

	bool is_vehicle() const { return is_vehicle_; }

	int passenger_x() const { return passenger_x_; }
	int passenger_y() const { return passenger_y_; }

	int feet_width() const { return feet_width_; }

	int teleport_offset_x() const { return teleport_offset_x_; }
	int teleport_offset_y() const { return teleport_offset_y_; }
	bool no_move_to_standing() const { return no_move_to_standing_; }
	bool reverse_global_vertical_zordering() const { return reverse_global_vertical_zordering_; }

	bool serializable() const { return serializable_; }

	bool use_image_for_collisions() const { return use_image_for_collisions_; }
	bool static_object() const { return static_object_; }
	bool collides_with_level() const { return collides_with_level_; }
	bool has_feet() const { return has_feet_; }
	bool adjust_feet_on_animation_change() const { return adjust_feet_on_animation_change_; }
	bool use_absolute_screen_coordinates() const { return use_absolute_screen_coordinates_; }

	const std::map<std::string, variant>& variables() const { return variables_; }
	const std::map<std::string, variant>& tmp_variables() const { return tmp_variables_; }
	game_logic::const_map_formula_callable_ptr consts() const { return consts_; }
	const std::map<std::string, variant>& tags() const { return tags_; }

	struct property_entry {
		property_entry() : slot(-1), storage_slot(-1), persistent(true), requires_initialization(false), has_editor_info(false) {}
		std::string id;
		game_logic::const_formula_ptr getter, setter, init;
		boost::shared_ptr<variant> const_value;
		variant default_value;
		variant_type_ptr type, set_type;
		int slot, storage_slot;
		bool persistent;
		bool requires_initialization;
		bool has_editor_info;
	};

	const std::map<std::string, property_entry>& properties() const { return properties_; }
	const std::vector<property_entry>& slot_properties() const { return slot_properties_; }
	const std::vector<int>& properties_with_init() const { return properties_with_init_; }
	const std::vector<int>& properties_requiring_initialization() const { return properties_requiring_initialization_; }
	const std::vector<int>& properties_requiring_dynamic_initialization() const { return properties_requiring_dynamic_initialization_; }

	//this is the last required initialization property that should be
	//initialized. It's the only such property that has a custom setter.
	const std::string& last_initialization_property() const { return last_initialization_property_; }
	int slot_properties_base() const { return slot_properties_base_; }

	game_logic::function_symbol_table* function_symbols() const;

	const const_solid_info_ptr& solid() const { return solid_; }
	const const_solid_info_ptr& platform() const { return platform_; }

	const std::vector<int>& platform_offsets() const { return platform_offsets_; }

	bool solid_platform() const { return solid_platform_; }

	//true if the object can ever be solid or standable
	bool has_solid() const { return has_solid_; }

	unsigned int solid_dimensions() const { return solid_dimensions_; }
	unsigned int collide_dimensions() const { return collide_dimensions_; }

	unsigned int weak_solid_dimensions() const { return weak_solid_dimensions_; }
	unsigned int weak_collide_dimensions() const { return weak_collide_dimensions_; }

	const_custom_object_type_ptr get_variation(const std::vector<std::string>& variations) const;
	void load_variations() const;

#ifndef NO_EDITOR
	const_editor_entity_info_ptr editor_info() const { return editor_info_; }
#endif // !NO_EDITOR

	variant node() const { return node_; }

	int activation_border() const { return activation_border_; }
	const variant& available_frames() const { return available_frames_; }

	bool editor_force_standing() const { return editor_force_standing_; }
	bool hidden_in_game() const { return hidden_in_game_; }
	bool stateless() const { return stateless_; }

#if defined(USE_SHADERS)
	const gles2::shader_program_ptr& shader() const { return shader_; }
	const std::vector<gles2::shader_program_ptr>& effects() const { return effects_; }
#endif

	static void reload_file_paths();

	bool is_strict() const { return is_strict_; }

	const graphics::blend_mode* blend_mode() const { return blend_mode_.get(); }

	bool is_shadow() const { return is_shadow_; }

	bool truez() const { return true_z_; }
	double tx() const { return tx_; }
	double ty() const { return ty_; }
	double tz() const { return tz_; }

	const std::string& get_lua_source() const { return lua_source_; }
	void set_lua_source(const std::string& ls) { lua_source_ = ls; }

private:
	void init_sub_objects(variant node, const custom_object_type* old_type);

	//recreate an object type, optionally given the old version to base
	//things off where possible
	static custom_object_type_ptr recreate(const std::string& id, const custom_object_type* old_type);

	custom_object_callable_ptr callable_definition_;

	std::string id_;
	int hitpoints_;

	int timer_frequency_;

	typedef boost::intrusive_ptr<frame> frame_ptr;

	typedef std::map<std::string, std::vector<frame_ptr> > frame_map;
	frame_map frames_;
	variant available_frames_;

	boost::intrusive_ptr<frame> default_frame_;

	game_logic::const_formula_ptr next_animation_formula_;

	event_handler_map event_handlers_;
	boost::shared_ptr<game_logic::function_symbol_table> object_functions_;

	boost::shared_ptr<std::pair<int, int> > parallax_scale_millis_;
	
	int zorder_;
	int zsub_order_;

	bool is_human_;
	bool goes_inactive_only_when_standing_;
	bool dies_on_inactive_;
	bool always_active_;
	bool body_harmful_;
	bool body_passthrough_;
	bool ignore_collide_;
	bool object_level_collisions_;

	int surface_friction_;
	int surface_traction_;
	int friction_, traction_, traction_in_air_, traction_in_water_;
	int mass_;

	bool respawns_;

	bool affected_by_currents_;

	std::map<std::string, variant> children_;

	variant node_;

	std::map<std::string, const_particle_system_factory_ptr> particle_factories_;

	bool is_vehicle_;
	int passenger_x_, passenger_y_;
	int feet_width_;

	bool use_image_for_collisions_, static_object_, collides_with_level_;

	bool has_feet_;

	bool use_absolute_screen_coordinates_;

	int mouseover_delay_;
	rect mouse_over_area_;

	bool adjust_feet_on_animation_change_;

	std::map<std::string, variant> variables_, tmp_variables_;
	game_logic::map_formula_callable_ptr consts_;
	std::map<std::string, variant> tags_;

	std::map<std::string, property_entry> properties_;
	std::vector<property_entry> slot_properties_;
	std::vector<int> properties_with_init_, properties_requiring_initialization_, properties_requiring_dynamic_initialization_;
	std::string last_initialization_property_;
	int slot_properties_base_;

	int teleport_offset_x_, teleport_offset_y_;
	bool no_move_to_standing_;
	bool reverse_global_vertical_zordering_;
	
	bool serializable_;

	const_solid_info_ptr solid_, platform_;

	bool solid_platform_;

	//variable which is true if the object is ever solid or standable
	bool has_solid_;

	unsigned int solid_dimensions_, collide_dimensions_;
	unsigned int weak_solid_dimensions_, weak_collide_dimensions_;

	int activation_border_;

	std::map<std::string, game_logic::const_formula_ptr> variations_;
	mutable std::map<std::vector<std::string>, const_custom_object_type_ptr> variations_cache_;

#ifndef NO_EDITOR
	const_editor_entity_info_ptr editor_info_;
#endif // !NO_EDITOR

	std::map<std::string, const_custom_object_type_ptr> sub_objects_;

	bool editor_force_standing_;

	//object should be hidden in the game but will show in the editor.
	bool hidden_in_game_;

	//object is stateless, meaning that a backup of the object to restore
	//later will not deep copy the object, just have another reference to it.
	bool stateless_;

	std::vector<int> platform_offsets_;

#ifdef USE_SHADERS
	gles2::shader_program_ptr shader_;
	std::vector<gles2::shader_program_ptr> effects_;
#endif

#ifdef USE_BOX2D
	box2d::body_ptr body_;
#endif

	//does this object use strict checking?
	bool is_strict_;

	boost::shared_ptr<graphics::blend_mode> blend_mode_;

	//if this is a shadow, it will render only on top of foreground level
	//components.
	bool is_shadow_;

	bool true_z_;
	double tx_, ty_, tz_;

	// For lua integration
	std::string lua_source_;
};

#endif
