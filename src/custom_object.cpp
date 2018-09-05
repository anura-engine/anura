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

#include <boost/math/special_functions/round.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "BlendModeScope.hpp"
#include "Canvas.hpp"
#include "ClipScope.hpp"
#include "ColorScope.hpp"
#include "Font.hpp"
#include "ModelMatrixScope.hpp"
#include "ParticleSystem.hpp"
#include "RenderManager.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "StencilScope.hpp"
#include "WindowManager.hpp"

#include <stdio.h>

#include <cassert>
#include <iostream>

#include "asserts.hpp"
#include "code_editor_dialog.hpp"
#include "collision_utils.hpp"
#include "ColorTransform.hpp"
#include "custom_object.hpp"
#include "custom_object_callable.hpp"
#include "custom_object_functions.hpp"
#include "debug_console.hpp"
#include "difficulty.hpp"
#include "draw_primitive.hpp"
#include "draw_scene.hpp"
#include "ffl_dom.hpp"
#include "formatter.hpp"
#include "formula_callable.hpp"
#include "formula_callable_visitor.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"
#include "geometry_callable.hpp"
#include "graphical_font.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "level_logic.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "playable_custom_object.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "rectangle_rotator.hpp"
#include "screen_handling.hpp"
#include "string_utils.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"
#include "unit_test.hpp"
#include "utils.hpp"
#include "sound.hpp"
#include "widget_factory.hpp"

class ActivePropertyScope 
{
	const CustomObject& obj_;
	int prev_prop_;
	bool pop_value_stack_;
public:
	ActivePropertyScope(const CustomObject& obj, int prop_num, const variant* value=nullptr) 
		: obj_(obj), 
		prev_prop_(obj.active_property_), 
		pop_value_stack_(false)
	{
		obj_.active_property_ = prop_num;
		if(value) {
			obj_.value_stack_.push(*value);
			pop_value_stack_ = true;
		}
	}

	~ActivePropertyScope() {
		obj_.active_property_ = prev_prop_;
		if(pop_value_stack_) {
			obj_.value_stack_.pop();
		}
	}
};

namespace 
{
	const int widget_zorder_draw_later_threshold = 1000;

	const game_logic::FormulaVariableStoragePtr& global_vars()
	{
		static game_logic::FormulaVariableStoragePtr obj(new game_logic::FormulaVariableStorage());
		return obj;
	}

	PREF_BOOL(draw_objects_on_even_pixel_boundaries, true, "If true will only draw objects on 2-pixel boundaries");

	PREF_STRING(play_sound_function, "", "");
}

struct CustomObjectText 
{
	std::string text;
	ConstGraphicalFontPtr font;
	int size;
	int align;
	rect dimensions;
	int alpha;
};

namespace 
{
	std::string current_error_msg;

	std::vector<variant> deep_copy_property_data(const std::vector<variant>& property_data)
	{
		std::vector<variant> result;
		result.reserve(property_data.size());
		for(const variant& v : property_data) {
			result.emplace_back(deep_copy_variant(v));
		}

		return result;
	}
}

const std::string* CustomObject::currentDebugError()
{
	if(current_error_msg == "") {
		return nullptr;
	}

	return &current_error_msg;
}

void CustomObject::resetCurrentDebugError()
{
	current_error_msg = "";
}

CustomObject::CustomObject(variant node)
  : Entity(node),
    previous_y_(y()),
	custom_type_(node["custom_type"]),
    type_(custom_type_.is_map() ?
	ConstCustomObjectTypePtr(new CustomObjectType(custom_type_["id"].as_string(), custom_type_)) :
	CustomObjectType::get(node["type"].as_string())),
	base_type_(type_),
    frame_(&type_->defaultFrame()),
	frame_name_(node.has_key("current_frame") ? node["current_frame"].as_string() : "normal"),
	time_in_frame_(node["time_in_frame"].as_int(0)),
	time_in_frame_delta_(node["time_in_frame_delta"].as_int(1)),
	velocity_x_(node["velocity_x"].as_decimal(decimal(0))),
	velocity_y_(node["velocity_y"].as_decimal(decimal(0))),
	accel_x_(node["accel_x"].as_decimal()),
	accel_y_(node["accel_y"].as_decimal()),
	gravity_shift_(node["gravity_shift"].as_int(0)),
	hitpoints_(node["hitpoints"].as_int(type_->getHitpoints())),
	max_hitpoints_(node["max_hitpoints"].as_int(type_->getHitpoints()) - type_->getHitpoints()),
	was_underwater_(false),
	has_feet_(node["has_feet"].as_bool(type_->hasFeet())),
	invincible_(0),
	sound_volume_(1.0f),
	vars_(new game_logic::FormulaVariableStorage(type_->variables())),
	tmp_vars_(new game_logic::FormulaVariableStorage(type_->tmpVariables())),
	active_property_(-1),
	last_hit_by_anim_(0),
	current_animation_id_(0),
	cycle_(node["cycle"].as_int()),
	created_(node["created"].as_bool(false)), loaded_(false),
	standing_on_prev_x_(std::numeric_limits<int>::min()), standing_on_prev_y_(std::numeric_limits<int>::min()),
	can_interact_with_(false), fall_through_platforms_(0),
	always_active_(node["always_active"].as_bool(false)),
	activation_border_(node["activation_border"].as_int(type_->getActivationBorder())),
	clip_area_absolute_(node["clip_area_absolute"].as_bool(false)),
	last_cycle_active_(0),
	parent_pivot_(node["pivot"].as_string_default()),
	parent_prev_x_(std::numeric_limits<int>::min()), parent_prev_y_(std::numeric_limits<int>::min()), parent_prev_facing_(true),
    relative_x_(node["relative_x"].as_int(0)), relative_y_(node["relative_y"].as_int(0)),
	editor_only_(node["editor_only"].as_bool(false)),
	collides_with_level_(node["collides_with_level"].as_bool(type_->collidesWithLevel())),
	currently_handling_die_event_(0),
	use_absolute_screen_coordinates_(node["use_absolute_screen_coordinates"].as_bool(type_->useAbsoluteScreenCoordinates())),
	paused_(false),
	shader_flags_(0),
	particles_(),
	document_(nullptr)
{
	setZOrder(node["zorder"].as_int(type_->zorder()));
	setZSubOrder(node["zsub_order"].as_int(type_->zSubOrder()));


	vars_->setObjectName(getDebugDescription());
	tmp_vars_->setObjectName(getDebugDescription());

	if(!created_) {
		properties_requiring_dynamic_initialization_ = type_->getPropertiesRequiringDynamicInitialization();
		properties_requiring_dynamic_initialization_.insert(properties_requiring_dynamic_initialization_.end(), type_->getPropertiesRequiringInitialization().begin(), type_->getPropertiesRequiringInitialization().end());
	}

	vars_->disallowNewKeys(type_->isStrict());
	tmp_vars_->disallowNewKeys(type_->isStrict());

	getAll().insert(this);
	getAll(base_type_->id()).insert(this);

	if(node.has_key("platform_area")) {
		set_platform_area(rect(node["platform_area"]));
	}

	if(node.has_key("x_schedule")) {
		if(position_schedule_.get() == nullptr) {
			position_schedule_.reset(new PositionSchedule);
		}

		if(node["x_schedule"].is_string()) {
			// old-style list of ints inside a string
			position_schedule_->x_pos = util::split_into_vector_int(node["x_schedule"].as_string());
		} else {
			position_schedule_->x_pos = node["x_schedule"].as_list_int();
		}
	}

	if(node.has_key("y_schedule")) {
		if(position_schedule_.get() == nullptr) {
			position_schedule_.reset(new PositionSchedule);
		}

		if(node["y_schedule"].is_string()) {
			// old-style list of ints inside a string
			position_schedule_->y_pos = util::split_into_vector_int(node["y_schedule"].as_string());
		} else {
			position_schedule_->y_pos = node["y_schedule"].as_list_int();
		}
	}

	if(node.has_key("rotation_schedule")) {
		if(position_schedule_.get() == nullptr) {
			position_schedule_.reset(new PositionSchedule);
		}

		position_schedule_->rotation = node["rotation_schedule"].as_list_decimal();	
	}

	if(position_schedule_.get() != nullptr && node.has_key("schedule_speed")) {
		position_schedule_->speed = node["schedule_speed"].as_int();
	}

	if(position_schedule_.get() != nullptr && node.has_key("schedule_base_cycle")) {
		position_schedule_->base_cycle = node["schedule_base_cycle"].as_int();
	}

	if(position_schedule_.get() != nullptr && node.has_key("schedule_expires") && node["schedule_expires"].as_bool()) {
		position_schedule_->expires = true;
	}

	if(node.has_key("draw_area")) {
		variant draw_area = node["draw_area"];
		ASSERT_LOG(draw_area.is_list() || draw_area.is_map(), "draw_area must be a list or map " << draw_area.debug_location());
		draw_area_.reset(new rect(draw_area));
	}

	if(node.has_key("draw_scale")) {
		draw_scale_.reset(new decimal(decimal::from_raw_value(static_cast<int64_t>(node["draw_scale"].as_int()))));
	}

	if(node.has_key("activation_area")) {
		activation_area_.reset(new rect(node["activation_area"]));
	}

	if(node.has_key("clip_area")) {
		clip_area_.reset(new rect(node["clip_area"]));
	}

	if(node.has_key("variations")) {
		current_variation_ = util::split(node["variations"].as_string());
		type_ = base_type_->getVariation(current_variation_);
	}

	if(node.has_key("parallax_scale_x") || node.has_key("parallax_scale_y")) {
		parallax_scale_millis_.reset(new std::pair<int, int>(node["parallax_scale_x"].as_int(type_->parallaxScaleMillisX()), node["parallax_scale_y"].as_int(type_->parallaxScaleMillisY())));
	} else {
		parallax_scale_millis_.reset(new std::pair<int, int>(type_->parallaxScaleMillisX(), type_->parallaxScaleMillisY()));
	}

	min_difficulty_ = node.has_key("min_difficulty") ? difficulty::from_variant(node["min_difficulty"]) : -1;
	max_difficulty_ = node.has_key("max_difficulty") ? difficulty::from_variant(node["max_difficulty"]) : -1;

	vars_->read(node["vars"]);

	unsigned int solid_dim = type_->getSolidDimensions();
	unsigned int weak_solid_dim = type_->getWeakSolidDimensions();
	unsigned int collide_dim = type_->getCollideDimensions();
	unsigned int weak_collide_dim = type_->getWeakCollideDimensions();

	if(node.has_key("solid_dimensions")) {
		weak_solid_dim = solid_dim = 0;
		std::vector<std::string> solid_dim_str = util::split(node["solid_dimensions"].as_string());
		for(const std::string& str : solid_dim_str) {
			if(str.empty() || str == "level_only") {
				continue;
			}

			if(str[0] == '~') {
				const int id = get_solid_dimension_id(std::string(str.begin() + 1, str.end()));
				weak_solid_dim |= 1 << id;
			} else {
				const int id = get_solid_dimension_id(str);
				solid_dim |= 1 << id;
			}
		}
	}

	if(node.has_key("collide_dimensions")) {
		weak_collide_dim = collide_dim = 0;
		std::vector<std::string> collide_dim_str = util::split(node["collide_dimensions"].as_string());
		for(const std::string& str : collide_dim_str) {
			if(str.empty() || str == "level_only") {
				continue;
			}

			if(str[0] == '~') {
				const int id = get_solid_dimension_id(std::string(str.begin() + 1, str.end()));
				weak_collide_dim |= 1 << id;
			} else {
				const int id = get_solid_dimension_id(str);
				collide_dim |= 1 << id;
			}
		}
	}

	setSolidDimensions(solid_dim, weak_solid_dim);
	setCollideDimensions(collide_dim, weak_collide_dim);

	variant tags_node = node["tags"];
	if(tags_node.is_null() == false) {
		tags_ = new game_logic::MapFormulaCallable(tags_node);
	} else {
		tags_ = new game_logic::MapFormulaCallable(type_->tags());
	}

	if(node.has_key("draw_color")) {
		draw_color_.reset(new KRE::ColorTransform(node["draw_color"]));
	}

	if(node.has_key("label")) {
		setLabel(node["label"].as_string());
	} else {
		setDistinctLabel();
	}

	if(!type_->respawns()) {
		setRespawn(false);
	}

	assert(type_.get());
	//setFrameNoAdjustments(frame_name_);

	if (node.has_key("frame_obj")) {
		FramePtr f(new Frame(node["frame_obj"]));
		f->SetNeedsSerialization(true);
		if (type_->useImageForCollisions()) {
			f->setImageAsSolid();
		}
		frame_ = f;
	}
	else {
		frame_.reset(&type_->getFrame(frame_name_));
	}
	calculateSolidRect();

	next_animation_formula_ = type_->nextAnimationFormula();

	type_->initEventHandlers(node, event_handlers_);

	can_interact_with_ = getEventHandler(OBJECT_EVENT_INTERACT).get() != nullptr;

	variant text_node = node["text"];
	if(!text_node.is_null()) {
		setText(text_node["text"].as_string(), text_node["font"].as_string(), text_node["size"].as_int(2), text_node["align"].as_int(-1));
	}

	if(node.has_key("particles")) {
		std::vector<std::string> particles = util::split(node["particles"].as_string());
		for(const std::string& p : particles) {
			addParticleSystem(p, p);
		}
	}

	if(node.has_key("lights")) {
		for(variant light_node : node["lights"].as_list()) {
			LightPtr new_light(Light::createLight(*this, light_node));
			if(new_light) {
				lights_.emplace_back(new_light);
			}
		}
	}

	if(node.has_key("parent")) {
		parent_loading_.serialize_from_string(node["parent"].as_string());
	}

	if(node.has_key("platform_offsets")) {
		platform_offsets_ = node["platform_offsets"].as_list_int();
	} else {
		platform_offsets_ = type_->getPlatformOffsets();
	}

	if(node.has_key("mouseover_area")) {
		setMouseOverArea(rect(node["mouseover_area"]));
	}

	setMouseoverDelay(node["mouseover_delay"].as_int(0));

	// XXX Process shader and effects here if needed.

#ifdef USE_BOX2D
	if(node.has_key("body")) {
		body_.reset(new box2d::body(node["body"]));
	}
#endif

#if defined(USE_LUA)
	if(type_->has_lua()) {
		lua_ptr_.reset(new lua::LuaContext());
	}
#endif

	if(type_->getShader() != nullptr) {
		shader_ = graphics::AnuraShaderPtr(new graphics::AnuraShader(*type_->getShader()));
	}

	if(node.has_key("shader")) {
		if(node["shader"].is_string()) {
			std::string shader_name = node["shader"].as_string();
			shader_.reset(new graphics::AnuraShader(shader_name));
		} else {
			std::string shader_name = node["shader"]["name"].as_string();
			shader_.reset(new graphics::AnuraShader(shader_name, node["shader"]));
		}
	}
	if(node.has_key("effects")) {
		const variant& value = node["effects"];
		if(value.is_list()) {
			for(int n = 0; n != value.num_elements(); ++n) {
				if(value[n].is_string()) {
					effects_shaders_.emplace_back(new graphics::AnuraShader(value[n].as_string()));
				} else {
					effects_shaders_.emplace_back(new graphics::AnuraShader(value[n]["name"].as_string(), value["name"]));
				}
			}
		} else if(value.is_map()) {
			effects_shaders_.emplace_back(new graphics::AnuraShader(value["name"].as_string(), value["name"]));
		} else if(value.is_string()) {
			effects_shaders_.emplace_back(new graphics::AnuraShader(value.as_string()));
		} else {
			effects_shaders_.emplace_back(graphics::AnuraShaderPtr(value.try_convert<graphics::AnuraShader>()));
			ASSERT_LOG(effects_shaders_.size() > 0, "Couldn't convert type to shader");
		}
	}

	if(node.has_key("xhtml")) {
		document_.reset(new xhtml::DocumentObject(node));
		document_->init(this);
	} else {		
		document_ = type_->getDocument();
		if(document_ != nullptr) {
			document_->init(this);
		}
	}

	createParticles(type_->getParticleSystemDesc());

	const variant property_data_node = node["property_data"];

	//Init all properties: pass 1, properties specified in the node, or that don't have an init.
	for (int i = 0; i != type_->getSlotProperties().size(); ++i) {
		const CustomObjectType::PropertyEntry& e = type_->getSlotProperties()[i];
		if (e.storage_slot < 0) {
			continue;
		}

		bool set = false;
		if (property_data_node.is_map()) {
			const variant key(e.id);
			if (property_data_node.has_key(key)) {
				get_property_data(e.storage_slot) = property_data_node[key];
				if (e.is_weak) { get_property_data(e.storage_slot).weaken(); }
				set = true;
			}
		}

		if (set == false && e.init) {
			//this initializes in the second pass below
			continue;
		}

		if (set == false) {
			get_property_data(e.storage_slot) = deep_copy_variant(e.default_value);
			if (e.is_weak) { get_property_data(e.storage_slot).weaken(); }
		}

		if (!get_property_data(e.storage_slot).is_null()) {
			properties_requiring_dynamic_initialization_.erase(std::remove(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), i), properties_requiring_dynamic_initialization_.end());
		}
	}

	//Init all properties pass 2, properties that have an init and weren't in the node.
	for (int i = 0; i != type_->getSlotProperties().size(); ++i) {
		const CustomObjectType::PropertyEntry& e = type_->getSlotProperties()[i];
		if (e.storage_slot < 0 || !e.init) {
			continue;
		}

		if (property_data_node.is_map()) {
			const variant key(e.id);
			if (property_data_node.has_key(key)) {
				continue;
			}
		}


		reference_counted_object_pin_norelease pin(this);
		get_property_data(e.storage_slot) = e.init->execute(*this);
		if(e.is_weak) { get_property_data(e.storage_slot).weaken(); }

		if(!get_property_data(e.storage_slot).is_null()) {
			properties_requiring_dynamic_initialization_.erase(std::remove(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), i), properties_requiring_dynamic_initialization_.end());
		}
	}
}

CustomObject::CustomObject(const std::string& type, int x, int y, bool face_right, bool deferInitProperties)
  : Entity(x, y, face_right),
    previous_y_(y),
    type_(CustomObjectType::getOrDie(type)),
	base_type_(type_),
	frame_(&type_->defaultFrame()),
    frame_name_("normal"),
	time_in_frame_(0), time_in_frame_delta_(1),
	velocity_x_(0), velocity_y_(0),
	accel_x_(0), accel_y_(0), gravity_shift_(0),
	hitpoints_(type_->getHitpoints()),
	max_hitpoints_(0),
	was_underwater_(false),
	has_feet_(type_->hasFeet()),
	invincible_(0),
	sound_volume_(1.0f),
	vars_(new game_logic::FormulaVariableStorage(type_->variables())),
	tmp_vars_(new game_logic::FormulaVariableStorage(type_->tmpVariables())),
	tags_(new game_logic::MapFormulaCallable(type_->tags())),
	active_property_(-1),
	last_hit_by_anim_(0),
	current_animation_id_(0),
	cycle_(0),
	created_(false), loaded_(false), fall_through_platforms_(0),
	always_active_(false),
	activation_border_(type_->getActivationBorder()),
	clip_area_absolute_(false),
	can_interact_with_(false),
	last_cycle_active_(0),
	parent_prev_x_(std::numeric_limits<int>::min()), parent_prev_y_(std::numeric_limits<int>::min()), parent_prev_facing_(true),
    relative_x_(0), relative_y_(0),
	editor_only_(type_->editorOnly()),
	collides_with_level_(type_->collidesWithLevel()),
	min_difficulty_(-1), max_difficulty_(-1),
	currently_handling_die_event_(0),
	use_absolute_screen_coordinates_(type_->useAbsoluteScreenCoordinates()),
	paused_(false),
	shader_flags_(0),
	particles_(),
	document_(nullptr)
{
	setZOrder(type_->zorder());
	setZSubOrder(type_->zSubOrder());

	properties_requiring_dynamic_initialization_ = type_->getPropertiesRequiringDynamicInitialization();
	properties_requiring_dynamic_initialization_.insert(properties_requiring_dynamic_initialization_.end(), type_->getPropertiesRequiringInitialization().begin(), type_->getPropertiesRequiringInitialization().end());

	vars_->setObjectName(getDebugDescription());
	tmp_vars_->setObjectName(getDebugDescription());

	vars_->disallowNewKeys(type_->isStrict());
	tmp_vars_->disallowNewKeys(type_->isStrict());

	for(std::map<std::string, CustomObjectType::PropertyEntry>::const_iterator i = type_->properties().begin(); i != type_->properties().end(); ++i) {
		if(i->second.storage_slot < 0) {
			continue;
		}

		get_property_data(i->second.storage_slot) = deep_copy_variant(i->second.default_value);
	}

	getAll().insert(this);
	getAll(base_type_->id()).insert(this);

	if(type_->getShader() != nullptr) {
		shader_ = graphics::AnuraShaderPtr(new graphics::AnuraShader(*type_->getShader()));
	}
	for(auto eff : type_->getEffectsShaders()) {
		effects_shaders_.emplace_back(new graphics::AnuraShader(*eff));
	}

	if(type_->getDocument() != nullptr) {
		document_ = type_->getDocument();
		if(document_ != nullptr) {
			document_->init(this);
		}
	}

#ifdef USE_BOX2D
	if(type_->body()) {
		body_.reset(new box2d::body(*type_->body()));
	}
#endif

	setSolidDimensions(type_->getSolidDimensions(),
	                     type_->getWeakSolidDimensions());
	setCollideDimensions(type_->getCollideDimensions(),
	                       type_->getWeakCollideDimensions());

	{
		//generate a random label for the object
		char buf[64];
		sprintf(buf, "_%x", rand());
		setLabel(buf);
	}

	parallax_scale_millis_.reset(new std::pair<int, int>(type_->parallaxScaleMillisX(), type_->parallaxScaleMillisY()));

	assert(type_.get());
	setFrameNoAdjustments(frame_name_);

	next_animation_formula_ = type_->nextAnimationFormula();

#if defined(USE_LUA)
	if(type_->has_lua()) {
		lua_ptr_.reset(new lua::LuaContext());
	}
#endif
	
	setMouseoverDelay(type_->getMouseoverDelay());
	if(type_->getMouseOverArea().w() != 0) {
		setMouseOverArea(type_->getMouseOverArea());
	}
	createParticles(type_->getParticleSystemDesc());
	initProperties(deferInitProperties);
}

CustomObject::CustomObject(const CustomObject& o)
	: Entity(o),
	previous_y_(o.previous_y_),
	custom_type_(o.custom_type_),
	type_(o.type_),
	base_type_(o.base_type_),
	current_variation_(o.current_variation_),
	frame_(o.frame_),
	frame_name_(o.frame_name_),
	time_in_frame_(o.time_in_frame_),
	time_in_frame_delta_(o.time_in_frame_delta_),
	velocity_x_(o.velocity_x_), velocity_y_(o.velocity_y_),
	accel_x_(o.accel_x_), accel_y_(o.accel_y_),
	gravity_shift_(o.gravity_shift_),
	parallax_scale_millis_(new std::pair<int, int>(*o.parallax_scale_millis_)),
	hitpoints_(o.hitpoints_),
	max_hitpoints_(o.max_hitpoints_),
	was_underwater_(o.was_underwater_),
	has_feet_(o.has_feet_),
	invincible_(o.invincible_),
	use_absolute_screen_coordinates_(o.use_absolute_screen_coordinates_),
	sound_volume_(o.sound_volume_),
	next_animation_formula_(o.next_animation_formula_),

	vars_(new game_logic::FormulaVariableStorage(*o.vars_)),
	tmp_vars_(new game_logic::FormulaVariableStorage(*o.tmp_vars_)),
	tags_(new game_logic::MapFormulaCallable(*o.tags_)),

	property_data_(deep_copy_property_data(o.property_data_)),

	active_property_(-1),
	last_hit_by_(o.last_hit_by_),
	last_hit_by_anim_(o.last_hit_by_anim_),
	current_animation_id_(o.current_animation_id_),
	cycle_(o.cycle_),
	created_(o.created_),
	loaded_(o.loaded_),
	event_handlers_(o.event_handlers_),
	standing_on_(o.standing_on_),
	standing_on_prev_x_(o.standing_on_prev_x_), standing_on_prev_y_(o.standing_on_prev_y_),
	draw_color_(o.draw_color_ ? new KRE::ColorTransform(*o.draw_color_) : nullptr),
	draw_scale_(o.draw_scale_ ? new decimal(*o.draw_scale_) : nullptr),
	draw_area_(o.draw_area_ ? new rect(*o.draw_area_) : nullptr),
	activation_area_(o.activation_area_ ? new rect(*o.activation_area_) : nullptr),
	clip_area_(o.clip_area_ ? new rect(*o.clip_area_) : nullptr),
	activation_border_(o.activation_border_),
	clip_area_absolute_(o.clip_area_absolute_),
	can_interact_with_(o.can_interact_with_),
	particle_systems_(o.particle_systems_),
	text_(o.text_),
	driver_(o.driver_),
	fall_through_platforms_(o.fall_through_platforms_),
	always_active_(o.always_active_),
	last_cycle_active_(0),
	parent_(o.parent_),
	parent_pivot_(o.parent_pivot_),
	parent_prev_x_(o.parent_prev_x_),
	parent_prev_y_(o.parent_prev_y_),
	parent_prev_facing_(o.parent_prev_facing_),
    relative_x_(o.relative_x_),
    relative_y_(o.relative_y_),
	min_difficulty_(o.min_difficulty_),
	max_difficulty_(o.max_difficulty_),
	custom_draw_(o.custom_draw_),
	platform_offsets_(o.platform_offsets_),
	editor_only_(o.editor_only_),
	collides_with_level_(o.collides_with_level_),
	currently_handling_die_event_(0),
	//do NOT copy widgets since they do not support deep copying
	//and re-seating references is difficult.
	//widgets_(o.widgets_),
	paused_(o.paused_),
	shader_flags_(0),
	particles_(o.particles_),
	document_(nullptr)
{
	properties_requiring_dynamic_initialization_ = o.properties_requiring_dynamic_initialization_;

	vars_->setObjectName(getDebugDescription());
	tmp_vars_->setObjectName(getDebugDescription());

	vars_->disallowNewKeys(type_->isStrict());
	tmp_vars_->disallowNewKeys(type_->isStrict());

	getAll().insert(this);
	getAll(base_type_->id()).insert(this);

#ifdef USE_BOX2D
	std::stringstream ss;
	if(o.body_) {
		body_.reset(new box2d::body(*o.body_));
	}
#endif
	setMouseoverDelay(o.getMouseoverDelay());
	setMouseOverArea(o.getMouseOverArea());
	
#if defined(USE_LUA)
	if(type_->has_lua()) {
		lua_ptr_.reset(new lua::LuaContext());
	}
#endif

	if(o.shader_ != nullptr) {
		shader_.reset(new graphics::AnuraShader(*o.shader_));
	}
	for(auto eff : o.effects_shaders_) {
		effects_shaders_.emplace_back(new graphics::AnuraShader(*eff));
	}

	if(o.document_) {
		document_.reset(new xhtml::DocumentObject(*o.document_));
		document_->init(this);
	}
}

