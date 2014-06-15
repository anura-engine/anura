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

#include <map>
#include <string>

#include "custom_object_callable.hpp"
#include "editor_variable_info.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula_function.hpp"
#include "frame.hpp"
#include "lua_iface.hpp"
#include "particle_system.hpp"
#include "solid_map_fwd.hpp"
#include "variant.hpp"
#include "variant_type.hpp"
#ifdef USE_BOX2D
#include "b2d_ffl.hpp"
#endif

class CustomObjectType;

typedef std::shared_ptr<CustomObjectType> CustomObjectTypePtr;
typedef std::shared_ptr<const CustomObjectType> ConstCustomObjectTypePtr;

std::map<std::string, std::string>& prototype_file_paths();

namespace wml 
{
	class modifier;
	typedef std::shared_ptr<const modifier> const_modifier_ptr;
}

class CustomObjectType
{
public:
	static void setPlayerVariantType(variant type_str);

	static game_logic::FormulaCallableDefinitionPtr getDefinition(const std::string& id);
	static bool isDerivedFrom(const std::string& base, const std::string& derived);
	static variant mergePrototype(variant node, std::vector<std::string>* proto_paths=NULL);
	static const std::string* getObjectPath(const std::string& id);
	static ConstCustomObjectTypePtr get(const std::string& id);
	static ConstCustomObjectTypePtr getOrDie(const std::string& id);
	static CustomObjectTypePtr create(const std::string& id);
	static void invalidateObject(const std::string& id);
	static void invalidateAllObjects();
	static std::vector<ConstCustomObjectTypePtr> getAll();
	static std::vector<std::string> getAllIds();

	//a function which returns all objects that have an editor category
	//mapped to the category they are in.
	struct EditorSummary {
		std::string category, help;
		variant first_frame;
	};

	static std::map<std::string,EditorSummary> getEditorCategories();

	static void setFileContents(const std::string& path, const std::string& contents);

	static int reloadModifiedCode();
	static void reloadObject(const std::string& type);

	static int numObjectReloads();

	typedef std::vector<game_logic::const_formula_ptr> event_handler_map;

	void initEventHandlers(variant node,
	                         event_handler_map& handlers,
							 game_logic::FunctionSymbolTable* symbols=0,
							 const event_handler_map* base_handlers=NULL) const;

	CustomObjectType(const std::string& id, variant node, const CustomObjectType* base_type=NULL, const CustomObjectType* old_type=NULL);
	~CustomObjectType();

	ConstCustomObjectTypePtr getSubObject(const std::string& id) const;

	ConstCustomObjectCallablePtr callableDefinition() const { return callable_definition_; }

	const std::string& id() const { return id_; }
	int getHitpoints() const { return hitpoints_; }

	int timerFrequency() const { return timerFrequency_; }

	const frame& defaultFrame() const;
	const frame& getFrame(const std::string& key) const;
	bool hasFrame(const std::string& key) const;

	const game_logic::const_formula_ptr& nextAnimationFormula() const { return next_animation_formula_; }

	game_logic::const_formula_ptr getEventHandler(int event) const;
	int parallaxScaleMillisX() const {
		if(parallax_scale_millis_.get() == NULL){
			return 1000;
		}else{
			return parallax_scale_millis_->first;
		}
	}
	int parallaxScaleMillisY() const {
		if(parallax_scale_millis_.get() == NULL){
			return 1000;
		}else{
			return parallax_scale_millis_->second;
		}
	}
	
	int zorder() const { return zorder_; }
	int zSubOrder() const { return zsub_order_; }
	bool isHuman() const { return is_human_;}
	bool goesInactiveOnlyWhenStanding() const { return goes_inactive_only_when_standing_; }
	bool diesOnInactive() const { return dies_on_inactive_;}
	bool isAlwaysActive() const { return always_active_;}
	bool isBodyHarmful() const { return body_harmful_; }
	bool isBodyPassthrough() const { return body_passthrough_; }
	bool hasIgnoreCollide() const { return ignore_collide_; }

#ifdef USE_BOX2D
	box2d::body_ptr body() { return body_; }
	box2d::const_body_ptr body() const { return body_; }
#endif