CustomObject::~CustomObject()
{
	getAll().erase(this);
	getAll(base_type_->id()).erase(this);

	sound::stop_looped_sounds(this);
}

void CustomObject::validate_properties()
{
	//TODO: make this more efficient. For now it errs on the side of
	//providing lots of debug info.
	for(int n = 0; n != type_->getSlotProperties().size(); ++n) {
		const CustomObjectType::PropertyEntry& e = type_->getSlotProperties()[n];
		if(e.storage_slot >= 0 && e.type && std::count(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), n) == 0) {
			assert(static_cast<unsigned>(e.storage_slot) < property_data_.size());
			variant result = property_data_[e.storage_slot];
			ASSERT_LOG(e.type->match(result), "Object " << getDebugDescription() << " is invalid, property " << e.id << " expected to be " << e.type->to_string() << " but found " << result.write_json() << " which is of type " << get_variant_type_from_value(result)->to_string() << " " << properties_requiring_dynamic_initialization_.size());
			
		}
	}
}

void CustomObject::initProperties(bool defer)
{
	for(std::map<std::string, CustomObjectType::PropertyEntry>::const_iterator i = type_->properties().begin(); i != type_->properties().end(); ++i) {
		if(!i->second.init || i->second.storage_slot == -1) {
			continue;
		}

		if(defer) {
			property_init_deferred_.push_back(&i->second);
			continue;
		}

		reference_counted_object_pin_norelease pin(this);
		initProperty(i->second);
		if(i->second.is_weak) { get_property_data(i->second.storage_slot).weaken(); }
	}
}

void CustomObject::initDeferredProperties()
{
	while(property_init_deferred_.empty() == false) {
		auto p = property_init_deferred_.back();
		property_init_deferred_.pop_back();
		initProperty(*p);
	}
}

void CustomObject::initProperty(const CustomObjectType::PropertyEntry& e)
{
	get_property_data(e.storage_slot) = e.init->execute(*this);
}


bool CustomObject::isA(const std::string& type) const
{
	return CustomObjectType::isDerivedFrom(type, type_->id());
}

bool CustomObject::isA(int type_index) const
{
	return CustomObjectType::isDerivedFrom(type_index, type_->numericId());
}

void CustomObject::finishLoading(Level* lvl)
{
	if(parent_loading_.is_null() == false) {
		EntityPtr p = parent_loading_.try_convert<Entity>();
		if(p) {
			parent_ = p;
		}
		parent_loading_ = variant();
	}

	// XXX Do shader/effects initialisation here (like setting object on them)

#ifdef USE_BOX2D
	if(body_) {
		body_->finishLoading(this);
	}
#endif

#if defined(USE_LUA)
	init_lua();
#endif

	if(shader_ != nullptr) {
		shader_->setParent(this);
		//LOG_DEBUG("shader '" << shader_->getName() << "' attached to object: " << type_->id());
	}
	for(auto eff : effects_shaders_) {
		eff->setParent(this);
	}
}


#if defined(USE_LUA)
void CustomObject::init_lua()
{
	if(lua_ptr_) {
		lua_ptr_->setSelfCallable(*this);
		if (auto init_script = type_->getLuaInit(*lua_ptr_)) {
			init_script->run(*lua_ptr_);
		}
	}
}
#endif

void CustomObject::createParticles(const variant& node)
{
	if(node.is_null()) {
		particles_.reset();
	} else {
		particles_.reset(new graphics::ParticleSystemContainerProxy(node));
	}
}

bool CustomObject::serializable() const
{
	return type_->serializable();
}

variant CustomObject::write() const
{
	variant_builder res;

	res.add("_uuid", write_uuid(uuid()));

	if(created_) {
		res.add("created", true);
	}

	if(parallax_scale_millis_.get() != nullptr) {
		if( (type_->parallaxScaleMillisX() !=  parallax_scale_millis_->first) || (type_->parallaxScaleMillisY() !=  parallax_scale_millis_->second)){
			res.add("parallax_scale_x", parallax_scale_millis_->first);
			res.add("parallax_scale_y", parallax_scale_millis_->second);
		}
	}

	if(platform_area_.get() != nullptr) {
		res.add("platform_area", platform_area_->write());
	}

	if(always_active_) {
		res.add("always_active", true);
	}

	if(activation_border_ != type_->getActivationBorder()) {
		res.add("activation_border", activation_border_);
	}

	if(clip_area_absolute_) {
		res.add("clip_area_absolute", true);
	}
	
	if(position_schedule_.get() != nullptr) {
		res.add("schedule_speed", position_schedule_->speed);
		if(position_schedule_->x_pos.empty() == false) {
			for(int xpos : position_schedule_->x_pos) {
				res.add("x_schedule", xpos);
			}
		}

		if(position_schedule_->y_pos.empty() == false) {
			for(int ypos : position_schedule_->y_pos) {
				res.add("y_schedule", ypos);
			}
		}

		if(position_schedule_->rotation.empty() == false) {
			res.add("rotation_schedule", vector_to_variant(position_schedule_->rotation));
		}

		if(position_schedule_->base_cycle != 0) {
			res.add("schedule_base_cycle", position_schedule_->base_cycle);
		}

		if(position_schedule_->expires) {
			res.add("schedule_expires", true);
		}
	}

	if(!attachedObjects().empty()) {
		std::string s;

		for(const EntityPtr& e : attachedObjects()) {
			if(s.empty() == false) {
				s += ",";
			}

			s += write_uuid(e->uuid());
		}

		res.add("attached_objects", s);
	}

	if(!current_variation_.empty()) {
		res.add("variations", util::join(current_variation_));
	}

	if(draw_color_ && (!draw_color_->fits_in_color() || draw_color_->toColor().asARGB() != 0xFFFFFFFF)) {
		res.add("draw_color", draw_color_->write());
	}

	if(label().empty() == false) {
		res.add("label", label());
	}

	if(cycle_ > 1) {
		res.add("cycle", cycle_);
	}

	if(frame_name_ != "default") {
		res.add("current_frame", frame_name_);
	}

	if (frame_ && frame_->GetNeedsSerialization()) {
		res.add("frame_obj", frame_->write());
	}

	res.add("custom", true);
	res.add("type", type_->id());
	res.add("x", x());
	res.add("y", y());

	if(getAnchorX() >= 0) {
		res.add("anchorx", getAnchorX());
	}

	if(getAnchorY() >= 0) {
		res.add("anchory", getAnchorY());
	}

	if(getRotateZ() != decimal()) {
		res.add("rotate", getRotateZ());
	}

    if(velocity_x_ != decimal(0)) {
        res.add("velocity_x", velocity_x_);
    }
    if(velocity_y_ != decimal(0)) {
        res.add("velocity_y", velocity_y_);
    }
	
	if(getPlatformMotionX()) {
		res.add("platform_motion_x", getPlatformMotionX());
	}

	if(getSolidDimensions() != type_->getSolidDimensions() ||
	   getWeakSolidDimensions() != type_->getWeakSolidDimensions()) {
		std::string solid_dim;
		for(int n = 0; n != 32; ++n) {
			if(getSolidDimensions()&(1 << n)) {
				if(!solid_dim.empty()) {
					solid_dim += ",";
				}

				solid_dim += get_solid_dimension_key(n);
			}

			if(getWeakSolidDimensions()&(1 << n)) {
				if(!solid_dim.empty()) {
					solid_dim += ",";
				}

				solid_dim += "~" + get_solid_dimension_key(n);
			}
		}

		if(solid_dim.empty()) {
			solid_dim = "level_only";
		}

		res.add("solid_dimensions", solid_dim);
	}

	if(getCollideDimensions() != type_->getCollideDimensions() ||
	   getWeakCollideDimensions() != type_->getWeakCollideDimensions()) {
		std::string collide_dim, weak_collide_dim;
		for(int n = 0; n != 32; ++n) {
			if(getCollideDimensions()&(1 << n)) {
				if(!collide_dim.empty()) {
					collide_dim += ",";
				}

				collide_dim += get_solid_dimension_key(n);
			}

			if(getWeakCollideDimensions()&(1 << n)) {
				if(!collide_dim.empty()) {
					collide_dim += ",";
				}

				collide_dim += "~" + get_solid_dimension_key(n);
			}
		}

		if(collide_dim.empty()) {
			collide_dim = "level_only";
		}

		res.add("collide_dimensions", collide_dim);
	}

	if(hitpoints_ != type_->getHitpoints() || max_hitpoints_ != 0) {
		res.add("hitpoints", hitpoints_);
		res.add("max_hitpoints", type_->getHitpoints() + max_hitpoints_);
	}


	// XXX write out shader and effects here.

#if defined(USE_BOX2D)
	if(body_) {
		res.add("body", body_->write()); 
	}
#endif

	if(zorder() != type_->zorder()) {
		res.add("zorder", zorder());
	}

	if(parallax_scale_millis_.get()) {
		if(parallax_scale_millis_->first != type_->parallaxScaleMillisX() || parallax_scale_millis_->second != type_->parallaxScaleMillisY()){
			res.add("parallax_scale_x", parallax_scale_millis_->first);
			res.add("parallax_scale_y", parallax_scale_millis_->second);
		}
	}
	   
	if(zSubOrder() != type_->zSubOrder()) {
		res.add("zsub_order", zSubOrder());
	}
	
    if(isFacingRight() != 1){
        res.add("face_right", isFacingRight());
    }
        
	if(isUpsideDown()) {
		res.add("upside_down", true);
	}

    if(time_in_frame_ != 0) {
        res.add("time_in_frame", time_in_frame_);
    }
        
	if(time_in_frame_delta_ != 1) {
		res.add("time_in_frame_delta", time_in_frame_delta_);
	}

	if(has_feet_ != type_->hasFeet()) {
		res.add("has_feet", has_feet_);
	}

	if(group() >= 0) {
		res.add("group", group());
	}

	for(int n = 0; n != event_handlers_.size(); ++n) {
		if(!event_handlers_[n]) {
			continue;
		}

		res.add("on_" + get_object_event_str(n), event_handlers_[n]->str());
	}

	if(!vars_->isEqualTo(type_->variables())) {
		res.add("vars", vars_->write());
	}

	if(tags_->values() != type_->tags()) {
		res.add("tags", tags_->write());
	}

	std::map<variant, variant> property_map;
	for(std::map<std::string, CustomObjectType::PropertyEntry>::const_iterator i = type_->properties().begin(); i != type_->properties().end(); ++i) {
		if(i->second.storage_slot == -1 || static_cast<unsigned>(i->second.storage_slot) >= property_data_.size() || i->second.persistent == false || i->second.const_value || property_data_[i->second.storage_slot] == i->second.default_value) {
			continue;
		}

		if(!created_ && i->second.init && Level::getCurrentPtr() && Level::current().in_editor() && !i->second.has_editor_info) {
			//In the editor try not to write out properties with an
			//initializer, so they'll get inited when the level is
			//actually started.
			continue;
		}

		property_map[variant(i->first)] = property_data_[i->second.storage_slot];
	}

	if(property_map.empty() == false) {
		res.add("property_data", variant(&property_map));
	}

	if(custom_type_.is_map()) {
		res.add("custom_type", custom_type_);
	}

	if(text_) {
		variant_builder node;
		node.add("text", text_->text);
		if(text_->font) {
			node.add("font", text_->font->id());
		}

		node.add("size", text_->size);
		node.add("align", text_->align);

		res.add("text", node.build());
	}

	if(draw_area_) {
		res.add("draw_area", draw_area_->write());
	}

	if(draw_scale_) {
		res.add("draw_scale", int(draw_scale_->value()));
	}

	if(activation_area_) {
		res.add("activation_area", activation_area_->write());
	}

	if(clip_area_) {
		res.add("clip_area", clip_area_->write());
	}

	if(!particle_systems_.empty()) {
		std::string systems;
		for(auto& i : particle_systems_) {
			if(i.second->shouldSave() == false) {
				continue;
			}

			if(!systems.empty()) {
				systems += ",";
			}

			systems += i.first;
		}

		if(!systems.empty()) {
			res.add("particles", systems);
		}
	}

	for(const LightPtr& p : lights_) {
		res.add("lights", p->write());
	}

	if(parent_.get() != nullptr) {
		std::string str;
		variant(parent_.get()).serializeToString(str);
		res.add("parent", str);
        
        res.add("relative_x", relative_x_);
        res.add("relative_y", relative_y_);
	}

	if(parent_pivot_.empty() == false) {
		res.add("pivot", parent_pivot_);
	}

	if(min_difficulty_ != -1) {
		std::string s = difficulty::to_string(min_difficulty_);
		if(s.empty()) {
			res.add("min_difficulty", min_difficulty_);
		} else {
			res.add("min_difficulty", s);
		}
	}

	if(max_difficulty_ != -1) {
		std::string s = difficulty::to_string(max_difficulty_);
		if(s.empty()) {
			res.add("max_difficulty", max_difficulty_);
		} else {
			res.add("max_difficulty", s);
		}
	}

	if(platform_offsets_.empty() == false) {
		res.add("platform_offsets", vector_to_variant(platform_offsets_));
	}

	if(use_absolute_screen_coordinates_) {
		res.add("use_absolute_screen_coordinates", use_absolute_screen_coordinates_);
	}

	return res.build();
}

void CustomObject::drawLater(int xx, int yy) const
{
	// custom object evil hackery part one.
	// Called nearer the end of rendering the scene, the
	// idea is to draw widgets with z-orders over the
	// threshold now rather than during the normal draw
	// processing.
	int offs_x = 0;
	int offs_y = 0;
	if(use_absolute_screen_coordinates_) {
		offs_x = xx + Level::current().absolute_object_adjust_x();
		offs_y = yy + Level::current().absolute_object_adjust_y();
	}
	KRE::Canvas::CameraScope cam_scope(graphics::GameScreen::get().getCurrentCamera());
	KRE::ModelManager2D model_matrix(x()+offs_x, y()+offs_y);
	for(const gui::WidgetPtr& w : widgets_) {
		if(w->zorder() >= widget_zorder_draw_later_threshold) {
			w->draw(0, 0, getRotateZ().as_float32(), draw_scale_ ? draw_scale_->as_float32() : 1);
		}
	}
}

namespace {

bool g_draw_zorder_manager_active = false;

std::unique_ptr<KRE::ClipScope::Manager> g_clip_stencil_scope;
std::unique_ptr<rect> g_clip_stencil_rect;

//This struct contains a batch of objects that are to be
//drawn together in a single call.
struct BatchDrawInfo {
	int xx, yy;
	std::vector<const CustomObject*> objects;
};

//Objects we have ready bo batch draw by batch ID
//this will be flushed when the CustomObjectDrawZOrderManager
//is destroyed.
std::map<std::string, BatchDrawInfo > g_batch_draw_objects;
}

CustomObjectDrawZOrderManager::CustomObjectDrawZOrderManager() : disabled_(g_draw_zorder_manager_active)
{
	g_draw_zorder_manager_active = true;
}

CustomObjectDrawZOrderManager::~CustomObjectDrawZOrderManager()
{
	if(disabled_) {
		return;
	}

	for(const auto& p : g_batch_draw_objects) {
		p.second.objects.front()->draw(p.second.xx, p.second.yy);
	}

	g_batch_draw_objects.clear();

	g_draw_zorder_manager_active = false;
	g_clip_stencil_scope.reset();
	g_clip_stencil_rect.reset();
}

extern int g_camera_extend_x, g_camera_extend_y;

void CustomObject::draw(int xx, int yy) const
{
	for(auto b : blur_objects_) {
		const_cast<BlurObject*>(b.get())->draw(xx, yy);
	}

	if(frame_ == nullptr) {
		return;
	}

	const BatchDrawInfo* batch = nullptr;

	const bool batch_draw = (type_->drawBatchID().empty() == false);

	if(batch_draw && g_draw_zorder_manager_active) {
		BatchDrawInfo& b = g_batch_draw_objects[type_->drawBatchID()];
		if(b.objects.empty() || b.objects.front() != this) {
			b.xx = xx;
			b.yy = yy;
			b.objects.emplace_back(this);
			return;
		}

		batch = &b;
	} else if(batch_draw) {

		//just a single object but it is designed to use batch drawing,
		//so make it use just a single-object batch.
		static BatchDrawInfo singleton_batch;
		singleton_batch.xx = xx;
		singleton_batch.yy = yy;
		singleton_batch.objects.push_back(this);

		batch = &singleton_batch;
	}

	auto wnd = KRE::WindowManager::getMainWindow();

	std::unique_ptr<KRE::ModelManager2D> model_scope;
	if(use_absolute_screen_coordinates_) {
		model_scope = std::unique_ptr<KRE::ModelManager2D>(new KRE::ModelManager2D(xx + g_camera_extend_x + Level::current().absolute_object_adjust_x(), yy + g_camera_extend_y + Level::current().absolute_object_adjust_y()));
	}

	for(const EntityPtr& attached : attachedObjects()) {
		if(attached->zorder() < zorder()) {
			attached->draw(xx, yy);
		}
	}

	CustomObjectDrawZOrderManager draw_manager;

	KRE::StencilScopePtr stencil_scope;

	if(!clip_area_) {
		g_clip_stencil_scope.reset();
		g_clip_stencil_rect.reset();
	}

	if(clip_area_) {
		rect area;
		if(clip_area_absolute_) {
			area = *clip_area_;
		} else {
			area = *clip_area_ + point(x(), y());
		}

		if(!g_clip_stencil_rect || *g_clip_stencil_rect != area) {
			g_clip_stencil_scope.reset(new KRE::ClipScope::Manager(area));
			g_clip_stencil_rect.reset(new rect(area));
		}
	} else if(type_->isShadow()) {
		stencil_scope = KRE::StencilScope::create(KRE::StencilSettings(true, 
			KRE::StencilFace::FRONT_AND_BACK, 
			KRE::StencilFunc::EQUAL, 
			0xff,
			0x02,
			0x00,
			KRE::StencilOperation::KEEP,
			KRE::StencilOperation::KEEP,
			KRE::StencilOperation::KEEP));
	}

	for(auto& eff : effects_shaders_) {
		if(eff->zorder() < 0 && eff->isEnabled()) {
			eff->draw(wnd);
		}
	}

	if(driver_) {
		driver_->draw(xx, yy);
	}

	std::unique_ptr<KRE::ColorScope> color_scope;
	if(draw_color_) {
		color_scope.reset(new KRE::ColorScope(draw_color_->toColor()));
	}

	int draw_x = x()/* - xx*/;
	int draw_y = y()/* - yy*/;

	if(g_draw_objects_on_even_pixel_boundaries) {
		draw_x -= draw_x%2;
		draw_y -= draw_y%2;
	}

	if(shader_) {
		shader_->setCycle(cycle_);
	}

	if(type_->isHiddenInGame() && !Level::current().in_editor()) {
		//pass
	} else if(batch != nullptr) {
		using namespace KRE;

		std::vector<Frame::BatchDrawItem> items;
		for(auto p : batch->objects) {
			Frame::BatchDrawItem item = { p->frame_.get(), p->x(), p->y(), p->isFacingRight(), p->isUpsideDown(), p->time_in_frame_, p->getRotateZ().as_float32(), p->draw_scale_ ? p->draw_scale_->as_float() : 1.0f };
			items.emplace_back(item);
		}

		//If the shader has any attributes it wants set, we query those attributes for each object
		//and set the attributes for every vertex that is part of that object.
		//
		//TODO: right now we only support float attributes. Add support for other kinds of attributes!
		std::vector<std::vector<float> > attributes;
		if(shader_ && shader_->getObjectPropertyAttributes().empty() == false) {
			shader_->getShader()->makeActive();
			attributes.resize(shader_->getObjectPropertyAttributes().size());
			auto attr_itor = attributes.begin();
			for(const auto& attr : shader_->getObjectPropertyAttributes()) {

				for(auto p : batch->objects) {
					const float f = p->queryValueBySlot(attr.slot).as_float();

					if(attr_itor->empty() == false) {
						attr_itor->emplace_back(attr_itor->back());
						attr_itor->emplace_back(attr_itor->back());
					}

					for(int n = 0; n != 4; ++n) {
						attr_itor->emplace_back(f);
					}
				}

				attr.attr_target->update(&(*attr_itor)[0], sizeof(float), attr_itor->size());

				shader_->getShader()->applyAttribute(attr.attr_target);

				++attr_itor;
			}
		}

		Frame::drawBatch(shader_, &items[0], &items[0] + items.size());
	} else if(custom_draw_xy_.size() >= 7 &&
	          custom_draw_xy_.size() == custom_draw_uv_.size()) {
		frame_->drawCustom(shader_, draw_x, draw_y, &custom_draw_xy_[0], &custom_draw_uv_[0], static_cast<int>(custom_draw_xy_.size())/2, isFacingRight(), isUpsideDown(), time_in_frame_, getRotateZ().as_float32(), cycle_);
	} else if(custom_draw_.get() != nullptr) {
		frame_->drawCustom(shader_, draw_x, draw_y, *custom_draw_, draw_area_.get(), isFacingRight(), isUpsideDown(), time_in_frame_, getRotateZ().as_float32());
	} else if(draw_scale_) {
		frame_->draw(shader_, draw_x, draw_y, isFacingRight(), isUpsideDown(), time_in_frame_, getRotateZ().as_float32(), draw_scale_->as_float32());
	} else if(!draw_area_.get()) {
		frame_->draw(shader_, draw_x, draw_y, isFacingRight(), isUpsideDown(), time_in_frame_, getRotateZ().as_float32());
	} else {
		frame_->draw(shader_, draw_x, draw_y, *draw_area_, isFacingRight(), isUpsideDown(), time_in_frame_, getRotateZ().as_float32());
	}

	if(draw_color_) {
		if(!draw_color_->fits_in_color()) {
			KRE::BlendModeScope blend_scope(KRE::BlendModeConstants::BM_SRC_ALPHA, KRE::BlendModeConstants::BM_ONE);
			KRE::ColorTransform transform = *draw_color_;
			while(!transform.fits_in_color()) {
				transform = transform - transform.toColor();
				KRE::ColorScope color_scope(transform.toColor());
				frame_->draw(shader_, draw_x, draw_y, isFacingRight(), isUpsideDown(), time_in_frame_, getRotateZ().as_float32());
			}
		}
	}

	for(const EntityPtr& attached : attachedObjects()) {
		if(attached->zorder() >= zorder()) {
			attached->draw(xx, yy);
		}
	}

	for(const graphics::DrawPrimitivePtr& p : draw_primitives_) {
		p->preRender(wnd);
		wnd->render(p.get());
	}

	drawDebugRects();

	{
		KRE::Canvas::CameraScope cam_scope(graphics::GameScreen::get().getCurrentCamera());
		for(const gui::WidgetPtr& w : widgets_) {
			if(w->zorder() < widget_zorder_draw_later_threshold) {
				if(w->drawWithObjectShader()) {
					w->draw(x()&~1, y()&~1, getRotateZ().as_float32(), draw_scale_ ? draw_scale_->as_float32() : 1.0f);
				}
			}
		}
	}


	auto& gs = graphics::GameScreen::get();

	for(auto& ps : particle_systems_) {
		ps.second->draw(wnd, rect(last_draw_position().x/100, last_draw_position().y/100, gs.getVirtualWidth(), gs.getVirtualHeight()), *this);
	}

	if(text_ && text_->font && text_->alpha) {
		const int half_width = getMidpoint().x - draw_x;
		int xpos = draw_x;
		if(text_->align == 0) {
			xpos += half_width - text_->dimensions.w()/2;
		} else if(text_->align > 0) {
			xpos += half_width*2 - text_->dimensions.w();
		}
		text_->font->draw(xpos, draw_y, text_->text, text_->size, KRE::Color(255,255,255,text_->alpha));
	}
	
	for(auto& eff : effects_shaders_) {
		if(eff->zorder() >= 0 && eff->isEnabled()) {
			eff->draw(wnd);
		}
	}

	if(particles_ != nullptr) {
		glm::vec3 translation = KRE::get_global_model_matrix()[3];
		KRE::Particles::ParticleSystem::TranslationScope translation_scope(translation - particles_->get_last_translation());
		particles_->get_last_translation() = translation;

		KRE::ModelManager2D mm(x(), y());
		particles_->draw(wnd);
	}

	if(Level::current().debug_properties().empty() == false) {
		std::vector<KRE::TexturePtr> left, right;
		int max_property_width = 0;
		for(const std::string& s : Level::current().debug_properties()) {
			try {
				const assert_recover_scope scope;
				variant result = game_logic::Formula(variant(s)).execute(*this);
				const std::string result_str = result.write_json();
				auto key_texture = KRE::Font::getInstance()->renderText(s, KRE::Color::colorWhite(), 16);
				auto value_texture = KRE::Font::getInstance()->renderText(result_str, KRE::Color::colorWhite(), 16);
				left.emplace_back(key_texture);
				right.emplace_back(value_texture);
	
				if(key_texture->width() > max_property_width) {
					max_property_width = key_texture->width();
				}
			} catch(validation_failure_exception&) {
			}
		}

		int pos = y();
		for(int n = 0; n != left.size(); ++n) {
			const int xpos = getMidpoint().x + 10;
			KRE::Blittable blit;
			blit.setTexture(left[n]);
			blit.setPosition(xpos, pos);
			blit.preRender(wnd);
			wnd->render(&blit);
			
			blit.setTexture(right[n]);
			blit.setPosition(xpos + max_property_width + 10, pos);
			blit.preRender(wnd);
			wnd->render(&blit);
			pos += std::max(left[n]->height(), right[n]->height());
		}
	}

	if(platform_area_ && (preferences::show_debug_hitboxes() || (!platform_offsets_.empty() && Level::current().in_editor()))) {
		std::vector<glm::u16vec2> v;
		const rect& r = platformRect();
		for(int x = 0; x < r.w(); x += 2) {
			v.emplace_back(static_cast<float>(r.x() + x), static_cast<float>(platformRectAt(r.x() + x).y()));
		}

		if(!v.empty()) {
			RectRenderable rr(false);
			auto shd = rr.getShader();
			int ps_loc = -1;
			if(shd && (ps_loc = shd->getUniform("u_point_size")) != KRE::ShaderProgram::INVALID_UNIFORM) {
				shd->setUniformValue(ps_loc, 2.0f);
			}
			rr.update(&v, KRE::Color::colorRed());
			wnd->render(&rr);
		}
	}

	if(document_) {
		KRE::ModelManager2D mm(xx + x(), yy + y());
		//KRE::Canvas::CameraScope cam_scope(graphics::GameScreen::get().getCurrentCamera());
		document_->draw(wnd);
	}

	if(!widgets_.empty()) {
		KRE::ModelManager2D mm(xx, yy);
		KRE::Canvas::CameraScope cam_scope(graphics::GameScreen::get().getCurrentCamera());
		for(const gui::WidgetPtr& w : widgets_) {
			if(w->zorder() < widget_zorder_draw_later_threshold) {
				if(w->drawWithObjectShader() == false) {
					w->draw(x(), y(), getRotateZ().as_float32(), draw_scale_ ? draw_scale_->as_float32() : 0);
				}
			}
		}
	}
}