	int getMouseoverDelay() const { return mouseover_delay_; }
	const rect& getMouseOverArea() const { return mouse_over_area_; }

	bool hasObjectLevelCollisions() const { return object_level_collisions_; }

	int getSurfaceFriction() const { return surface_friction_; }
	int getSurfaceTraction() const { return surface_traction_; }
	int mass() const { return mass_; }

	//amount of friction we experience.
	int friction() const { return friction_; }
	int traction() const { return traction_; }
	int getTractionInAir() const { return traction_in_air_; }
	int getTractionInWater() const { return traction_in_water_; }

	bool respawns() const { return respawns_; }

	bool isAffectedByCurrents() const { return affected_by_currents_; }

	variant getChild(const std::string& key) const {
		if(children_.count(key)) {
			return children_.find(key)->second;
		}

		return variant();
	}

	ConstParticleSystemFactoryPtr getParticleSystemFactory(const std::string& id) const;

	bool isVehicle() const { return is_vehicle_; }

	int getPassengerX() const { return passenger_x_; }
	int getPassengerY() const { return passenger_y_; }

	int getFeetWidth() const { return feet_width_; }

	int getTeleportOffsetX() const { return teleport_offset_x_; }
	int getTeleportOffsetY() const { return teleport_offset_y_; }
	bool hasNoMoveToStanding() const { return no_move_to_standing_; }
	bool hasReverseGlobalVerticalZordering() const { return reverse_global_vertical_zordering_; }

	bool serializable() const { return serializable_; }

	bool useImageForCollisions() const { return use_image_for_collisions_; }
	bool isStaticObject() const { return static_object_; }
	bool collidesWithLevel() const { return collides_with_level_; }
	bool hasFeet() const { return has_feet_; }
	bool adjustFeetOnAnimationChange() const { return adjust_feet_on_animation_change_; }
	bool useAbsoluteScreenCoordinates() const { return use_absolute_screen_coordinates_; }

	const std::map<std::string, variant>& variables() const { return variables_; }
	const std::map<std::string, variant>& tmpVariables() const { return tmp_variables_; }
	game_logic::ConstMapFormulaCallablePtr consts() const { return consts_; }
	const std::map<std::string, variant>& tags() const { return tags_; }

	struct PropertyEntry {
		PropertyEntry() : slot(-1), storage_slot(-1), persistent(true), requires_initialization(false), has_editor_info(false) {}
		std::string id;
		game_logic::const_formula_ptr getter, setter, init;
		std::shared_ptr<variant> const_value;
		variant default_value;
		variant_type_ptr type, set_type;
		int slot, storage_slot;
		bool persistent;
		bool requires_initialization;
		bool has_editor_info;
	};

	const std::map<std::string, PropertyEntry>& properties() const { return properties_; }
	const std::vector<PropertyEntry>& getSlotProperties() const { return slot_properties_; }
	const std::vector<int>& getPropertiesWithInit() const { return properties_with_init_; }
	const std::vector<int>& getPropertiesRequiringInitialization() const { return properties_requiring_initialization_; }
	const std::vector<int>& getPropertiesRequiringDynamicInitialization() const { return properties_requiring_dynamic_initialization_; }

	//this is the last required initialization property that should be
	//initialized. It's the only such property that has a custom setter.
	const std::string& getLastInitializationProperty() const { return last_initialization_property_; }
	int getSlotPropertiesBase() const { return slot_properties_base_; }

	game_logic::FunctionSymbolTable* getFunctionSymbols() const;

	const ConstSolidInfoPtr& solid() const { return solid_; }
	const ConstSolidInfoPtr& platform() const { return platform_; }

	const std::vector<int>& getPlatformOffsets() const { return platform_offsets_; }

	bool isSolidPlatform() const { return solid_platform_; }

	//true if the object can ever be solid or standable
	bool hasSolid() const { return has_solid_; }