void CustomObject::drawGroup() const
{
	auto wnd = KRE::WindowManager::getMainWindow();
	if(label().empty() == false && label()[0] != '_') {
		auto label_tex = KRE::Font::getInstance()->renderText(label(), KRE::Color::colorYellow(), 32);
		KRE::Blittable blit;
		blit.setTexture(label_tex);
		blit.setPosition(x(), y() + 26);
		blit.preRender(wnd);
		wnd->render(&blit);
	}

	if(group() >= 0) {
		auto group_tex = KRE::Font::getInstance()->renderText(formatter() << group(), KRE::Color::colorYellow(), 24);
		KRE::Blittable blit;
		blit.setTexture(group_tex);
		blit.setPosition(x(), y());
		blit.preRender(wnd);
		wnd->render(&blit);
	}
}

void CustomObject::construct()
{
	initDeferredProperties();
	handleEvent(OBJECT_EVENT_CONSTRUCT);
}

bool CustomObject::createObject()
{
	if(!created_) {
		validate_properties();
		created_ = true;
		handleEvent(OBJECT_EVENT_CREATE);
		ASSERT_LOG(properties_requiring_dynamic_initialization_.empty(), "Object property " << getDebugDescription() << "." << type_->getSlotProperties()[properties_requiring_dynamic_initialization_.front()].id << " not initialized at end of on_create.");
		return true;
	}

	return false;
}

void CustomObject::checkInitialized()
{
	ASSERT_LOG(properties_requiring_dynamic_initialization_.empty(), "Object property " << getDebugDescription() << "." << type_->getSlotProperties()[properties_requiring_dynamic_initialization_.front()].id << " not initialized");

	validate_properties();
}

void CustomObject::process(Level& lvl)
{
	if(paused_) {
		return;
	}

#if defined(USE_BOX2D)
	box2d::world_ptr world = box2d::world::our_world_ptr();
	if(body_) {
		const b2Vec2 v = body_->get_body_ptr()->GetPosition();
		const double a = body_->get_body_ptr()->GetAngle();
		setRotateZ(decimal(a * 180.0 / M_PI));
		setX(int(v.x * world->scale() - (solidRect().w() ? (solidRect().w()/2) : getCurrentFrame().width()/2)));
		setY(int(v.y * world->scale() - (solidRect().h() ? (solidRect().h()/2) : getCurrentFrame().height()/2)));
		//setY(graphics::screen_height() - v.y * world->scale() - getCurrentFrame().height());
		/*setX((v.x + world->x1()) * graphics::screen_width() / (world->x2() - world->x1()));
		if(world->y2() < 0) {
			setY(graphics::screen_height() - (v.y + world->y1()) * graphics::screen_height() / -(world->y2() + world->y1()));
		} else {
			setY((v.y + world->y1()) * graphics::screen_height() / (world->y2() - world->y1()));
		}*/
	}
#endif

	if(type_->useImageForCollisions()) {
		//anything that uses their image for collisions is a static,
		//un-moving object that will stay immobile.
		return;
	}

	if(lvl.in_editor()) {
		if(!type_->isStaticObject() && entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE)) {
			//The object collides illegally, but we're in the editor. Freeze
			//the object by returning, since we can't process it.
			return;
		}

		if(Level::current().is_editor_dragging_objects() && std::count(Level::current().editor_selection().begin(), Level::current().editor_selection().end(), EntityPtr(this))) {
			//this object is being dragged and so gets frozen.
			return;
		}
	}

	CollisionInfo debug_collide_info;
	ASSERT_LOG(type_->isStaticObject() || lvl.in_editor() || !entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE, &debug_collide_info), "ENTITY " << getDebugDescription() << " COLLIDES WITH " << (debug_collide_info.collide_with ? debug_collide_info.collide_with->getDebugDescription() : "THE LEVEL") << " AT START OF PROCESS, WITH ITS SOLID RECT BEING [x,y,x2,y2]: " << solidRect());

	if(parent_.get() != nullptr) {
		const point pos = parent_position();
		const bool parent_facing = parent_->isFacingRight();
        const int parent_facing_sign = parent_->isFacingRight() ? 1 : -1;

		if(parent_prev_x_ != std::numeric_limits<int>::min()) {
            setMidX(pos.x + (relative_x_ * parent_facing_sign));
            setMidY(pos.y + relative_y_);
   		}

		parent_prev_x_ = pos.x;
		parent_prev_y_ = pos.y;
		parent_prev_facing_ = parent_facing;
	}

	if(last_cycle_active_ < lvl.cycle() - 5) {
		handleEvent(OBJECT_EVENT_BECOME_ACTIVE);
	}

	last_cycle_active_ = lvl.cycle();

	Entity::process(lvl);

	//the object should never be colliding with the level at the start of processing.
//	assert(!entity_collides_with_level(lvl, *this, MOVE_DIRECTION::NONE));
//	assert(!entity_collides(lvl, *this, MOVE_DIRECTION::NONE));

	//this is a flag which tracks whether we've fired a collide_feet
	//event. If we don't fire a collide_feet event through normal collision
	//detection, but we change the object we're standing on, we should
	//still fire a collide_feet event.
	bool fired_collide_feet = false;

	CollisionInfo stand_info;
	const bool started_standing = isStanding(lvl, &stand_info) != STANDING_STATUS::NOT_STANDING;
	if(!started_standing && standing_on_) {
		//if we were standing on something the previous frame, but aren't
		//standing any longer, we use the value of what we were previously
		//standing on.
		stand_info.traction = standing_on_->getSurfaceTraction();
		stand_info.friction = standing_on_->getSurfaceFriction();
	} else if(!standing_on_ && started_standing && stand_info.collide_with && velocity_y_ >= 0 && !fired_collide_feet) {
		//We weren't standing on something last frame, but now we suddenly
		//are. We should fire a collide_feet event as a result.

		game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
		variant v(callable);
	
		if(stand_info.area_id != nullptr) {
			callable->add("area", variant(*stand_info.area_id));
		}

		if(stand_info.collide_with) {
			callable->add("collide_with", variant(stand_info.collide_with.get()));
			if(stand_info.collide_with_area_id) {
				callable->add("collide_with_area", variant(*stand_info.collide_with_area_id));
			}

		}

		handleEvent(OBJECT_EVENT_COLLIDE_FEET, callable);
		fired_collide_feet = true;
	}

	if(y() > lvl.boundaries().y2() || y() < lvl.boundaries().y() || x() > lvl.boundaries().x2() || x() < lvl.boundaries().x()) {
		handleEvent(OBJECT_EVENT_OUTSIDE_LEVEL);
	}
	
	previous_y_ = y();
	if(started_standing && velocity_y_ > 0) {
		velocity_y_ = decimal(0);
	}

	const int start_x = x();
	const int start_y = y();
	const decimal start_rotate = getRotateZ();
	++cycle_;

	if(invincible_) {
		--invincible_;
	}

	if(!loaded_) {
		handleEvent(OBJECT_EVENT_LOAD);
		loaded_ = true;
	}

	createObject();

	if(cycle_ == 1) {
		//these events are for backwards compatibility. It's not recommended
		//to use them for new objects.
		handleEvent("first_cycle");
		handleEvent(OBJECT_EVENT_DONE_CREATE);
	}

	bool debug_commands = false;
	std::vector<variant> scheduled_commands = popScheduledCommands(&debug_commands);
	for(const variant& cmd : scheduled_commands) {
		formula_profiler::Instrument anim_instrument("SCHEDULED_CMD");

		if(debug_commands) {
			try {
				debug_console::ExecuteDebugConsoleScope debug_scope;
				const assert_recover_scope scope;
				executeCommand(cmd);
			} catch(validation_failure_exception&) {
			}
		} else {
			executeCommand(cmd);
		}
	}

	std::vector<std::pair<variant,variant> > follow_ons;

	if(!animated_movement_.empty()) {
		formula_profiler::Instrument anim_instrument("ANIMATED_MOVEMENT");
		std::vector<std::shared_ptr<AnimatedMovement> > movement = animated_movement_, removal;
		for(int i = 0; i != movement.size(); ++i) {
			auto& move = movement[i];

			if(move->pos >= move->getAnimationFrames()) {
				if(move->on_complete.is_null() == false) {
					formula_profiler::Instrument anim_instrument("COMPLETE_ANIMATION");
					executeCommandOrFn(move->on_complete);
				}

				follow_ons.insert(follow_ons.end(), move->follow_on.begin(), move->follow_on.end());

				removal.emplace_back(move);
			} else {
				ASSERT_LOG(move->animation_values.size()%move->animation_slots.size() == 0, "Bad animation sizes");
				variant* v = &move->animation_values[0] + move->pos*move->animation_slots.size();
	
				for(int n = 0; n != move->animation_slots.size(); ++n) {
					mutateValueBySlot(move->animation_slots[n], v[n]);
				}

				if(move->on_begin.is_null() == false) {
					executeCommandOrFn(move->on_begin);
					move->on_begin = variant();
				}

				if(move->on_process.is_null() == false) {
					executeCommandOrFn(move->on_process);
				}
	
				move->pos++;
			}
		}

		for(auto& move : animated_movement_) {
			if(std::count(removal.begin(), removal.end(), move)) {
				move.reset();
			}
		}

		animated_movement_.erase(std::remove(animated_movement_.begin(), animated_movement_.end(), std::shared_ptr<AnimatedMovement>()), animated_movement_.end());
	}

	for(const auto& p : follow_ons) {
		addAnimatedMovement(p.first, p.second);
	}

	if(position_schedule_.get() != nullptr) {
		const int pos = (cycle_ - position_schedule_->base_cycle)/position_schedule_->speed;

		if(position_schedule_->expires &&
		   size_t(pos) >= position_schedule_->x_pos.size() &&
		   size_t(pos) >= position_schedule_->y_pos.size() &&
		   size_t(pos) >= position_schedule_->rotation.size()) {
			handleEvent(OBJECT_EVENT_SCHEDULE_FINISHED);
			position_schedule_.reset();
		} else {

			const int next_fraction = (cycle_ - position_schedule_->base_cycle)%position_schedule_->speed;
			const int this_fraction = position_schedule_->speed - next_fraction;

			int xpos = std::numeric_limits<int>::min(), ypos = std::numeric_limits<int>::min();
			if(position_schedule_->x_pos.empty() == false) {
				xpos = position_schedule_->x_pos[pos%position_schedule_->x_pos.size()];
				if(next_fraction && pos+1 != position_schedule_->x_pos.size()) {
					xpos = (xpos*this_fraction + next_fraction*position_schedule_->x_pos[(pos+1)%position_schedule_->x_pos.size()])/position_schedule_->speed;
				}
			}

			if(position_schedule_->y_pos.empty() == false) {
				ypos = position_schedule_->y_pos[pos%position_schedule_->y_pos.size()];
				if(next_fraction && pos+1 != position_schedule_->y_pos.size()) {
					ypos = (ypos*this_fraction + next_fraction*position_schedule_->y_pos[(pos+1)%position_schedule_->y_pos.size()])/position_schedule_->speed;
				}
			}

			if(xpos != std::numeric_limits<int>::min() && ypos != std::numeric_limits<int>::min()) {
				setPos(xpos, ypos);
			} else if(xpos != std::numeric_limits<int>::min()) {
				setX(xpos);
			} else if(ypos != std::numeric_limits<int>::min()) {
				setY(ypos);
			}

			if(position_schedule_->rotation.empty() == false) {
				setRotateZ(position_schedule_->rotation[pos%position_schedule_->rotation.size()]);
				while(rotate_z_ >= 360) {
					rotate_z_ -= 360;
				}

				if(next_fraction) {
					setRotateZ(decimal((getRotateZ()*this_fraction + next_fraction*position_schedule_->rotation[(pos+1)%position_schedule_->rotation.size()])/position_schedule_->speed));
				}
			}
		}
	}

	if(stand_info.damage) {
		game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
		callable->add("surface_damage", variant(stand_info.damage));
		variant v(callable);
		handleEvent(OBJECT_EVENT_COLLIDE_DAMAGE, callable);

		//DEPRECATED -- can we remove surface_damage and just have
		//collide_damage?
		handleEvent(OBJECT_EVENT_SURFACE_DAMAGE, callable);
	}

	if(cycle_ != 1) {
		//don't advance to the next frame in the object's very first cycle.
		time_in_frame_ += time_in_frame_delta_;
	}
	if(time_in_frame_ < 0) {
		time_in_frame_ = 0;
	}

	if(time_in_frame_ > frame_->duration()) {
		time_in_frame_ = frame_->duration();
	}

	if(time_in_frame_ == frame_->duration()) {
		std::vector<variant> cmd = popEndAnimCommands();
		if(cmd.empty() == false) {
			formula_profiler::Instrument anim_instrument("END_ANIM_CMD");
			for(const variant& c : cmd) {
				executeCommand(c);
			}
		} else {
			handleEvent(frame_->endEventId());
			handleEvent(OBJECT_EVENT_END_ANIM);
			if(next_animation_formula_) {
				variant var = next_animation_formula_->execute(*this);
				setFrame(var.as_string());
			}
		}
	}

	const std::string* event = frame_->getEvent(time_in_frame_);
	if(event) {
		handleEvent(*event);
	}
    

	rect water_bounds;
	const bool is_underwater = solid() 
		? lvl.isUnderwater(solidRect(), &water_bounds) 
		: lvl.isUnderwater(rect(x(), y(), getCurrentFrame().width(), getCurrentFrame().height()), &water_bounds);
    
	if( is_underwater && !was_underwater_){
		//event on_enter_water
		handleEvent(OBJECT_EVENT_ENTER_WATER);
		was_underwater_ = true;
	}else if ( !is_underwater && was_underwater_ ){
		//event on_exit_water
		handleEvent(OBJECT_EVENT_EXIT_WATER);
		was_underwater_ = false;
	}

	previous_water_bounds_ = water_bounds;
	
	if(type_->isStaticObject()) {
		staticProcess(lvl);
		return;
	}

	const int traction_from_surface = (stand_info.traction*type_->traction())/1000;
	velocity_x_ += (accel_x_ * (stand_info.traction ? traction_from_surface : (is_underwater?type_->getTractionInWater() : type_->getTractionInAir())) * (isFacingRight() ? 1 : -1))/1000;
	if((!standing_on_ && !started_standing) || accel_y_ < 0) {
		//do not accelerate downwards if standing on something.
		velocity_y_ += accel_y_ * (gravity_shift_ + (is_underwater ? type_->getTractionInWater() : 1000))/1000;
	}

	if(type_->friction()) {

		const int air_resistance = is_underwater ? lvl.water_resistance() : lvl.air_resistance();

		const int friction = ((stand_info.friction + air_resistance)*type_->friction())/1000;
		int vertical_resistance = (air_resistance*type_->friction())/1000;
		if(velocity_y_ > 0 && !is_underwater) {
			//vertical air resistance is reduced when moving downwards.
			//This works well for most objects, though consider making it
			//configurable in future.
			vertical_resistance /= 2;
		}

		velocity_x_ = (velocity_x_*(1000 - friction))/1000;
		velocity_y_ = (velocity_y_*(1000 - vertical_resistance))/1000;
	}

	if(type_->isAffectedByCurrents()) {
		int velocity_x = velocity_x_.as_int();
		int velocity_y = velocity_y_.as_int();
		lvl.getCurrent(*this, &velocity_x, &velocity_y);
		const int diff_x = velocity_x - velocity_x_.as_int();
		const int diff_y = velocity_y - velocity_y_.as_int();
		velocity_x_ += diff_x;
		velocity_y_ += diff_y;
	}

	bool collide = false;

	//calculate velocity which takes into account velocity of the object we're standing on.
	int effective_velocity_x = velocity_x_.as_int();
	int effective_velocity_y = velocity_y_.as_int();

	if(effective_velocity_y > 0 && (standing_on_ || started_standing)) {
		effective_velocity_y = 0;
	}

	int platform_motion_x_movement = 0;
	if(standing_on_) {

		platform_motion_x_movement = standing_on_->getPlatformMotionX() + standing_on_->mapPlatformPos(getFeetX())*100;
		effective_velocity_x += (standing_on_->getFeetX() - standing_on_prev_x_)*100 + platform_motion_x_movement;
		effective_velocity_y += (standing_on_->getFeetY() - standing_on_prev_y_)*100;
	}

	if(stand_info.collide_with != standing_on_ && stand_info.adjust_y) {
		//if we're landing on a new platform, we might have to adjust our
		//y position to suit its last movement and put us on top of
		//the platform.

		effective_velocity_y = stand_info.adjust_y*100;
	}

	if(effective_velocity_x || effective_velocity_y) {
		if(!solid() && !type_->hasObjectLevelCollisions()) {
			moveCentipixels(effective_velocity_x, effective_velocity_y);
			effective_velocity_x = 0;
			effective_velocity_y = 0;
		} else if(!hasFeet() && solid()) {
			moveCentipixels(effective_velocity_x, effective_velocity_y);
			if(is_flightpath_clear(lvl, *this, solidRect())) {
				effective_velocity_x = 0;
				effective_velocity_y = 0;
			} else {
				//we can't guarantee smooth movement to this location, so
				//roll the move back and we'll do a pixel-by-pixel move
				//until we collide.
				moveCentipixels(-effective_velocity_x, -effective_velocity_y);
			}
		}
	}


	CollisionInfo collide_info;
	CollisionInfo jump_on_info;

	bool is_stuck = false;

	collide = false;
	int move_left;
	for(move_left = std::abs(effective_velocity_y); move_left > 0 && !collide && !type_->hasIgnoreCollide(); move_left -= 100) {
		const int dir = effective_velocity_y > 0 ? 1 : -1;
		int damage = 0;

		const int move_amount = std::min(std::max(move_left, 0), 100);
		
		const bool moved = moveCentipixels(0, move_amount*dir);
		if(!moved) {
			//we didn't actually move any pixels, so just abort.
			break;
		}

		if(type_->hasObjectLevelCollisions() && non_solid_entity_collides_with_level(lvl, *this)) {
			handleEvent(OBJECT_EVENT_COLLIDE_LEVEL);
		}

		if(effective_velocity_y > 0) {
			if(entity_collides(lvl, *this, MOVE_DIRECTION::DOWN, &collide_info)) {
				//our 'legs' but not our feet collide with the level. Try to
				//move one pixel to the left or right and see if either
				//direction makes us no longer colliding.
				setX(x() + 1);
				if(entity_collides(lvl, *this, MOVE_DIRECTION::DOWN) || entity_collides(lvl, *this, MOVE_DIRECTION::RIGHT)) {
					setX(x() - 2);
					if(entity_collides(lvl, *this, MOVE_DIRECTION::DOWN) || entity_collides(lvl, *this, MOVE_DIRECTION::LEFT)) {
						//moving in either direction fails to resolve the collision.
						//This effectively means the object is 'stuck' in a small
						//pit.
						setX(x() + 1);
						moveCentipixels(0, -move_amount*dir);
						collide = true;
						is_stuck = true;
						break;
					}
				}
				

			}
		} else {
			//effective_velocity_y < 0 -- going up
			if(entity_collides(lvl, *this, MOVE_DIRECTION::UP, &collide_info)) {
				collide = true;
				moveCentipixels(0, -move_amount*dir);
				break;
			}
		}

		if(!collide && !type_->hasIgnoreCollide() && effective_velocity_y > 0 && isStanding(lvl, &jump_on_info) != STANDING_STATUS::NOT_STANDING) {
			if(!jump_on_info.collide_with || jump_on_info.collide_with != standing_on_) {
				collide = true;
				collide_info = jump_on_info;
			}

			break;
		}

		if(collide) {
			break;
		}
	}

	//this variable handled whether we already landed in our vertical movement
	//in which case horizontal movement won't consider us to land.
	bool vertical_landed = false;

	if(is_stuck) {
		handleEvent(OBJECT_EVENT_STUCK);
	}

	if(collide) {
		if(effective_velocity_y > 0) {
			vertical_landed = true;
		}

		if(!fired_collide_feet && (effective_velocity_y < 0 || !started_standing)) {

			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
			variant v(callable);
	
			if(collide_info.area_id != nullptr) {
				callable->add("area", variant(*collide_info.area_id));
			}

			if(collide_info.collide_with) {
				callable->add("collide_with", variant(collide_info.collide_with.get()));
				if(collide_info.collide_with_area_id) {
					callable->add("collide_with_area", variant(*collide_info.collide_with_area_id));
				}

			}

			handleEvent(effective_velocity_y < 0 ? OBJECT_EVENT_COLLIDE_HEAD : OBJECT_EVENT_COLLIDE_FEET, callable);
			fired_collide_feet = true;
		}

		if(collide_info.damage || jump_on_info.damage) {
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
			callable->add("surface_damage", variant(std::max(collide_info.damage, jump_on_info.damage)));
			variant v(callable);
			handleEvent(OBJECT_EVENT_COLLIDE_DAMAGE, callable);
		}
	}

	//If the object started out standing on a platform, keep it doing so.
	if(standing_on_ && !fall_through_platforms_ && velocity_y_ >= 0) {
		const int left_foot = getFeetX() - type_->getFeetWidth();
		const int right_foot = getFeetX() + type_->getFeetWidth();

		int target_y = std::numeric_limits<int>::max();
		rect area = standing_on_->platformRect();
		if(left_foot >= area.x() && left_foot < area.x() + area.w()) {
			rect area = standing_on_->platformRectAt(left_foot);
			target_y = area.y();
		}

		if(right_foot >= area.x() && right_foot < area.x() + area.w()) {
			rect area = standing_on_->platformRectAt(right_foot);
			if(area.y() < target_y) {
				target_y = area.y();
			}
		}

		if(target_y != std::numeric_limits<int>::max()) {
			const int delta = target_y - getFeetY();
			const int dir = delta > 0 ? 1 : -1;
			int nmoves = 0;
			for(int n = 0; n != delta; n += dir) {
				setY(y()+dir);
				++nmoves;
				if(entity_collides(lvl, *this, dir < 0 ? MOVE_DIRECTION::UP : MOVE_DIRECTION::DOWN)) {
					setY(y()-dir);
					break;
				}
			}
		}
	}

	collide = false;

	bool horizontal_landed = false;

	//we go through up to two passes of moving an object horizontally. On the
	//first pass, we are 'optimistic' and move the object along, assuming there
	//will be no collisions. Then at the end of the pass we see if the object is
	//colliding. If it's not, all is good, but if it is, we'll re-do the movement,
	//detecting for collisions at each step, until we work out where exactly
	//the collision occurs, and stop the object there.
	for(int detect_collisions = 0; detect_collisions <= 1 && effective_velocity_x; ++detect_collisions) {
		const int backup_centi_x = centiX();
		const int backup_centi_y = centiY();


		for(move_left = std::abs(effective_velocity_x); move_left > 0 && !collide && !type_->hasIgnoreCollide(); move_left -= 100) {
			if(type_->hasObjectLevelCollisions() && non_solid_entity_collides_with_level(lvl, *this)) {
				handleEvent(OBJECT_EVENT_COLLIDE_LEVEL);
			}

			const STANDING_STATUS previous_standing = isStanding(lvl);

			const int dir = effective_velocity_x > 0 ? 1 : -1;
			const int original_centi_y = centiY();

			const int move_amount = std::min(std::max(move_left, 0), 100);
		
			const bool moved = moveCentipixels(move_amount*dir, 0);
			if(!moved) {
				//we didn't actually move any pixels, so just abort.
				break;
			}

			const int left_foot = getFeetX() - type_->getFeetWidth();
			const int right_foot = getFeetX() + type_->getFeetWidth();
			bool place_on_object = false;
			if(standing_on_ && !fall_through_platforms_ && velocity_y_ >= 0) {
				rect area = standing_on_->platformRect();
				if((left_foot >= area.x() && left_foot < area.x() + area.w()) ||
					(right_foot >= area.x() && right_foot < area.x() + area.w())) {
					place_on_object = true;
				}
			}

			//if we go up or down a slope, and we began the frame standing,
			//move the character up or down as appropriate to try to keep
			//them standing.

			const STANDING_STATUS standing = isStanding(lvl);
			if(place_on_object) {
				int target_y = std::numeric_limits<int>::max();
				rect area = standing_on_->platformRect();
				if(left_foot >= area.x() && left_foot < area.x() + area.w()) {
					const rect area = standing_on_->platformRectAt(left_foot);
					target_y = area.y();
				}

				if(right_foot >= area.x() && right_foot < area.x() + area.w()) {
					const rect area = standing_on_->platformRectAt(right_foot);
					if(area.y() < target_y) {
						target_y = area.y();
					}
				}

				const int delta = target_y - getFeetY();
				const int dir = delta > 0 ? 1 : -1;
				for(int n = 0; n != delta; n += dir) {
					setY(y()+dir);
					if(detect_collisions && entity_collides(lvl, *this, dir < 0 ? MOVE_DIRECTION::UP : MOVE_DIRECTION::DOWN)) {
						setY(y()-dir);
						break;
					}
				}
			} else if(previous_standing != STANDING_STATUS::NOT_STANDING && standing < previous_standing) {

				//we were standing, but we're not now. We want to look for
				//slopes that will enable us to still be standing. We see
				//if the object is trying to walk down stairs, in which case
				//we look downwards first, otherwise we look upwards first,
				//then downwards.
				int dir = walkUpOrDownStairs() > 0 ? 1 : -1;

				for(int tries = 0; tries != 2; ++tries) {
					bool resolved = false;
					const int SearchRange = 2;
					for(int n = 0; n != SearchRange; ++n) {
						setY(y()+dir);
						if(detect_collisions && entity_collides(lvl, *this, dir < 0 ? MOVE_DIRECTION::UP : MOVE_DIRECTION::DOWN)) {
							break;
						}

						if(isStanding(lvl) >= previous_standing) {
							resolved = true;
							break;
						}
					}

					if(resolved) {
						break;
					}

					dir *= -1;
					setCentiY(original_centi_y);
				}
			} else if(standing != STANDING_STATUS::NOT_STANDING) {
				if(!vertical_landed && !started_standing && !standing_on_) {
					horizontal_landed = true;
				}

				CollisionInfo slope_standing_info;

				bool collide_head = false;

				//we are standing, but we need to see if we should be standing
				//on a higher point. If there are solid points immediately above
				//where we are, we adjust our feet to be on them.
				//
				//However, if there is a platform immediately above us, we only
				//adjust our feet upward if the object is trying to walk up
				//stairs, normally by the player pressing up while walking.
				int max_slope = 5;
				while(--max_slope && isStanding(lvl, &slope_standing_info) != STANDING_STATUS::NOT_STANDING) {
					if(slope_standing_info.platform && walkUpOrDownStairs() >= 0) {
						if(max_slope == 4) {
							//we always move at least one pixel up, if there is
							//solid, otherwise we'll fall through.
							setY(y()-1);
							if(detect_collisions && entity_collides(lvl, *this, MOVE_DIRECTION::UP)) {
								collide_head = true;
								break;
							}
						}
						break;
					}
	
					setY(y()-1);
					if(detect_collisions && entity_collides(lvl, *this, MOVE_DIRECTION::UP)) {
						collide_head = true;
						break;
					}
				}
	
				if(!max_slope || collide_head) {
					setCentiY(original_centi_y);
				} else {
					setY(y()+1);
				}
	
				if(walkUpOrDownStairs() > 0) {
					//if we are trying to walk down stairs and we're on a platform
					//and one pixel below is walkable, then we move down by
					//one pixel.
					isStanding(lvl, &slope_standing_info);
					if(slope_standing_info.platform) {
						setY(y()+1);
						if(isStanding(lvl) == STANDING_STATUS::NOT_STANDING || (detect_collisions && entity_collides(lvl, *this, MOVE_DIRECTION::DOWN))) {
							setY(y()-1);
						}
					}
				}
			}

			if(detect_collisions && entity_collides(lvl, *this, centiY() != original_centi_y ? MOVE_DIRECTION::NONE : (dir > 0 ? MOVE_DIRECTION::RIGHT : MOVE_DIRECTION::LEFT), &collide_info)) {
				collide = true;
			}

			if(collide) {
				//undo the move to cancel out the collision
				moveCentipixels(-dir*move_amount, 0);
				setCentiY(original_centi_y);
				break;
			}
		}

		if(!detect_collisions) {
			if(entity_collides(lvl, *this, MOVE_DIRECTION::NONE)) {
				setCentiX(backup_centi_x);
				setCentiY(backup_centi_y);
			} else {
				break;
			}
		}
	}

	if(collide || horizontal_landed) {

		game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
		variant v(callable);

		if(collide_info.area_id != nullptr) {
			callable->add("area", variant(*collide_info.area_id));
		}

		if(collide_info.collide_with) {
			callable->add("collide_with", variant(collide_info.collide_with.get()));
			if(collide_info.collide_with_area_id) {
				callable->add("collide_with_area", variant(*collide_info.collide_with_area_id));
			}
		}

		handleEvent(collide ? OBJECT_EVENT_COLLIDE_SIDE : OBJECT_EVENT_COLLIDE_FEET, callable);
		fired_collide_feet = true;
		if(collide_info.damage) {
			game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
			callable->add("surface_damage", variant(collide_info.damage));
			variant v(callable);
			handleEvent(OBJECT_EVENT_COLLIDE_DAMAGE, callable);
		}
	}

	stand_info = CollisionInfo();
	if(velocity_y_ >= 0) {
		isStanding(lvl, &stand_info);
	}

	if(stand_info.collide_with && standing_on_ != stand_info.collide_with &&
	   effective_velocity_y < stand_info.collide_with->velocityY()) {
		stand_info.collide_with = nullptr;
	}

	if(standing_on_ && standing_on_ != stand_info.collide_with) {
		//we were previously standing on an object and we're not anymore.
		//add the object we were standing on's velocity to ours
		velocity_x_ += standing_on_->getLastMoveX()*100 + platform_motion_x_movement;
		velocity_y_ += standing_on_->getLastMoveY()*100;
	}

	if(stand_info.collide_with && standing_on_ != stand_info.collide_with) {
		if(!fired_collide_feet) {
		}

		//we are standing on a new object. Adjust our velocity relative to
		//the object we're standing on
		velocity_x_ -= stand_info.collide_with->getLastMoveX()*100 + stand_info.collide_with->getPlatformMotionX();
		velocity_y_ = decimal(0);

		game_logic::MapFormulaCallable* callable(new game_logic::MapFormulaCallable(this));
		callable->add("jumped_on_by", variant(this));
		game_logic::FormulaCallablePtr callable_ptr(callable);

		stand_info.collide_with->handleEvent(OBJECT_EVENT_JUMPED_ON, callable);
	}

	standing_on_ = stand_info.collide_with;
	if(standing_on_) {
		standing_on_prev_x_ = standing_on_->getFeetX();
		standing_on_prev_y_ = standing_on_->getFeetY();
	}

	if(lvl.players().empty() == false) {
		lvl.set_touched_player(lvl.players().front());
	}

	if(fall_through_platforms_ > 0) {
		--fall_through_platforms_;
	}

#if defined(USE_BOX2D)
	if(body_) {
		for(b2ContactEdge* ce = body_->get_body_ptr()->GetContactList(); ce != nullptr; ce = ce->next) {
			b2Contact* c = ce->contact;
			// process c
			if(c->IsTouching()) {
				using namespace game_logic;
				//std::cerr << "bodies touching: 0x" << std::hex << uint32_t(body_->get_body_ptr()) << " 0x" << uint32_t(ce->other) << std::dec << std::endl;
				//b2WorldManifold wmf;
				//c->GetWorldManifold(&wmf);
				//std::cerr << "Collision points: " << wmf.points[0].x << ", " << wmf.points[0].y << "; " << wmf.points[1].x << "," << wmf.points[1].y << "; " << wmf.normal.x << "," << wmf.normal.y << std::endl;
				MapFormulaCallablePtr fc = MapFormulaCallablePtr(new MapFormulaCallable);
				fc->add("collide_with", variant((box2d::body*)ce->other->GetUserData()));
				handleEvent("b2collide", fc.get());
			}
			//c->GetManifold()->
		}
	}
#endif

	if(Level::current().cycle() > int(getMouseoverTriggerCycle())) {
		if(isMouseOverEntity() == false) {
			game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable);
			int mx, my;
			input::sdl_get_mouse_state(&mx, &my);
			callable->add("mouse_x", variant(mx));
			callable->add("mouse_y", variant(my));
			handleEvent("mouse_enter", callable.get());
			setMouseOverEntity();
			setMouseoverTriggerCycle(std::numeric_limits<int>::max());
		}
	}

	if(document_) {
		document_->process();
	}

	for(const gui::WidgetPtr& w : widgets_) {
		w->process();
	}

	if(shader_) {
		shader_->process();
	}
	for(auto& eff : effects_shaders_) {
		eff->process();
	}
	if(particles_) {
		particles_->process();
	}

	for(auto& b : blur_objects_) {
		b->process();
		if(b->expired()) {
			b.reset();
		}
	}

	blur_objects_.erase(std::remove(blur_objects_.begin(), blur_objects_.end(), ffl::IntrusivePtr<BlurObject>()), blur_objects_.end());

	staticProcess(lvl);
}

void CustomObject::staticProcess(Level& lvl)
{
	handleEvent(OBJECT_EVENT_PROCESS);
	handleEvent(frame_->processEventId());

	if(type_->timerFrequency() > 0 && (cycle_%type_->timerFrequency()) == 0) {
		static const std::string TimerStr = "timer";
		handleEvent(OBJECT_EVENT_TIMER);
	}

	for(std::map<std::string, ParticleSystemPtr>::iterator i = particle_systems_.begin(); i != particle_systems_.end(); ) {
		i->second->process(*this);
		if(i->second->isDestroyed()) {
			particle_systems_.erase(i++);
		} else {
			++i;
		}
	}

	set_driver_position();

	for(const LightPtr& p : lights_) {
		p->process();
	}

	const std::vector<const CustomObjectType::PropertyEntry*>& shader_flags = type_->getShaderFlags();
	if(shader_flags.empty() == false) {
		unsigned int flags = 0;

		for(unsigned int n = 0; n < shader_flags.size(); ++n) {
			const CustomObjectType::PropertyEntry& e = *shader_flags[n];
			variant result;
			if(e.getter) {
				ActivePropertyScope scope(*this, e.storage_slot);
				result = e.getter->execute(*this);
			} else if(e.const_value) {
				result = *e.const_value;
			} else if(e.storage_slot >= 0) {
				result = get_property_data(e.storage_slot);
				result.strengthen();
			} else {
				ASSERT_LOG(false, "Illegal property: " << e.id);
			}

			if(result.as_bool()) {
				flags = flags | (1 << n);
			}
		}

		if(flags != shader_flags_) {
			shader_flags_ = flags;
			shader_ = graphics::AnuraShaderPtr(new graphics::AnuraShader(*type_->getShaderWithParms(shader_flags_)));
			shader_->setParent(this);
		}
	}
}

void CustomObject::set_driver_position()
{
	if(driver_) {
		const int pos_right = x() + type_->getPassengerX();
		const int pos_left = x() + getCurrentFrame().width() - driver_->getCurrentFrame().width() - type_->getPassengerX();
		driver_->setFacingRight(isFacingRight());

		driver_->setPos(isFacingRight() ? pos_right : pos_left, y() + type_->getPassengerY());
	}
}

#ifndef NO_EDITOR
ConstEditorEntityInfoPtr CustomObject::getEditorInfo() const
{
	return type_->getEditorInfo();
}
#endif // !NO_EDITOR

int CustomObject::velocityX() const
{
	return velocity_x_.as_int();
}

int CustomObject::velocityY() const
{
	return velocity_y_.as_int();
}

int CustomObject::getSurfaceFriction() const
{
	return type_->getSurfaceFriction();
}

int CustomObject::getSurfaceTraction() const
{
	return type_->getSurfaceTraction();
}

bool CustomObject::hasFeet() const
{
	return has_feet_ && solid();
}

bool CustomObject::isStandable(int xpos, int ypos, int* friction, int* traction, int* adjust_y) const
{
	if(!isBodyPassthrough() && !isBodyHarmful() && pointCollides(xpos, ypos)) {
		if(friction) {
			*friction = type_->getSurfaceFriction();
		}

		if(traction) {
			*traction = type_->getSurfaceTraction();
		}

		if(adjust_y) {
			if(type_->useImageForCollisions()) {
				for(*adjust_y = 0; pointCollides(xpos, ypos - *adjust_y - 1); --(*adjust_y)) {
				}
			} else {
				*adjust_y = ypos - getBodyRect().y();
			}
		}

		return true;
	}

	if(frame_->hasPlatform()) {
		const Frame& f = *frame_;
		int y1 = y() + f.platformY();
		int y2 = previous_y_ + f.platformY();

		if(y1 > y2) {
			std::swap(y1, y2);
		}

		if(ypos < y1 || ypos > y2) {
			return false;
		}

		if(xpos < x() + f.platformX() || xpos >= x() + f.platformX() + f.platformW()) {
			return false;
		}

		if(friction) {
			*friction = type_->getSurfaceFriction();
		}

		if(traction) {
			*traction = type_->getSurfaceTraction();
		}

		if(adjust_y) {
			*adjust_y = y() + f.platformY() - ypos;
		}

		return true;
	}

	return false;
}

bool CustomObject::destroyed() const
{
	return hitpoints_ <= 0;
}

bool CustomObject::pointCollides(int xpos, int ypos) const
{
	if(type_->useImageForCollisions()) {
		const bool result = !getCurrentFrame().isAlpha(xpos - x(), ypos - y(), time_in_frame_, isFacingRight());
		return result;
	} else {
		return pointInRect(point(xpos, ypos), getBodyRect());
	}
}

bool CustomObject::rectCollides(const rect& r) const
{
	if(type_->useImageForCollisions()) {
		rect myrect(x(), y(), getCurrentFrame().width(), getCurrentFrame().height());
		if(rects_intersect(myrect, r)) {
			rect intersection = intersection_rect(myrect, r);
			for(int y = intersection.y(); y < intersection.y2(); ++y) {
				for(int x = intersection.x(); x < intersection.x2(); ++x) {
					if(pointCollides(x, y)) {
						return true;
					}
				}
			}

			return false;
		} else {
			return false;
		}
	} else {
		return rects_intersect(r, getBodyRect());
	}
}

ConstSolidInfoPtr CustomObject::calculateSolid() const
{
	if(!type_->hasSolid()) {
		return ConstSolidInfoPtr();
	}

	const Frame& f = getCurrentFrame();
	if(f.solid()) {
		return f.solid();
	}

	return type_->solid();
}

ConstSolidInfoPtr CustomObject::calculatePlatform() const
{
	if(platform_solid_info_.get()) {
		return platform_solid_info_;
	} else if(platform_area_) {
		//if platform_solid_info_ is nullptr but we have a rect, that
		//means there is no platform, so return nullptr instead of
		//defaulting to the type.
		return ConstSolidInfoPtr();
	}

	const Frame& f = getCurrentFrame();
	if(f.platform()) {
		return f.platform();
	}


	return type_->platform();
}

void CustomObject::control(const Level& lvl)
{
}

CustomObject::STANDING_STATUS CustomObject::isStanding(const Level& lvl, CollisionInfo* info) const
{
	if(!hasFeet()) {
		return STANDING_STATUS::NOT_STANDING;
	}

	const int width = type_->getFeetWidth();

	if(width >= 1) {
		const int facing = isFacingRight() ? 1 : -1;
		if(point_standable(lvl, *this, getFeetX() + width*facing, getFeetY(), info, fall_through_platforms_ ? SOLID_ONLY : SOLID_AND_PLATFORMS)) {
			return STANDING_STATUS::FRONT_FOOT;
		}

		if(point_standable(lvl, *this, getFeetX() - width*facing, getFeetY(), info, fall_through_platforms_ ? SOLID_ONLY : SOLID_AND_PLATFORMS)) {
			return STANDING_STATUS::BACK_FOOT;
		}

		return STANDING_STATUS::NOT_STANDING;
	}

	if(point_standable(lvl, *this, getFeetX(), getFeetY(), info, fall_through_platforms_ ? SOLID_ONLY : SOLID_AND_PLATFORMS)) {
		return STANDING_STATUS::FRONT_FOOT;
	} else {
		return STANDING_STATUS::NOT_STANDING;
	}
}

namespace {

#ifndef DISABLE_FORMULA_PROFILER
using formula_profiler::event_call_stack;
#endif

variant call_stack(const CustomObject& obj) {
	std::vector<variant> result;

#ifndef DISABLE_FORMULA_PROFILER
	for(int n = 0; n != event_call_stack.size(); ++n) {
		result.emplace_back(variant(get_object_event_str(event_call_stack[n].event_id)));
	}
#endif

	return variant(&result);
}

}

std::set<CustomObject*>& CustomObject::getAll()
{
	typedef std::set<CustomObject*> Set;
	static Set* all = new Set;
	return *all;
}

std::set<CustomObject*>& CustomObject::getAll(const std::string& type)
{
	typedef std::map<std::string, std::set<CustomObject*> > Map;
	static Map* all = new Map;
	return (*all)[type];
}

void CustomObject::init()
{
}

void CustomObject::run_garbage_collection()
{
	const int starting_ticks = profile::get_tick_time();

	LOG_INFO("RUNNING GARBAGE COLLECTION FOR " << getAll().size() << " OBJECTS...");

	std::vector<EntityPtr> references;
	for(CustomObject* obj : getAll()) {
		references.emplace_back(EntityPtr(obj));
	}

	std::set<const void*> safe;
	std::vector<gc_object_reference> refs;

	for(CustomObject* obj : getAll()) {
		obj->extractGcObjectReferences(refs);
	}
	
	for(int pass = 1;; ++pass) {
		const int starting_safe = static_cast<int>(safe.size());
		for(auto* obj : getAll()) {
			if(obj->refcount() > 1) {
				safe.insert(obj);
			}
		}

		if(starting_safe == safe.size()) {
			break;
		}

		LOG_INFO("PASS " << pass << ": " << safe.size() << " OBJECTS SAFE");

		for(gc_object_reference& ref : refs) {
			if(ref.owner == nullptr) {
				continue;
			}

			if(safe.count(ref.owner)) {
				restoreGcObjectReference(ref);
				ref.owner = nullptr;
			}
		}
	}

	for(gc_object_reference& ref : refs) {
		if(ref.owner == nullptr || !ref.visitor) {
			continue;
		}

		for(game_logic::FormulaCallableSuspendedPtr ptr : ref.visitor->pointers()) {
			if(safe.count(ptr->value())) {
				ptr->restore_ref();
			}
		}
	}

	LOG_INFO("RAN GARBAGE COLLECTION IN " << (profile::get_tick_time() - starting_ticks) << "ms. Releasing " << (getAll().size() - safe.size()) << "/" << getAll().size() << " OBJECTS");
}

void CustomObject::beingRemoved()
{
	Entity::beingRemoved();

	animated_movement_.clear();

	handleEvent(OBJECT_EVENT_BEING_REMOVED);

	for(auto w : widgets_) {
		w->onHide();
	}

#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active(false);
	}
#endif
}

void CustomObject::beingAdded()
{
#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active();
	}
#endif
	handleEvent(OBJECT_EVENT_BEING_ADDED);
}

void CustomObject::setAnimatedSchedule(std::shared_ptr<AnimatedMovement> movement)
{
	assert(movement.get() != nullptr);
	animated_movement_.emplace_back(movement);
}

void CustomObject::addAnimatedMovement(variant attr_var, variant options)
{
	if(options["sleep"].as_bool(false)) {
		variant cmd = game_logic::deferCurrentCommandSequence();
		if(cmd.is_null() == false) {
			static const variant OnComplete("on_complete");
			variant on_complete = options[OnComplete];
			if(on_complete.is_null()) {
				on_complete = cmd;
			} else {
				std::vector<variant> v;
				v.push_back(on_complete);
				v.push_back(cmd);
				on_complete = variant(&v);
			}

			options = options.add_attr(OnComplete, on_complete);
		}
	}

	const std::string& name = options["name"].as_string_default("");
	if(options["replace_existing"].as_bool(false)) {
		cancelAnimatedSchedule(name);
	} else if(name != "") {
		for(auto move : animated_movement_) {
			if(move->name == name) {
				move->follow_on.emplace_back(std::make_pair(attr_var, options));
				return;
			}
		}
	}

	const std::string type = queryValueBySlot(CUSTOM_OBJECT_TYPE).as_string();
	game_logic::FormulaCallableDefinitionPtr def = CustomObjectType::getDefinition(type);
	ASSERT_LOG(def.get() != nullptr, "Could not get definition for object: " << type);

	std::vector<int> slots;
	std::vector<variant> begin_values, end_values;

	const auto& attr = attr_var.as_map();

	for(const auto& p : attr) {
		slots.emplace_back(def->getSlot(p.first.as_string()));
		ASSERT_LOG(slots.back() >= 0, "Using animate on " << type << " object with unknown property: " << p.first.as_string());
		end_values.emplace_back(p.second);
		begin_values.emplace_back(queryValueBySlot(slots.back()));

		ASSERT_LOG(begin_values.back().is_null() == false, "Using animate on " << type << " object property " << p.first.as_string() << " which is null");
	}

	const int ncycles = options["duration"].as_int(10);

	std::function<decimal(decimal)> easing_fn;
	variant easing_var = options["easing"];
	if(easing_var.is_function()) {
		easing_fn = [=](decimal x) { std::vector<variant> args; args.emplace_back(variant(decimal(x))); return easing_var(args).as_decimal(); };
	} else {
		const std::string& easing = easing_var.as_string_default("linear");
		if(easing == "linear") {
			easing_fn = [](decimal x) { return x; };
		} else if(easing == "swing") {
			easing_fn = [](decimal x) { return decimal(0.5*(1 - cos(x.as_float()*3.14))); };
		} else {
			ASSERT_LOG(false, "Unknown easing: " << easing);
		}
	}

	std::vector<variant> values;
	values.reserve(slots.size()*ncycles);

	for(int cycle = 0; cycle != ncycles; ++cycle) {
		decimal ratio = ncycles <= 1 ? decimal(1) : decimal(cycle)/decimal(ncycles-1);
		ratio = easing_fn(ratio);
		for(int n = 0; n != slots.size(); ++n) {
			values.emplace_back(interpolate_variants(begin_values[n], end_values[n], ratio));
		}
	}

	std::shared_ptr<CustomObject::AnimatedMovement> movement(new CustomObject::AnimatedMovement);
	movement->name = name;
	movement->animation_values.swap(values);
	movement->animation_slots.swap(slots);

	movement->on_begin = options["on_begin"];
	movement->on_process = options["on_process"];
	movement->on_complete = options["on_complete"];

	setAnimatedSchedule(movement);
}

void CustomObject::cancelAnimatedSchedule(const std::string& name)
{
	if(name.empty()) {
		animated_movement_.clear();
		return;
	}

	for(auto& p : animated_movement_) {
		if(name == p->name) {
			p.reset();
		}
	}

	animated_movement_.erase(std::remove(animated_movement_.begin(), animated_movement_.end(), std::shared_ptr<AnimatedMovement>()), animated_movement_.end());
}

namespace 
{
	using game_logic::FormulaCallable;

	//Object that provides an FFL interface to an object's event handlers.
	class event_handlers_callable : public FormulaCallable 
	{
		ffl::IntrusivePtr<CustomObject> obj_;

		variant getValue(const std::string& key) const override {
			game_logic::ConstFormulaPtr f = obj_->getEventHandler(get_object_event_id(key));
			if(!f) {
				return variant();
			} else {
				return variant(f->str());
			}
		}
		void setValue(const std::string& key, const variant& value) override {
			static ffl::IntrusivePtr<CustomObjectCallable> custom_object_definition(new CustomObjectCallable);

			game_logic::FormulaPtr f(new game_logic::Formula(value, &get_custom_object_functions_symbol_table(), custom_object_definition.get()));
			obj_->setEventHandler(get_object_event_id(key), f);
		}
	public:
		explicit event_handlers_callable(const CustomObject& obj) : obj_(const_cast<CustomObject*>(&obj))
		{}

		const CustomObject& obj() const { return *obj_; }
	};

	// FFL widget interface.
	class widgets_callable : public FormulaCallable 
	{
		ffl::IntrusivePtr<CustomObject> obj_;

		variant getValue(const std::string& key) const override {
			if(key == "children") {
				std::vector<variant> v = obj_->getVariantWidgetList();
				return variant(&v);
			}
			return variant(obj_->getWidgetById(key).get());
		}
		void setValue(const std::string& key, const variant& value) override {
			if(key == "child") {

				gui::WidgetPtr new_widget = widget_factory::create(value, obj_.get());

				if(new_widget->id().empty() == false) {
					gui::WidgetPtr existing = obj_->getWidgetById(new_widget->id());
					if(existing != nullptr) {
						obj_->removeWidget(existing);
					}
				}

				obj_->addWidget(new_widget);
				return;
			}
			if(value.is_null()) {
				gui::WidgetPtr w = obj_->getWidgetById(key);
				if(w != nullptr) {
					obj_->removeWidget(w);
				}
			} else {
				gui::WidgetPtr w = obj_->getWidgetById(key);
				ASSERT_LOG(w != nullptr, "no widget with identifier " << key << " found");
				obj_->removeWidget(w);
				obj_->addWidget(widget_factory::create(value, obj_.get()));
			}
		}
	public:
		explicit widgets_callable(const CustomObject& obj) : obj_(const_cast<CustomObject*>(&obj))
		{}
	};

	decimal calculate_velocity_magnitude(decimal velocity_x, decimal velocity_y)
	{
		decimal val = velocity_x*velocity_x + velocity_y*velocity_y;
		double value = sqrt(val.as_float());
		decimal result(decimal::from_int(static_cast<int>(value)));
		result /= 1000;
		return result;
	}

	static const double radians_to_degrees = 57.29577951308232087;
	decimal calculate_velocity_angle(decimal velocity_x, decimal velocity_y)
	{
		if(velocity_y.as_int() == 0 && velocity_x.as_int() == 0) {
			return decimal::from_int(0);
		}

		const double theta = atan2(velocity_y.as_float(), velocity_x.as_float());
		return decimal(theta*radians_to_degrees);
	}

	variant two_element_variant_list(const variant& a, const variant&b) 
	{
		std::vector<variant> v;
		v.emplace_back(a);
		v.emplace_back(b);
		return variant(&v);
	}
}