	unsigned int getSolidDimensions() const { return solid_dimensions_; }
	unsigned int getCollideDimensions() const { return collide_dimensions_; }

	unsigned int getWeakSolidDimensions() const { return weak_solid_dimensions_; }
	unsigned int getWeakCollideDimensions() const { return weak_collide_dimensions_; }

	ConstCustomObjectTypePtr getVariation(const std::vector<std::string>& variations) const;
	void loadVariations() const;

#ifndef NO_EDITOR
	const_editor_entity_info_ptr getEditorInfo() const { return editor_info_; }
#endif // !NO_EDITOR

	variant node() const { return node_; }

	int getActivationBorder() const { return activation_border_; }
	const variant& getAvailableFrames() const { return available_frames_; }

	bool editorForceStanding() const { return editor_force_standing_; }
	bool isHiddenInGame() const { return hidden_in_game_; }
	bool stateless() const { return stateless_; }

	static void ReloadFilePaths();

	bool isStrict() const { return is_strict_; }

	bool isShadow() const { return is_shadow_; }

	const std::string& getLuaSource() const { return lua_source_; }
	void setLuaSource(const std::string& ls) { lua_source_ = ls; }

private:
	void initSubObjects(variant node, const CustomObjectType* old_type);

	//recreate an object type, optionally given the old version to base
	//things off where possible
	static CustomObjectTypePtr recreate(const std::string& id, const CustomObjectType* old_type);

	CustomObjectCallablePtr callable_definition_;

	std::string id_;
	int hitpoints_;

	int timerFrequency_;

	typedef boost::intrusive_ptr<frame> frame_ptr;

	typedef std::map<std::string, std::vector<frame_ptr> > frame_map;
	frame_map frames_;
	variant available_frames_;

	boost::intrusive_ptr<frame> defaultFrame_;

	game_logic::const_formula_ptr next_animation_formula_;

	event_handler_map event_handlers_;
	std::shared_ptr<game_logic::FunctionSymbolTable> object_functions_;

	std::shared_ptr<std::pair<int, int> > parallax_scale_millis_;
	
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

	std::map<std::string, ConstParticleSystemFactoryPtr> particle_factories_;

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
	game_logic::MapFormulaCallablePtr consts_;
	std::map<std::string, variant> tags_;

	std::map<std::string, PropertyEntry> properties_;
	std::vector<PropertyEntry> slot_properties_;
	std::vector<int> properties_with_init_, properties_requiring_initialization_, properties_requiring_dynamic_initialization_;
	std::string last_initialization_property_;
	int slot_properties_base_;

	int teleport_offset_x_, teleport_offset_y_;
	bool no_move_to_standing_;
	bool reverse_global_vertical_zordering_;
	
	bool serializable_;

	ConstSolidInfoPtr solid_, platform_;

	bool solid_platform_;

	//variable which is true if the object is ever solid or standable
	bool has_solid_;

	unsigned int solid_dimensions_, collide_dimensions_;
	unsigned int weak_solid_dimensions_, weak_collide_dimensions_;

	int activation_border_;

	std::map<std::string, game_logic::const_formula_ptr> variations_;
	mutable std::map<std::vector<std::string>, ConstCustomObjectTypePtr> variations_cache_;

#ifndef NO_EDITOR
	const_editor_entity_info_ptr editor_info_;
#endif // !NO_EDITOR

	std::map<std::string, ConstCustomObjectTypePtr> sub_objects_;

	bool editor_force_standing_;

	//object should be hidden in the game but will show in the editor.
	bool hidden_in_game_;

	//object is stateless, meaning that a backup of the object to restore
	//later will not deep copy the object, just have another reference to it.
	bool stateless_;

	std::vector<int> platform_offsets_;

#ifdef USE_BOX2D
	box2d::body_ptr body_;
#endif

	//does this object use strict checking?
	bool is_strict_;

	//if this is a shadow, it will render only on top of foreground level
	//components.
	bool is_shadow_;

	// For lua integration
	std::string lua_source_;
};