variant CustomObject::getValueBySlot(int slot) const
{
	switch(slot) {
	case CUSTOM_OBJECT_VALUE: {
		ASSERT_LOG(value_stack_.empty() == false, "Query of 'value' symbol in illegal context");
		return value_stack_.top();
	}
	case CUSTOM_OBJECT_DATA: {
		ASSERT_LOG(active_property_ >= 0, "Access of 'data' outside of an object property which has data");
		if(static_cast<unsigned>(active_property_) < property_data_.size()) {
			variant result = property_data_[active_property_];
			result.strengthen();
			return result;
		} else {
			return variant();
		}
	}

	case CUSTOM_OBJECT_CREATED: {
		return variant::from_bool(created_);
	}

	case CUSTOM_OBJECT_ARG: {
		if(backup_callable_stack_.empty() == false && backup_callable_stack_.top()) {
			return variant(backup_callable_stack_.top());
		}

		game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable(this));
		return variant(callable.get());
	}

	case CUSTOM_OBJECT_CONSTS:            return variant(type_->consts().get());
	case CUSTOM_OBJECT_TYPE:              return variant(type_->id());
	case CUSTOM_OBJECT_ACTIVE:            return variant::from_bool(last_cycle_active_ >= Level::current().cycle() - 2);
	case CUSTOM_OBJECT_LIB:               return variant(game_logic::get_library_object().get());
	case CUSTOM_OBJECT_TIME_IN_ANIMATION: return variant(time_in_frame_);
	case CUSTOM_OBJECT_TIME_IN_ANIMATION_DELTA: return variant(time_in_frame_delta_);
	case CUSTOM_OBJECT_FRAME_IN_ANIMATION: return variant(getCurrentFrame().frameNumber(time_in_frame_));
	case CUSTOM_OBJECT_LEVEL:             return variant(&Level::current());
	case CUSTOM_OBJECT_ANIMATION:         return frame_->variantId();
	case CUSTOM_OBJECT_ANIMATION_OBJ:     return variant(frame_.get());
	case CUSTOM_OBJECT_ANIMATION_MAP:     return frame_->write();
	case CUSTOM_OBJECT_AVAILABLE_ANIMATIONS: return type_->getAvailableFrames();
	case CUSTOM_OBJECT_HITPOINTS:         return variant(hitpoints_);
	case CUSTOM_OBJECT_MAX_HITPOINTS:     return variant(type_->getHitpoints() + max_hitpoints_);
	case CUSTOM_OBJECT_MASS:              return variant(type_->mass());
	case CUSTOM_OBJECT_LABEL:             return variant(label());
	case CUSTOM_OBJECT_X:                 return variant(x());
	case CUSTOM_OBJECT_Y:                 return variant(y());
	case CUSTOM_OBJECT_XY:                {
			 				 				std::vector<variant> v;
											v.emplace_back(variant(x()));
											v.emplace_back(variant(y()));
											return variant(&v);
										  }
	case CUSTOM_OBJECT_Z:
	case CUSTOM_OBJECT_ZORDER:            return variant(zorder());
	case CUSTOM_OBJECT_ZSUB_ORDER:        return variant(zSubOrder());
    case CUSTOM_OBJECT_RELATIVE_X:        return variant(relative_x_);
	case CUSTOM_OBJECT_RELATIVE_Y:        return variant(relative_y_);
	case CUSTOM_OBJECT_SPAWNED_BY:        if(wasSpawnedBy().empty()) return variant(); else return variant(Level::current().get_entity_by_label(wasSpawnedBy()).get());
	case CUSTOM_OBJECT_SPAWNED_CHILDREN: {
		std::vector<variant> children;
		for(const EntityPtr& e : Level::current().get_chars()) {
			if(e->wasSpawnedBy() == label()) {
				children.emplace_back(variant(e.get()));
			}
		}

		return variant(&children);
	}
	case CUSTOM_OBJECT_PARENT:            return variant(parent_.get());
	case CUSTOM_OBJECT_PIVOT:             return variant(parent_pivot_);
	case CUSTOM_OBJECT_PREVIOUS_Y:        return variant(previous_y_);
	case CUSTOM_OBJECT_X1:                return variant(solidRect().x());
	case CUSTOM_OBJECT_X2:                return variant(solidRect().w() ? solidRect().x2() : x() + getCurrentFrame().width());
	case CUSTOM_OBJECT_Y1:                return variant(solidRect().y());
	case CUSTOM_OBJECT_Y2:                return variant(solidRect().h() ? solidRect().y2() : y() + getCurrentFrame().height());
	case CUSTOM_OBJECT_W:                 return variant(solidRect().w());
	case CUSTOM_OBJECT_H:                 return variant(solidRect().h());

	case CUSTOM_OBJECT_ACTIVATION_BORDER: return variant(activation_border_);
	case CUSTOM_OBJECT_MID_X:
	case CUSTOM_OBJECT_MIDPOINT_X:        return variant(solidRect().w() ? solidRect().x() + solidRect().w()/2 : x() + getCurrentFrame().width()/2);
	case CUSTOM_OBJECT_MID_Y:
	case CUSTOM_OBJECT_MIDPOINT_Y:        return variant(solidRect().h() ? solidRect().y() + solidRect().h()/2 : y() + getCurrentFrame().height()/2);
	case CUSTOM_OBJECT_MID_XY:
	case CUSTOM_OBJECT_MIDPOINT_XY: {
		return two_element_variant_list(
			variant(solidRect().w() ? solidRect().x() + solidRect().w()/2 : x() + getCurrentFrame().width()/2),
			variant(solidRect().h() ? solidRect().y() + solidRect().h()/2 : y() + getCurrentFrame().height()/2));
	}

	case CUSTOM_OBJECT_ANCHORX:			  { decimal res = getAnchorX(); if(res < 0) { return variant(); } else { return variant(res); } }
	case CUSTOM_OBJECT_ANCHORY:			  { decimal res = getAnchorY(); if(res < 0) { return variant(); } else { return variant(res); } }

    case CUSTOM_OBJECT_IS_SOLID:		  return variant::from_bool(solid() != nullptr);
	case CUSTOM_OBJECT_SOLID_RECT:        return variant(RectCallable::create(solidRect()));
	case CUSTOM_OBJECT_SOLID_MID_X:       return variant(solidRect().x() + solidRect().w()/2);
	case CUSTOM_OBJECT_SOLID_MID_Y:       return variant(solidRect().y() + solidRect().h()/2);
	case CUSTOM_OBJECT_SOLID_MID_XY: {
		return two_element_variant_list(
			variant(solidRect().x() + solidRect().w()/2),
			variant(solidRect().y() + solidRect().h()/2));
	}
	case CUSTOM_OBJECT_IMG_MID_X:       return variant(x() + getCurrentFrame().width()/2);
	case CUSTOM_OBJECT_IMG_MID_Y:       return variant(y() + getCurrentFrame().height()/2);
	case CUSTOM_OBJECT_IMG_MID_XY: {
		return two_element_variant_list(
			variant(x() + getCurrentFrame().width()/2),
			variant(y() + getCurrentFrame().height()/2));
	}
	case CUSTOM_OBJECT_IMG_W:             return variant(getCurrentFrame().width());
	case CUSTOM_OBJECT_IMG_H:             return variant(getCurrentFrame().height());
	case CUSTOM_OBJECT_IMG_WH: {
		return two_element_variant_list(
			variant(getCurrentFrame().width()),
			variant(getCurrentFrame().height()));
	}
	case CUSTOM_OBJECT_FRONT:             return variant(isFacingRight() ? getBodyRect().x2() : getBodyRect().x());
	case CUSTOM_OBJECT_BACK:              return variant(isFacingRight() ? getBodyRect().x() : getBodyRect().x2());
	case CUSTOM_OBJECT_CYCLE:             return variant(cycle_);
	case CUSTOM_OBJECT_FACING:            return variant(isFacingRight() ? 1 : -1);
	case CUSTOM_OBJECT_UPSIDE_DOWN:       return variant(isUpsideDown() ? 1 : -1);
	case CUSTOM_OBJECT_UP:                return variant(isUpsideDown() ? 1 : -1);
	case CUSTOM_OBJECT_DOWN:              return variant(isUpsideDown() ? -1 : 1);
	case CUSTOM_OBJECT_VELOCITY_X:        return variant(velocity_x_);
	case CUSTOM_OBJECT_VELOCITY_Y:        return variant(velocity_y_);
	case CUSTOM_OBJECT_VELOCITY_XY: {
		return two_element_variant_list(
			variant(velocity_x_),
			variant(velocity_y_));
	}

	case CUSTOM_OBJECT_VELOCITY_MAGNITUDE: return variant(calculate_velocity_magnitude(velocity_x_, velocity_y_));
	case CUSTOM_OBJECT_VELOCITY_ANGLE:     return variant(calculate_velocity_angle(velocity_x_, velocity_y_));

	case CUSTOM_OBJECT_ACCEL_X:           return variant(accel_x_);
	case CUSTOM_OBJECT_ACCEL_Y:           return variant(accel_y_);
	case CUSTOM_OBJECT_ACCEL_XY: {
		return two_element_variant_list(
			variant(accel_x_),
			variant(accel_y_));
	}
	case CUSTOM_OBJECT_GRAVITY_SHIFT:     return variant(gravity_shift_);
	case CUSTOM_OBJECT_PLATFORM_MOTION_X: return variant(getPlatformMotionX());
	case CUSTOM_OBJECT_REGISTRY:          return variant(preferences::registry());
	case CUSTOM_OBJECT_GLOBALS:           return variant(global_vars().get());
	case CUSTOM_OBJECT_VARS:              return variant(vars_.get());
	case CUSTOM_OBJECT_TMP:               return variant(tmp_vars_.get());
	case CUSTOM_OBJECT_GROUP:             return variant(group());
	case CUSTOM_OBJECT_ROTATE:            return variant(getRotateZ());
	case CUSTOM_OBJECT_ME:
	case CUSTOM_OBJECT_SELF:              return variant(this);
	case CUSTOM_OBJECT_BRIGHTNESS:		  return variant((draw_color().addRed() + draw_color().addGreen() + draw_color().addBlue())/3);
	case CUSTOM_OBJECT_RED:               return variant(draw_color().addRed());
	case CUSTOM_OBJECT_GREEN:             return variant(draw_color().addGreen());
	case CUSTOM_OBJECT_BLUE:              return variant(draw_color().addBlue());
	case CUSTOM_OBJECT_ALPHA:             return variant(draw_color().addAlpha());
	case CUSTOM_OBJECT_TEXT_ALPHA:        return variant(text_ ? text_->alpha : 255);
	case CUSTOM_OBJECT_TEXT_ATTRS: {
		std::map<variant, variant> v;
		v[variant("text")] = variant(text_->text);
		//v[variant("font")] = variant(text_->font); //kre/Font.cpp was unforthcoming about how to get a font name out of this.
		v[variant("size")] = variant(text_->size);
		v[variant("align")] = variant(text_->align);
		v[variant("alpha")] = variant(text_->alpha);
		
		std::vector<variant> d;
		d.emplace_back(variant(text_->dimensions.x()));
		d.emplace_back(variant(text_->dimensions.y()));
		d.emplace_back(variant(text_->dimensions.x2()));
		d.emplace_back(variant(text_->dimensions.y2()));
		v[variant("dimensions")] = variant(&d);
		return variant(&v);
	}
	case CUSTOM_OBJECT_DAMAGE:            return variant(getCurrentFrame().damage());
	case CUSTOM_OBJECT_HIT_BY:            return variant(last_hit_by_.get());
	case CUSTOM_OBJECT_IS_STANDING:       return variant::from_bool(standing_on_.get() || isStanding(Level::current()) != STANDING_STATUS::NOT_STANDING);
	case CUSTOM_OBJECT_STANDING_INFO:     {
		CollisionInfo info;
		isStanding(Level::current(), &info);
		if(info.surf_info && info.surf_info->info) {
			return variant(*info.surf_info->info);
		} else {
			return variant();
		}
	}
	case CUSTOM_OBJECT_NEAR_CLIFF_EDGE:   return variant::from_bool(isStanding(Level::current()) != STANDING_STATUS::NOT_STANDING && cliff_edge_within(Level::current(), getFeetX(), getFeetY(), getFaceDir()*15));
	case CUSTOM_OBJECT_DISTANCE_TO_CLIFF: return variant(::distance_to_cliff(Level::current(), getFeetX(), getFeetY(), getFaceDir()));
    case CUSTOM_OBJECT_DISTANCE_TO_CLIFF_REVERSE: return variant(::distance_to_cliff(Level::current(), getFeetX(), getFeetY(), -getFaceDir()));
	case CUSTOM_OBJECT_SLOPE_STANDING_ON: {
		if(standing_on_ && standing_on_->platform() && !standing_on_->isSolidPlatform()) {
			return variant(standing_on_->platformSlopeAt(getFeetX()));
		}
		return variant(-slopeStandingOn(6)*getFaceDir());
	}
	case CUSTOM_OBJECT_UNDERWATER:        return variant(Level::current().isUnderwater(solid() ? solidRect() : rect(x(), y(), getCurrentFrame().width(), getCurrentFrame().height())));
	case CUSTOM_OBJECT_PREVIOUS_WATER_BOUNDS: {
		std::vector<variant> v;
		v.emplace_back(variant(previous_water_bounds_.x()));
		v.emplace_back(variant(previous_water_bounds_.y()));
		v.emplace_back(variant(previous_water_bounds_.x2()));
		v.emplace_back(variant(previous_water_bounds_.y2()));
		return variant(&v);

	}
	case CUSTOM_OBJECT_WATER_BOUNDS: {
		rect area;
		if(Level::current().isUnderwater(solidRect(), &area)) {
			std::vector<variant> v;
			v.emplace_back(variant(area.x()));
			v.emplace_back(variant(area.y()));
			v.emplace_back(variant(area.x2()));
			v.emplace_back(variant(area.y2()));
			return variant(&v);
        } else if( Level::current().isUnderwater(rect(x(), y(), getCurrentFrame().width(), getCurrentFrame().height()), &area)) {
            //N.B:  has a baked-in assumption that the image-rect will always be bigger than the solid-rect; the idea being that this will only fall through if the solid-rect just doesn't exist.  Make an object where the solidity is bigger than the image, and this will break down.
            std::vector<variant> v;
            v.emplace_back(variant(area.x()));
            v.emplace_back(variant(area.y()));
            v.emplace_back(variant(area.x2()));
            v.emplace_back(variant(area.y2()));
            return variant(&v);
        } else {
			return variant();
        }

	}
	case CUSTOM_OBJECT_WATER_OBJECT: {
		variant v;
		Level::current().isUnderwater(solidRect(), nullptr, &v);
		return v;
	}
	case CUSTOM_OBJECT_DRIVER:            return variant(driver_ ? driver_.get() : this);
	case CUSTOM_OBJECT_IS_HUMAN:          return variant::from_bool(isHuman() != nullptr);
	case CUSTOM_OBJECT_INVINCIBLE:        return variant::from_bool(invincible_ != 0);
	case CUSTOM_OBJECT_SOUND_VOLUME:      return variant(sound_volume_);
	case CUSTOM_OBJECT_AUDIO:		      return variant(new sound::AudioEngine(ffl::IntrusivePtr<const CustomObject>(this)));
	case CUSTOM_OBJECT_DESTROYED:         return variant::from_bool(destroyed());

	case CUSTOM_OBJECT_IS_STANDING_ON_PLATFORM: {
		if(standing_on_ && standing_on_->platform() && !standing_on_->isSolidPlatform()) {
			return variant::from_bool(true);
		}

		CollisionInfo info;
		isStanding(Level::current(), &info);
		return variant(info.platform);
	}

	case CUSTOM_OBJECT_STANDING_ON: {
		if(standing_on_) {
			return variant(standing_on_.get());
		}

		EntityPtr stand_on;
		CollisionInfo info;
		isStanding(Level::current(), &info);
		return variant(info.collide_with.get());
	}

	case CUSTOM_OBJECT_SHADER: {
		return variant(shader_.get());
	}

	case CUSTOM_OBJECT_EFFECTS: {
		std::vector<variant> v;
		for(auto eff : effects_shaders_) {
			v.emplace_back(eff.get());
		}
		return variant(&v);
	}

	case CUSTOM_OBJECT_DOCUMENT: {
		return variant(document_.get());
	}

	case CUSTOM_OBJECT_ACTIVATION_AREA: {
		if(activation_area_.get() != nullptr) {
			std::vector<variant> v(4);
			v[0] = variant(activation_area_->x());
			v[1] = variant(activation_area_->y());
			v[2] = variant(activation_area_->w());
			v[3] = variant(activation_area_->h());
			return variant(&v);
		} else {
			return variant();
		}
	}

	case CUSTOM_OBJECT_CLIPAREA: {
		if(clip_area_.get() != nullptr) {
			std::vector<variant> v(4);
			v[0] = variant(clip_area_->x());
			v[1] = variant(clip_area_->y());
			v[2] = variant(clip_area_->w());
			v[3] = variant(clip_area_->h());
			return variant(&v);
		} else {
			return variant();
		}
	}

	case CUSTOM_OBJECT_CLIPAREA_ABSOLUTE: {
		return variant::from_bool(clip_area_absolute_);
	}

	case CUSTOM_OBJECT_VARIATIONS: {
		std::vector<variant> result;
		for(const std::string& s : current_variation_) {
			result.emplace_back(variant(s));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_PARALLAX_SCALE_X: {
		return variant(parallaxScaleMillisX()/1000.0);
	}

	case CUSTOM_OBJECT_PARALLAX_SCALE_Y: {
		return variant(parallaxScaleMillisY()/1000.0);
	}

	case CUSTOM_OBJECT_ATTACHED_OBJECTS: {
		std::vector<variant> result;
		for(const EntityPtr& e : attachedObjects()) {
			result.emplace_back(variant(e.get()));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_CALL_STACK: {
		return call_stack(*this);
	}

	case CUSTOM_OBJECT_LIGHTS: {
		std::vector<variant> result;
		for(const LightPtr& p : lights_) {
			result.emplace_back(variant(p.get()));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_PLATFORM_AREA: {
		if(platform_area_) {
			return platform_area_->write();
		} else {
			return variant();
		}
	}
	case CUSTOM_OBJECT_PLATFORM_OFFSETS: {
		std::vector<variant> result;
		for(int n : platform_offsets_) {
			result.emplace_back(variant(n));
		}
		return variant(&result);
	}

	case CUSTOM_OBJECT_SOLID_DIMENSIONS_IN: {
		std::vector<variant> v;
		v.emplace_back(variant(getSolidDimensions()));
		v.emplace_back(variant(getWeakSolidDimensions()));
		return variant(&v);
	}

	case CUSTOM_OBJECT_COLLIDES_WITH_LEVEL: {
		return variant::from_bool(collides_with_level_);
	}

	case CUSTOM_OBJECT_ALWAYS_ACTIVE: return variant::from_bool(always_active_);
	case CUSTOM_OBJECT_TAGS: return variant(tags_.get());
	case CUSTOM_OBJECT_SCALE:
		if(draw_scale_) {
			return variant(*draw_scale_);
		} else {
			return variant(decimal::from_int(1));
		}
	case CUSTOM_OBJECT_HAS_FEET: return variant::from_bool(has_feet_);

	case CUSTOM_OBJECT_UV_ARRAY: {
		std::vector<variant> result;
		result.reserve(custom_draw_uv_.size());
		for(float f : custom_draw_uv_) {
			result.emplace_back(variant(decimal(f)));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_XY_ARRAY: {
		std::vector<variant> result;
		result.reserve(custom_draw_xy_.size());
		for(float f : custom_draw_xy_) {
			result.emplace_back(variant(decimal(f)));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_EVENT_HANDLERS: {
		return variant(new event_handlers_callable(*this));
	}

	case CUSTOM_OBJECT_USE_ABSOLUTE_SCREEN_COORDINATES: {
		return variant::from_bool(use_absolute_screen_coordinates_);
	}

	case CUSTOM_OBJECT_WIDGETS: {
		return variant(new widgets_callable(*this));
	}

	case CUSTOM_OBJECT_WIDGET_LIST: {
		std::vector<variant> v = getVariantWidgetList();
		return variant(&v);
	}

#if defined(USE_BOX2D)
	case CUSTOM_OBJECT_BODY: {
		return variant(body_.get());
	}
#endif

	case CUSTOM_OBJECT_PAUSED: {
		return variant::from_bool(paused_);
	}

	case CUSTOM_OBJECT_MOUSEOVER_DELAY: {
		return variant(getMouseoverDelay());
	}
	case CUSTOM_OBJECT_MOUSEOVER_AREA: {
		return getMouseOverArea().write();
	}
	case CUSTOM_OBJECT_PARTICLE_SYSTEMS: {
		std::map<variant, variant> v;
		for(auto& ps : particle_systems_) {
			v[variant(ps.first)] = variant(ps.second.get());
		}
		return variant(&v);
	}

	case CUSTOM_OBJECT_PARTICLES: {
		return variant(particles_.get());	
	}
	
	case CUSTOM_OBJECT_BLUR: {
		std::vector<variant> result;
		for(auto p : blur_objects_) {
			result.push_back(variant(p.get()));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_ANIMATED_MOVEMENTS: {
		std::vector<variant> result;
		for(auto p : animated_movement_) {
			result.emplace_back(variant(p->name));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_CTRL_USER_OUTPUT: {
		return controls::user_ctrl_output();
	}

	case CUSTOM_OBJECT_DRAWPRIMITIVES: {
		std::vector<variant> v;
		for(auto& p : draw_primitives_) {
			v.emplace_back(variant(p.get()));
		}
		return variant(&v);
	}

	case CUSTOM_OBJECT_CTRL_UP:
	case CUSTOM_OBJECT_CTRL_DOWN:
	case CUSTOM_OBJECT_CTRL_LEFT:
	case CUSTOM_OBJECT_CTRL_RIGHT:
	case CUSTOM_OBJECT_CTRL_ATTACK:
	case CUSTOM_OBJECT_CTRL_JUMP:
	case CUSTOM_OBJECT_CTRL_TONGUE:
		return variant::from_bool(controlStatus(static_cast<controls::CONTROL_ITEM>(slot - CUSTOM_OBJECT_CTRL_UP)));
	
	case CUSTOM_OBJECT_CTRL_USER:
		return controlStatusUser();

	case CUSTOM_OBJECT_PLAYER_DIFFICULTY:
	case CUSTOM_OBJECT_PLAYER_CAN_INTERACT:
	case CUSTOM_OBJECT_PLAYER_UNDERWATER_CONTROLS:
	case CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEY:
	case CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEYS:
	case CUSTOM_OBJECT_PLAYER_CTRL_KEYS:
	case CUSTOM_OBJECT_PLAYER_CTRL_PREV_KEYS:
	case CUSTOM_OBJECT_PLAYER_CTRL_MICE:
	case CUSTOM_OBJECT_PLAYER_CTRL_TILT:
	case CUSTOM_OBJECT_PLAYER_CTRL_X:
	case CUSTOM_OBJECT_PLAYER_CTRL_Y:
	case CUSTOM_OBJECT_PLAYER_CONTROL_SCHEME:
	case CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK:
	case CUSTOM_OBJECT_PLAYER_CONTROL_LOCK:
		return getPlayerValueBySlot(slot);

	default:
		if(slot >= type_->getSlotPropertiesBase() && (size_t(slot - type_->getSlotPropertiesBase()) < type_->getSlotProperties().size())) {
			const CustomObjectType::PropertyEntry& e = type_->getSlotProperties()[slot - type_->getSlotPropertiesBase()];
			if(e.getter) {
				if(std::find(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), slot - type_->getSlotPropertiesBase()) != properties_requiring_dynamic_initialization_.end()) {
					ASSERT_LOG(false, "Read of uninitialized property " << getDebugDescription() << "." << e.id << " " << get_full_call_stack());
				}
				ActivePropertyScope scope(*this, e.storage_slot);
				return e.getter->execute(*this);
			} else if(e.const_value) {
				return *e.const_value;
			} else if(e.storage_slot >= 0) {
				variant result = get_property_data(e.storage_slot);
				result.strengthen();
				return result;
			} else {
				ASSERT_LOG(false, "PROPERTY HAS NO GETTER OR CONST VALUE");
			}
		}

		break;
	}

	auto entry = CustomObjectCallable::instance().getEntry(slot);
	if(entry != nullptr) {
		return variant();
	}
	
	ASSERT_LOG(false, "UNKNOWN SLOT QUERIED FROM OBJECT: " << slot);
	return variant();
}

variant CustomObject::getPlayerValueBySlot(int slot) const
{
	assert(CustomObjectCallable::instance().getEntry(slot));
	ASSERT_LOG(false, "Query of value for player objects on non-player object. Key: " << CustomObjectCallable::instance().getEntry(slot)->id);
	return variant();
}

void CustomObject::setPlayerValueBySlot(int slot, const variant& value)
{
	assert(CustomObjectCallable::instance().getEntry(slot));
	ASSERT_LOG(false, "Set of value for player objects on non-player object. Key: " << CustomObjectCallable::instance().getEntry(slot)->id);
}

namespace 
{
	using game_logic::FormulaCallable;

	class BackupCallableStackScope 
	{
		std::stack<const FormulaCallable*>* stack_;
	public:
		BackupCallableStackScope(std::stack<const FormulaCallable*>* s, const FormulaCallable* item) 
			: stack_(s) 
		{
			stack_->push(item);
		}

		~BackupCallableStackScope() {
			stack_->pop();
		}
	};
}

int CustomObject::getValueSlot(const std::string& key) const
{
	return type_->callableDefinition()->getSlot(key);
}

variant CustomObject::getValue(const std::string& key) const
{
	const int slot = type_->callableDefinition()->getSlot(key);
	if(slot >= 0 && slot < NUM_CUSTOM_OBJECT_PROPERTIES) {
		return getValueBySlot(slot);
	}

	std::map<std::string, CustomObjectType::PropertyEntry>::const_iterator property_itor = type_->properties().find(key);
	if(property_itor != type_->properties().end()) {
		if(property_itor->second.getter) {
			ActivePropertyScope scope(*this, property_itor->second.storage_slot);
			return property_itor->second.getter->execute(*this);
		} else if(property_itor->second.const_value) {
			return *property_itor->second.const_value;
		} else if(property_itor->second.storage_slot >= 0) {
			variant result = get_property_data(property_itor->second.storage_slot);
			result.strengthen();
			return result;
		}
	}

	if(!type_->isStrict()) {
		variant var_result = tmp_vars_->queryValue(key);
		if(!var_result.is_null()) {
			return var_result;
		}

		var_result = vars_->queryValue(key);
		if(!var_result.is_null()) {
			return var_result;
		}
	}

	std::map<std::string, variant>::const_iterator i = type_->variables().find(key);
	if(i != type_->variables().end()) {
		return i->second;
	}

	std::map<std::string, ParticleSystemPtr>::const_iterator particle_itor = particle_systems_.find(key);
	if(particle_itor != particle_systems_.end()) {
		return variant(particle_itor->second.get());
	}

	if(backup_callable_stack_.empty() == false && backup_callable_stack_.top()) {
		if(backup_callable_stack_.top() != this) {
			const FormulaCallable* callable = backup_callable_stack_.top();
			BackupCallableStackScope callable_scope(&backup_callable_stack_, nullptr);
			return callable->queryValue(key);
		}
	}

	ASSERT_LOG(!type_->isStrict(), "ILLEGAL OBJECT ACCESS WITH STRICT CHECKING IN " << getDebugDescription() << ": " << key << " At " << get_full_call_stack());

	return variant();
}

void CustomObject::getInputs(std::vector<game_logic::FormulaInput>* inputs) const
{
	const int end = isHuman() ? static_cast<int>(NUM_CUSTOM_OBJECT_PROPERTIES) : static_cast<int>(NUM_CUSTOM_OBJECT_NON_PLAYER_PROPERTIES);
	for(int n = CUSTOM_OBJECT_ARG+1; n != end; ++n) {
		auto entry = CustomObjectCallable::instance().getEntry(n);
		if(!getValueBySlot(n).is_null()) {
			inputs->emplace_back(entry->id);
		}
	}
}

void CustomObject::setValue(const std::string& key, const variant& value)
{
	const int slot = CustomObjectCallable::getKeySlot(key);
	if(slot != -1) {
		setValueBySlot(slot, value);
		return;
	}

	auto property_itor = type_->properties().find(key);
	if(property_itor != type_->properties().end()) {
		setValueBySlot(type_->getSlotPropertiesBase() + property_itor->second.slot, value);
		return;
	}

	if(key == "animation") {
		setFrame(value.as_string());
	} else if(key == "time_in_animation") {
		ASSERT_GE(value.as_int(), 0);
		time_in_frame_ = value.as_int()%frame_->duration();
	} else if(key == "time_in_animation_delta") {
		time_in_frame_delta_ = value.as_int();
	} else if(key == "x") {
		const int start_x = centiX();
		setX(value.as_int());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
		}
	} else if(key == "y") {
		const int start_y = centiY();
		setY(value.as_int());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiY(start_y);
		}
	} else if(key == "xy") {
		const int start_x = centiX();
		const int start_y = centiY();
		setX(value[0].as_int());
		setY(value[1].as_int());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
			setCentiY(start_y);
		}
	} else if(key == "z" || key == "zorder") {
		setZOrder(value.as_int());
	} else if(key == "zsub_order") {
		setZSubOrder(value.as_int());
	} else if(key == "midpoint_x" || key == "mid_x") {
        setMidX(value.as_int());
	} else if(key == "midpoint_y" || key == "mid_y") {
        setMidY(value.as_int());
	} else if(key == "facing") {
		setFacingRight(value.as_int() > 0);
	} else if(key == "upside_down") {
		setUpsideDown(value.as_int() != 0);
	} else if(key == "hitpoints") {
		const int old_hitpoints = hitpoints_;
		hitpoints_ = value.as_int();
		if(old_hitpoints > 0 && hitpoints_ <= 0) {
			die();
		}
	} else if(key == "max_hitpoints") {
		max_hitpoints_ = value.as_int() - type_->getHitpoints();
		if(hitpoints_ > type_->getHitpoints() + max_hitpoints_) {
			hitpoints_ = type_->getHitpoints() + max_hitpoints_;
		}
	} else if(key == "velocity_x") {
		velocity_x_ = value.as_decimal();
	} else if(key == "velocity_y") {
		velocity_y_ = value.as_decimal();
	} else if(key == "accel_x") {
		accel_x_ = value.as_decimal();
	} else if(key == "accel_y") {
		accel_y_ = value.as_decimal();
	} else if(key == "rotate" || key == "rotate_z") {
		setRotateZ(value.as_decimal());
	} else if(key == "red") {
		make_draw_color();
		draw_color_->setAddRed(value.as_int());
	} else if(key == "green") {
		make_draw_color();
		draw_color_->setAddGreen(value.as_int());
	} else if(key == "blue") {
		make_draw_color();
		draw_color_->setAddBlue(value.as_int());
	} else if(key == "alpha") {
		make_draw_color();
		draw_color_->setAddAlpha(value.as_int());
	} else if(key == "brightness"){
		make_draw_color();
		draw_color_->setAddRed(value.as_int());
		draw_color_->setAddGreen(value.as_int());
		draw_color_->setAddBlue(value.as_int());
	} else if(key == "current_generator") {
		setCurrentGenerator(value.try_convert<CurrentGenerator>());
	} else if(key == "invincible") {
		invincible_ = value.as_int();
	} else if(key == "fall_through_platforms") {
		fall_through_platforms_ = value.as_int();
	} else if(key == "tags") {
		if(value.is_list()) {
			tags_ = new game_logic::MapFormulaCallable;
			for(int n = 0; n != value.num_elements(); ++n) {
				tags_->add(value[n].as_string(), variant(1));
			}
		}
	} else if(key == "shader") {
		if(value.is_string()) {
			shader_.reset(new graphics::AnuraShader(value.as_string()));
		} else if(value.is_map()) {
			shader_.reset(new graphics::AnuraShader(value["name"].as_string(), value));
		} else {
			shader_.reset(value.try_convert<graphics::AnuraShader>());
		}
	} else if(key == "effects") {
		effects_shaders_.clear();
		if(value.is_list()) {
			for(int n = 0; n != value.num_elements(); ++n) {
				if(value[n].is_string()) {
					effects_shaders_.emplace_back(new graphics::AnuraShader(value[n].as_string()));
				} else {
					effects_shaders_.emplace_back(new graphics::AnuraShader(value[n]["name"].as_string(), value["name"]));
				}
			}
		} else if(value.is_map()) {
			effects_shaders_.emplace_back(new graphics::AnuraShader(value["name"].as_string(), value["name"]));
		} else if(value.is_string()) {
			effects_shaders_.emplace_back(new graphics::AnuraShader(value.as_string()));
		} else {
			effects_shaders_.emplace_back(graphics::AnuraShaderPtr(value.try_convert<graphics::AnuraShader>()));
			ASSERT_LOG(effects_shaders_.size() > 0, "Couldn't convert type to shader");
		}
	} else if(key == "document") {
		document_.reset(new xhtml::DocumentObject(value));
		document_->init(this);
	} else if(key == "particles") {
		createParticles(value);
	} else if(key == "draw_area") {
		if(value.is_list() && value.num_elements() == 4) {
			draw_area_.reset(new rect(value[0].as_int(), value[1].as_int(), value[2].as_int(), value[3].as_int()));
		} else {
			draw_area_.reset();
		}
	} else if(key == "scale") {
		draw_scale_.reset(new decimal(value.as_decimal()));
		if(draw_scale_->as_int() == 1 && draw_scale_->fractional() == 0) {
			draw_scale_.reset();
		}
	} else if(key == "activation_area") {
		if(value.is_list() && value.num_elements() == 4) {
			activation_area_.reset(new rect(value[0].as_int(), value[1].as_int(), value[2].as_int(), value[3].as_int()));
		} else {
			ASSERT_LOG(value.is_null(), "BAD ACTIVATION AREA: " << value.to_debug_string());
			activation_area_.reset();
		}
	} else if(key == "clip_area") {
		if(value.is_list() && value.num_elements() == 4) {
			clip_area_.reset(new rect(value[0].as_int(), value[1].as_int(), value[2].as_int(), value[3].as_int()));
		} else {
			ASSERT_LOG(value.is_null(), "BAD CLIP AREA: " << value.to_debug_string());
			clip_area_.reset();
		}
	} else if(key == "variations") {
		handleEvent("reset_variations");
		current_variation_.clear();
		if(value.is_list()) {
			for(int n = 0; n != value.num_elements(); ++n) {
				current_variation_.emplace_back(value[n].as_string());
			}
		} else if(value.is_string()) {
			current_variation_.emplace_back(value.as_string());
		}

		if(current_variation_.empty()) {
			type_ = base_type_;
		} else {
			type_ = base_type_->getVariation(current_variation_);
		}

		calculateSolidRect();

		handleEvent("set_variations");
	} else if(key == "attached_objects") {
		std::vector<EntityPtr> v;
		for(int n = 0; n != value.num_elements(); ++n) {
			Entity* e = value[n].try_convert<Entity>();
			if(e) {
				v.emplace_back(EntityPtr(e));
			}
		}

		setAttachedObjects(v);
	} else if(key == "solid_dimensions_in" || key == "solid_dimensions_not_in") {

		unsigned int solid = 0, weak = 0;
		for(int n = 0; n != value.num_elements(); ++n) {
			std::string str = value[n].as_string();
			if(!str.empty() && str[0] == '~') {
				str = std::string(str.begin() + 1, str.end());
				const int id = get_solid_dimension_id(str);
				weak |= 1 << id;
			} else {
				const int id = get_solid_dimension_id(value[n].as_string());
				solid |= 1 << id;
			}
		}

		if(key == "solid_dimensions_not_in") {
			solid = ~solid;
			weak = ~weak;
		}

		weak |= solid;

		const unsigned int old_solid = getSolidDimensions();
		const unsigned int old_weak = getWeakSolidDimensions();
		setSolidDimensions(solid, weak);
		CollisionInfo collide_info;
		if(entity_in_current_level(this) && entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE, &collide_info)) {
			setSolidDimensions(old_solid, old_weak);
			ASSERT_EQ(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE), false);

			game_logic::MapFormulaCallable* callable(new game_logic::MapFormulaCallable(this));
			callable->add("collide_with", variant(collide_info.collide_with.get()));
			game_logic::FormulaCallablePtr callable_ptr(callable);

			handleEvent(OBJECT_EVENT_CHANGE_SOLID_DIMENSIONS_FAIL, callable);
		}

	} else if(key == "xscale" || key == "yscale") {
		if(parallax_scale_millis_.get() == nullptr) {
			parallax_scale_millis_.reset(new std::pair<int,int>(1000,1000));
		}

		const int v = value.as_int();

		if(key == "xscale") {
			const int current = (parallax_scale_millis_->first*x())/1000;
			const int new_value = (v*current)/1000;
			setX(new_value);
			parallax_scale_millis_->first = v;
		} else {
			const int current = (parallax_scale_millis_->second*y())/1000;
			const int new_value = (v*current)/1000;
			setY(new_value);
			parallax_scale_millis_->second = v;
		}
	} else if(key == "type") {
		ConstCustomObjectTypePtr p = CustomObjectType::get(value.as_string());
		if(p) {
			game_logic::FormulaVariableStoragePtr old_vars = vars_, old_tmp_vars_ = tmp_vars_;

			getAll(base_type_->id()).erase(this);
			base_type_ = type_ = p;
			getAll(base_type_->id()).insert(this);
			has_feet_ = type_->hasFeet();
			vars_.reset(new game_logic::FormulaVariableStorage(type_->variables())),
			tmp_vars_.reset(new game_logic::FormulaVariableStorage(type_->tmpVariables())),
			vars_->setObjectName(getDebugDescription());
			tmp_vars_->setObjectName(getDebugDescription());

			vars_->add(*old_vars);
			tmp_vars_->add(*old_tmp_vars_);

			vars_->disallowNewKeys(type_->isStrict());
			tmp_vars_->disallowNewKeys(type_->isStrict());

			//set the animation to the default animation for the new type.
			setFrame(type_->defaultFrame().id());
		}
	} else if(key == "use_absolute_screen_coordinates") {
		use_absolute_screen_coordinates_ = value.as_bool();
	} else if(key == "mouseover_delay") {
		setMouseoverDelay(value.as_int());
#if defined(USE_BOX2D)
	} else if(key == "body") {
		body_.reset(new box2d::body(value));
		body_->finishLoading(this);
#endif
	} else if(key == "mouseover_area") {
		setMouseOverArea(rect(value));
	} else if(!type_->isStrict()) {
		vars_->add(key, value);
	} else {
		std::ostringstream known_properties;
		for(std::map<std::string, CustomObjectType::PropertyEntry>::const_iterator property_itor = type_->properties().begin(); property_itor != type_->properties().end(); ++property_itor) {
			known_properties << property_itor->first << ", ";
		}

		ASSERT_LOG(false, "ILLEGAL OBJECT ACCESS WITH STRICT CHECKING IN " << getDebugDescription() << ": " << key << " KNOWN PROPERTIES ARE: " << known_properties.str());
	}
}

void CustomObject::setValueBySlot(int slot, const variant& value)
{
	switch(slot) {
	case CUSTOM_OBJECT_DATA: {
		ASSERT_LOG(active_property_ >= 0, "Illegal access of 'data' in object when not in writable property");
		get_property_data(active_property_) = value;
		if(type_->getSlotProperties()[active_property_].is_weak) {
			get_property_data(active_property_).weaken();
		}

		//see if this initializes a property that requires dynamic
		//initialization and if so mark is as now initialized.
		for(auto itor = properties_requiring_dynamic_initialization_.begin(); itor != properties_requiring_dynamic_initialization_.end(); ++itor) {
			if(type_->getSlotProperties()[*itor].storage_slot == active_property_) {
				properties_requiring_dynamic_initialization_.erase(itor);
				break;
			}
		}
		
		break;
	}
	case CUSTOM_OBJECT_CREATED: {
		break;
	}
	case CUSTOM_OBJECT_TYPE: {
		ConstCustomObjectTypePtr p = CustomObjectType::get(value.as_string());
		if(p) {
			game_logic::FormulaVariableStoragePtr old_vars = vars_, old_tmp_vars_ = tmp_vars_;

			ConstCustomObjectTypePtr old_type = type_;

			getAll(base_type_->id()).erase(this);
			base_type_ = type_ = p;
			getAll(base_type_->id()).insert(this);
			has_feet_ = type_->hasFeet();
			vars_.reset(new game_logic::FormulaVariableStorage(type_->variables())),
			tmp_vars_.reset(new game_logic::FormulaVariableStorage(type_->tmpVariables())),
			vars_->setObjectName(getDebugDescription());
			tmp_vars_->setObjectName(getDebugDescription());

			vars_->add(*old_vars);
			tmp_vars_->add(*old_tmp_vars_);

			vars_->disallowNewKeys(type_->isStrict());
			tmp_vars_->disallowNewKeys(type_->isStrict());

			std::vector<variant> props = property_data_;
			property_data_.clear();
			
			for(auto i = type_->properties().begin(); i != type_->properties().end(); ++i) {
				if(i->second.storage_slot < 0) {
					continue;
				}

				get_property_data(i->second.storage_slot) = deep_copy_variant(i->second.default_value);
			}

			for(auto i = old_type->properties().begin(); i != old_type->properties().end(); ++i) {
				if(i->second.storage_slot < 0 || static_cast<unsigned>(i->second.storage_slot) >= props.size() || props[i->second.storage_slot] == i->second.default_value) {
					continue;
				}

				auto j = type_->properties().find(i->first);
				if(j == type_->properties().end() || j->second.storage_slot < 0) {
					continue;
				}

				get_property_data(j->second.storage_slot) = props[i->second.storage_slot];
				if(j->second.is_weak) { get_property_data(j->second.storage_slot).weaken(); }
			}

			//set the animation to the default animation for the new type.
			setFrame(type_->defaultFrame().id());
		}
	}
		break;
	case CUSTOM_OBJECT_TIME_IN_ANIMATION:
		ASSERT_GE(value.as_int(), 0);
		time_in_frame_ = value.as_int()%frame_->duration();
		break;
	case CUSTOM_OBJECT_TIME_IN_ANIMATION_DELTA:
		time_in_frame_delta_ = value.as_int();
		break;
	case CUSTOM_OBJECT_ANIMATION:
	case CUSTOM_OBJECT_ANIMATION_OBJ:
		if(value.is_string()) {
			setFrame(value.as_string());
		} else if(value.is_map()) {
			FramePtr f(new Frame(value));
			f->SetNeedsSerialization(true);
			if(type_->useImageForCollisions()) {
				f->setImageAsSolid();
			}
			setFrame(*f);
		} else {
			setFrame(*value.convert_to<Frame>());
		}
		break;

	case CUSTOM_OBJECT_ANIMATION_MAP: {
		FramePtr f(new Frame(value));
		if(type_->useImageForCollisions()) {
			f->setImageAsSolid();
		}
		setFrame(*f);
		break;
	}
	
	case CUSTOM_OBJECT_X1:
	case CUSTOM_OBJECT_X: {
		const int start_x = centiX();
		setX(value.as_int());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
		}

		break;
	}
	
	case CUSTOM_OBJECT_Y1:
	case CUSTOM_OBJECT_Y: {
		const int start_y = centiY();
		setY(value.as_int());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiY(start_y);
		}

		break;
	}

	case CUSTOM_OBJECT_X2: {
		const int start_x = centiX();
		const int current_x = solidRect().w() ? solidRect().x2() :
		                                         x() + getCurrentFrame().width();
		const int delta_x = value.as_int() - current_x;
		setX(x() + delta_x);
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) &&
		   entity_in_current_level(this)) {
			setCentiX(start_x);
		}
		break;
	}

	case CUSTOM_OBJECT_Y2: {
		const int start_y = centiY();
		const int current_y = solidRect().h() ? solidRect().y2() :
		                                         y() + getCurrentFrame().height();
		const int delta_y = value.as_int() - current_y;
		setY(y() + delta_y);
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) &&
		   entity_in_current_level(this)) {
			setCentiY(start_y);
		}
		break;
	}
	
	case CUSTOM_OBJECT_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		const int start_x = centiX();
		const int start_y = centiY();
		setX(value[0].as_int());
		setY(value[1].as_int());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
			setCentiY(start_y);
		}

		break;
	}

	case CUSTOM_OBJECT_Z:
	case CUSTOM_OBJECT_ZORDER:
		setZOrder(value.as_int());
		break;
		
	case CUSTOM_OBJECT_ZSUB_ORDER:
		setZSubOrder(value.as_int());
		break;
	
	case CUSTOM_OBJECT_RELATIVE_X: {
        relative_x_ = value.as_int();
		break;
	}

	case CUSTOM_OBJECT_RELATIVE_Y: {
		relative_y_ = value.as_int();
		break;
	}

	case CUSTOM_OBJECT_PARENT: {
		EntityPtr e(value.try_convert<Entity>());
		setParent(e, parent_pivot_);
		break;
	}

	case CUSTOM_OBJECT_PIVOT: {
		setParent(parent_, value.as_string());
		break;
	}
	
	case CUSTOM_OBJECT_MID_X:
	case CUSTOM_OBJECT_MIDPOINT_X: {
		//midpoint is, unlike IMG_MID or SOLID_MID, meant to be less-rigorous, but more convenient; it default to basing the "midpoint" on solidity, but drops down to using img_mid if there is no solidity.  The rationale is that generally it doesn't matter which it comes from, and our form of failure (which is silent and returns just x1) is sneaky and can be very expensive because it can take a long time to realize that the value returned actually means the object doesn't have a midpoint for that criteria.  If you need to be rigorous, always use IMG_MID and SOLID_MID.
		const int start_x = centiX();
			
		const int solid_diff_x = solidRect().x() - x();
		const int current_x = solidRect().w() ? (x() + solid_diff_x + solidRect().w()/2) : x() + getCurrentFrame().width()/2;

		const int xdiff = current_x - x();
		setPos(value.as_int() - xdiff, y());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
		}
		break;
	}

	case CUSTOM_OBJECT_MID_Y:
	case CUSTOM_OBJECT_MIDPOINT_Y: {
		const int start_y = centiY();
		
		const int solid_diff_y = solidRect().y() - y();
		const int current_y = solidRect().h() ? (y() + solid_diff_y + solidRect().h()/2) : y() + getCurrentFrame().height()/2;

		const int ydiff = current_y - y();
		setPos(x(), value.as_int() - ydiff);
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiY(start_y);
		}
		break;
	}

	case CUSTOM_OBJECT_MID_XY:
	case CUSTOM_OBJECT_MIDPOINT_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set midpoint_xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		const int start_x = centiX();
		const int solid_diff_x = solidRect().x() - x();
		const int current_x = solidRect().w() ? (x() + solid_diff_x + solidRect().w()/2) : x() + getCurrentFrame().width()/2;
		const int xdiff = current_x - x();

		const int start_y = centiY();
		const int solid_diff_y = solidRect().y() - y();
		const int current_y = solidRect().h() ? (y() + solid_diff_y + solidRect().h()/2) : y() + getCurrentFrame().height()/2;
		const int ydiff = current_y - y();

		setPos(value[0].as_int() - xdiff, value[1].as_int() - ydiff);
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
			setCentiY(start_y);
		}
		break;
	}

	case CUSTOM_OBJECT_ANCHORX: {
		decimal d = value.as_decimal(decimal::from_int(-1));
		setAnchorX(d);
		break;
	}

	case CUSTOM_OBJECT_ANCHORY: {
		decimal d = value.as_decimal(decimal::from_int(-1));
		setAnchorY(d);
		break;
	}

	case CUSTOM_OBJECT_SOLID_RECT: {
		ASSERT_LOG(false, "Cannot set immutable solid_rect");
		break;
	}

	case CUSTOM_OBJECT_SOLID_MID_X: {
		const int start_x = centiX();
		const int solid_diff = solidRect().x() - x();
		const int current_x = x() + solid_diff + solidRect().w()/2;
		const int xdiff = current_x - x();
		setPos(value.as_int() - xdiff, y());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
		}
		break;
	}
			
	case CUSTOM_OBJECT_SOLID_MID_Y: {
		const int start_y= centiY();
		const int solid_diff = solidRect().y() - y();
		const int current_y = y() + solid_diff + solidRect().h()/2;
		const int ydiff = current_y - y();
		setPos(x(), value.as_int() - ydiff);
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiY(start_y);
		}
		break;
	}

	case CUSTOM_OBJECT_SOLID_MID_XY: {
		const int start_x = centiX();
		const int solid_diff_x = solidRect().x() - x();
		const int current_x = x() + solid_diff_x + solidRect().w()/2;
		const int xdiff = current_x - x();
		const int start_y= centiY();
		const int solid_diff_y = solidRect().y() - y();
		const int current_y = y() + solid_diff_y + solidRect().h()/2;
		const int ydiff = current_y - y();
		setPos(value[0].as_int() - xdiff, value[1].as_int() - ydiff);
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
			setCentiY(start_y);
		}
		break;
	}

	case CUSTOM_OBJECT_IMG_MID_X: {
		const int start_x = centiX();
		const int current_x = x() + getCurrentFrame().width()/2;
		const int xdiff = current_x - x();
		setPos(value.as_int() - xdiff, y());
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
		}
		break;
	}
		
	case CUSTOM_OBJECT_IMG_MID_Y: {
		const int start_y = centiY();
		const int current_y = y() + getCurrentFrame().height()/2;
		const int ydiff = current_y - y();
		setPos(x(), value.as_int() - ydiff);
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiY(start_y);
		}
		break;
	}
		
	case CUSTOM_OBJECT_IMG_MID_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set midpoint_xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		const int start_x = centiX();
		const int current_x = x() + getCurrentFrame().width()/2;
		const int xdiff = current_x - x();
		const int start_y = centiY();
		const int current_y = y() + getCurrentFrame().height()/2;
		const int ydiff = current_y - y();
		setPos(value[0].as_int() - xdiff, value[1].as_int() - ydiff);
		if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
			setCentiX(start_x);
			setCentiY(start_y);
		}
		break;
	}
			
	case CUSTOM_OBJECT_CYCLE:
		cycle_ = value.as_int();
		break;

	case CUSTOM_OBJECT_FACING:
		setFacingRight(value.as_int() > 0);
		break;
	
	case CUSTOM_OBJECT_UPSIDE_DOWN:
		setUpsideDown(value.as_int() > 0);
		break;

	case CUSTOM_OBJECT_HITPOINTS: {
		const int old_hitpoints = hitpoints_;
		hitpoints_ = value.as_int();
		if(old_hitpoints > 0 && hitpoints_ <= 0) {
			die();
		}
		break;
	}
	case CUSTOM_OBJECT_MAX_HITPOINTS:
		max_hitpoints_ = value.as_int() - type_->getHitpoints();
		if(hitpoints_ > type_->getHitpoints() + max_hitpoints_) {
			hitpoints_ = type_->getHitpoints() + max_hitpoints_;
		}
		break;

	case CUSTOM_OBJECT_VELOCITY_X:
		velocity_x_ = value.as_decimal();
		break;
	
	case CUSTOM_OBJECT_VELOCITY_Y:
		velocity_y_ = value.as_decimal();
		break;
	
	case CUSTOM_OBJECT_VELOCITY_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set velocity_xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		velocity_x_ = value[0].as_decimal();
		velocity_y_ = value[1].as_decimal();
		break;
	}

	case CUSTOM_OBJECT_VELOCITY_MAGNITUDE: {
		break;
	}
	case CUSTOM_OBJECT_VELOCITY_ANGLE: {
		const double radians = value.as_decimal().as_float()/radians_to_degrees;
		const decimal magnitude = calculate_velocity_magnitude(velocity_x_, velocity_y_);
		const decimal xval = magnitude*decimal(cos(radians));
		const decimal yval = magnitude*decimal(sin(radians));
		velocity_x_ = (xval*1000);
		velocity_y_ = (yval*1000);
		break;
	}
	case CUSTOM_OBJECT_ACCEL_X:
		accel_x_ = value.as_decimal();
		break;

	case CUSTOM_OBJECT_ACCEL_Y:
		accel_y_ = value.as_decimal();
		break;

	case CUSTOM_OBJECT_ACCEL_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set accel_xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		accel_x_ = value[0].as_decimal();
		accel_y_ = value[1].as_decimal();
		break;
	}

	case CUSTOM_OBJECT_GRAVITY_SHIFT:
		gravity_shift_ = value.as_int();
		break;

	case CUSTOM_OBJECT_PLATFORM_MOTION_X:
		setPlatformMotionX(value.as_int());
		break;

	case CUSTOM_OBJECT_ROTATE:
		setRotateZ(value.as_decimal());
		break;

	case CUSTOM_OBJECT_RED:
		make_draw_color();
		draw_color_->setAddRed(value.as_int());
		break;
	
	case CUSTOM_OBJECT_GREEN:
		make_draw_color();
		draw_color_->setAddGreen(value.as_int());
		break;
	
	case CUSTOM_OBJECT_BLUE:
		make_draw_color();
		draw_color_->setAddBlue(value.as_int());
		break;

	case CUSTOM_OBJECT_ALPHA:
		make_draw_color();
		draw_color_->setAddAlpha(value.as_int());
		break;

	case CUSTOM_OBJECT_TEXT_ALPHA:
		if(!text_) {
			setText("", "default", 10, false);
		}

		text_->alpha = value.as_int();
		break;

	case CUSTOM_OBJECT_BRIGHTNESS:
		make_draw_color();
		draw_color_->setAddRed(value.as_int());
		draw_color_->setAddGreen(value.as_int());
		draw_color_->setAddBlue(value.as_int());
		break;
	
	case CUSTOM_OBJECT_CURRENTGENERATOR:
		setCurrentGenerator(value.try_convert<CurrentGenerator>());
		break;

	case CUSTOM_OBJECT_INVINCIBLE:
		invincible_ = value.as_int();
		break;
	
	case CUSTOM_OBJECT_FALL_THROUGH_PLATFORMS:
		fall_through_platforms_ = value.as_int();
		break;
	
	case CUSTOM_OBJECT_HAS_FEET:
		has_feet_ = value.as_bool();
		break;
	
	case CUSTOM_OBJECT_TAGS:
		if(value.is_list()) {
			tags_ = new game_logic::MapFormulaCallable;
			for(int n = 0; n != value.num_elements(); ++n) {
				tags_->add(value[n].as_string(), variant(1));
			}
		}

		break;

	case CUSTOM_OBJECT_SHADER: {
		if(value.is_string()) {
			shader_.reset(new graphics::AnuraShader(value.as_string()));
		} else {
			shader_.reset(new graphics::AnuraShader(value["name"].as_string(), value));
		}
		break;
	}

	case CUSTOM_OBJECT_EFFECTS: {
		effects_shaders_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_callable()) {
				graphics::AnuraShader* shader = value[n].try_convert<graphics::AnuraShader>();
				ASSERT_LOG(shader, "Bad object type given as shader");
				effects_shaders_.emplace_back(shader);
			} else if(value[n].is_string()) {
				effects_shaders_.emplace_back(new graphics::AnuraShader(value[n].as_string()));
			} else {
				effects_shaders_.emplace_back(new graphics::AnuraShader(value[n]["name"].as_string(), value["name"]));
			}
		}
		break;
	}

	case CUSTOM_OBJECT_DOCUMENT: {
		document_.reset(new xhtml::DocumentObject(value));
		document_->init(this);
		break;
	}

	case CUSTOM_OBJECT_DRAW_AREA:
		if(value.is_list() && value.num_elements() == 4) {
			draw_area_.reset(new rect(value[0].as_int(), value[1].as_int(), value[2].as_int(), value[3].as_int()));
		} else {
			draw_area_.reset();
		}

		break;

	case CUSTOM_OBJECT_SCALE:
		draw_scale_.reset(new decimal(value.as_decimal()));
		if(draw_scale_->as_int() == 1 && draw_scale_->fractional() == 0) {
			draw_scale_.reset();
		}

		break;
	
	case CUSTOM_OBJECT_ACTIVATION_BORDER:
		activation_border_ = value.as_int();
	
		break;

			
	case CUSTOM_OBJECT_ACTIVATION_AREA:
		if(value.is_list() && value.num_elements() == 4) {
			activation_area_.reset(new rect(value[0].as_int(), value[1].as_int(), value[2].as_int(), value[3].as_int()));
		} else {
			ASSERT_LOG(value.is_null(), "BAD ACTIVATION AREA: " << value.to_debug_string());
			activation_area_.reset();
		}

		break;
	
	case CUSTOM_OBJECT_CLIPAREA:
		if(value.is_list() && value.num_elements() == 4) {
			clip_area_.reset(new rect(value[0].as_int(), value[1].as_int(), value[2].as_int(), value[3].as_int()));
		} else {
			ASSERT_LOG(value.is_null(), "BAD CLIP AREA: " << value.to_debug_string());
			clip_area_.reset();
		}

		break;

	case CUSTOM_OBJECT_CLIPAREA_ABSOLUTE:
		clip_area_absolute_ = value.as_bool();
		break;

	case CUSTOM_OBJECT_ALWAYS_ACTIVE:
		always_active_ = value.as_bool();
		break;
			
	case CUSTOM_OBJECT_VARIATIONS:
		handleEvent("reset_variations");
		current_variation_.clear();
		if(value.is_list()) {
			for(int n = 0; n != value.num_elements(); ++n) {
				current_variation_.emplace_back(value[n].as_string());
			}
		} else if(value.is_string()) {
			current_variation_.emplace_back(value.as_string());
		}

		if(current_variation_.empty()) {
			type_ = base_type_;
		} else {
			type_ = base_type_->getVariation(current_variation_);
		}

		calculateSolidRect();
		handleEvent("set_variations");
		break;

	case CUSTOM_OBJECT_PARALLAX_SCALE_X: {
		parallax_scale_millis_.reset(new std::pair<int, int>(static_cast<int>(value.as_float()*1000), parallaxScaleMillisY()));
		break;
	}

	case CUSTOM_OBJECT_PARALLAX_SCALE_Y: {
		parallax_scale_millis_.reset(new std::pair<int, int>(parallaxScaleMillisX(), static_cast<int>(value.as_float()*1000)));
		break;
	}
	
	case CUSTOM_OBJECT_ATTACHED_OBJECTS: {
		std::vector<EntityPtr> v;
		for(int n = 0; n != value.num_elements(); ++n) {
			Entity* e = value[n].try_convert<Entity>();
			if(e) {
				v.emplace_back(EntityPtr(e));
			}

			//this will initialize shaders and such, which is
			//desired for attached objects
			e->addToLevel();
			e->createObject();
		}

		setAttachedObjects(v);
		break;
	}

	case CUSTOM_OBJECT_COLLIDE_DIMENSIONS_IN:
	case CUSTOM_OBJECT_COLLIDE_DIMENSIONS_NOT_IN: {
		unsigned int solid = 0, weak = 0;
		for(int n = 0; n != value.num_elements(); ++n) {
			std::string str = value[n].as_string();
			if(!str.empty() && str[0] == '~') {
				str = std::string(str.begin() + 1, str.end());
				const int id = get_solid_dimension_id(str);
				weak |= 1 << id;
			} else {
				const int id = get_solid_dimension_id(value[n].as_string());
				solid |= 1 << id;
			}
		}

		if(slot == CUSTOM_OBJECT_COLLIDE_DIMENSIONS_NOT_IN) {
			solid = ~solid;
			weak = ~weak;
		}

		weak |= solid;

		setCollideDimensions(solid, weak);
		break;
	}

	case CUSTOM_OBJECT_LIGHTS: {
		lights_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			Light* p = value[n].try_convert<Light>();
			if(p) {
				lights_.emplace_back(LightPtr(p));
			}
		}
		break;
	}

	case CUSTOM_OBJECT_SOLID_DIMENSIONS_IN:
	case CUSTOM_OBJECT_SOLID_DIMENSIONS_NOT_IN: {
		unsigned int solid = 0, weak = 0;
		for(int n = 0; n != value.num_elements(); ++n) {
			std::string str = value[n].as_string();
			if(!str.empty() && str[0] == '~') {
				str = std::string(str.begin() + 1, str.end());
				const int id = get_solid_dimension_id(str);
				weak |= 1 << id;
			} else {
				const int id = get_solid_dimension_id(value[n].as_string());
				solid |= 1 << id;
			}
		}

		if(slot == CUSTOM_OBJECT_SOLID_DIMENSIONS_NOT_IN) {
			solid = ~solid;
			weak = ~weak;
		}

		weak |= solid;

		const unsigned int old_solid = getSolidDimensions();
		const unsigned int old_weak = getWeakSolidDimensions();
		setSolidDimensions(solid, weak);
		CollisionInfo collide_info;
		if(entity_in_current_level(this) && entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE, &collide_info)) {
			setSolidDimensions(old_solid, old_weak);
			ASSERT_EQ(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE), false);

			game_logic::MapFormulaCallable* callable(new game_logic::MapFormulaCallable(this));
			callable->add("collide_with", variant(collide_info.collide_with.get()));
			game_logic::FormulaCallablePtr callable_ptr(callable);

			handleEvent(OBJECT_EVENT_CHANGE_SOLID_DIMENSIONS_FAIL, callable);
		}

		break;
	}

	case CUSTOM_OBJECT_COLLIDES_WITH_LEVEL: {
		bool starting_value = collides_with_level_;
		collides_with_level_ = value.as_bool();
		
		CollisionInfo collide_info;
		if(entity_in_current_level(this) && entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE, &collide_info)) {
			collides_with_level_ = starting_value;

			ASSERT_EQ(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE), false);
			handleEvent(OBJECT_EVENT_CHANGE_SOLID_DIMENSIONS_FAIL);
		}

		break;
	}

	case CUSTOM_OBJECT_X_SCHEDULE: {
		if(position_schedule_.get() == nullptr) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->x_pos.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			position_schedule_->x_pos.emplace_back(value[n].as_int());
		}
		break;
	}
	case CUSTOM_OBJECT_Y_SCHEDULE: {
		if(position_schedule_.get() == nullptr) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->y_pos.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			position_schedule_->y_pos.emplace_back(value[n].as_int());
		}
		break;
	}
	case CUSTOM_OBJECT_ROTATION_SCHEDULE: {
		if(position_schedule_.get() == nullptr) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->rotation.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			position_schedule_->rotation.emplace_back(value[n].as_decimal());
		}
		break;
	}

	case CUSTOM_OBJECT_SCHEDULE_SPEED: {
		if(position_schedule_.get() == nullptr) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->speed = value.as_int();

		break;
	}

	case CUSTOM_OBJECT_SCHEDULE_EXPIRES: {
		if(position_schedule_.get() == nullptr) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->expires = true;
		break;
	}

	case CUSTOM_OBJECT_PLATFORM_AREA: {
		if(value.is_null()) {
			platform_area_.reset();
			platform_solid_info_ = ConstSolidInfoPtr();
			calculateSolidRect();
			break;
		} else if(value.is_list() && value.num_elements() == 0) {
			set_platform_area(rect());
			break;
		}

		ASSERT_GE(value.num_elements(), 3);
		ASSERT_LE(value.num_elements(), 4);

		set_platform_area(rect(value));
		break;
	}

	case CUSTOM_OBJECT_PLATFORM_OFFSETS: {
		platform_offsets_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			platform_offsets_.emplace_back(value[n].as_int());
		}
		break;
	}

	case CUSTOM_OBJECT_USE_ABSOLUTE_SCREEN_COORDINATES: {
		use_absolute_screen_coordinates_ = value.as_bool();
		break;
	}

	case CUSTOM_OBJECT_WIDGETS:
	case CUSTOM_OBJECT_WIDGET_LIST: {
		std::vector<gui::WidgetPtr> w;
		clearWidgets();
		if(value.is_list()) {
			for(const variant& v : value.as_list()) {
				w.emplace_back(widget_factory::create(v, this));
			}
		} else {
			w.emplace_back(widget_factory::create(value, this));
		}
		addWidgets(&w);
		break;
	}

	case CUSTOM_OBJECT_MOUSEOVER_DELAY: {
		setMouseoverDelay(value.as_int());
		break;
	}

	case CUSTOM_OBJECT_MOUSEOVER_AREA: {
		setMouseOverArea(rect(value));
		break;
	}

	case CUSTOM_OBJECT_ANIMATED_MOVEMENTS: {
		break;
	}

	case CUSTOM_OBJECT_CTRL_USER_OUTPUT: {
		controls::set_user_ctrl_output(value);
		break;
	}

#if defined(USE_BOX2D)
	case CUSTOM_OBJECT_BODY: {
		body_.reset(new box2d::body(value));
		body_->finishLoading(this);
		break;
	}
#endif

	case CUSTOM_OBJECT_PAUSED: {
		paused_ = value.as_bool();
		handleEvent("paused");
		break;
	}

	case CUSTOM_OBJECT_CUSTOM_DRAW: {
		// XXX Figure this mess out
		if(value.is_null()) {
			custom_draw_.reset();
		}

		std::vector<Frame::CustomPoint>* v = new std::vector<Frame::CustomPoint>;

		custom_draw_.reset(v);

		std::vector<float> positions;

		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_decimal() || value[n].is_int()) {
				positions.emplace_back(float(value[n].as_decimal().as_float()));
			} else if(value[n].is_list()) {
				for(int index = 0; index != value[n].num_elements(); index += 2) {
					ASSERT_LOG(value[n].num_elements() - index >= 2, "ILLEGAL VALUE TO custom_draw: " << value.to_debug_string() << ", " << n << ", " << index << "/" << value[n].num_elements());

					ASSERT_LOG(v->size() < positions.size(), "ILLEGAL VALUE TO custom_draw -- not enough positions for number of offsets: " << value.to_debug_string() << " " << v->size() << " VS " << positions.size());
					const float pos = positions[v->size()];

					v->emplace_back(Frame::CustomPoint());
					v->back().pos = pos;
					v->back().offset = point(value[n][index].as_int(), value[n][index + 1].as_int());
				}
			}
		}

		ASSERT_LOG(v->size() >= 3, "ILLEGAL VALUE TO custom_draw: " << value.to_debug_string());

		std::vector<Frame::CustomPoint> draw_order;
		int n1 = 0, n2 = static_cast<int>(v->size()) - 1;
		while(n1 <= n2) {
			draw_order.emplace_back((*v)[n1]);
			if(n2 > n1) {
				draw_order.emplace_back((*v)[n2]);
			}

			++n1;
			--n2;
		}

		v->swap(draw_order);

		break;
	}

	case CUSTOM_OBJECT_UV_ARRAY: {
		if(value.is_null()) {
			custom_draw_uv_.clear();
		} else {
			custom_draw_uv_.clear();
			for(const variant& v : value.as_list()) {
				custom_draw_uv_.emplace_back(v.as_float());
			}
		}
		break;
	}

	case CUSTOM_OBJECT_XY_ARRAY: {
		if(value.is_null()) {
			custom_draw_xy_.clear();
		} else {
			custom_draw_xy_.clear();
			for(const variant& v : value.as_list()) {
				custom_draw_xy_.emplace_back(v.as_float());
			}
		}
		break;
	}

	case CUSTOM_OBJECT_EVENT_HANDLERS: {
		const event_handlers_callable* callable = value.try_convert<const event_handlers_callable>();
		ASSERT_LOG(callable, "Tried to set event_handlers to an illegal value: " << value.write_json());
		event_handlers_ = callable->obj().event_handlers_;

		break;
	}

	case CUSTOM_OBJECT_UV_SEGMENTS: {
		const std::vector<variant>& items = value.as_list();
		ASSERT_LOG(items.size() == 2, "Invalid value passed to uv_segments: " << value.write_json() << ". Requires [int,int]");
		const int xdim = items[0].as_int() + 2;
		const int ydim = items[1].as_int() + 2;

		custom_draw_xy_.clear();
		custom_draw_uv_.clear();

		for(int ypos = 0; ypos < ydim-1; ++ypos) {
			const float y = static_cast<float>(ypos)/static_cast<float>(ydim-1);
			const float y2 = static_cast<float>(ypos+1)/static_cast<float>(ydim-1);
			for(int xpos = 0; xpos < xdim; ++xpos) {
				const float x = static_cast<float>(xpos)/static_cast<float>(xdim-1);

				if(xpos == 0 && ypos > 0) {
					custom_draw_uv_.emplace_back(x);
					custom_draw_uv_.emplace_back(y);
				}

				custom_draw_uv_.emplace_back(x);
				custom_draw_uv_.emplace_back(y);
				custom_draw_uv_.emplace_back(x);
				custom_draw_uv_.emplace_back(y2);

				if(xpos == xdim-1 && ypos != ydim-2) {
					custom_draw_uv_.emplace_back(x);
					custom_draw_uv_.emplace_back(y2);
				}
			}
		}

		custom_draw_xy_ = custom_draw_uv_;
		break;
	}

	case CUSTOM_OBJECT_DRAWPRIMITIVES: {
		draw_primitives_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_callable()) {
				ffl::IntrusivePtr<graphics::DrawPrimitive> obj(value[n].try_convert<graphics::DrawPrimitive>());
				ASSERT_LOG(obj.get() != nullptr, "BAD OBJECT PASSED WHEN SETTING DrawPrimitives");
				draw_primitives_.emplace_back(obj);
			} else if(!value[n].is_null()) {
				draw_primitives_.emplace_back(graphics::DrawPrimitive::create(value[n]));
			}
		}
		break;
	}

	case CUSTOM_OBJECT_PARTICLES: {
		createParticles(value);
		break;
	}

	case CUSTOM_OBJECT_BLUR: {
		blur_objects_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_callable()) {
				ffl::IntrusivePtr<BlurObject> obj(value[n].try_convert<BlurObject>());
				obj->setObject(this);
				ASSERT_LOG(obj.get() != nullptr, "BAD OBJECT PASSED WHEN SETTING BLUR");
				blur_objects_.emplace_back(obj);
			} else {
				ASSERT_LOG(false, "BAD OBJECT PASSED WHEN SETTING BLUR");
			}
		}

		break;
	}

	default:
		if(slot >= type_->getSlotPropertiesBase() && (size_t(slot - type_->getSlotPropertiesBase()) < type_->getSlotProperties().size())) {
			const CustomObjectType::PropertyEntry& e = type_->getSlotProperties()[slot - type_->getSlotPropertiesBase()];
			ASSERT_LOG(!e.const_value, "Attempt to set const property: " << getDebugDescription() << "." << e.id);
			if(e.setter) {

				if(e.set_type) {
					ASSERT_LOG(e.set_type->match(value), "Setting " << getDebugDescription() << "." << e.id << " to illegal value " << value.write_json() << " of type " << get_variant_type_from_value(value)->to_string() << " expected type " << e.set_type->to_string());
				}

				ActivePropertyScope scope(*this, e.storage_slot, &value);
				variant value = e.setter->execute(*this);
				executeCommand(value);
			} else if(e.storage_slot >= 0) {
				variant& target = get_property_data(e.storage_slot);
				const bool do_on_change = (e.onchange && value != target);
				target = value;

				if(do_on_change) {
					ActivePropertyScope scope(*this, e.storage_slot, &value);
					variant value = e.onchange->execute(*this);
					executeCommand(value);
				}
				if(e.is_weak) { get_property_data(e.storage_slot).weaken(); }
			} else {
				ASSERT_LOG(false, "Attempt to set const property: " << getDebugDescription() << "." << e.id);
			}

			if(!properties_requiring_dynamic_initialization_.empty()) {
				std::vector<int>::iterator itor = std::find(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), slot - type_->getSlotPropertiesBase());
				if(itor != properties_requiring_dynamic_initialization_.end()) {
					properties_requiring_dynamic_initialization_.erase(itor);
				}
			}
		}
		break;

		case CUSTOM_OBJECT_PLAYER_DIFFICULTY:
		case CUSTOM_OBJECT_PLAYER_CAN_INTERACT:
		case CUSTOM_OBJECT_PLAYER_UNDERWATER_CONTROLS:
		case CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEY:
		case CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEYS:
		case CUSTOM_OBJECT_PLAYER_CTRL_KEYS:
		case CUSTOM_OBJECT_PLAYER_CTRL_PREV_KEYS:
		case CUSTOM_OBJECT_PLAYER_CTRL_MICE:
		case CUSTOM_OBJECT_PLAYER_CTRL_TILT:
		case CUSTOM_OBJECT_PLAYER_CTRL_X:
		case CUSTOM_OBJECT_PLAYER_CTRL_Y:
		case CUSTOM_OBJECT_PLAYER_CONTROL_SCHEME:
		case CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK:
		case CUSTOM_OBJECT_PLAYER_CONTROL_LOCK:
			setPlayerValueBySlot(slot, value);
		break;
	}
}

void CustomObject::setFrame(const std::string& name)
{
	setFrame(type_->getFrame(name));
}

void CustomObject::setFrame(const Frame& new_frame)
{
	const std::string& name = new_frame.id();
	const std::string previous_animation = frame_name_;

	const bool changing_anim = name != frame_name_;

	//fire an event to say that we're leaving the current frame.
	if(frame_ && changing_anim) {
		handleEvent(frame_->leaveEventId());
	}

	const int start_x = getFeetX();
	const int start_y = getFeetY();

	frame_.reset(&new_frame);
	calculateSolidRect();
	++current_animation_id_;

	const int diff_x = getFeetX() - start_x;
	const int diff_y = getFeetY() - start_y;

	if(type_->adjustFeetOnAnimationChange()) {
		moveCentipixels(-diff_x*100, -diff_y*100);
	}

	setFrameNoAdjustments(new_frame);

	if(g_play_sound_function.empty() == false) {
		if(frame_->getSounds().empty() == false) {
			const std::string& sound = frame_->getSounds()[rand()%frame_->getSounds().size()];

			static game_logic::FormulaPtr ffl(new game_logic::Formula(variant(g_play_sound_function), &get_custom_object_functions_symbol_table()));

			game_logic::MapFormulaCallablePtr callable(new game_logic::MapFormulaCallable);
			callable->add("sound", variant(sound));

			BackupCallableStackScope callable_scope(&backup_callable_stack_, callable.get());

			variant cmd = ffl->execute(*this);
			executeCommand(cmd);
		}
	} else {
		frame_->playSound(this);
	}

	if(entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE) && entity_in_current_level(this)) {
		game_logic::MapFormulaCallable* callable(new game_logic::MapFormulaCallable);
		callable->add("previous_animation", variant(previous_animation));
		game_logic::FormulaCallablePtr callable_ptr(callable);
		static int change_animation_failure_recurse = 0;
		ASSERT_LOG(change_animation_failure_recurse < 5, "OBJECT " << type_->id() << " FAILS TO RESOLVE ANIMATION CHANGE FAILURES");
		++change_animation_failure_recurse;
		handleEvent(OBJECT_EVENT_CHANGE_ANIMATION_FAILURE, callable);
		handleEvent("change_animation_failure_" + frame_name_, callable);
		--change_animation_failure_recurse;
		ASSERT_LOG(destroyed() || !entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE),
		  "Object '" << type_->id() << "' has different solid areas when changing from frame " << previous_animation << " to " << frame_name_ << " and doesn't handle it properly");
	}

	handleEvent(OBJECT_EVENT_ENTER_ANIM);
	handleEvent(frame_->enterEventId());
}

rect CustomObject::getDrawRect() const
{
	if(draw_area_) {
		return rect(x(), y(), draw_area_->w()*2, draw_area_->h()*2);
	} else {
		return rect(x(), y(), frame_->width(), frame_->height());
	}
}

void CustomObject::setFrameNoAdjustments(const std::string& name)
{
	setFrameNoAdjustments(type_->getFrame(name));
}

void CustomObject::setFrameNoAdjustments(const Frame& new_frame)
{
	frame_.reset(&new_frame);
	frame_name_ = new_frame.id();
	time_in_frame_ = 0;
	if(frame_->velocityX() != std::numeric_limits<int>::min()) {
		velocity_x_ = decimal::from_int(frame_->velocityX() * (isFacingRight() ? 1 : -1));
	}

	if(frame_->velocityY() != std::numeric_limits<int>::min()) {
		velocity_y_ = decimal::from_int(frame_->velocityY());
	}

	if(frame_->accelX() != std::numeric_limits<int>::min()) {
		accel_x_ = decimal::from_int(frame_->accelX());
	}
	
	if(frame_->accelY() != std::numeric_limits<int>::min()) {
		accel_y_ = decimal::from_int(frame_->accelY());
	}

	calculateSolidRect();
}

void CustomObject::die()
{
	hitpoints_ = 0;
	handleEvent(OBJECT_EVENT_DIE);

#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active(false);
	}
#endif
}

void CustomObject::dieWithNoEvent()
{
	hitpoints_ = 0;

#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active(false);
	}
#endif
}


bool CustomObject::isActive(const rect& screen_area) const
{
	if(controls::num_players() > 1) {
		//in multiplayer, make all objects always active
		//TODO: review this behavior
		return true;
	}

	if(isAlwaysActive()) {
		return true;
	}

	if(type_->goesInactiveOnlyWhenStanding() && isStanding(Level::current()) == STANDING_STATUS::NOT_STANDING) {
		return true;
	}

	if(activation_area_) {
		return rects_intersect(*activation_area_, screen_area);
		//Can we wrap activation area in a call to rotated_scaled_rect_bounds(*activation_area_, rotate_z_.as_float32(), draw_scale_->as_float32());
	}

	if(text_) {
		const rect text_area(x(), y(), text_->dimensions.w(), text_->dimensions.h());
		if(rects_intersect(screen_area, text_area)) {
			return true;
		}
	}

	const rect& area = frameRect();
	if(draw_area_) {
		rect draw_area(area.x(), area.y(), draw_area_->w()*2, draw_area_->h()*2);
		return rects_intersect(draw_area, screen_area);
	}
	
	if(parallax_scale_millis_.get() != nullptr) {
		if(parallax_scale_millis_->first != 1000 || parallax_scale_millis_->second != 1000){
			const int diffx = ((parallax_scale_millis_->first - 1000)*screen_area.x())/1000;
			const int diffy = ((parallax_scale_millis_->second - 1000)*screen_area.y())/1000;
			rect screen(screen_area.x() - diffx, screen_area.y() - diffy,
						screen_area.w(), screen_area.h());
			const rect& area = frameRect();
			return rects_intersect(screen, area);
		}
	}

	const int border = activation_border_;
	if(area.x() < screen_area.x2() + border &&
	   area.x2() > screen_area.x() - border &&
	   area.y() < screen_area.y2() + border &&
	   area.y2() > screen_area.y() - border) {
		return true;
	}

	
	return false;
}

bool CustomObject::moveToStanding(Level& lvl, int max_displace)
{
	int start_y = y();
	const bool result = moveToStandingInternal(lvl, max_displace);
	if(!result || entity_collides(Level::current(), *this, MOVE_DIRECTION::NONE)) {
		setPos(x(), start_y);
		return false;
	}

	return result;
}

bool CustomObject::moveToStandingInternal(Level& lvl, int max_displace)
{
	int start_y = y();
	//descend from the initial-position (what the player was at in the prev level) until we're standing
	for(int n = 0; n != max_displace; ++n) {
		if(isStanding(lvl) != STANDING_STATUS::NOT_STANDING) {
			
			if(n == 0) {  //if we've somehow managed to be standing on the very first frame, try to avoid the possibility that this is actually some open space underground on a cave level by scanning up till we reach the surface.
				for(int n = 0; n != max_displace; ++n) {
					setPos(x(), y() - 1);
					if(isStanding(lvl) == STANDING_STATUS::NOT_STANDING) {
						setPos(x(), y() + 1);
						
						if(y() < lvl.boundaries().y()) {
							//we are too high, out of the level. Move the
							//character down, under the solid, and then
							//call this function again to move them down
							//to standing on the solid below.
							for(int n = 0; n != max_displace; ++n) {
								setPos(x(), y() + 1);
								if(isStanding(lvl) == STANDING_STATUS::NOT_STANDING) {
									return moveToStandingInternal(lvl, max_displace);
								}
							}
						}
						
						return true;
					}
				}
				return true;
			}
			return true;
		}
		
		setPos(x(), y() + 1);
	}
	
	setPos(x(), start_y);
	return false;
}


bool CustomObject::diesOnInactive() const
{
	return type_->diesOnInactive();
}

bool CustomObject::isAlwaysActive() const
{
	return always_active_ || type_->isAlwaysActive();
}

bool CustomObject::isBodyHarmful() const
{
	return type_->isBodyHarmful();
}

bool CustomObject::isBodyPassthrough() const
{
	return type_->isBodyPassthrough();
}

const Frame& CustomObject::getIconFrame() const
{
	return type_->defaultFrame();
}

EntityPtr CustomObject::clone() const
{
	EntityPtr res(new CustomObject(*this));
	res->setDistinctLabel();
	return res;
}

EntityPtr CustomObject::backup() const
{
	if(type_->stateless()) {
		return EntityPtr(const_cast<CustomObject*>(this));
	}

	EntityPtr res(new CustomObject(*this));
	return res;
}

bool CustomObject::handleEvent(const std::string& event, const FormulaCallable* context)
{
	return handleEvent(get_object_event_id(event), context);
}

bool CustomObject::handleEventDelay(int event, const FormulaCallable* context)
{
	return handleEventInternal(event, context, false);
}

namespace 
{
	void run_expression_for_edit_and_continue(std::function<bool()> fn, bool* success, bool* res)
	{
		*success = false;
		*res = fn();
		*success = true;
	}
}

bool CustomObject::handleEvent(int event, const FormulaCallable* context)
{
	if(preferences::edit_and_continue()) {
		try {
			ConstCustomObjectTypePtr type_back = type_;
			ConstCustomObjectTypePtr base_type_back = base_type_;
			assert_edit_and_continue_fn_scope scope([&](){ this->handleEventInternal(event, context, true); });
			return handleEventInternal(event, context);
		} catch(validation_failure_exception&) {
			return true;
		}
	} else {
		try {
			return handleEventInternal(event, context);
		} catch(validation_failure_exception& e) {
			if(Level::current().in_editor()) {
				return true;
			}

			throw e;
		}
	}
}

namespace 
{
	struct DieEventScope 
	{
		int event_;
		int& flag_;
		DieEventScope(int event, int& flag) 
			: event_(event), flag_(flag) 
		{
			if(event_ == OBJECT_EVENT_DIE) {
				++flag_;
			}
		}

		~DieEventScope() {
			if(event_ == OBJECT_EVENT_DIE) {
				--flag_;
			}
		}
	};
}

bool CustomObject::handleEventInternal(int event, const FormulaCallable* context, bool executeCommands_now)
{
	if(paused_ && event != OBJECT_EVENT_BEING_REMOVED) {
		static const int MouseLeaveID = get_object_event_id("mouse_leave");
		if(event != MouseLeaveID) {
			return false;
		}
	}

	const DieEventScope die_scope(event, currently_handling_die_event_);
	if(hitpoints_ <= 0 && event != OBJECT_EVENT_BEING_REMOVED && !currently_handling_die_event_) {
		return false;
	}

#ifndef NO_EDITOR
	if(((event != OBJECT_EVENT_ANY && static_cast<size_t>(event) < event_handlers_.size() && event_handlers_[OBJECT_EVENT_ANY]) || type_->getEventHandler(OBJECT_EVENT_ANY))) {
		game_logic::MapFormulaCallable* callable = new game_logic::MapFormulaCallable;
		variant v(callable);

		callable->add("event", variant(get_object_event_str(event)));

		handleEventInternal(OBJECT_EVENT_ANY, callable, true);
	}
#endif

	const game_logic::Formula* handlers[2];
	int nhandlers = 0;

	if(size_t(event) < event_handlers_.size() && event_handlers_[event]) {
		handlers[nhandlers++] = event_handlers_[event].get();
	}

	const game_logic::Formula* type_handler = type_->getEventHandler(event).get();
	if(type_handler != nullptr) {
		handlers[nhandlers++] = type_handler;
	}

	if(!nhandlers) {
		return false;
	}

	BackupCallableStackScope callable_scope(&backup_callable_stack_, context);

	for(int n = 0; n != nhandlers; ++n) {
		const game_logic::Formula* handler = handlers[n];

#ifndef DISABLE_FORMULA_PROFILER
		formula_profiler::CustomObjectEventFrame event_frame = { type_.get(), event, false };
		event_call_stack.emplace_back(event_frame);
#endif

		++events_handled_per_second;

		variant var;
		
		try {
			formula_profiler::Instrument instrumentation("FFL", handler);
			var = handler->execute(*this);
		} catch(validation_failure_exception& e) {
#ifndef DISABLE_FORMULA_PROFILER
			event_call_stack.pop_back();
#endif
			current_error_msg = "Runtime error evaluating formula: " + e.msg;
			throw e;
		}

#ifndef DISABLE_FORMULA_PROFILER
		event_call_stack.back().executing_commands = true;
#endif

		bool result = false;
		
		try {
			if(executeCommands_now) {
				formula_profiler::Instrument instrumentation("COMMANDS", handler);
				result = executeCommand(var);
			} else {
				delayed_commands_.emplace_back(var);
			}
		} catch(validation_failure_exception& e) {
			current_error_msg = "Runtime error executing event commands: " + e.msg;
			throw e;
		}

#ifndef DISABLE_FORMULA_PROFILER
		event_call_stack.pop_back();
#endif
		if(!result) {
			break;
		}
	}
	return true;
}

void CustomObject::resolveDelayedEvents()
{
	if(delayed_commands_.empty()) {
		return;
	}

	try {
		for(const variant& v : delayed_commands_) {
			executeCommand(v);
		}
	} catch(validation_failure_exception&) {
	}

	delayed_commands_.clear();
}

bool CustomObject::executeCommandOrFn(const variant& var)
{
	if(var.is_function()) {
		std::vector<variant> args;
		variant cmd = var(args);
		return executeCommand(cmd);
	} else {
		return executeCommand(var);
	}
}

bool CustomObject::executeCommand(const variant& var)
{
	bool result = true;
	if(var.is_null()) { return result; }
	if(var.is_list()) {
		const int num_elements = var.num_elements();
		for(int n = 0; n != num_elements; ++n) {
			result = executeCommand(var[n]) && result;
		}
	} else {
		game_logic::CommandCallable* cmd = var.try_convert<game_logic::CommandCallable>();
		if(cmd != nullptr) {
			cmd->runCommand(*this);
		} else {
			CustomObjectCommandCallable* cmd = var.try_convert<CustomObjectCommandCallable>();
			if(cmd != nullptr) {
				cmd->runCommand(Level::current(), *this);
			} else {
				EntityCommandCallable* cmd = var.try_convert<EntityCommandCallable>();
				if(cmd != nullptr) {
					cmd->runCommand(Level::current(), *this);
				} else {
					SwallowObjectCommandCallable* cmd = var.try_convert<SwallowObjectCommandCallable>();
					if(cmd) {
						result = false;
					} else {
						ASSERT_LOG(false, "COMMAND WAS EXPECTED, BUT FOUND: " << var.to_debug_string() << "\nFORMULA INFO: " << output_formula_error_info() << "\n");
					}
				}
			}
		}
	}

	return result;
}

int CustomObject::slopeStandingOn(int range) const
{
	if(isStanding(Level::current()) == STANDING_STATUS::NOT_STANDING) {
		return 0;
	}

	const int forward = isFacingRight() ? 1 : -1;
	const int xpos = getFeetX();
	int ypos = getFeetY();


	for(int n = 0; !Level::current().standable(xpos, ypos) && n != 10; ++n) {
		++ypos;
	}

	if(range == 1) {
		if(Level::current().standable(xpos + forward, ypos - 1) &&
		   !Level::current().standable(xpos - forward, ypos)) {
			return 45;
		}

		if(!Level::current().standable(xpos + forward, ypos) &&
		   Level::current().standable(xpos - forward, ypos - 1)) {
			return -45;
		}

		return 0;
	} else {
		if(isStanding(Level::current()) == STANDING_STATUS::NOT_STANDING) {
			return 0;
		}

		int y1 = find_ground_level(Level::current(), xpos + forward*range, ypos, range+1);
		int y2 = find_ground_level(Level::current(), xpos - forward*range, ypos, range+1);
		while((y1 == std::numeric_limits<int>::min() || y2 == std::numeric_limits<int>::min()) && range > 0) {
			y1 = find_ground_level(Level::current(), xpos + forward*range, ypos, range+1);
			y2 = find_ground_level(Level::current(), xpos - forward*range, ypos, range+1);
			--range;
		}

		if(range == 0) {
			return 0;
		}

		const int dy = y2 - y1;
		const int dx = range*2;
		return (dy*45)/dx;
	}
}

void CustomObject::make_draw_color()
{
	if(!draw_color_.get()) {
		draw_color_.reset(new KRE::ColorTransform(draw_color()));
	}
}

const KRE::ColorTransform& CustomObject::draw_color() const
{
	if(draw_color_.get()) {
		return *draw_color_;
	}

	static const KRE::ColorTransform white(255, 255, 255, 255, 255, 255, 255, 255);
	return white;
}

game_logic::ConstFormulaPtr CustomObject::getEventHandler(int key) const
{
	if(size_t(key) < event_handlers_.size()) {
		return event_handlers_[key];
	} else {
		return game_logic::ConstFormulaPtr();
	}
}

void CustomObject::setEventHandler(int key, game_logic::ConstFormulaPtr f)
{
	if(size_t(key) >= event_handlers_.size()) {
		event_handlers_.resize(key+1);
	}

	event_handlers_[key] = f;
}

bool CustomObject::canInteractWith() const
{
	return can_interact_with_;
}

std::string CustomObject::getDebugDescription() const
{
	return type_->id();
}

namespace 
{
	bool map_variant_entities(variant& v, const std::map<EntityPtr, EntityPtr>& m)
	{
		if(v.is_list()) {
			for(int n = 0; n != v.num_elements(); ++n) {
				variant var = v[n];
				if(map_variant_entities(var, m)) {
					std::vector<variant> new_values;
					for(int i = 0; i != n; ++i) {
						new_values.emplace_back(v[i]);
					}

					new_values.emplace_back(var);
					for(int i = n+1; i < v.num_elements(); ++i) {
						var = v[i];
						map_variant_entities(var, m);
						new_values.emplace_back(var);
					}

					v = variant(&new_values);
					return true;
				}
			}
		} else if(v.try_convert<Entity>()) {
			Entity* e = v.try_convert<Entity>();
			auto i = m.find(EntityPtr(e));
			if(i != m.end()) {
				v = variant(i->second.get());
				return true;
			} else {
				EntityPtr back = e->backup();
				v = variant(back.get());
				return true;
			}
		}

		return false;
	}

	void do_map_entity(EntityPtr& e, const std::map<EntityPtr, EntityPtr>& m)
	{
		if(e) {
			auto i = m.find(e);
			if(i != m.end()) {
				e = i->second;
			}
		}
	}
}

void CustomObject::mapEntities(const std::map<EntityPtr, EntityPtr>& m)
{
	do_map_entity(last_hit_by_, m);
	do_map_entity(standing_on_, m);
	do_map_entity(parent_, m);

	for(variant& v : vars_->values()) {
		map_variant_entities(v, m);
	}

	for(variant& v : tmp_vars_->values()) {
		map_variant_entities(v, m);
	}

	for(variant& v : property_data_) {
		map_variant_entities(v, m);
	}
}

void CustomObject::cleanup_references()
{
	last_hit_by_.reset();
	standing_on_.reset();
	parent_.reset();
	for(variant& v : vars_->values()) {
		v = variant();
	}

	for(variant& v : tmp_vars_->values()) {
		v = variant();
	}

	for(variant& v : property_data_) {
		v = variant();
	}
}

void CustomObject::extractGcObjectReferences(std::vector<gc_object_reference>& v)
{
	extractGcObjectReferences(last_hit_by_, v);
	extractGcObjectReferences(standing_on_, v);
	extractGcObjectReferences(parent_, v);
	for(variant& var : vars_->values()) {
		extractGcObjectReferences(var, v);
	}

	for(variant& var : tmp_vars_->values()) {
		extractGcObjectReferences(var, v);
	}

	for(variant& var : property_data_) {
		extractGcObjectReferences(var, v);
	}

	gc_object_reference visitor;
	visitor.owner = this;
	visitor.target = nullptr;
	visitor.from_variant = nullptr;
	visitor.visitor.reset(new game_logic::FormulaCallableVisitor);
	for(gui::WidgetPtr w : widgets_) {
		w->performVisitValues(*visitor.visitor);
	}

	for(game_logic::FormulaCallableSuspendedPtr ptr : visitor.visitor->pointers()) {
		if(dynamic_cast<const CustomObject*>(ptr->value())) {
			ptr->destroy_ref();
		}
	}

	v.emplace_back(visitor);
}

void CustomObject::extractGcObjectReferences(EntityPtr& e, std::vector<gc_object_reference>& v)
{
	if(!e) {
		return;
	}

	v.resize(v.size()+1);
	gc_object_reference& ref = v.back();
	ref.owner = this;
	ref.target = e.get();
	ref.from_variant = nullptr;
	ref.from_ptr = &e;

	e.reset();
}

void CustomObject::extractGcObjectReferences(variant& var, std::vector<gc_object_reference>& v)
{
	if(var.is_callable()) {
		if(var.try_convert<Entity>()) {
			v.resize(v.size()+1);
			gc_object_reference& ref = v.back();
			ref.owner = this;
			ref.target = var.try_convert<Entity>();
			ref.from_variant = &var;
			ref.from_ptr = nullptr;

			var = variant();
		}
	} else if(var.is_list()) {
		for(int n = 0; n != var.num_elements(); ++n) {
			extractGcObjectReferences(*var.get_index_mutable(n), v);
		}
	} else if(var.is_map()) {
		for(variant k : var.getKeys().as_list()) {
			extractGcObjectReferences(*var.get_attr_mutable(k), v);
		}
	}
}

void CustomObject::restoreGcObjectReference(gc_object_reference ref)
{
	if(ref.visitor) {
		for(game_logic::FormulaCallableSuspendedPtr ptr : ref.visitor->pointers()) {
			ptr->restore_ref();
		}
	} else if(ref.from_variant) {
		*ref.from_variant = variant(ref.target);
	} else {
		ref.from_ptr->reset(ref.target);
	}
}

void CustomObject::surrenderReferences(GarbageCollector* collector)
{
	assert(value_stack_.empty());

	const std::vector<const CustomObjectType::PropertyEntry*>& entries = type_->getVariableProperties();
	int index = 0;
	for(variant& var : property_data_) {
		collector->surrenderVariant(&var, entries[index] ? entries[index]->id.c_str() : nullptr);
		++index;
	}

	for(const gui::WidgetPtr& w : widgets_) {
		collector->surrenderPtr(&w, "WIDGET");
	}

	collector->surrenderPtr(&vars_, "VARS");
	collector->surrenderPtr(&tmp_vars_, "TMP_VARS");
	collector->surrenderPtr(&tags_, "TAGS");

	collector->surrenderPtr(&last_hit_by_, "LAST_HIT_BY");
	collector->surrenderPtr(&standing_on_, "STANDING_ON");
	collector->surrenderPtr(&parent_, "PARENT");

	collector->surrenderPtr(&shader_, "SHADER");
	for(graphics::AnuraShaderPtr& shader : effects_shaders_) {
		collector->surrenderPtr(&shader, "EFFECTS_SHADER");
	}

	collector->surrenderPtr(&document_, "XHTML_DOCUMENT");

	for(auto move : animated_movement_) {
		collector->surrenderVariant(&move->on_begin, "ANIMATE_ON_BEGIN");
		collector->surrenderVariant(&move->on_process, "ANIMATE_ON_PROCESS");
		collector->surrenderVariant(&move->on_complete, "ANIMATE_ON_COMPLETE");
		for(variant& v : move->animation_values) {
			collector->surrenderVariant(&v, "ANIMATE_VALUES");
		}

		for(std::pair<variant,variant>& p : move->follow_on) {
			collector->surrenderVariant(&p.first, "ANIMATE_FOLLOW_ON");
			collector->surrenderVariant(&p.second, "ANIMATE_FOLLOW_ON");
		}
	}

	Entity::surrenderReferences(collector);
}

std::string CustomObject::debugObjectName() const
{
	return "obj " + type_->id();
}

void CustomObject::addParticleSystem(const std::string& key, const std::string& type)
{
	particle_systems_[key] = type_->getParticleSystemFactory(type)->create(*this);
	particle_systems_[key]->setType(type);
}

void CustomObject::removeParticleSystem(const std::string& key)
{
	particle_systems_.erase(key);
}

void CustomObject::setText(const std::string& text, const std::string& font, int size, int align)
{
	text_.reset(new CustomObjectText);
	text_->text = text;
	text_->font = GraphicalFont::get(font);
	text_->size = size;
	text_->align = align;
	text_->alpha = 255;
	ASSERT_LOG(text_->font, "UNKNOWN FONT: " << font);
	text_->dimensions = text_->font->dimensions(text_->text, size);
}

bool CustomObject::boardableVehicle() const
{
	return type_->isVehicle() && driver_.get() == nullptr;
}

void CustomObject::boarded(Level& lvl, const EntityPtr& player)
{
	if(!player) {
		return;
	}

	player->boardVehicle();

	if(player->isHuman()) {
		PlayableCustomObject* new_player(new PlayableCustomObject(*this));
		new_player->driver_ = player;

		lvl.add_player(new_player);

		new_player->getPlayerInfo()->swapPlayerState(*player->getPlayerInfo());
		lvl.remove_character(this);
	} else {
		driver_ = player;
		lvl.remove_character(player);
	}
}

void CustomObject::unboarded(Level& lvl)
{
	if(velocityX() > 100) {
		driver_->setFacingRight(false);
	}

	if(velocityX() < -100) {
		driver_->setFacingRight(true);
	}

	if(isHuman()) {
		CustomObject* vehicle(new CustomObject(*this));
		vehicle->driver_ = EntityPtr();
		lvl.add_character(vehicle);

		lvl.add_player(driver_);

		driver_->unboardVehicle();

		driver_->getPlayerInfo()->swapPlayerState(*getPlayerInfo());
	} else {
		lvl.add_character(driver_);
		driver_->unboardVehicle();
		driver_ = EntityPtr();
	}
}

void CustomObject::boardVehicle()
{
}

void CustomObject::unboardVehicle()
{
}

void CustomObject::setSoundVolume(float sound_volume, float nseconds)
{
	sound::change_volume(this, sound_volume, nseconds);
	sound_volume_ = sound_volume;
}

bool CustomObject::allowLevelCollisions() const
{
	return type_->isStaticObject() || !collides_with_level_;
}

void CustomObject::set_platform_area(const rect& area)
{
	if(area.w() <= 0 || area.h() <= 0) {
		platform_area_.reset(new rect(area));
		platform_solid_info_ = ConstSolidInfoPtr();
	} else {
		platform_area_.reset(new rect(area));
		platform_solid_info_ = SolidInfo::createPlatform(area);
	}

	calculateSolidRect();
}

void CustomObject::shiftPosition(int x, int y)
{
	Entity::shiftPosition(x, y);
	if(standing_on_prev_x_ != std::numeric_limits<int>::min()) {
		standing_on_prev_x_ += x;
	}

	if(standing_on_prev_y_ != std::numeric_limits<int>::min()) {
		standing_on_prev_y_ += y;
	}

	if(position_schedule_.get() != nullptr) {
		for(int& xpos : position_schedule_->x_pos) {
			xpos += x;
		}

		for(int& ypos : position_schedule_->y_pos) {
			ypos += y;
		}
	}

	if(activation_area_.get() != nullptr) {
		activation_area_.reset(new rect(activation_area_->x() + x,
		                                activation_area_->y() + y,
										activation_area_->w(),
										activation_area_->h()));
	}
}

bool CustomObject::appearsAtDifficulty(int difficulty) const
{
	return (min_difficulty_ == -1 || difficulty >= min_difficulty_) &&
	       (max_difficulty_ == -1 || difficulty <= max_difficulty_);
}

void CustomObject::setParent(EntityPtr e, const std::string& pivot_point)
{
	parent_ = e;
	parent_pivot_ = pivot_point;

	const point pos = parent_position();

	if(parent_.get() != nullptr) {
		point my_pos = getMidpoint();

        const int parent_facing_sign = parent_->isFacingRight() ? 1 : -1;
        relative_x_ = parent_facing_sign * (my_pos.x - pos.x);
        relative_y_ = (my_pos.y - pos.y);
    }
        
	parent_prev_x_ = pos.x;
	parent_prev_y_ = pos.y;
    
	if(parent_.get() != nullptr) {
		parent_prev_facing_ = parent_->isFacingRight();
	}
}

int CustomObject::parentDepth(bool* has_human_parent, int cur_depth) const
{
	if(!parent_ || cur_depth > 10) {
		if(has_human_parent) {
			*has_human_parent = isHuman() != nullptr;
		}
		return cur_depth;
	}

	return parent_->parentDepth(has_human_parent, cur_depth+1);
}

bool CustomObject::editorForceStanding() const
{
	return type_->editorForceStanding();
}

game_logic::ConstFormulaCallableDefinitionPtr CustomObject::getDefinition() const
{
	return type_->callableDefinition();
}

rect CustomObject::platformRectAt(int xpos) const
{
	if(platform_offsets_.empty()) {
		return platformRect();
	}

	rect area = platformRect();
	if(xpos < area.x() || xpos >= area.x() + area.w()) {
		return area;
	}

	if(platform_offsets_.size() == 1) {
		return rect(area.x(), area.y() + platform_offsets_[0], area.w(), area.h());
	}

	const int pos = (xpos - area.x())*1024;
	const int seg_width = (area.w()*1024)/(static_cast<int>(platform_offsets_.size())-1);
	const size_t segment = pos/seg_width;
	ASSERT_LT(segment, platform_offsets_.size()-1);

	const int partial = pos%seg_width;

	const int offset = (partial*platform_offsets_[segment+1] + (seg_width-partial)*platform_offsets_[segment])/seg_width;
	return rect(area.x(), area.y() + offset, area.w(), area.h());
}

int CustomObject::platformSlopeAt(int xpos) const
{
	if(platform_offsets_.size() <= 1) {
		return 0;
	}

	rect area = platformRect();
	if(xpos < area.x() || xpos >= area.x() + area.w()) {
		return 0;
	}

	const int pos = (xpos - area.x())*1024;
	const int dx = (area.w()*1024)/(static_cast<int>(platform_offsets_.size())-1);
	const size_t segment = pos/dx;
	ASSERT_LT(segment, platform_offsets_.size()-1);

	const int dy = (platform_offsets_[segment+1] - platform_offsets_[segment])*1024;

	return (dy*45)/dx;
}

bool CustomObject::isSolidPlatform() const
{
	return type_->isSolidPlatform();
}

point CustomObject::parent_position() const
{
	if(parent_.get() == nullptr) {
		return point(0,0);
	}

	return parent_->pivot(parent_pivot_);
}

void CustomObject::updateType(ConstCustomObjectTypePtr old_type,
                                ConstCustomObjectTypePtr new_type)
{
	if(old_type != base_type_) {
		return;
	}

	base_type_ = new_type;
	if(current_variation_.empty()) {
		type_ = base_type_;
	} else {
		type_ = base_type_->getVariation(current_variation_);
	}

	game_logic::FormulaVariableStoragePtr old_vars = vars_;

	vars_.reset(new game_logic::FormulaVariableStorage(type_->variables()));
	vars_->setObjectName(getDebugDescription());
	for(const std::string& key : old_vars->keys()) {
		const variant old_value = old_vars->queryValue(key);
		std::map<std::string, variant>::const_iterator old_type_value =
		    old_type->variables().find(key);
		if(old_type_value == old_type->variables().end() ||
		   old_type_value->second != old_value) {
			vars_->mutateValue(key, old_value);
		}
	}

	old_vars = tmp_vars_;

	tmp_vars_.reset(new game_logic::FormulaVariableStorage(type_->tmpVariables()));
	tmp_vars_->setObjectName(getDebugDescription());
	for(const std::string& key : old_vars->keys()) {
		const variant old_value = old_vars->queryValue(key);
		std::map<std::string, variant>::const_iterator old_type_value =
		    old_type->tmpVariables().find(key);
		if(old_type_value == old_type->tmpVariables().end() ||
		   old_type_value->second != old_value) {
			tmp_vars_->mutateValue(key, old_value);
		}
	}

	vars_->disallowNewKeys(type_->isStrict());
	tmp_vars_->disallowNewKeys(type_->isStrict());

	if(type_->hasFrame(frame_name_)) {
		frame_.reset(&type_->getFrame(frame_name_));
	}

	std::map<std::string, ParticleSystemPtr> systems;
	systems.swap(particle_systems_);
	for(std::map<std::string, ParticleSystemPtr>::const_iterator i = systems.begin(); i != systems.end(); ++i) {
		addParticleSystem(i->first, i->second->type());
	}

	if (type_->getShader() != nullptr) {
		shader_ = graphics::AnuraShaderPtr(new graphics::AnuraShader(*type_->getShader()));
	}

	if(shader_ != nullptr) {
		shader_->setParent(this);
		//LOG_DEBUG("shader '" << shader_->getName() << "' attached to object: " << type_->id());
	}
	for(auto eff : effects_shaders_) {
		eff->setParent(this);
	}

#if defined(USE_LUA)
	if(type_->has_lua()) {
	//	lua_ptr_.reset(new lua::lua_context());
		init_lua();
	}
#endif

	handleEvent(OBJECT_EVENT_TYPE_UPDATED);
}

std::vector<variant> CustomObject::getVariantWidgetList() const
{
	std::vector<variant> v;
	for(widget_list::iterator it = widgets_.begin(); it != widgets_.end(); ++it) {
		v.emplace_back(variant(it->get()));
	}
	return v;
}

bool CustomObject::getClipArea(rect* clip_area) const
{
	if(clip_area_.get() != nullptr && clip_area) {
		if(clip_area_absolute_) {
			*clip_area = *clip_area_.get();
		} else {
			*clip_area = *clip_area_.get() + point(x(), y());
		}
		return true;
	} else if(clip_area_.get() != nullptr) {
		return true;
	}

	return false;
}

void CustomObject::addWidget(const gui::WidgetPtr& w)
{ 
	widgets_.insert(w); 
}

void CustomObject::addWidgets(std::vector<gui::WidgetPtr>* widgets) 
{
	widgets_.clear();
	std::copy(widgets->begin(), widgets->end(), std::inserter(widgets_, widgets_.end()));
}

void CustomObject::clearWidgets() 
{ 
	widgets_.clear(); 
}

void CustomObject::removeWidget(gui::WidgetPtr w)
{
	widget_list::iterator it = widgets_.find(w);
	ASSERT_LOG(it != widgets_.end(), "Tried to erase widget not in list.");
	widgets_.erase(it);
}

bool CustomObject::handle_sdl_event(const SDL_Event& event, bool claimed)
{
	point p(x(), y());

	if(document_ &&  !claimed) {
		claimed |= document_->handleEvents(p, event);
	}

	widget_list w = widgets_;
	widget_list::const_reverse_iterator ritor = w.rbegin();
	while(ritor != w.rend()) {
		claimed |= (*ritor++)->processEvent(p, event, claimed);
	}
	return claimed;
}

game_logic::FormulaPtr CustomObject::createFormula(const variant& v)
{
	return game_logic::FormulaPtr(new game_logic::Formula(v, &get_custom_object_functions_symbol_table()));
}

gui::ConstWidgetPtr CustomObject::getWidgetById(const std::string& id) const
{
	for(const gui::WidgetPtr& w : widgets_) {
		gui::WidgetPtr wx = w->getWidgetById(id);
		if(wx) {
			return wx;
		}
	}
	return gui::ConstWidgetPtr();
}

gui::WidgetPtr CustomObject::getWidgetById(const std::string& id)
{
	for(const gui::WidgetPtr& w : widgets_) {
		gui::WidgetPtr wx = w->getWidgetById(id);
		if(wx) {
			return wx;
		}
	}
	return gui::WidgetPtr();
}

void CustomObject::addToLevel()
{
	Entity::addToLevel();
	standing_on_.reset();
#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active();
	}
#endif
	if(shader_ != nullptr) {
		shader_->setParent(this);
		//LOG_DEBUG("shader '" << shader_->getName() << "' attached to object: " << type_->id());
	}
	for(auto eff : effects_shaders_) {
		eff->setParent(this);
	}
}

int CustomObject::mouseDragThreshold(int default_value) const
{
	const int v = type_->getMouseDragThreshold();
	if(v >= 0) {
		return v;
	} else {
		return default_value;
	}
}

BENCHMARK(custom_object_spike) {
	static Level* lvl = nullptr;
	if(!lvl) {	
		lvl = new Level("test.cfg");
		static variant v(lvl);
		lvl->finishLoading();
		lvl->setAsCurrentLevel();
	}
	BENCHMARK_LOOP {
		CustomObject* obj = new CustomObject("chain_base", 0, 0, false);
		variant v(obj);
		obj->handleEvent(OBJECT_EVENT_CREATE);
	}
}

int CustomObject::events_handled_per_second = 0;

BENCHMARK_ARG(custom_object_get_attr, const std::string& attr)
{
	static CustomObject* obj = new CustomObject("ant_black", 0, 0, false);
	BENCHMARK_LOOP {
		obj->queryValue(attr);
	}
}

BENCHMARK_ARG_CALL(custom_object_get_attr, easy_lookup, "x");
BENCHMARK_ARG_CALL(custom_object_get_attr, hard_lookup, "xxxx");

BENCHMARK_ARG(custom_object_handle_event, const std::string& object_event)
{
	auto i = std::find(object_event.begin(), object_event.end(), ':');
	ASSERT_LOG(i != object_event.end(), "custom_object_event_handle argument must have a colon seperator: " << object_event);
	std::string obj_type(object_event.begin(), i);
	std::string event_name(i+1, object_event.end());
	static Level* lvl = new Level("titlescreen.cfg");
	lvl->setAsCurrentLevel();
	static CustomObject* obj = new CustomObject(obj_type, 0, 0, false);
	obj->setLevel(*lvl);
	const int event_id = get_object_event_id(event_name);
	BENCHMARK_LOOP {
		obj->handleEvent(event_id);
	}
}

BENCHMARK_ARG_CALL(custom_object_handle_event, ant_non_exist, "ant_black:blahblah");

BENCHMARK_ARG_CALL_COMMAND_LINE(custom_object_handle_event);
