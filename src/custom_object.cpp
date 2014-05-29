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
#include "graphics.hpp"

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/math/special_functions/round.hpp>
#include <glm/gtc/matrix_transform.hpp>


#include <stdio.h>

#include <cassert>
#include <iostream>

#include "asserts.hpp"
#include "code_editor_dialog.hpp"
#include "collision_utils.hpp"
#include "custom_object.hpp"
#include "custom_object_callable.hpp"
#include "custom_object_functions.hpp"
#include "debug_console.hpp"
#include "difficulty.hpp"
#include "draw_scene.hpp"
#include "font.hpp"
#include "formatter.hpp"
#include "formula_callable.hpp"
#include "formula_callable_visitor.hpp"
#include "formula_object.hpp"
#include "formula_profiler.hpp"
#include "geometry.hpp"
#include "graphical_font.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "level_logic.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "playable_custom_object.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "string_utils.hpp"
#include "surface_formula.hpp"
#include "variant.hpp"
#include "variant_utils.hpp"
#include "unit_test.hpp"
#include "utils.hpp"
#include "sound.hpp"
#include "widget_factory.hpp"

class active_property_scope {
	const custom_object& obj_;
	int prev_prop_;
	bool pop_value_stack_;
public:
	active_property_scope(const custom_object& obj, int prop_num, const variant* value=NULL) : obj_(obj), prev_prop_(obj.active_property_), pop_value_stack_(false)
	{
		obj_.active_property_ = prop_num;
		if(value) {
			obj_.value_stack_.push(*value);
			pop_value_stack_ = true;
		}
	}

	~active_property_scope() {
		obj_.active_property_ = prev_prop_;
		if(pop_value_stack_) {
			obj_.value_stack_.pop();
		}
	}
};

namespace {

const int widget_zorder_draw_later_threshold = 1000;

const game_logic::formula_variable_storage_ptr& global_vars()
{
	static game_logic::formula_variable_storage_ptr obj(new game_logic::formula_variable_storage);
	return obj;
}
}

struct custom_object_text {
	std::string text;
	const_graphical_font_ptr font;
	int size;
	int align;
	rect dimensions;
	int alpha;
};

namespace {
std::string current_error_msg;

std::vector<variant> deep_copy_property_data(const std::vector<variant>& property_data)
{
	std::vector<variant> result;
	result.reserve(property_data.size());
	for(const variant& v : property_data) {
		result.push_back(deep_copy_variant(v));
	}

	return result;
}

}

const std::string* custom_object::current_debug_error()
{
	if(current_error_msg == "") {
		return NULL;
	}

	return &current_error_msg;
}

void custom_object::reset_current_debug_error()
{
	current_error_msg = "";
}

custom_object::custom_object(variant node)
  : entity(node),
    previous_y_(y()),
	custom_type_(node["custom_type"]),
    type_(custom_type_.is_map() ?
	      const_custom_object_type_ptr(new custom_object_type(custom_type_["id"].as_string(), custom_type_)) :
		  custom_object_type::get(node["type"].as_string())),
	base_type_(type_),
    frame_(&type_->default_frame()),
	frame_name_(node.has_key("current_frame") ? node["current_frame"].as_string() : "normal"),
	time_in_frame_(node["time_in_frame"].as_int(0)),
	time_in_frame_delta_(node["time_in_frame_delta"].as_int(1)),
	velocity_x_(node["velocity_x"].as_int(0)),
	velocity_y_(node["velocity_y"].as_int(0)),
	accel_x_(node["accel_x"].as_int()),
	accel_y_(node["accel_y"].as_int()),
	gravity_shift_(node["gravity_shift"].as_int(0)),
	rotate_x_(), rotate_y_(), rotate_z_(node["rotate"].as_decimal()), zorder_(node["zorder"].as_int(type_->zorder())),
	zsub_order_(node["zsub_order"].as_int(type_->zsub_order())),
	hitpoints_(node["hitpoints"].as_int(type_->hitpoints())),
	max_hitpoints_(node["max_hitpoints"].as_int(type_->hitpoints()) - type_->hitpoints()),
	was_underwater_(false),
	has_feet_(node["has_feet"].as_bool(type_->has_feet())),
	invincible_(0),
	sound_volume_(128),
	vars_(new game_logic::formula_variable_storage(type_->variables())),
	tmp_vars_(new game_logic::formula_variable_storage(type_->tmp_variables())),
	active_property_(-1),
	last_hit_by_anim_(0),
	current_animation_id_(0),
	cycle_(node["cycle"].as_int()),
	created_(node["created"].as_bool(false)), loaded_(false),
	standing_on_prev_x_(INT_MIN), standing_on_prev_y_(INT_MIN),
	can_interact_with_(false), fall_through_platforms_(0),
	always_active_(node["always_active"].as_bool(false)),
	activation_border_(node["activation_border"].as_int(type_->activation_border())),
	last_cycle_active_(0),
	parent_pivot_(node["pivot"].as_string_default()),
	parent_prev_x_(INT_MIN), parent_prev_y_(INT_MIN), parent_prev_facing_(true),
    relative_x_(node["relative_x"].as_int(0)), relative_y_(node["relative_y"].as_int(0)),
	swallow_mouse_event_(false),
	currently_handling_die_event_(0),
	use_absolute_screen_coordinates_(node["use_absolute_screen_coordinates"].as_bool(type_->use_absolute_screen_coordinates())),
	vertex_location_(-1), texcoord_location_(-1),
	paused_(false), model_(glm::mat4(1.0f))
{

	vars_->set_object_name(debug_description());
	tmp_vars_->set_object_name(debug_description());

	if(!created_) {
		properties_requiring_dynamic_initialization_ = type_->properties_requiring_dynamic_initialization();
		properties_requiring_dynamic_initialization_.insert(properties_requiring_dynamic_initialization_.end(), type_->properties_requiring_initialization().begin(), type_->properties_requiring_initialization().end());
	}

	vars_->disallow_new_keys(type_->is_strict());
	tmp_vars_->disallow_new_keys(type_->is_strict());

	get_all().insert(this);
	get_all(base_type_->id()).insert(this);

	if(node.has_key("platform_area")) {
		set_platform_area(rect(node["platform_area"]));
	}

	if(node.has_key("x_schedule")) {
		if(position_schedule_.get() == NULL) {
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
		if(position_schedule_.get() == NULL) {
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
		if(position_schedule_.get() == NULL) {
			position_schedule_.reset(new PositionSchedule);
		}

		position_schedule_->rotation = node["rotation_schedule"].as_list_decimal();	
	}

	if(position_schedule_.get() != NULL && node.has_key("schedule_speed")) {
		position_schedule_->speed = node["schedule_speed"].as_int();
	}

	if(position_schedule_.get() != NULL && node.has_key("schedule_base_cycle")) {
		position_schedule_->base_cycle = node["schedule_base_cycle"].as_int();
	}

	if(position_schedule_.get() != NULL && node.has_key("schedule_expires") && node["schedule_expires"].as_bool()) {
		position_schedule_->expires = true;
	}

	if(node.has_key("draw_area")) {
		draw_area_.reset(new rect(node["draw_area"].as_string()));
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
		type_ = base_type_->get_variation(current_variation_);
	}

	if(node.has_key("parallax_scale_x") || node.has_key("parallax_scale_y")) {
		parallax_scale_millis_.reset(new std::pair<int, int>(node["parallax_scale_x"].as_int(type_->parallax_scale_millis_x()), node["parallax_scale_y"].as_int(type_->parallax_scale_millis_y())));
	} else {
		parallax_scale_millis_.reset(new std::pair<int, int>(type_->parallax_scale_millis_x(), type_->parallax_scale_millis_y()));
	}

	min_difficulty_ = node.has_key("min_difficulty") ? difficulty::from_variant(node["min_difficulty"]) : -1;
	max_difficulty_ = node.has_key("max_difficulty") ? difficulty::from_variant(node["max_difficulty"]) : -1;

	vars_->read(node["vars"]);

	unsigned int solid_dim = type_->solid_dimensions();
	unsigned int weak_solid_dim = type_->weak_solid_dimensions();
	unsigned int collide_dim = type_->collide_dimensions();
	unsigned int weak_collide_dim = type_->weak_collide_dimensions();

	if(node.has_key("solid_dimensions")) {
		weak_solid_dim = solid_dim = 0;
		std::vector<std::string> solid_dim_str = util::split(node["solid_dimensions"].as_string());
		foreach(const std::string& str, solid_dim_str) {
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
		foreach(const std::string& str, collide_dim_str) {
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

	set_solid_dimensions(solid_dim, weak_solid_dim);
	set_collide_dimensions(collide_dim, weak_collide_dim);

	variant tags_node = node["tags"];
	if(tags_node.is_null() == false) {
		tags_ = new game_logic::map_formula_callable(tags_node);
	} else {
		tags_ = new game_logic::map_formula_callable(type_->tags());
	}

	if(node.has_key("draw_color")) {
		draw_color_.reset(new graphics::color_transform(node["draw_color"]));
	}

	if(node.has_key("label")) {
		set_label(node["label"].as_string());
	} else {
		set_distinct_label();
	}

	if(!type_->respawns()) {
		set_respawn(false);
	}

	assert(type_.get());
	//set_frame_no_adjustments(frame_name_);
	frame_.reset(&type_->get_frame(frame_name_));
	calculate_solid_rect();

	next_animation_formula_ = type_->next_animation_formula();

	type_->init_event_handlers(node, event_handlers_);

	can_interact_with_ = get_event_handler(OBJECT_EVENT_INTERACT).get() != NULL;

	variant text_node = node["text"];
	if(!text_node.is_null()) {
		set_text(text_node["text"].as_string(), text_node["font"].as_string(), text_node["size"].as_int(2), text_node["align"].as_int(-1));
	}

	if(node.has_key("particles")) {
		std::vector<std::string> particles = util::split(node["particles"].as_string());
		foreach(const std::string& p, particles) {
			add_particle_system(p, p);
		}
	}

	if(node.has_key("lights")) {
		foreach(variant light_node, node["lights"].as_list()) {
			light_ptr new_light(light::create_light(*this, light_node));
			if(new_light) {
				lights_.push_back(new_light);
			}
		}
	}

	if(node.has_key("parent")) {
		parent_loading_.serialize_from_string(node["parent"].as_string());
	}

	if(node.has_key("platform_offsets")) {
		platform_offsets_ = node["platform_offsets"].as_list_int();
	} else {
		platform_offsets_ = type_->platform_offsets();
	}

	if(node.has_key("mouseover_area")) {
		set_mouse_over_area(rect(node["mouseover_area"]));
	}

	set_mouseover_delay(node["mouseover_delay"].as_int(0));

#if defined(USE_SHADERS)
	if(node.has_key("shader")) {
		shader_.reset(new gles2::shader_program(node["shader"]));
	} else if(type_->shader()) {
		shader_.reset(new gles2::shader_program(*type_->shader()));
	}

	if(node.has_key("effects")) {
		variant effects = node["effects"];
		for(int n = 0; n != effects.num_elements(); ++n) {
			effects_.push_back(new gles2::shader_program(effects[n]));
		}
	} else {
		for(size_t n = 0; n < type_->effects().size(); ++n) {
			effects_.push_back(new gles2::shader_program(*type_->effects()[n]));
		}
	}
#endif

#ifdef USE_BOX2D
	if(node.has_key("body")) {
		body_.reset(new box2d::body(node["body"]));
	}
#endif

#if defined(USE_LUA)
	if(!type_->get_lua_source().empty()) {
		lua_ptr_.reset(new lua::lua_context());
	}
#endif

	if(node.has_key("truez")) {
		set_truez(node["truez"].as_bool());
	} else if(type_ != NULL) {
		set_truez(type_->truez());
	}
	if(node.has_key("tx")) {
		set_tx(node["tx"].as_decimal().as_float());
	} else if(type_ != NULL) {
		set_tx(type_->tx());
	}
	if(node.has_key("ty")) {
		set_ty(node["ty"].as_decimal().as_float());
	} else if(type_ != NULL) {
		set_ty(type_->ty());
	}
	if(node.has_key("tz")) {
		set_tz(node["tz"].as_decimal().as_float());
	} else if(type_ != NULL) {
		set_tz(type_->tz());
	}

	const variant property_data_node = node["property_data"];
	for(int i = 0; i != type_->slot_properties().size(); ++i) {
		const custom_object_type::property_entry& e = type_->slot_properties()[i];
		if(e.storage_slot < 0) {
			continue;
		}

		bool set = false;

		if(property_data_node.is_map()) {
			const variant key(e.id);
			if(property_data_node.has_key(key)) {
				get_property_data(e.storage_slot) = property_data_node[key];
				set = true;
			}
		}

		if(!set) {
			if(e.init) {
				reference_counted_object_pin_norelease pin(this);
				get_property_data(e.storage_slot) = e.init->execute(*this);
			} else {
				get_property_data(e.storage_slot) = deep_copy_variant(e.default_value);
			}
		}

		if(!get_property_data(e.storage_slot).is_null()) {
			properties_requiring_dynamic_initialization_.erase(std::remove(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), i), properties_requiring_dynamic_initialization_.end());
		}
	}

	//fprintf(stderr, "object address= %p, ", this);
	//fprintf(stderr, "zsub_order=%d,", zsub_order_);
}

custom_object::custom_object(const std::string& type, int x, int y, bool face_right)
  : entity(x, y, face_right),
    previous_y_(y),
    type_(custom_object_type::get_or_die(type)),
	base_type_(type_),
	frame_(&type_->default_frame()),
    frame_name_("normal"),
	time_in_frame_(0), time_in_frame_delta_(1),
	velocity_x_(0), velocity_y_(0),
	accel_x_(0), accel_y_(0), gravity_shift_(0),
	rotate_x_(), rotate_y_(), rotate_z_(), zorder_(type_->zorder()),
	zsub_order_(type_->zsub_order()),
	hitpoints_(type_->hitpoints()),
	max_hitpoints_(0),
	was_underwater_(false),
	has_feet_(type_->has_feet()),
	invincible_(0),
	sound_volume_(128),
	vars_(new game_logic::formula_variable_storage(type_->variables())),
	tmp_vars_(new game_logic::formula_variable_storage(type_->tmp_variables())),
	tags_(new game_logic::map_formula_callable(type_->tags())),
	active_property_(-1),
	last_hit_by_anim_(0),
	cycle_(0),
	created_(false), loaded_(false), fall_through_platforms_(0),
	always_active_(false),
	activation_border_(type_->activation_border()),
	last_cycle_active_(0),
	parent_prev_x_(INT_MIN), parent_prev_y_(INT_MIN), parent_prev_facing_(true),
    relative_x_(0), relative_y_(0),
	swallow_mouse_event_(false),
	min_difficulty_(-1), max_difficulty_(-1),
	currently_handling_die_event_(0),
	use_absolute_screen_coordinates_(type_->use_absolute_screen_coordinates()),
	vertex_location_(-1), texcoord_location_(-1),
	paused_(false), model_(glm::mat4(1.0f))
{
	properties_requiring_dynamic_initialization_ = type_->properties_requiring_dynamic_initialization();
	properties_requiring_dynamic_initialization_.insert(properties_requiring_dynamic_initialization_.end(), type_->properties_requiring_initialization().begin(), type_->properties_requiring_initialization().end());

	vars_->set_object_name(debug_description());
	tmp_vars_->set_object_name(debug_description());

	vars_->disallow_new_keys(type_->is_strict());
	tmp_vars_->disallow_new_keys(type_->is_strict());

	for(std::map<std::string, custom_object_type::property_entry>::const_iterator i = type_->properties().begin(); i != type_->properties().end(); ++i) {
		if(i->second.storage_slot < 0) {
			continue;
		}

		get_property_data(i->second.storage_slot) = deep_copy_variant(i->second.default_value);
	}

	get_all().insert(this);
	get_all(base_type_->id()).insert(this);

#if defined(USE_SHADERS)
	if(type_->shader()) {
		shader_.reset(new gles2::shader_program(*type_->shader()));
	}
	effects_.clear();
	for(size_t n = 0; n < type_->effects().size(); ++n) {
		effects_.push_back(new gles2::shader_program(*type_->effects()[n]));
	}
#endif

#ifdef USE_BOX2D
	if(type_->body()) {
		body_.reset(new box2d::body(*type_->body()));
	}
#endif

	set_solid_dimensions(type_->solid_dimensions(),
	                     type_->weak_solid_dimensions());
	set_collide_dimensions(type_->collide_dimensions(),
	                       type_->weak_collide_dimensions());

	{
		//generate a random label for the object
		char buf[64];
		sprintf(buf, "_%x", rand());
		set_label(buf);
	}

	parallax_scale_millis_.reset(new std::pair<int, int>(type_->parallax_scale_millis_x(), type_->parallax_scale_millis_y()));

	assert(type_.get());
	set_frame_no_adjustments(frame_name_);

	next_animation_formula_ = type_->next_animation_formula();

#if defined(USE_LUA)
	if(!type_->get_lua_source().empty()) {
		lua_ptr_.reset(new lua::lua_context());
	}
#endif

	set_mouseover_delay(type_->get_mouseover_delay());
	if(type_->mouse_over_area().w() != 0) {
		set_mouse_over_area(type_->mouse_over_area());
	}
	set_truez(type_->truez());
	set_tx(type_->tx());
	set_ty(type_->ty());
	set_tz(type_->tz());
	//std::cerr << type << " " << truez() << " " << tx() << "," << ty() << "," << tz() << std::endl;
	init_properties();
}

custom_object::custom_object(const custom_object& o) :
	entity(o),
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
	rotate_x_(o.rotate_x_), rotate_y_(o.rotate_y_), rotate_z_(o.rotate_z_),
	parallax_scale_millis_(new std::pair<int, int>(*o.parallax_scale_millis_)),
	zorder_(o.zorder_),
	zsub_order_(o.zsub_order_),
	hitpoints_(o.hitpoints_),
	max_hitpoints_(o.max_hitpoints_),
	was_underwater_(o.was_underwater_),
	has_feet_(o.has_feet_),
	invincible_(o.invincible_),
	use_absolute_screen_coordinates_(o.use_absolute_screen_coordinates_),
	sound_volume_(o.sound_volume_),
	next_animation_formula_(o.next_animation_formula_),

	vars_(new game_logic::formula_variable_storage(*o.vars_)),
	tmp_vars_(new game_logic::formula_variable_storage(*o.tmp_vars_)),
	tags_(new game_logic::map_formula_callable(*o.tags_)),

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
	distortion_(o.distortion_),
	draw_color_(o.draw_color_ ? new graphics::color_transform(*o.draw_color_) : NULL),
	draw_scale_(o.draw_scale_ ? new decimal(*o.draw_scale_) : NULL),
	draw_area_(o.draw_area_ ? new rect(*o.draw_area_) : NULL),
	activation_area_(o.activation_area_ ? new rect(*o.activation_area_) : NULL),
	clip_area_(o.clip_area_ ? new rect(*o.clip_area_) : NULL),
	activation_border_(o.activation_border_),
	can_interact_with_(o.can_interact_with_),
	particle_systems_(o.particle_systems_),
	text_(o.text_),
	driver_(o.driver_),
	blur_(o.blur_),
	fall_through_platforms_(o.fall_through_platforms_),
	shader_(o.shader_),
	effects_(o.effects_),
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
	swallow_mouse_event_(false),
	currently_handling_die_event_(0),
	vertex_location_(o.vertex_location_), texcoord_location_(o.texcoord_location_),
	//do NOT copy widgets since they do not support deep copying
	//and re-seating references is difficult.
	//widgets_(o.widgets_),
	paused_(o.paused_), model_(glm::mat4(1.0f))
{
	vars_->set_object_name(debug_description());
	tmp_vars_->set_object_name(debug_description());

	vars_->disallow_new_keys(type_->is_strict());
	tmp_vars_->disallow_new_keys(type_->is_strict());

	get_all().insert(this);
	get_all(base_type_->id()).insert(this);

#if defined(USE_SHADERS)
	if(o.shader_) {
		shader_.reset(new gles2::shader_program(*o.shader_));
	}
	for(size_t n = 0; n < o.effects_.size(); ++n) {
		effects_.push_back(new gles2::shader_program(*o.effects_[n]));
	}
#endif

#ifdef USE_BOX2D
	std::stringstream ss;
	if(o.body_) {
		body_.reset(new box2d::body(*o.body_));
	}
#endif
	set_mouseover_delay(o.get_mouseover_delay());
	set_mouse_over_area(o.mouse_over_area());
	
	set_truez(o.truez());
	set_tx(o.tx());
	set_ty(o.ty());
	set_tz(o.tz());

#if defined(USE_LUA)
	if(!type_->get_lua_source().empty()) {
		lua_ptr_.reset(new lua::lua_context());
	}
#endif
}

custom_object::~custom_object()
{
	get_all().erase(this);
	get_all(base_type_->id()).erase(this);

	sound::stop_looped_sounds(this);
}

void custom_object::validate_properties()
{
	//TODO: make this more efficient. For now it errs on the side of
	//providing lots of debug info.
	for(int n = 0; n != type_->slot_properties().size(); ++n) {
		const custom_object_type::property_entry& e = type_->slot_properties()[n];
		if(e.storage_slot >= 0 && e.type && std::count(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), n) == 0) {
			assert(e.storage_slot < property_data_.size());
			variant result = property_data_[e.storage_slot];
			ASSERT_LOG(e.type->match(result), "Object " << debug_description() << " is invalid, property " << e.id << " expected to be " << e.type->to_string() << " but found " << result.write_json() << " which is of type " << get_variant_type_from_value(result)->to_string() << " " << properties_requiring_dynamic_initialization_.size());
			
		}
	}
}

void custom_object::init_properties()
{
	for(std::map<std::string, custom_object_type::property_entry>::const_iterator i = type_->properties().begin(); i != type_->properties().end(); ++i) {
		if(!i->second.init || i->second.storage_slot == -1) {
			continue;
		}

		reference_counted_object_pin_norelease pin(this);
		get_property_data(i->second.storage_slot) = i->second.init->execute(*this);
	}
}

bool custom_object::is_a(const std::string& type) const
{
	return custom_object_type::is_derived_from(type, type_->id());
}

void custom_object::finish_loading(level* lvl)
{
	if(parent_loading_.is_null() == false) {
		entity_ptr p = parent_loading_.try_convert<entity>();
		if(p) {
			parent_ = p;
		}
		parent_loading_ = variant();
	}
#if defined(USE_SHADERS)
	if(shader_) { shader_->init(this); }
	for(size_t n = 0; n < effects_.size(); ++n) {
		effects_[n]->init(this);
	}
#endif
#ifdef USE_BOX2D
	if(body_) {
		body_->finish_loading(this);
	}
#endif

#if defined(USE_LUA)
	init_lua();
#endif
}


#if defined(USE_LUA)
void custom_object::init_lua()
{
	if(lua_ptr_) {
		lua_ptr_->set_self_callable(*this);
		lua_chunk_.reset(lua_ptr_->compile_chunk(type_->id(), type_->get_lua_source()));
		lua_chunk_->run(lua_ptr_->context_ptr());
	}
}
#endif

bool custom_object::serializable() const
{
	return type_->serializable();
}

variant custom_object::write() const
{
	variant_builder res;

	char addr_buf[256];
	sprintf(addr_buf, "%p", this);
	res.add("_addr", addr_buf);

	if(created_) {
		res.add("created", true);
	}

	if(parallax_scale_millis_.get() != NULL) {
		if( (type_->parallax_scale_millis_x() !=  parallax_scale_millis_->first) || (type_->parallax_scale_millis_y() !=  parallax_scale_millis_->second)){
			res.add("parallax_scale_x", parallax_scale_millis_->first);
			res.add("parallax_scale_y", parallax_scale_millis_->second);
		}
	}

	if(platform_area_.get() != NULL) {
		res.add("platform_area", platform_area_->write());
	}

	if(always_active_) {
		res.add("always_active", true);
	}

	if(activation_border_ != type_->activation_border()) {
		res.add("activation_border", activation_border_);
	}
	
	if(position_schedule_.get() != NULL) {
		res.add("schedule_speed", position_schedule_->speed);
		if(position_schedule_->x_pos.empty() == false) {
			foreach(int xpos, position_schedule_->x_pos) {
				res.add("x_schedule", xpos);
			}
		}

		if(position_schedule_->y_pos.empty() == false) {
			foreach(int ypos, position_schedule_->y_pos) {
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

	if(!attached_objects().empty()) {
		std::string s;

		foreach(const entity_ptr& e, attached_objects()) {
			if(s.empty() == false) {
				s += ",";
			}

			char buf[256];
			sprintf(buf, "%p", e.get());
			s += buf;
		}

		res.add("attached_objects", s);
	}

	if(!current_variation_.empty()) {
		res.add("variations", util::join(current_variation_));
	}

	if(draw_color_ && (!draw_color_->fits_in_color() || draw_color_->to_color().value() != 0xFFFFFFFF)) {
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

	res.add("custom", true);
	res.add("type", type_->id());
	res.add("x", x());
	res.add("y", y());

	if(rotate_z_ != decimal()) {
		res.add("rotate", rotate_z_);
	}

    if(velocity_x_ != 0) {
        res.add("velocity_x", velocity_x_);
    }
    if(velocity_y_ != 0) {
        res.add("velocity_y", velocity_y_);
    }
	
	if(platform_motion_x()) {
		res.add("platform_motion_x", platform_motion_x());
	}

	if(solid_dimensions() != type_->solid_dimensions() ||
	   weak_solid_dimensions() != type_->weak_solid_dimensions()) {
		std::string solid_dim;
		for(int n = 0; n != 32; ++n) {
			if(solid_dimensions()&(1 << n)) {
				if(!solid_dim.empty()) {
					solid_dim += ",";
				}

				solid_dim += get_solid_dimension_key(n);
			}

			if(weak_solid_dimensions()&(1 << n)) {
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

	if(collide_dimensions() != type_->collide_dimensions() ||
	   weak_collide_dimensions() != type_->weak_collide_dimensions()) {
		std::string collide_dim, weak_collide_dim;
		for(int n = 0; n != 32; ++n) {
			if(collide_dimensions()&(1 << n)) {
				if(!collide_dim.empty()) {
					collide_dim += ",";
				}

				collide_dim += get_solid_dimension_key(n);
			}

			if(weak_collide_dimensions()&(1 << n)) {
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

	if(hitpoints_ != type_->hitpoints() || max_hitpoints_ != 0) {
		res.add("hitpoints", hitpoints_);
		res.add("max_hitpoints", type_->hitpoints() + max_hitpoints_);
	}

#if defined(USE_SHADERS)
	if(shader_ &&
	   (!type_->shader() || type_->shader()->name() != shader_->name())) {
		res.add("shader", shader_->write());
	}

	bool write_effects = effects_.size() != type_->effects().size();
	if(!write_effects) {
		for(size_t n = 0; n < effects_.size(); ++n) {
			if(effects_[n]->name() != type_->effects()[n]->name()) {
				write_effects = true;
				break;
			}
		}
	}

	if(write_effects) {
		for(size_t n = 0; n < effects_.size(); ++n) {
			res.add("effects", effects_[n]->write());
		}
	}
#endif

#if defined(USE_BOX2D)
	if(body_) {
		res.add("body", body_->write()); 
	}
#endif

	if(zorder_ != type_->zorder()) {
		res.add("zorder", zorder_);
	}

	if(parallax_scale_millis_.get()) {
		if(parallax_scale_millis_->first != type_->parallax_scale_millis_x() || parallax_scale_millis_->second != type_->parallax_scale_millis_y()){
			res.add("parallax_scale_x", parallax_scale_millis_->first);
			res.add("parallax_scale_y", parallax_scale_millis_->second);
		}
	}
	   
	if(zsub_order_ != type_->zsub_order()) {
		res.add("zsub_order", zsub_order_);
	}
	
    if(face_right() != 1){
        res.add("face_right", face_right());
    }
        
	if(upside_down()) {
		res.add("upside_down", true);
	}

    if(time_in_frame_ != 0) {
        res.add("time_in_frame", time_in_frame_);
    }
        
	if(time_in_frame_delta_ != 1) {
		res.add("time_in_frame_delta", time_in_frame_delta_);
	}

	if(has_feet_ != type_->has_feet()) {
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

	if(!vars_->equal_to(type_->variables())) {
		res.add("vars", vars_->write());
	}

	if(tags_->values() != type_->tags()) {
		res.add("tags", tags_->write());
	}

	std::map<variant, variant> property_map;
	for(std::map<std::string, custom_object_type::property_entry>::const_iterator i = type_->properties().begin(); i != type_->properties().end(); ++i) {
		if(i->second.storage_slot == -1 || i->second.storage_slot >= property_data_.size() || i->second.persistent == false || i->second.const_value || property_data_[i->second.storage_slot] == i->second.default_value) {
			continue;
		}

		if(!created_ && i->second.init && level::current_ptr() && level::current().in_editor() && !i->second.has_editor_info) {
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
		res.add("draw_area", draw_area_->to_string());
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
		for(std::map<std::string, particle_system_ptr>::const_iterator i = particle_systems_.begin(); i != particle_systems_.end(); ++i) {
			if(i->second->should_save() == false) {
				continue;
			}

			if(!systems.empty()) {
				systems += ",";
			}

			systems += i->first;
		}

		if(!systems.empty()) {
			res.add("particles", systems);
		}
	}

	foreach(const light_ptr& p, lights_) {
		res.add("lights", p->write());
	}

	if(parent_.get() != NULL) {
		std::string str;
		variant(parent_.get()).serialize_to_string(str);
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

	if(truez()) {
		res.add("truez", truez());
		res.add("tx", tx());
		res.add("ty", ty());
		res.add("tz", tz());
	}
	
	return res.build();
}

void custom_object::setup_drawing() const
{
	if(distortion_) {
		graphics::add_raster_distortion(distortion_.get());
	}
}

void custom_object::draw_later(int xx, int yy) const
{
	// custom object evil hackery part one.
	// Called nearer the end of rendering the scene, the
	// idea is to draw widgets with z-orders over the
	// threshold now rather than during the normal draw
	// processing.
	if(use_absolute_screen_coordinates_) {
		glPushMatrix();
		glTranslatef(GLfloat(xx), GLfloat(yy), 0.0);
		adjusted_draw_position_.x = xx;
		adjusted_draw_position_.y = yy;
	}
	glPushMatrix();
	glTranslatef(GLfloat(x()), GLfloat(y()), 0.0);
	foreach(const gui::widget_ptr& w, widgets_) {
		if(w->zorder() >= widget_zorder_draw_later_threshold) {
			w->draw();
		}
	}
	glPopMatrix();

	if(use_absolute_screen_coordinates_) {
		glPopMatrix();
	}
}

void custom_object::draw(int xx, int yy) const
{
	if(frame_ == NULL) {
		return;
	}

	if(use_absolute_screen_coordinates_) {
		glPushMatrix();
		glTranslatef(GLfloat(xx), GLfloat(yy), 0.0);
		adjusted_draw_position_.x = xx;
		adjusted_draw_position_.y = yy;
	}

	foreach(const entity_ptr& attached, attached_objects()) {
		if(attached->zorder() < zorder()) {
			attached->draw(xx, yy);
		}
	}

	if(type_->blend_mode()) {
		glBlendFunc(type_->blend_mode()->sfactor, type_->blend_mode()->dfactor);
	}

#if defined(USE_SHADERS)
	const gles2::shader_program_ptr active = gles2::active_shader();
#ifndef NO_EDITOR
	try {
#endif
	for(size_t n = 0; n < effects_.size(); ++n) {
		if(effects_[n]->zorder() < 0 && effects_[n]->enabled()) {
			effects_[n]->refresh_for_draw();
			gles2::manager gles2_manager(effects_[n]);
		}
	}

	gles2::manager manager(truez() ? 0 : shader_);
	if(shader_ && truez() == false) {
		shader_->refresh_for_draw();
	}
#endif

	boost::scoped_ptr<graphics::clip_scope> clip_scope;
	boost::scoped_ptr<graphics::stencil_scope> stencil_scope;
	if(clip_area_) {
		clip_scope.reset(new graphics::clip_scope(clip_area_->sdl_rect()));
	} else if(type_->is_shadow()) {
		stencil_scope.reset(new graphics::stencil_scope(true, 0x0, GL_EQUAL, 0x02, 0xFF, GL_KEEP, GL_KEEP, GL_KEEP));
	}

	if(driver_) {
		driver_->draw(xx, yy);
	}

	if(draw_color_) {
		draw_color_->to_color().set_as_current_color();
	}

	const int draw_x = x();
	const int draw_y = y();

	if(type_->hidden_in_game() && !level::current().in_editor()) {
		//pass
#if defined(USE_ISOMAP)
	} else if(truez()) {
		ASSERT_LOG(shader_ != NULL, "No shader found in the object, to use truez a shader must be given.");
		//XXX All this is a big hack till I fix up frames/objects to use shaders differently
		glUseProgram(shader_->shader()->get());
		if(vertex_location_ == -1) {
			vertex_location_ = shader_->shader()->get_attribute("a_position");
		}
		if(texcoord_location_ == -1) {
			texcoord_location_ = shader_->shader()->get_attribute("a_texcoord");
		}

		glm::mat4 flip(1.0f);
		if(face_right()) {
			flip = glm::rotate(glm::mat4(1.0f), 180.0f, glm::vec3(0.0f,1.0f,0.0f));
		}
		if(upside_down()) {
			flip = flip * glm::rotate(glm::mat4(1.0f), 180.0f, glm::vec3(1.0f,0.0f,0.0f));
		}
		GLfloat scale = draw_scale_ ? GLfloat(draw_scale_->as_float()) : 1.0f;
		glm::mat4 model = model_ * glm::translate(glm::mat4(1.0f), glm::vec3(tx(),ty(),tz()))
			* glm::rotate(glm::mat4(1.0f), GLfloat(rotate_x_.as_float()), glm::vec3(1.0f,0.0f,0.0f)) 
			* glm::rotate(glm::mat4(1.0f), GLfloat(rotate_y_.as_float()), glm::vec3(0.0f,1.0f,0.0f)) 
			* glm::rotate(glm::mat4(1.0f), GLfloat(rotate_z_.as_float()), glm::vec3(0.0f,0.0f,1.0f)) 
			* flip
			* glm::scale(glm::mat4(1.0f), glm::vec3(GLfloat(scale), GLfloat(scale), GLfloat(scale)));

		glm::mat4 mvp = level::current().projection_mat() * level::current().view_mat() * model;
		glUniformMatrix4fv(shader_->shader()->mvp_matrix_uniform(), 1, GL_FALSE, glm::value_ptr(mvp));

		frame_->draw3(time_in_frame_, vertex_location_, texcoord_location_);
		glUseProgram(active->shader()->get());
#endif
	} else if(custom_draw_xy_.size() >= 6 &&
	          custom_draw_xy_.size() == custom_draw_uv_.size()) {
		frame_->draw_custom(draw_x-draw_x%2, draw_y-draw_y%2, &custom_draw_xy_[0], &custom_draw_uv_[0], custom_draw_xy_.size()/2, face_right(), upside_down(), time_in_frame_, GLfloat(rotate_z_.as_float()), cycle_);
	} else if(custom_draw_.get() != NULL) {
		frame_->draw_custom(draw_x-draw_x%2, draw_y-draw_y%2, *custom_draw_, draw_area_.get(), face_right(), upside_down(), time_in_frame_, GLfloat(rotate_z_.as_float()));
	} else if(draw_scale_) {
		frame_->draw(draw_x-draw_x%2, draw_y-draw_y%2, face_right(), upside_down(), time_in_frame_, GLfloat(rotate_z_.as_float()), GLfloat(draw_scale_->as_float()));
	} else if(!draw_area_.get()) {
		frame_->draw(draw_x-draw_x%2, draw_y-draw_y%2, face_right(), upside_down(), time_in_frame_, GLfloat(rotate_z_.as_float()));
	} else {
		frame_->draw(draw_x-draw_x%2, draw_y-draw_y%2, *draw_area_, face_right(), upside_down(), time_in_frame_, GLfloat(rotate_z_.as_float()));
	}

	if(blur_) {
		blur_->draw();
	}

	if(draw_color_) {
		if(!draw_color_->fits_in_color()) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			graphics::color_transform transform = *draw_color_;
			while(!transform.fits_in_color()) {
				transform = transform - transform.to_color();
				transform.to_color().set_as_current_color();
				frame_->draw(draw_x-draw_x%2, draw_y-draw_y%2, face_right(), upside_down(), time_in_frame_, GLfloat(rotate_z_.as_float()));
			}

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		glColor4ub(255, 255, 255, 255);
	}

	foreach(const entity_ptr& attached, attached_objects()) {
		if(attached->zorder() >= zorder()) {
			attached->draw(xx, yy);
		}
	}

//	if(draw_color_int_ != DefaultColor) {
//		glColor4ub(255, 255, 255, 255);
//	}

#if defined(USE_SHADERS)
	foreach(const graphics::draw_primitive_ptr& p, draw_primitives_) {
		p->draw();
	}
#endif

	draw_debug_rects();

	glPushMatrix();
	glTranslatef(GLfloat(x()), GLfloat(y()), 0.0);
	foreach(const gui::widget_ptr& w, widgets_) {
		if(w->zorder() < widget_zorder_draw_later_threshold) {
			if(w->draw_with_object_shader()) {
				w->draw();
			}
		}
	}
	foreach(const gui::vector_text_ptr& txt, vector_text_) {
		txt->draw();
	}
	glPopMatrix();

	for(std::map<std::string, particle_system_ptr>::const_iterator i = particle_systems_.begin(); i != particle_systems_.end(); ++i) {
		i->second->draw(rect(last_draw_position().x/100, last_draw_position().y/100, graphics::screen_width(), graphics::screen_height()), *this);
	}

	if(text_ && text_->font && text_->alpha) {
		glColor4ub(255, 255, 255, text_->alpha);
		const int half_width = midpoint().x - draw_x;
		int xpos = draw_x;
		if(text_->align == 0) {
			xpos += half_width - text_->dimensions.w()/2;
		} else if(text_->align > 0) {
			xpos += half_width*2 - text_->dimensions.w();
		}
		text_->font->draw(xpos, draw_y, text_->text, text_->size);

		glColor4ub(255, 255, 255, 255);
	}
	
	clip_scope.reset();

#if defined(USE_SHADERS)
	for(size_t n = 0; n < effects_.size(); ++n) {
		if(effects_[n]->zorder() >= 0 && effects_[n]->enabled()) {
			gles2::manager gles2_manager(effects_[n]);
		}
	}
#endif

	if(level::current().debug_properties().empty() == false) {
		std::vector<graphics::texture> left, right;
		int max_property_width = 0;
		foreach(const std::string& s, level::current().debug_properties()) {
			try {
				const assert_recover_scope scope;
				variant result = game_logic::formula(variant(s)).execute(*this);
				const std::string result_str = result.write_json();
				graphics::texture key_texture = font::render_text(s, graphics::color_white(), 16);
				graphics::texture value_texture = font::render_text(result_str, graphics::color_white(), 16);
				left.push_back(key_texture);
				right.push_back(value_texture);
	
				if(key_texture.width() > size_t(max_property_width)) {
					max_property_width = key_texture.width();
				}
			} catch(validation_failure_exception&) {
			}
		}

		int pos = y();
		for(int n = 0; n != left.size(); ++n) {
			const int xpos = midpoint().x + 10;
			graphics::blit_texture(left[n], xpos, pos);
			graphics::blit_texture(right[n], xpos + max_property_width + 10, pos);
			pos += std::max(left[n].height(), right[n].height());
		}
	}

	if(platform_area_ && (preferences::show_debug_hitboxes() || !platform_offsets_.empty() && level::current().in_editor())) {
		std::vector<GLfloat> v;
		const rect& r = platform_rect();
		for(int x = 0; x < r.w(); x += 2) {
			v.push_back(GLfloat(r.x() + x));
			v.push_back(GLfloat(platform_rect_at(r.x() + x).y()));
		}

		if(!v.empty()) {
#if defined(USE_SHADERS)
			glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
			glPointSize(2.0f);
			gles2::manager gles2_manager(gles2::get_simple_shader());
			gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &v[0]);
			glDrawArrays(GL_POINTS, 0, v.size()/2);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
			glPointSize(2);
			glDisable(GL_TEXTURE_2D);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glColor4ub(255, 0, 0, 255);

			glVertexPointer(2, GL_FLOAT, 0, &v[0]);
			glDrawArrays(GL_POINTS, 0, v.size()/2);

			glColor4ub(255, 255, 255, 255);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnable(GL_TEXTURE_2D);
#endif
		}
	}

#if defined(USE_SHADERS) && !defined(NO_EDITOR)
	//catch errors that result from bad shaders etc while in the editor.
	} catch(validation_failure_exception& e) {
		gles2::shader::set_runtime_error("HEX MAP SHADER ERROR: " + e.msg);
	}
#endif

	glPushMatrix();
	glTranslatef(GLfloat(x()&~1), GLfloat(y()&~1), 0.0);
	foreach(const gui::widget_ptr& w, widgets_) {
		if(w->zorder() < widget_zorder_draw_later_threshold) {
			if(w->draw_with_object_shader() == false) {
				w->draw();
			}
		}
	}
	glPopMatrix();

	if(use_absolute_screen_coordinates_) {
		glPopMatrix();
	}

	if(type_->blend_mode()) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
}

void custom_object::draw_group() const
{
	if(label().empty() == false && label()[0] != '_') {
		blit_texture(font::render_text(label(), graphics::color_yellow(), 32), x(), y() + 26);
	}

	if(group() >= 0) {
		blit_texture(font::render_text(formatter() << group(), graphics::color_yellow(), 24), x(), y());
	}
}

void custom_object::construct()
{
	handle_event(OBJECT_EVENT_CONSTRUCT);
}

bool custom_object::create_object()
{
	if(!created_) {
		validate_properties();
		created_ = true;
		handle_event(OBJECT_EVENT_CREATE);
		ASSERT_LOG(properties_requiring_dynamic_initialization_.empty(), "Object property " << debug_description() << "." << type_->slot_properties()[properties_requiring_dynamic_initialization_.front()].id << " not initialized at end of on_create.");
		return true;
	}

	return false;
}

void custom_object::check_initialized()
{
	ASSERT_LOG(properties_requiring_dynamic_initialization_.empty(), "Object property " << debug_description() << "." << type_->slot_properties()[properties_requiring_dynamic_initialization_.front()].id << " not initialized");

	validate_properties();
}

void custom_object::process(level& lvl)
{
	if(paused_) {
		return;
	}

#if defined(USE_BOX2D)
	box2d::world_ptr world = box2d::world::our_world_ptr();
	if(body_) {
		const b2Vec2 v = body_->get_body_ptr()->GetPosition();
		const float a = body_->get_body_ptr()->GetAngle();
		rotate_z_ = decimal(double(a) * 180.0 / M_PI);
		set_x(int(v.x * world->scale() - (solid_rect().w() ? (solid_rect().w()/2) : current_frame().width()/2)));
		set_y(int(v.y * world->scale() - (solid_rect().h() ? (solid_rect().h()/2) : current_frame().height()/2)));
		//set_y(graphics::screen_height() - v.y * world->scale() - current_frame().height());
		/*set_x((v.x + world->x1()) * graphics::screen_width() / (world->x2() - world->x1()));
		if(world->y2() < 0) {
			set_y(graphics::screen_height() - (v.y + world->y1()) * graphics::screen_height() / -(world->y2() + world->y1()));
		} else {
			set_y((v.y + world->y1()) * graphics::screen_height() / (world->y2() - world->y1()));
		}*/
	}
#endif

	if(type_->use_image_for_collisions()) {
		//anything that uses their image for collisions is a static,
		//un-moving object that will stay immobile.
		return;
	}

	if(lvl.in_editor()) {
		if(!type_->static_object() && entity_collides(level::current(), *this, MOVE_NONE)) {
			//The object collides illegally, but we're in the editor. Freeze
			//the object by returning, since we can't process it.
			return;
		}

		if(level::current().is_editor_dragging_objects() && std::count(level::current().editor_selection().begin(), level::current().editor_selection().end(), entity_ptr(this))) {
			//this object is being dragged and so gets frozen.
			return;
		}
	}

	collision_info debug_collide_info;
	ASSERT_LOG(type_->static_object() || lvl.in_editor() || !entity_collides(level::current(), *this, MOVE_NONE, &debug_collide_info), "ENTITY " << debug_description() << " COLLIDES WITH " << (debug_collide_info.collide_with ? debug_collide_info.collide_with->debug_description() : "THE LEVEL") << " AT START OF PROCESS");

	if(parent_.get() != NULL) {
		const point pos = parent_position();
		const bool parent_facing = parent_->face_right();
        const int parent_facing_sign = parent_->face_right() ? 1 : -1;

		if(parent_prev_x_ != INT_MIN) {
            set_mid_x(pos.x + (relative_x_ * parent_facing_sign));
            set_mid_y(pos.y + relative_y_);
   		}

		parent_prev_x_ = pos.x;
		parent_prev_y_ = pos.y;
		parent_prev_facing_ = parent_facing;
	}

	if(last_cycle_active_ < lvl.cycle() - 5) {
		handle_event(OBJECT_EVENT_BECOME_ACTIVE);
	}

	last_cycle_active_ = lvl.cycle();

	entity::process(lvl);

	//the object should never be colliding with the level at the start of processing.
//	assert(!entity_collides_with_level(lvl, *this, MOVE_NONE));
//	assert(!entity_collides(lvl, *this, MOVE_NONE));

	//this is a flag which tracks whether we've fired a collide_feet
	//event. If we don't fire a collide_feet event through normal collision
	//detection, but we change the object we're standing on, we should
	//still fire a collide_feet event.
	bool fired_collide_feet = false;

	collision_info stand_info;
	const bool started_standing = is_standing(lvl, &stand_info) != NOT_STANDING;
	if(!started_standing && standing_on_) {
		//if we were standing on something the previous frame, but aren't
		//standing any longer, we use the value of what we were previously
		//standing on.
		stand_info.traction = standing_on_->surface_traction();
		stand_info.friction = standing_on_->surface_friction();
	} else if(!standing_on_ && started_standing && stand_info.collide_with && velocity_y_ >= 0 && !fired_collide_feet) {
		//We weren't standing on something last frame, but now we suddenly
		//are. We should fire a collide_feet event as a result.

		game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
		variant v(callable);
	
		if(stand_info.area_id != NULL) {
			callable->add("area", variant(*stand_info.area_id));
		}

		if(stand_info.collide_with) {
			callable->add("collide_with", variant(stand_info.collide_with.get()));
			if(stand_info.collide_with_area_id) {
				callable->add("collide_with_area", variant(*stand_info.collide_with_area_id));
			}

		}

		handle_event(OBJECT_EVENT_COLLIDE_FEET, callable);
		fired_collide_feet = true;
	}

	if(y() > lvl.boundaries().y2() || y() < lvl.boundaries().y() || x() > lvl.boundaries().x2() || x() < lvl.boundaries().x()) {
		handle_event(OBJECT_EVENT_OUTSIDE_LEVEL);
	}
	
	previous_y_ = y();
	if(started_standing && velocity_y_ > 0) {
		velocity_y_ = 0;
	}

	const int start_x = x();
	const int start_y = y();
	const decimal start_rotate = rotate_z_;
	++cycle_;

	if(invincible_) {
		--invincible_;
	}

	if(!loaded_) {
		handle_event(OBJECT_EVENT_LOAD);
		loaded_ = true;
	}

	create_object();

	if(cycle_ == 1) {
		//these events are for backwards compatibility. It's not recommended
		//to use them for new objects.
		handle_event("first_cycle");
		handle_event(OBJECT_EVENT_DONE_CREATE);
	}

	std::vector<variant> scheduled_commands = pop_scheduled_commands();
	foreach(const variant& cmd, scheduled_commands) {
		execute_command(cmd);
	}

	std::vector<std::pair<variant,variant> > follow_ons;

	if(!animated_movement_.empty()) {
		std::vector<boost::shared_ptr<AnimatedMovement> > movement = animated_movement_, removal;
		for(int i = 0; i != movement.size(); ++i) {
			auto& move = movement[i];

			if(move->pos >= move->animation_frames()) {
				if(move->on_complete.is_null() == false) {
					execute_command(move->on_complete);
				}

				follow_ons.insert(follow_ons.end(), move->follow_on.begin(), move->follow_on.end());

				removal.push_back(move);
			} else {
				ASSERT_LOG(move->animation_values.size()%move->animation_slots.size() == 0, "Bad animation sizes");
				variant* v = &move->animation_values[0] + move->pos*move->animation_slots.size();
	
				for(int n = 0; n != move->animation_slots.size(); ++n) {
					mutate_value_by_slot(move->animation_slots[n], v[n]);
				}

				if(move->on_process.is_null() == false) {
					execute_command(move->on_process);
				}
	
				move->pos++;
			}
		}

		for(auto& move : animated_movement_) {
			if(std::count(removal.begin(), removal.end(), move)) {
				move.reset();
			}
		}

		animated_movement_.erase(std::remove(animated_movement_.begin(), animated_movement_.end(), boost::shared_ptr<AnimatedMovement>()), animated_movement_.end());
	}

	for(const auto& p : follow_ons) {
		add_animated_movement(p.first, p.second);
	}

	if(position_schedule_.get() != NULL) {
		const int pos = (cycle_ - position_schedule_->base_cycle)/position_schedule_->speed;

		if(position_schedule_->expires &&
		   size_t(pos) >= position_schedule_->x_pos.size() &&
		   size_t(pos) >= position_schedule_->y_pos.size() &&
		   size_t(pos) >= position_schedule_->rotation.size()) {
			handle_event(OBJECT_EVENT_SCHEDULE_FINISHED);
			position_schedule_.reset();
		} else {

			const int next_fraction = (cycle_ - position_schedule_->base_cycle)%position_schedule_->speed;
			const int this_fraction = position_schedule_->speed - next_fraction;

			int xpos = INT_MIN, ypos = INT_MIN;
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

			if(xpos != INT_MIN && ypos != INT_MIN) {
				set_pos(xpos, ypos);
			} else if(xpos != INT_MIN) {
				set_x(xpos);
			} else if(ypos != INT_MIN) {
				set_y(ypos);
			}

			if(position_schedule_->rotation.empty() == false) {
				rotate_z_ = position_schedule_->rotation[pos%position_schedule_->rotation.size()];
				while(rotate_z_ >= 360) {
					rotate_z_ -= 360;
				}

				if(next_fraction) {
					rotate_z_ = decimal((rotate_z_*this_fraction + next_fraction*position_schedule_->rotation[(pos+1)%position_schedule_->rotation.size()])/position_schedule_->speed);
				}
			}
		}
	}

	if(stand_info.damage) {
		game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
		callable->add("surface_damage", variant(stand_info.damage));
		variant v(callable);
		handle_event(OBJECT_EVENT_COLLIDE_DAMAGE, callable);

		//DEPRECATED -- can we remove surface_damage and just have
		//collide_damage?
		handle_event(OBJECT_EVENT_SURFACE_DAMAGE, callable);
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
		handle_event(frame_->end_event_id());
		handle_event(OBJECT_EVENT_END_ANIM);
		if(next_animation_formula_) {
			variant var = next_animation_formula_->execute(*this);
			set_frame(var.as_string());
		}
	}

	const std::string* event = frame_->get_event(time_in_frame_);
	if(event) {
		handle_event(*event);
	}

	rect water_bounds;
	const bool is_underwater = solid() && lvl.is_underwater(solid_rect(), &water_bounds);
	
	if( is_underwater && !was_underwater_){
		//event on_enter_water
		handle_event(OBJECT_EVENT_ENTER_WATER);
		was_underwater_ = true;
	}else if ( !is_underwater && was_underwater_ ){
		//event on_exit_water
		handle_event(OBJECT_EVENT_EXIT_WATER);
		was_underwater_ = false;
	}

	previous_water_bounds_ = water_bounds;
	
	if(type_->static_object()) {
		static_process(lvl);
		return;
	}

	const int traction_from_surface = (stand_info.traction*type_->traction())/1000;
	velocity_x_ += (accel_x_ * (stand_info.traction ? traction_from_surface : (is_underwater?type_->traction_in_water() : type_->traction_in_air())) * (face_right() ? 1 : -1))/1000;
	if(!standing_on_ && !started_standing || accel_y_ < 0) {
		//do not accelerate downwards if standing on something.
		velocity_y_ += accel_y_ * (gravity_shift_ + (is_underwater ? type_->traction_in_water() : 1000))/1000;
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

	if(type_->affected_by_currents()) {
		lvl.get_current(*this, &velocity_x_, &velocity_y_);
	}

	bool collide = false;

	//calculate velocity which takes into account velocity of the object we're standing on.
	int effective_velocity_x = velocity_x_;
	int effective_velocity_y = velocity_y_;

	if(effective_velocity_y > 0 && (standing_on_ || started_standing)) {
		effective_velocity_y = 0;
	}

	int platform_motion_x_movement = 0;
	if(standing_on_) {

		platform_motion_x_movement = standing_on_->platform_motion_x() + standing_on_->map_platform_pos(feet_x())*100;
		effective_velocity_x += (standing_on_->feet_x() - standing_on_prev_x_)*100 + platform_motion_x_movement;
		effective_velocity_y += (standing_on_->feet_y() - standing_on_prev_y_)*100;
	}

	if(stand_info.collide_with != standing_on_ && stand_info.adjust_y) {
		//if we're landing on a new platform, we might have to adjust our
		//y position to suit its last movement and put us on top of
		//the platform.

		effective_velocity_y = stand_info.adjust_y*100;
	}

	if(effective_velocity_x || effective_velocity_y) {
		if(!solid() && !type_->object_level_collisions()) {
			move_centipixels(effective_velocity_x, effective_velocity_y);
			effective_velocity_x = 0;
			effective_velocity_y = 0;
		} else if(!has_feet() && solid()) {
			move_centipixels(effective_velocity_x, effective_velocity_y);
			if(is_flightpath_clear(lvl, *this, solid_rect())) {
				effective_velocity_x = 0;
				effective_velocity_y = 0;
			} else {
				//we can't guarantee smooth movement to this location, so
				//roll the move back and we'll do a pixel-by-pixel move
				//until we collide.
				move_centipixels(-effective_velocity_x, -effective_velocity_y);
			}
		}
	}


	collision_info collide_info;
	collision_info jump_on_info;

	bool is_stuck = false;

	collide = false;
	int move_left;
	for(move_left = std::abs(effective_velocity_y); move_left > 0 && !collide && !type_->ignore_collide(); move_left -= 100) {
		const int dir = effective_velocity_y > 0 ? 1 : -1;
		int damage = 0;

		const int original_centi_y = centi_y();

		const int move_amount = std::min(std::max(move_left, 0), 100);
		
		const bool moved = move_centipixels(0, move_amount*dir);
		if(!moved) {
			//we didn't actually move any pixels, so just abort.
			break;
		}

		if(type_->object_level_collisions() && non_solid_entity_collides_with_level(lvl, *this)) {
			handle_event(OBJECT_EVENT_COLLIDE_LEVEL);
		}

		if(effective_velocity_y > 0) {
			if(entity_collides(lvl, *this, MOVE_DOWN, &collide_info)) {
				//our 'legs' but not our feet collide with the level. Try to
				//move one pixel to the left or right and see if either
				//direction makes us no longer colliding.
				set_x(x() + 1);
				if(entity_collides(lvl, *this, MOVE_DOWN) || entity_collides(lvl, *this, MOVE_RIGHT)) {
					set_x(x() - 2);
					if(entity_collides(lvl, *this, MOVE_DOWN) || entity_collides(lvl, *this, MOVE_LEFT)) {
						//moving in either direction fails to resolve the collision.
						//This effectively means the object is 'stuck' in a small
						//pit.
						set_x(x() + 1);
						move_centipixels(0, -move_amount*dir);
						collide = true;
						is_stuck = true;
						break;
					}
				}
				

			}
		} else {
			//effective_velocity_y < 0 -- going up
			if(entity_collides(lvl, *this, MOVE_UP, &collide_info)) {
				collide = true;
				move_centipixels(0, -move_amount*dir);
				break;
			}
		}

		if(!collide && !type_->ignore_collide() && effective_velocity_y > 0 && is_standing(lvl, &jump_on_info)) {
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
		handle_event(OBJECT_EVENT_STUCK);
	}

	if(collide) {
		if(effective_velocity_y > 0) {
			vertical_landed = true;
		}

		if(!fired_collide_feet && (effective_velocity_y < 0 || !started_standing)) {

			game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
			variant v(callable);
	
			if(collide_info.area_id != NULL) {
				callable->add("area", variant(*collide_info.area_id));
			}

			if(collide_info.collide_with) {
				callable->add("collide_with", variant(collide_info.collide_with.get()));
				if(collide_info.collide_with_area_id) {
					callable->add("collide_with_area", variant(*collide_info.collide_with_area_id));
				}

			}

			handle_event(effective_velocity_y < 0 ? OBJECT_EVENT_COLLIDE_HEAD : OBJECT_EVENT_COLLIDE_FEET, callable);
			fired_collide_feet = true;
		}

		if(collide_info.damage || jump_on_info.damage) {
			game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
			callable->add("surface_damage", variant(std::max(collide_info.damage, jump_on_info.damage)));
			variant v(callable);
			handle_event(OBJECT_EVENT_COLLIDE_DAMAGE, callable);
		}
	}

	//If the object started out standing on a platform, keep it doing so.
	if(standing_on_ && !fall_through_platforms_ && velocity_y_ >= 0) {
		const int left_foot = feet_x() - type_->feet_width();
		const int right_foot = feet_x() + type_->feet_width();

		int target_y = INT_MAX;
		rect area = standing_on_->platform_rect();
		if(left_foot >= area.x() && left_foot < area.x() + area.w()) {
			rect area = standing_on_->platform_rect_at(left_foot);
			target_y = area.y();
		}

		if(right_foot >= area.x() && right_foot < area.x() + area.w()) {
			rect area = standing_on_->platform_rect_at(right_foot);
			if(area.y() < target_y) {
				target_y = area.y();
			}
		}

		if(target_y != INT_MAX) {
			const int delta = target_y - feet_y();
			const int dir = delta > 0 ? 1 : -1;
			int nmoves = 0;
			for(int n = 0; n != delta; n += dir) {
				set_y(y()+dir);
				++nmoves;
				if(entity_collides(lvl, *this, dir < 0 ? MOVE_UP : MOVE_DOWN)) {
					set_y(y()-dir);
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
		const int backup_centi_x = centi_x();
		const int backup_centi_y = centi_y();


		for(move_left = std::abs(effective_velocity_x); move_left > 0 && !collide && !type_->ignore_collide(); move_left -= 100) {
			if(type_->object_level_collisions() && non_solid_entity_collides_with_level(lvl, *this)) {
				handle_event(OBJECT_EVENT_COLLIDE_LEVEL);
			}

			const STANDING_STATUS previous_standing = is_standing(lvl);

			const int dir = effective_velocity_x > 0 ? 1 : -1;
			const int original_centi_y = centi_y();

			const int move_amount = std::min(std::max(move_left, 0), 100);
		
			const bool moved = move_centipixels(move_amount*dir, 0);
			if(!moved) {
				//we didn't actually move any pixels, so just abort.
				break;
			}

			const int left_foot = feet_x() - type_->feet_width();
			const int right_foot = feet_x() + type_->feet_width();
			bool place_on_object = false;
			if(standing_on_ && !fall_through_platforms_ && velocity_y_ >= 0) {
				rect area = standing_on_->platform_rect();
				if(left_foot >= area.x() && left_foot < area.x() + area.w() ||
					right_foot >= area.x() && right_foot < area.x() + area.w()) {
					place_on_object = true;
				}
			}

			//if we go up or down a slope, and we began the frame standing,
			//move the character up or down as appropriate to try to keep
			//them standing.

			const STANDING_STATUS standing = is_standing(lvl);
			if(place_on_object) {
				int target_y = INT_MAX;
				rect area = standing_on_->platform_rect();
				if(left_foot >= area.x() && left_foot < area.x() + area.w()) {
					const rect area = standing_on_->platform_rect_at(left_foot);
					target_y = area.y();
				}

				if(right_foot >= area.x() && right_foot < area.x() + area.w()) {
					const rect area = standing_on_->platform_rect_at(right_foot);
					if(area.y() < target_y) {
						target_y = area.y();
					}
				}

				const int delta = target_y - feet_y();
				const int dir = delta > 0 ? 1 : -1;
				for(int n = 0; n != delta; n += dir) {
					set_y(y()+dir);
					if(detect_collisions && entity_collides(lvl, *this, dir < 0 ? MOVE_UP : MOVE_DOWN)) {
						set_y(y()-dir);
						break;
					}
				}
			} else if(previous_standing && standing < previous_standing) {

				//we were standing, but we're not now. We want to look for
				//slopes that will enable us to still be standing. We see
				//if the object is trying to walk down stairs, in which case
				//we look downwards first, otherwise we look upwards first,
				//then downwards.
				int dir = walk_up_or_down_stairs() > 0 ? 1 : -1;

				for(int tries = 0; tries != 2; ++tries) {
					bool resolved = false;
					const int SearchRange = 2;
					for(int n = 0; n != SearchRange; ++n) {
						set_y(y()+dir);
						if(detect_collisions && entity_collides(lvl, *this, dir < 0 ? MOVE_UP : MOVE_DOWN)) {
							break;
						}

						if(is_standing(lvl) >= previous_standing) {
							resolved = true;
							break;
						}
					}

					if(resolved) {
						break;
					}

					dir *= -1;
					set_centi_y(original_centi_y);
				}
			} else if(standing) {
				if(!vertical_landed && !started_standing && !standing_on_) {
					horizontal_landed = true;
				}

				collision_info slope_standing_info;

				bool collide_head = false;

				//we are standing, but we need to see if we should be standing
				//on a higher point. If there are solid points immediately above
				//where we are, we adjust our feet to be on them.
				//
				//However, if there is a platform immediately above us, we only
				//adjust our feet upward if the object is trying to walk up
				//stairs, normally by the player pressing up while walking.
				const int begin_y = feet_y();
				int max_slope = 5;
				while(--max_slope && is_standing(lvl, &slope_standing_info)) {
					if(slope_standing_info.platform && walk_up_or_down_stairs() >= 0) {
						if(max_slope == 4) {
							//we always move at least one pixel up, if there is
							//solid, otherwise we'll fall through.
							set_y(y()-1);
							if(detect_collisions && entity_collides(lvl, *this, MOVE_UP)) {
								collide_head = true;
								break;
							}
						}
						break;
					}
	
					set_y(y()-1);
					if(detect_collisions && entity_collides(lvl, *this, MOVE_UP)) {
						collide_head = true;
						break;
					}
				}
	
				if(!max_slope || collide_head) {
					set_centi_y(original_centi_y);
				} else {
					set_y(y()+1);
				}
	
				if(walk_up_or_down_stairs() > 0) {
					//if we are trying to walk down stairs and we're on a platform
					//and one pixel below is walkable, then we move down by
					//one pixel.
					is_standing(lvl, &slope_standing_info);
					if(slope_standing_info.platform) {
						set_y(y()+1);
						if(!is_standing(lvl) || detect_collisions && entity_collides(lvl, *this, MOVE_DOWN)) {
							set_y(y()-1);
						}
					}
				}
			}

			if(detect_collisions && entity_collides(lvl, *this, centi_y() != original_centi_y ? MOVE_NONE : (dir > 0 ? MOVE_RIGHT : MOVE_LEFT), &collide_info)) {
				collide = true;
			}

			if(collide) {
				//undo the move to cancel out the collision
				move_centipixels(-dir*move_amount, 0);
				set_centi_y(original_centi_y);
				break;
			}
		}

		if(!detect_collisions) {
			if(entity_collides(lvl, *this, MOVE_NONE)) {
				set_centi_x(backup_centi_x);
				set_centi_y(backup_centi_y);
			} else {
				break;
			}
		}
	}

	if(collide || horizontal_landed) {

		game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
		variant v(callable);

		if(collide_info.area_id != NULL) {
			callable->add("area", variant(*collide_info.area_id));
		}

		if(collide_info.collide_with) {
			callable->add("collide_with", variant(collide_info.collide_with.get()));
			if(collide_info.collide_with_area_id) {
				callable->add("collide_with_area", variant(*collide_info.collide_with_area_id));
			}
		}

		handle_event(collide ? OBJECT_EVENT_COLLIDE_SIDE : OBJECT_EVENT_COLLIDE_FEET, callable);
		fired_collide_feet = true;
		if(collide_info.damage) {
			game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
			callable->add("surface_damage", variant(collide_info.damage));
			variant v(callable);
			handle_event(OBJECT_EVENT_COLLIDE_DAMAGE, callable);
		}
	}

	stand_info = collision_info();
	if(velocity_y_ >= 0) {
		is_standing(lvl, &stand_info);
	}

	if(stand_info.collide_with && standing_on_ != stand_info.collide_with &&
	   effective_velocity_y < stand_info.collide_with->velocity_y()) {
		stand_info.collide_with = NULL;
	}

	if(standing_on_ && standing_on_ != stand_info.collide_with) {
		//we were previously standing on an object and we're not anymore.
		//add the object we were standing on's velocity to ours
		velocity_x_ += standing_on_->last_move_x()*100 + platform_motion_x_movement;
		velocity_y_ += standing_on_->last_move_y()*100;
	}

	if(stand_info.collide_with && standing_on_ != stand_info.collide_with) {
		if(!fired_collide_feet) {
		}

		//we are standing on a new object. Adjust our velocity relative to
		//the object we're standing on
		velocity_x_ -= stand_info.collide_with->last_move_x()*100 + stand_info.collide_with->platform_motion_x();
		velocity_y_ = 0;

		game_logic::map_formula_callable* callable(new game_logic::map_formula_callable(this));
		callable->add("jumped_on_by", variant(this));
		game_logic::formula_callable_ptr callable_ptr(callable);

		stand_info.collide_with->handle_event(OBJECT_EVENT_JUMPED_ON, callable);
	}

	standing_on_ = stand_info.collide_with;
	if(standing_on_) {
		standing_on_prev_x_ = standing_on_->feet_x();
		standing_on_prev_y_ = standing_on_->feet_y();
	}

	if(lvl.players().empty() == false) {
		lvl.set_touched_player(lvl.players().front());
	}

	if(fall_through_platforms_ > 0) {
		--fall_through_platforms_;
	}

	if(blur_) {
		blur_->next_frame(start_x, start_y, x(), y(), frame_.get(), time_in_frame_, face_right(), upside_down(), float(start_rotate.as_float()), float(rotate_z_.as_float()));
		if(blur_->destroyed()) {
			blur_.reset();
		}
	}

#if defined(USE_BOX2D)
	if(body_) {
		for(b2ContactEdge* ce = body_->get_body_ptr()->GetContactList(); ce != NULL; ce = ce->next) {
			b2Contact* c = ce->contact;
			// process c
			if(c->IsTouching()) {
				using namespace game_logic;
				//std::cerr << "bodies touching: 0x" << std::hex << uint32_t(body_->get_body_ptr()) << " 0x" << uint32_t(ce->other) << std::dec << std::endl;
				//b2WorldManifold wmf;
				//c->GetWorldManifold(&wmf);
				//std::cerr << "Collision points: " << wmf.points[0].x << ", " << wmf.points[0].y << "; " << wmf.points[1].x << "," << wmf.points[1].y << "; " << wmf.normal.x << "," << wmf.normal.y << std::endl;
				map_formula_callable_ptr fc = map_formula_callable_ptr(new map_formula_callable);
				fc->add("collide_with", variant((box2d::body*)ce->other->GetUserData()));
				handle_event("b2collide", fc.get());
			}
			//c->GetManifold()->
		}
	}
#endif

	if(level::current().cycle() > int(get_mouseover_trigger_cycle())) {
		if(is_mouse_over_entity() == false) {
			game_logic::map_formula_callable_ptr callable(new game_logic::map_formula_callable);
			int mx, my;
			input::sdl_get_mouse_state(&mx, &my);
			callable->add("mouse_x", variant(mx));
			callable->add("mouse_y", variant(my));
			handle_event("mouse_enter", callable.get());
			set_mouse_over_entity();
			set_mouseover_trigger_cycle(INT_MAX);
		}
	}

	foreach(const gui::widget_ptr& w, widgets_) {
		w->process();
	}

	static_process(lvl);
}

void custom_object::static_process(level& lvl)
{
	handle_event(OBJECT_EVENT_PROCESS);
	handle_event(frame_->process_event_id());

	if(type_->timer_frequency() > 0 && (cycle_%type_->timer_frequency()) == 0) {
		static const std::string TimerStr = "timer";
		handle_event(OBJECT_EVENT_TIMER);
	}

	for(std::map<std::string, particle_system_ptr>::iterator i = particle_systems_.begin(); i != particle_systems_.end(); ) {
		i->second->process(*this);
		if(i->second->is_destroyed()) {
			particle_systems_.erase(i++);
		} else {
			++i;
		}
	}

	set_driver_position();

	foreach(const light_ptr& p, lights_) {
		p->process();
	}
}

void custom_object::set_driver_position()
{
	if(driver_) {
		const int pos_right = x() + type_->passenger_x();
		const int pos_left = x() + current_frame().width() - driver_->current_frame().width() - type_->passenger_x();
		driver_->set_face_right(face_right());

		driver_->set_pos(face_right() ? pos_right : pos_left, y() + type_->passenger_y());
	}
}

#ifndef NO_EDITOR
const_editor_entity_info_ptr custom_object::editor_info() const
{
	return type_->editor_info();
}
#endif // !NO_EDITOR

int custom_object::zorder() const
{
	return zorder_;
}

int custom_object::zsub_order() const
{
	return zsub_order_;
}

int custom_object::velocity_x() const
{
	return velocity_x_;
}

int custom_object::velocity_y() const
{
	return velocity_y_;
}

int custom_object::surface_friction() const
{
	return type_->surface_friction();
}

int custom_object::surface_traction() const
{
	return type_->surface_traction();
}

bool custom_object::has_feet() const
{
	return has_feet_ && solid();
}

bool custom_object::is_standable(int xpos, int ypos, int* friction, int* traction, int* adjust_y) const
{
	if(!body_passthrough() && !body_harmful() && point_collides(xpos, ypos)) {
		if(friction) {
			*friction = type_->surface_friction();
		}

		if(traction) {
			*traction = type_->surface_traction();
		}

		if(adjust_y) {
			if(type_->use_image_for_collisions()) {
				for(*adjust_y = 0; point_collides(xpos, ypos - *adjust_y - 1); --(*adjust_y)) {
				}
			} else {
				*adjust_y = ypos - body_rect().y();
			}
		}

		return true;
	}

	if(frame_->has_platform()) {
		const frame& f = *frame_;
		int y1 = y() + f.platform_y();
		int y2 = previous_y_ + f.platform_y();

		if(y1 > y2) {
			std::swap(y1, y2);
		}

		if(ypos < y1 || ypos > y2) {
			return false;
		}

		if(xpos < x() + f.platform_x() || xpos >= x() + f.platform_x() + f.platform_w()) {
			return false;
		}

		if(friction) {
			*friction = type_->surface_friction();
		}

		if(traction) {
			*traction = type_->surface_traction();
		}

		if(adjust_y) {
			*adjust_y = y() + f.platform_y() - ypos;
		}

		return true;
	}

	return false;
}

bool custom_object::destroyed() const
{
	return hitpoints_ <= 0;
}

bool custom_object::point_collides(int xpos, int ypos) const
{
	if(type_->use_image_for_collisions()) {
		const bool result = !current_frame().is_alpha(xpos - x(), ypos - y(), time_in_frame_, face_right());
		return result;
	} else {
		return point_in_rect(point(xpos, ypos), body_rect());
	}
}

bool custom_object::rect_collides(const rect& r) const
{
	if(type_->use_image_for_collisions()) {
		rect myrect(x(), y(), current_frame().width(), current_frame().height());
		if(rects_intersect(myrect, r)) {
			rect intersection = intersection_rect(myrect, r);
			for(int y = intersection.y(); y < intersection.y2(); ++y) {
				for(int x = intersection.x(); x < intersection.x2(); ++x) {
					if(point_collides(x, y)) {
						return true;
					}
				}
			}

			return false;
		} else {
			return false;
		}
	} else {
		return rects_intersect(r, body_rect());
	}
}

const_solid_info_ptr custom_object::calculate_solid() const
{
	if(!type_->has_solid()) {
		return const_solid_info_ptr();
	}

	const frame& f = current_frame();
	if(f.solid()) {
		return f.solid();
	}

	return type_->solid();
}

const_solid_info_ptr custom_object::calculate_platform() const
{
	if(platform_solid_info_.get()) {
		return platform_solid_info_;
	} else if(platform_area_) {
		//if platform_solid_info_ is NULL but we have a rect, that
		//means there is no platform, so return NULL instead of
		//defaulting to the type.
		return const_solid_info_ptr();
	}

	return type_->platform();
}

void custom_object::control(const level& lvl)
{
}

custom_object::STANDING_STATUS custom_object::is_standing(const level& lvl, collision_info* info) const
{
	if(!has_feet()) {
		return NOT_STANDING;
	}

	const int width = type_->feet_width();

	if(width >= 1) {
		const int facing = face_right() ? 1 : -1;
		if(point_standable(lvl, *this, feet_x() + width*facing, feet_y(), info, fall_through_platforms_ ? SOLID_ONLY : SOLID_AND_PLATFORMS)) {
			return STANDING_FRONT_FOOT;
		}

		if(point_standable(lvl, *this, feet_x() - width*facing, feet_y(), info, fall_through_platforms_ ? SOLID_ONLY : SOLID_AND_PLATFORMS)) {
			return STANDING_BACK_FOOT;
		}

		return NOT_STANDING;
	}

	if(point_standable(lvl, *this, feet_x(), feet_y(), info, fall_through_platforms_ ? SOLID_ONLY : SOLID_AND_PLATFORMS)) {
		return STANDING_FRONT_FOOT;
	} else {
		return NOT_STANDING;
	}
}

namespace {

#ifndef DISABLE_FORMULA_PROFILER
using formula_profiler::event_call_stack;
#endif

variant call_stack(const custom_object& obj) {
	std::vector<variant> result;

#ifndef DISABLE_FORMULA_PROFILER
	for(int n = 0; n != event_call_stack.size(); ++n) {
		result.push_back(variant(get_object_event_str(event_call_stack[n].event_id)));
	}
#endif

	return variant(&result);
}

}

std::set<custom_object*>& custom_object::get_all()
{
	typedef std::set<custom_object*> Set;
	static Set* all = new Set;
	return *all;
}

std::set<custom_object*>& custom_object::get_all(const std::string& type)
{
	typedef std::map<std::string, std::set<custom_object*> > Map;
	static Map* all = new Map;
	return (*all)[type];
}

void custom_object::init()
{
}

void custom_object::run_garbage_collection()
{
	const int starting_ticks = SDL_GetTicks();

	std::cerr << "RUNNING GARBAGE COLLECTION FOR " << get_all().size() << " OBJECTS...\n";

	std::vector<entity_ptr> references;
	foreach(custom_object* obj, get_all()) {
		references.push_back(entity_ptr(obj));
	}

	std::set<const void*> safe;
	std::vector<gc_object_reference> refs;

	foreach(custom_object* obj, get_all()) {
		obj->extract_gc_object_references(refs);
	}
	
	for(int pass = 1;; ++pass) {
		const int starting_safe = safe.size();
		foreach(custom_object* obj, get_all()) {
			if(obj->refcount() > 1) {
				safe.insert(obj);
			}
		}

		if(starting_safe == safe.size()) {
			break;
		}

		std::cerr << "PASS " << pass << ": " << safe.size() << " OBJECTS SAFE\n";

		foreach(gc_object_reference& ref, refs) {
			if(ref.owner == NULL) {
				continue;
			}

			if(safe.count(ref.owner)) {
				restore_gc_object_reference(ref);
				ref.owner = NULL;
			}
		}
	}

	foreach(gc_object_reference& ref, refs) {
		if(ref.owner == NULL || !ref.visitor) {
			continue;
		}

		foreach(game_logic::formula_callable_suspended_ptr ptr, ref.visitor->pointers()) {
			if(safe.count(ptr->value())) {
				ptr->restore_ref();
			}
		}
	}

	std::cerr << "RAN GARBAGE COLLECTION IN " << (SDL_GetTicks() - starting_ticks) << "ms. Releasing " << (get_all().size() - safe.size()) << "/" << get_all().size() << " OBJECTS\n";
}

void custom_object::being_removed()
{
	handle_event(OBJECT_EVENT_BEING_REMOVED);
#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active(false);
	}
#endif
}

void custom_object::being_added()
{
#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active();
	}
#endif
	handle_event(OBJECT_EVENT_BEING_ADDED);
}

void custom_object::set_animated_schedule(boost::shared_ptr<AnimatedMovement> movement)
{
	assert(movement.get() != NULL);
	animated_movement_.push_back(movement);
}

void custom_object::add_animated_movement(variant attr_var, variant options)
{
	const std::string& name = options["name"].as_string_default("");
	if(options["replace_existing"].as_bool(false)) {
		cancel_animated_schedule(name);
	} else if(name != "") {
		for(auto move : animated_movement_) {
			if(move->name == name) {
				move->follow_on.push_back(std::make_pair(attr_var, options));
				return;
			}
		}
	}

	const std::string type = query_value_by_slot(CUSTOM_OBJECT_TYPE).as_string();
	game_logic::formula_callable_definition_ptr def = custom_object_type::get_definition(type);
	ASSERT_LOG(def.get() != NULL, "Could not get definition for object: " << type);

	std::vector<int> slots;
	std::vector<variant> begin_values, end_values;

	const auto& attr = attr_var.as_map();

	for(const auto& p : attr) {
		slots.push_back(def->get_slot(p.first.as_string()));
		ASSERT_LOG(slots.back() >= 0, "Unknown attribute in object: " << p.first.as_string());
		end_values.push_back(p.second);
		begin_values.push_back(query_value_by_slot(slots.back()));
	}

	const int ncycles = options["duration"].as_int(10);

	std::function<double(double)> easing_fn;
	variant easing_var = options["easing"];
	if(easing_var.is_function()) {
		easing_fn = [=](double x) { std::vector<variant> args; args.push_back(variant(decimal(x))); return easing_var(args).as_decimal().as_float(); };
	} else {
		const std::string& easing = easing_var.as_string_default("swing");
		if(easing == "linear") {
			easing_fn = [](double x) { return x; };
		} else if(easing == "swing") {
			easing_fn = [](double x) { return 0.5*(1 - cos(x*3.14)); };
		} else {
			ASSERT_LOG(false, "Unknown easing: " << easing);
		}
	}

	std::vector<variant> values;
	values.reserve(slots.size()*ncycles);

	for(int cycle = 0; cycle != ncycles; ++cycle) {
		GLfloat ratio = 1.0;
		if(cycle < ncycles-1) {
			ratio = GLfloat(cycle)/GLfloat(ncycles-1);
			ratio = easing_fn(ratio);
		}
		for(int n = 0; n != slots.size(); ++n) {
			values.push_back(interpolate_variants(begin_values[n], end_values[n], ratio));
		}
	}

	boost::shared_ptr<custom_object::AnimatedMovement> movement(new custom_object::AnimatedMovement);
	movement->name = name;
	movement->animation_values.swap(values);
	movement->animation_slots.swap(slots);

	movement->on_process = options["on_process"];
	movement->on_complete = options["on_complete"];

	set_animated_schedule(movement);
}

void custom_object::cancel_animated_schedule(const std::string& name)
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

	animated_movement_.erase(std::remove(animated_movement_.begin(), animated_movement_.end(), boost::shared_ptr<AnimatedMovement>()), animated_movement_.end());
}

namespace {

using game_logic::formula_callable;

//Object that provides an FFL interface to an object's event handlers.
class event_handlers_callable : public formula_callable {
	boost::intrusive_ptr<custom_object> obj_;

	variant get_value(const std::string& key) const {
		game_logic::const_formula_ptr f = obj_->get_event_handler(get_object_event_id(key));
		if(!f) {
			return variant();
		} else {
			return variant(f->str());
		}
	}
	void set_value(const std::string& key, const variant& value) {
		static boost::intrusive_ptr<custom_object_callable> custom_object_definition(new custom_object_callable);

		game_logic::formula_ptr f(new game_logic::formula(value, &get_custom_object_functions_symbol_table(), custom_object_definition.get()));
		obj_->set_event_handler(get_object_event_id(key), f);
	}
public:
	explicit event_handlers_callable(const custom_object& obj) : obj_(const_cast<custom_object*>(&obj))
	{}

	const custom_object& obj() const { return *obj_; }
};

// FFL widget interface.
class widgets_callable : public formula_callable {
	boost::intrusive_ptr<custom_object> obj_;

	variant get_value(const std::string& key) const {
		if(key == "children") {
			std::vector<variant> v = obj_->get_variant_widget_list();
			return variant(&v);
		}
		return variant(obj_->get_widget_by_id(key).get());
	}
	void set_value(const std::string& key, const variant& value) {
		if(key == "child") {

			gui::widget_ptr new_widget = widget_factory::create(value, obj_.get());

			if(new_widget->id().empty() == false) {
				gui::widget_ptr existing = obj_->get_widget_by_id(new_widget->id());
				if(existing != NULL) {
					obj_->remove_widget(existing);
				}
			}

			obj_->add_widget(new_widget);
			return;
		}
		if(value.is_null()) {
			gui::widget_ptr w = obj_->get_widget_by_id(key);
			if(w != NULL) {
				obj_->remove_widget(w);
			}
		} else {
			gui::widget_ptr w = obj_->get_widget_by_id(key);
			ASSERT_LOG(w != NULL, "no widget with identifier " << key << " found");
			obj_->remove_widget(w);
			obj_->add_widget(widget_factory::create(value, obj_.get()));
		}
	}
public:
	explicit widgets_callable(const custom_object& obj) : obj_(const_cast<custom_object*>(&obj))
	{}
};

decimal calculate_velocity_magnitude(int velocity_x, int velocity_y)
{
	const int64_t xval = velocity_x;
	const int64_t yval = velocity_y;
	int64_t value = xval*xval + yval*yval;
	value = int64_t(sqrt(double(value)));
	decimal result(decimal::from_int(static_cast<int>(value)));
	result /= 1000;
	return result;
}

static const double radians_to_degrees = 57.29577951308232087;
decimal calculate_velocity_angle(int velocity_x, int velocity_y)
{
	if(velocity_y == 0 && velocity_x == 0) {
		return decimal::from_int(0);
	}

	const double theta = atan2(double(velocity_y), double(velocity_x));
	return decimal(theta*radians_to_degrees);
}

variant two_element_variant_list(const variant& a, const variant&b) 
{
	std::vector<variant> v;
	v.push_back(a);
	v.push_back(b);
	return variant(&v);
}
}

variant custom_object::get_value_by_slot(int slot) const
{
	switch(slot) {
	case CUSTOM_OBJECT_VALUE: {
		ASSERT_LOG(value_stack_.empty() == false, "Query of value in illegal context");
		return value_stack_.top();
	}
	case CUSTOM_OBJECT_DATA: {
		ASSERT_LOG(active_property_ >= 0, "Access of 'data' outside of an object property which has data");
		if(active_property_ < property_data_.size()) {
			return property_data_[active_property_];
		} else {
			return variant();
		}
	}

	case CUSTOM_OBJECT_ARG: {
		if(backup_callable_stack_.empty() == false && backup_callable_stack_.top()) {
			return variant(backup_callable_stack_.top());
		}

		game_logic::map_formula_callable_ptr callable(new game_logic::map_formula_callable(this));
		return variant(callable.get());
	}

	case CUSTOM_OBJECT_CONSTS:            return variant(type_->consts().get());
	case CUSTOM_OBJECT_TYPE:              return variant(type_->id());
	case CUSTOM_OBJECT_ACTIVE:            return variant::from_bool(last_cycle_active_ >= level::current().cycle() - 2);
	case CUSTOM_OBJECT_LIB:               return variant(game_logic::get_library_object().get());
	case CUSTOM_OBJECT_TIME_IN_ANIMATION: return variant(time_in_frame_);
	case CUSTOM_OBJECT_TIME_IN_ANIMATION_DELTA: return variant(time_in_frame_delta_);
	case CUSTOM_OBJECT_FRAME_IN_ANIMATION: return variant(current_frame().frame_number(time_in_frame_));
	case CUSTOM_OBJECT_LEVEL:             return variant(&level::current());
	case CUSTOM_OBJECT_ANIMATION:         return frame_->variant_id();
	case CUSTOM_OBJECT_AVAILABLE_ANIMATIONS: return type_->available_frames();
	case CUSTOM_OBJECT_HITPOINTS:         return variant(hitpoints_);
	case CUSTOM_OBJECT_MAX_HITPOINTS:     return variant(type_->hitpoints() + max_hitpoints_);
	case CUSTOM_OBJECT_MASS:              return variant(type_->mass());
	case CUSTOM_OBJECT_LABEL:             return variant(label());
	case CUSTOM_OBJECT_X:                 return variant(x());
	case CUSTOM_OBJECT_Y:                 return variant(y());
	case CUSTOM_OBJECT_XY:                {
			 				 				std::vector<variant> v;
											v.push_back(variant(x()));
											v.push_back(variant(y()));
											return variant(&v);
										  }
	case CUSTOM_OBJECT_Z:
	case CUSTOM_OBJECT_ZORDER:            return variant(zorder_);
	case CUSTOM_OBJECT_ZSUB_ORDER:        return variant(zsub_order_);
    case CUSTOM_OBJECT_RELATIVE_X:        return variant(relative_x_);
	case CUSTOM_OBJECT_RELATIVE_Y:        return variant(relative_y_);
	case CUSTOM_OBJECT_SPAWNED_BY:        if(spawned_by().empty()) return variant(); else return variant(level::current().get_entity_by_label(spawned_by()).get());
	case CUSTOM_OBJECT_SPAWNED_CHILDREN: {
		std::vector<variant> children;
		foreach(const entity_ptr& e, level::current().get_chars()) {
			if(e->spawned_by() == label()) {
				children.push_back(variant(e.get()));
			}
		}

		return variant(&children);
	}
	case CUSTOM_OBJECT_PARENT:            return variant(parent_.get());
	case CUSTOM_OBJECT_PIVOT:             return variant(parent_pivot_);
	case CUSTOM_OBJECT_PREVIOUS_Y:        return variant(previous_y_);
	case CUSTOM_OBJECT_X1:                return variant(solid_rect().x());
	case CUSTOM_OBJECT_X2:                return variant(solid_rect().w() ? solid_rect().x2() : x() + current_frame().width());
	case CUSTOM_OBJECT_Y1:                return variant(solid_rect().y());
	case CUSTOM_OBJECT_Y2:                return variant(solid_rect().h() ? solid_rect().y2() : y() + current_frame().height());
	case CUSTOM_OBJECT_W:                 return variant(solid_rect().w());
	case CUSTOM_OBJECT_H:                 return variant(solid_rect().h());

	case CUSTOM_OBJECT_ACTIVATION_BORDER: return variant(activation_border_);
	case CUSTOM_OBJECT_MID_X:
	case CUSTOM_OBJECT_MIDPOINT_X:        return variant(solid_rect().w() ? solid_rect().x() + solid_rect().w()/2 : x() + current_frame().width()/2);
	case CUSTOM_OBJECT_MID_Y:
	case CUSTOM_OBJECT_MIDPOINT_Y:        return variant(solid_rect().h() ? solid_rect().y() + solid_rect().h()/2 : y() + current_frame().height()/2);
	case CUSTOM_OBJECT_MID_XY:
	case CUSTOM_OBJECT_MIDPOINT_XY: {
		return two_element_variant_list(
			variant(solid_rect().w() ? solid_rect().x() + solid_rect().w()/2 : x() + current_frame().width()/2),
			variant(solid_rect().h() ? solid_rect().y() + solid_rect().h()/2 : y() + current_frame().height()/2));
	}

	case CUSTOM_OBJECT_SOLID_RECT:        return variant(solid_rect().callable());
	case CUSTOM_OBJECT_SOLID_MID_X:       return variant(solid_rect().x() + solid_rect().w()/2);
	case CUSTOM_OBJECT_SOLID_MID_Y:       return variant(solid_rect().y() + solid_rect().h()/2);
	case CUSTOM_OBJECT_SOLID_MID_XY: {
		return two_element_variant_list(
			variant(solid_rect().x() + solid_rect().w()/2),
			variant(solid_rect().y() + solid_rect().h()/2));
	}
	case CUSTOM_OBJECT_IMG_MID_X:       return variant(x() + current_frame().width()/2);
	case CUSTOM_OBJECT_IMG_MID_Y:       return variant(y() + current_frame().height()/2);
	case CUSTOM_OBJECT_IMG_MID_XY: {
		return two_element_variant_list(
			variant(x() + current_frame().width()/2),
			variant(y() + current_frame().height()/2));
	}
	case CUSTOM_OBJECT_IMG_W:             return variant(current_frame().width());
	case CUSTOM_OBJECT_IMG_H:             return variant(current_frame().height());
	case CUSTOM_OBJECT_IMG_WH: {
		return two_element_variant_list(
			variant(current_frame().width()),
			variant(current_frame().height()));
	}
	case CUSTOM_OBJECT_FRONT:             return variant(face_right() ? body_rect().x2() : body_rect().x());
	case CUSTOM_OBJECT_BACK:              return variant(face_right() ? body_rect().x() : body_rect().x2());
	case CUSTOM_OBJECT_CYCLE:             return variant(cycle_);
	case CUSTOM_OBJECT_FACING:            return variant(face_right() ? 1 : -1);
	case CUSTOM_OBJECT_UPSIDE_DOWN:       return variant(upside_down() ? 1 : -1);
	case CUSTOM_OBJECT_UP:                return variant(upside_down() ? 1 : -1);
	case CUSTOM_OBJECT_DOWN:              return variant(upside_down() ? -1 : 1);
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
	case CUSTOM_OBJECT_PLATFORM_MOTION_X: return variant(platform_motion_x());
	case CUSTOM_OBJECT_REGISTRY:          return variant(preferences::registry());
	case CUSTOM_OBJECT_GLOBALS:           return variant(global_vars().get());
	case CUSTOM_OBJECT_VARS:              return variant(vars_.get());
	case CUSTOM_OBJECT_TMP:               return variant(tmp_vars_.get());
	case CUSTOM_OBJECT_GROUP:             return variant(group());
	case CUSTOM_OBJECT_ROTATE:            return variant(rotate_z_);
	case CUSTOM_OBJECT_ROTATE_X:            return variant(rotate_x_);
	case CUSTOM_OBJECT_ROTATE_Y:            return variant(rotate_y_);
	case CUSTOM_OBJECT_ROTATE_Z:            return variant(rotate_z_);
	case CUSTOM_OBJECT_ME:
	case CUSTOM_OBJECT_SELF:              return variant(this);
	case CUSTOM_OBJECT_BRIGHTNESS:		  return variant((draw_color().r() + draw_color().g() + draw_color().b())/3);
	case CUSTOM_OBJECT_RED:               return variant(draw_color().r());
	case CUSTOM_OBJECT_GREEN:             return variant(draw_color().g());
	case CUSTOM_OBJECT_BLUE:              return variant(draw_color().b());
	case CUSTOM_OBJECT_ALPHA:             return variant(draw_color().a());
	case CUSTOM_OBJECT_TEXT_ALPHA:        return variant(text_ ? text_->alpha : 255);
	case CUSTOM_OBJECT_DAMAGE:            return variant(current_frame().damage());
	case CUSTOM_OBJECT_HIT_BY:            return variant(last_hit_by_.get());
	case CUSTOM_OBJECT_DISTORTION:        return variant(distortion_.get());
	case CUSTOM_OBJECT_IS_STANDING:       return variant(standing_on_.get() || is_standing(level::current()));
	case CUSTOM_OBJECT_STANDING_INFO:     {
		collision_info info;
		is_standing(level::current(), &info);
		if(info.surf_info && info.surf_info->info) {
			return variant(*info.surf_info->info);
		} else {
			return variant();
		}
	}
	case CUSTOM_OBJECT_NEAR_CLIFF_EDGE:   return variant::from_bool(is_standing(level::current()) && cliff_edge_within(level::current(), feet_x(), feet_y(), face_dir()*15));
	case CUSTOM_OBJECT_DISTANCE_TO_CLIFF: return variant(::distance_to_cliff(level::current(), feet_x(), feet_y(), face_dir()));
	case CUSTOM_OBJECT_SLOPE_STANDING_ON: {
		if(standing_on_ && standing_on_->platform() && !standing_on_->solid_platform()) {
			return variant(standing_on_->platform_slope_at(feet_x()));
		}
		return variant(-slope_standing_on(6)*face_dir());
	}
	case CUSTOM_OBJECT_UNDERWATER:        return variant(level::current().is_underwater(solid() ? solid_rect() : rect(x(), y(), current_frame().width(), current_frame().height())));
	case CUSTOM_OBJECT_PREVIOUS_WATER_BOUNDS: {
		std::vector<variant> v;
		v.push_back(variant(previous_water_bounds_.x()));
		v.push_back(variant(previous_water_bounds_.y()));
		v.push_back(variant(previous_water_bounds_.x2()));
		v.push_back(variant(previous_water_bounds_.y2()));
		return variant(&v);

	}
	case CUSTOM_OBJECT_WATER_BOUNDS: {
		rect area;
		if(level::current().is_underwater(solid_rect(), &area)) {
			std::vector<variant> v;
			v.push_back(variant(area.x()));
			v.push_back(variant(area.y()));
			v.push_back(variant(area.x2()));
			v.push_back(variant(area.y2()));
			return variant(&v);
		} else {
			return variant();
		}
	}
	case CUSTOM_OBJECT_WATER_OBJECT: {
		variant v;
		level::current().is_underwater(solid_rect(), NULL, &v);
		return v;
	}
	case CUSTOM_OBJECT_DRIVER:            return variant(driver_ ? driver_.get() : this);
	case CUSTOM_OBJECT_IS_HUMAN:          return variant::from_bool(is_human() != NULL);
	case CUSTOM_OBJECT_INVINCIBLE:        return variant::from_bool(invincible_ != 0);
	case CUSTOM_OBJECT_SOUND_VOLUME:      return variant(sound_volume_);
	case CUSTOM_OBJECT_DESTROYED:         return variant::from_bool(destroyed());

	case CUSTOM_OBJECT_IS_STANDING_ON_PLATFORM: {
		if(standing_on_ && standing_on_->platform() && !standing_on_->solid_platform()) {
			return variant::from_bool(true);
		}

		collision_info info;
		is_standing(level::current(), &info);
		return variant(info.platform);
	}

	case CUSTOM_OBJECT_STANDING_ON: {
		if(standing_on_) {
			return variant(standing_on_.get());
		}

		entity_ptr stand_on;
		collision_info info;
		is_standing(level::current(), &info);
		return variant(info.collide_with.get());
	}

	case CUSTOM_OBJECT_EFFECTS: {
#if defined(USE_SHADERS)
		std::vector<variant> v;
		for(size_t n = 0; n < effects_.size(); ++n) {
			v.push_back(variant(effects_[n].get()));
		}
		return variant(&v);
#else
		return variant();
#endif
	}

	case CUSTOM_OBJECT_SHADER: {
#if defined(USE_SHADERS)
		return variant(shader_.get());
#endif
		return variant();
	}

	case CUSTOM_OBJECT_ACTIVATION_AREA: {
		if(activation_area_.get() != NULL) {
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

	case CUSTOM_OBJECT_CLIP_AREA: {
		if(clip_area_.get() != NULL) {
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

	case CUSTOM_OBJECT_VARIATIONS: {
		std::vector<variant> result;
		foreach(const std::string& s, current_variation_) {
			result.push_back(variant(s));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_ATTACHED_OBJECTS: {
		std::vector<variant> result;
		foreach(const entity_ptr& e, attached_objects()) {
			result.push_back(variant(e.get()));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_CALL_STACK: {
		return call_stack(*this);
	}

	case CUSTOM_OBJECT_LIGHTS: {
		std::vector<variant> result;
		foreach(const light_ptr& p, lights_) {
			result.push_back(variant(p.get()));
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
		foreach(int n, platform_offsets_) {
			result.push_back(variant(n));
		}
		return variant(&result);
	}

	case CUSTOM_OBJECT_SOLID_DIMENSIONS_IN: {
		std::vector<variant> v;
		v.push_back(variant(solid_dimensions()));
		v.push_back(variant(weak_solid_dimensions()));
		return variant(&v);
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
		foreach(GLfloat f, custom_draw_uv_) {
			result.push_back(variant(decimal(f)));
		}

		return variant(&result);
	}

	case CUSTOM_OBJECT_XY_ARRAY: {
		std::vector<variant> result;
		result.reserve(custom_draw_xy_.size());
		foreach(GLfloat f, custom_draw_xy_) {
			result.push_back(variant(decimal(f)));
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
		std::vector<variant> v = get_variant_widget_list();
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

	case CUSTOM_OBJECT_TEXTV: {
		std::vector<variant> v;
		foreach(const gui::vector_text_ptr& vt, vector_text_) {
			v.push_back(variant(vt.get()));
		}
		return(variant(&v));
	}

	case CUSTOM_OBJECT_MOUSEOVER_DELAY: {
		return variant(get_mouseover_delay());
	}
	case CUSTOM_OBJECT_MOUSEOVER_AREA: {
		return mouse_over_area().write();
	}
	case CUSTOM_OBJECT_PARTICLE_SYSTEMS: {
		std::map<variant, variant> v;
		for(std::map<std::string, particle_system_ptr>::const_iterator i = particle_systems_.begin(); i != particle_systems_.end(); ++i) {
			v[variant(i->first)] = variant(i->second.get());
		}
		return variant(&v);
	}
	case CUSTOM_OBJECT_TRUEZ: {
		return variant::from_bool(truez());
	}
	case CUSTOM_OBJECT_TX: {
		return variant(tx());
	}
	case CUSTOM_OBJECT_TY: {
		return variant(ty());
	}
	case CUSTOM_OBJECT_TZ: {
		return variant(tz());
	}

	case CUSTOM_OBJECT_CTRL_USER_OUTPUT: {
		return controls::user_ctrl_output();
	}

	case CUSTOM_OBJECT_DRAW_PRIMITIVES: {
#if defined(USE_SHADERS)
		std::vector<variant> v;
		foreach(boost::intrusive_ptr<graphics::draw_primitive> p, draw_primitives_) {
			v.push_back(variant(p.get()));
		}

		return variant(&v);
#else
		return variant();
#endif
	}

	case CUSTOM_OBJECT_CTRL_UP:
	case CUSTOM_OBJECT_CTRL_DOWN:
	case CUSTOM_OBJECT_CTRL_LEFT:
	case CUSTOM_OBJECT_CTRL_RIGHT:
	case CUSTOM_OBJECT_CTRL_ATTACK:
	case CUSTOM_OBJECT_CTRL_JUMP:
	case CUSTOM_OBJECT_CTRL_TONGUE:
		return variant::from_bool(control_status(static_cast<controls::CONTROL_ITEM>(slot - CUSTOM_OBJECT_CTRL_UP)));
	
	case CUSTOM_OBJECT_CTRL_USER:
		return control_status_user();

	case CUSTOM_OBJECT_PLAYER_DIFFICULTY:
	case CUSTOM_OBJECT_PLAYER_CAN_INTERACT:
	case CUSTOM_OBJECT_PLAYER_UNDERWATER_CONTROLS:
	case CUSTOM_OBJECT_PLAYER_CTRL_MOD_KEY:
	case CUSTOM_OBJECT_PLAYER_CTRL_KEYS:
	case CUSTOM_OBJECT_PLAYER_CTRL_MICE:
	case CUSTOM_OBJECT_PLAYER_CTRL_TILT:
	case CUSTOM_OBJECT_PLAYER_CTRL_X:
	case CUSTOM_OBJECT_PLAYER_CTRL_Y:
	case CUSTOM_OBJECT_PLAYER_CTRL_REVERSE_AB:
	case CUSTOM_OBJECT_PLAYER_CONTROL_SCHEME:
	case CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK:
	case CUSTOM_OBJECT_PLAYER_CONTROL_LOCK:
		return get_player_value_by_slot(slot);

	default:
		if(slot >= type_->slot_properties_base() && (size_t(slot - type_->slot_properties_base()) < type_->slot_properties().size())) {
			const custom_object_type::property_entry& e = type_->slot_properties()[slot - type_->slot_properties_base()];
			if(e.getter) {
				if(std::find(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), slot - type_->slot_properties_base()) != properties_requiring_dynamic_initialization_.end()) {
					ASSERT_LOG(false, "Read of uninitialized property " << debug_description() << "." << e.id << " " << get_full_call_stack());
				}
				active_property_scope scope(*this, e.storage_slot);
				return e.getter->execute(*this);
			} else if(e.const_value) {
				return *e.const_value;
			} else if(e.storage_slot >= 0) {
				return get_property_data(e.storage_slot);
			} else {
				ASSERT_LOG(false, "PROPERTY HAS NO GETTER OR CONST VALUE");
			}
		}

		break;
	}

	const game_logic::formula_callable_definition::entry* entry = 
		    custom_object_callable::instance().get_entry(slot);
	if(entry != NULL) {
		return variant();
	}
	
	ASSERT_LOG(false, "UNKNOWN SLOT QUERIED FROM OBJECT: " << slot);
	return variant();
}

variant custom_object::get_player_value_by_slot(int slot) const
{
	assert(custom_object_callable::instance().get_entry(slot));
	ASSERT_LOG(false, "Query of value for player objects on non-player object. Key: " << custom_object_callable::instance().get_entry(slot)->id);
	return variant();
}

void custom_object::set_player_value_by_slot(int slot, const variant& value)
{
	assert(custom_object_callable::instance().get_entry(slot));
	ASSERT_LOG(false, "Set of value for player objects on non-player object. Key: " << custom_object_callable::instance().get_entry(slot)->id);
}

namespace {

using game_logic::formula_callable;

class backup_callable_stack_scope {
	std::stack<const formula_callable*>* stack_;
public:
	backup_callable_stack_scope(std::stack<const formula_callable*>* s, const formula_callable* item) : stack_(s) {
		stack_->push(item);
	}

	~backup_callable_stack_scope() {
		stack_->pop();
	}
};
}

variant custom_object::get_value(const std::string& key) const
{
	const int slot = type_->callable_definition()->get_slot(key);
	if(slot >= 0 && slot < NUM_CUSTOM_OBJECT_PROPERTIES) {
		return get_value_by_slot(slot);
	}

	std::map<std::string, custom_object_type::property_entry>::const_iterator property_itor = type_->properties().find(key);
	if(property_itor != type_->properties().end()) {
		if(property_itor->second.getter) {
			active_property_scope scope(*this, property_itor->second.storage_slot);
			return property_itor->second.getter->execute(*this);
		} else if(property_itor->second.const_value) {
			return *property_itor->second.const_value;
		} else if(property_itor->second.storage_slot >= 0) {
			return get_property_data(property_itor->second.storage_slot);
		}
	}

	if(!type_->is_strict()) {
		variant var_result = tmp_vars_->query_value(key);
		if(!var_result.is_null()) {
			return var_result;
		}

		var_result = vars_->query_value(key);
		if(!var_result.is_null()) {
			return var_result;
		}
	}

	std::map<std::string, variant>::const_iterator i = type_->variables().find(key);
	if(i != type_->variables().end()) {
		return i->second;
	}

	std::map<std::string, particle_system_ptr>::const_iterator particle_itor = particle_systems_.find(key);
	if(particle_itor != particle_systems_.end()) {
		return variant(particle_itor->second.get());
	}

	if(backup_callable_stack_.empty() == false && backup_callable_stack_.top()) {
		if(backup_callable_stack_.top() != this) {
			const formula_callable* callable = backup_callable_stack_.top();
			backup_callable_stack_scope callable_scope(&backup_callable_stack_, NULL);
			return callable->query_value(key);
		}
	}

	ASSERT_LOG(!type_->is_strict(), "ILLEGAL OBJECT ACCESS WITH STRICT CHECKING IN " << debug_description() << ": " << key << " At " << get_full_call_stack());

	return variant();
}

void custom_object::get_inputs(std::vector<game_logic::formula_input>* inputs) const
{
	for(int n = CUSTOM_OBJECT_ARG+1; n != NUM_CUSTOM_OBJECT_PROPERTIES; ++n) {
		const game_logic::formula_callable_definition::entry* entry = 
		    custom_object_callable::instance().get_entry(n);
		if(!get_value_by_slot(n).is_null()) {
			inputs->push_back(entry->id);
		}
	}
}

void custom_object::set_value(const std::string& key, const variant& value)
{
	const int slot = custom_object_callable::get_key_slot(key);
	if(slot != -1) {
		set_value_by_slot(slot, value);
		return;
	}

	std::map<std::string, custom_object_type::property_entry>::const_iterator property_itor = type_->properties().find(key);
	if(property_itor != type_->properties().end()) {
		set_value_by_slot(type_->slot_properties_base() + property_itor->second.slot, value);
		return;
	}

	if(key == "animation") {
		set_frame(value.as_string());
	} else if(key == "time_in_animation") {
		ASSERT_GE(value.as_int(), 0);
		time_in_frame_ = value.as_int()%frame_->duration();
	} else if(key == "frame_in_animation") {
		ASSERT_GE(value.as_int(), 0);
		time_in_frame_ = value.as_int()%frame_->duration();
	} else if(key == "time_in_animation_delta") {
		time_in_frame_delta_ = value.as_int();
	} else if(key == "x") {
		const int start_x = centi_x();
		set_x(value.as_int());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
		}
	} else if(key == "y") {
		const int start_y = centi_y();
		set_y(value.as_int());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_y(start_y);
		}
	} else if(key == "xy") {
		const int start_x = centi_x();
		const int start_y = centi_y();
		set_x(value[0].as_int());
		set_y(value[1].as_int());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
			set_centi_y(start_y);
		}
	} else if(key == "z" || key == "zorder") {
		zorder_ = value.as_int();
	} else if(key == "zsub_order") {
		zsub_order_ = value.as_int();
	} else if(key == "midpoint_x" || key == "mid_x") {
        set_mid_x(value.as_int());
	} else if(key == "midpoint_y" || key == "mid_y") {
        set_mid_y(value.as_int());
	} else if(key == "facing") {
		set_face_right(value.as_int() > 0);
	} else if(key == "upside_down") {
		set_upside_down(value.as_int() != 0);
	} else if(key == "hitpoints") {
		const int old_hitpoints = hitpoints_;
		hitpoints_ = value.as_int();
		if(old_hitpoints > 0 && hitpoints_ <= 0) {
			die();
		}
	} else if(key == "max_hitpoints") {
		max_hitpoints_ = value.as_int() - type_->hitpoints();
		if(hitpoints_ > type_->hitpoints() + max_hitpoints_) {
			hitpoints_ = type_->hitpoints() + max_hitpoints_;
		}
	} else if(key == "velocity_x") {
		velocity_x_ = value.as_int();
	} else if(key == "velocity_y") {
		velocity_y_ = value.as_int();
	} else if(key == "accel_x") {
		accel_x_ = value.as_int();
	} else if(key == "accel_y") {
		accel_y_ = value.as_int();
	} else if(key == "rotate" || key == "rotate_z") {
		rotate_z_ = value.as_decimal();
	} else if(key == "rotate_x") {
		rotate_x_ = value.as_decimal();
	} else if(key == "rotate_y") {
		rotate_y_ = value.as_decimal();
	} else if(key == "red") {
		make_draw_color();
		draw_color_->buf()[0] = truncate_to_char(value.as_int());
	} else if(key == "green") {
		make_draw_color();
		draw_color_->buf()[1] = truncate_to_char(value.as_int());
	} else if(key == "blue") {
		make_draw_color();
		draw_color_->buf()[2] = truncate_to_char(value.as_int());
	} else if(key == "alpha") {
		make_draw_color();
		draw_color_->buf()[3] = truncate_to_char(value.as_int());
	} else if(key == "brightness"){
		make_draw_color();
		draw_color_->buf()[0] = value.as_int();
		draw_color_->buf()[1] = value.as_int();
		draw_color_->buf()[2] = value.as_int();
	} else if(key == "distortion") {
		distortion_ = value.try_convert<graphics::raster_distortion>();
	} else if(key == "current_generator") {
		set_current_generator(value.try_convert<current_generator>());
	} else if(key == "invincible") {
		invincible_ = value.as_int();
	} else if(key == "fall_through_platforms") {
		fall_through_platforms_ = value.as_int();
	} else if(key == "tags") {
		if(value.is_list()) {
			tags_ = new game_logic::map_formula_callable;
			for(int n = 0; n != value.num_elements(); ++n) {
				tags_->add(value[n].as_string(), variant(1));
			}
		}
#if defined(USE_SHADERS)
	} else if(key == "shader") {
		using namespace gles2;
		if(value.is_map()) {
			shader_.reset(new shader_program(value));
		} else {
			shader_.reset(value.try_convert<shader_program>());
		}
	} else if(key == "effects") {
		using namespace gles2;
		effects_.clear();
		if(value.is_list()) {
			for(size_t n = 0; n < value.num_elements(); ++n) {
				if(value[n].is_map()) {
					effects_.push_back(new shader_program(value[n]));
				} else {
					effects_.push_back(shader_program_ptr(value[n].try_convert<shader_program>()));
				}
			}
		} else if(value.is_map()) {
			effects_.push_back(new shader_program(value));
		} else {
			effects_.push_back(shader_program_ptr(value.try_convert<shader_program>()));
			ASSERT_LOG(effects_.size() > 0, "Couldn't convert type to shader");
		}
#endif
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
		handle_event("reset_variations");
		current_variation_.clear();
		if(value.is_list()) {
			for(int n = 0; n != value.num_elements(); ++n) {
				current_variation_.push_back(value[n].as_string());
			}
		} else if(value.is_string()) {
			current_variation_.push_back(value.as_string());
		}

		if(current_variation_.empty()) {
			type_ = base_type_;
		} else {
			type_ = base_type_->get_variation(current_variation_);
		}

		calculate_solid_rect();

		handle_event("set_variations");
	} else if(key == "attached_objects") {
		std::vector<entity_ptr> v;
		for(int n = 0; n != value.num_elements(); ++n) {
			entity* e = value[n].try_convert<entity>();
			if(e) {
				v.push_back(entity_ptr(e));
			}
		}

		set_attached_objects(v);
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

		const unsigned int old_solid = solid_dimensions();
		const unsigned int old_weak = weak_solid_dimensions();
		set_solid_dimensions(solid, weak);
		collision_info collide_info;
		if(entity_in_current_level(this) && entity_collides(level::current(), *this, MOVE_NONE, &collide_info)) {
			set_solid_dimensions(old_solid, old_weak);
			ASSERT_EQ(entity_collides(level::current(), *this, MOVE_NONE), false);

			game_logic::map_formula_callable* callable(new game_logic::map_formula_callable(this));
			callable->add("collide_with", variant(collide_info.collide_with.get()));
			game_logic::formula_callable_ptr callable_ptr(callable);

			handle_event(OBJECT_EVENT_CHANGE_SOLID_DIMENSIONS_FAIL, callable);
		}

	} else if(key == "xscale" || key == "yscale") {
		if(parallax_scale_millis_.get() == NULL) {
			parallax_scale_millis_.reset(new std::pair<int,int>(1000,1000));
		}

		const int v = value.as_int();

		if(key == "xscale") {
			const int current = (parallax_scale_millis_->first*x())/1000;
			const int new_value = (v*current)/1000;
			set_x(new_value);
			parallax_scale_millis_->first = v;
		} else {
			const int current = (parallax_scale_millis_->second*y())/1000;
			const int new_value = (v*current)/1000;
			set_y(new_value);
			parallax_scale_millis_->second = v;
		}
	} else if(key == "type") {
		const_custom_object_type_ptr p = custom_object_type::get(value.as_string());
		if(p) {
			game_logic::formula_variable_storage_ptr old_vars = vars_, old_tmp_vars_ = tmp_vars_;

			get_all(base_type_->id()).erase(this);
			base_type_ = type_ = p;
			get_all(base_type_->id()).insert(this);
			has_feet_ = type_->has_feet();
			vars_.reset(new game_logic::formula_variable_storage(type_->variables())),
			tmp_vars_.reset(new game_logic::formula_variable_storage(type_->tmp_variables())),
			vars_->set_object_name(debug_description());
			tmp_vars_->set_object_name(debug_description());

			vars_->add(*old_vars);
			tmp_vars_->add(*old_tmp_vars_);

			vars_->disallow_new_keys(type_->is_strict());
			tmp_vars_->disallow_new_keys(type_->is_strict());

			//set the animation to the default animation for the new type.
			set_frame(type_->default_frame().id());
			//std::cerr << "SET TYPE WHEN CHANGING TO '" << type_->id() << "'\n";
		}
	} else if(key == "use_absolute_screen_coordinates") {
		use_absolute_screen_coordinates_ = value.as_bool();
	} else if(key == "mouseover_delay") {
		set_mouseover_delay(value.as_int());
#if defined(USE_BOX2D)
	} else if(key == "body") {
		//if(body_) {
		//	box2d::world::our_world_ptr()->destroy_body(body_);
		//}
		body_.reset(new box2d::body(value));
		body_->finish_loading(this);
#endif
	} else if(key == "mouseover_area") {
		set_mouse_over_area(rect(value));
	} else if(key == "truez") {
		set_truez(value.as_bool());
	} else if(key == "tx") {
		set_tx(value.as_decimal().as_float());
	} else if(key == "ty") {
		set_ty(value.as_decimal().as_float());
	} else if(key == "tz") {
		set_tz(value.as_decimal().as_float());
	} else if(!type_->is_strict()) {
		vars_->add(key, value);
	} else {
		std::ostringstream known_properties;
		for(std::map<std::string, custom_object_type::property_entry>::const_iterator property_itor = type_->properties().begin(); property_itor != type_->properties().end(); ++property_itor) {
			known_properties << property_itor->first << ", ";
		}

		ASSERT_LOG(false, "ILLEGAL OBJECT ACCESS WITH STRICT CHECKING IN " << debug_description() << ": " << key << " KNOWN PROPERTIES ARE: " << known_properties.str());
	}
}

void custom_object::set_value_by_slot(int slot, const variant& value)
{
	switch(slot) {
	case CUSTOM_OBJECT_DATA: {
		ASSERT_LOG(active_property_ >= 0, "Illegal access of 'data' in object when not in writable property");
		get_property_data(active_property_) = value;

		//see if this initializes a property that requires dynamic
		//initialization and if so mark is as now initialized.
		for(auto itor = properties_requiring_dynamic_initialization_.begin(); itor != properties_requiring_dynamic_initialization_.end(); ++itor) {
			if(type_->slot_properties()[*itor].storage_slot == active_property_) {
				properties_requiring_dynamic_initialization_.erase(itor);
				break;
			}
		}
		
		break;
	}
	case CUSTOM_OBJECT_TYPE: {
		const_custom_object_type_ptr p = custom_object_type::get(value.as_string());
		if(p) {
			game_logic::formula_variable_storage_ptr old_vars = vars_, old_tmp_vars_ = tmp_vars_;

			const_custom_object_type_ptr old_type = type_;

			get_all(base_type_->id()).erase(this);
			base_type_ = type_ = p;
			get_all(base_type_->id()).insert(this);
			has_feet_ = type_->has_feet();
			vars_.reset(new game_logic::formula_variable_storage(type_->variables())),
			tmp_vars_.reset(new game_logic::formula_variable_storage(type_->tmp_variables())),
			vars_->set_object_name(debug_description());
			tmp_vars_->set_object_name(debug_description());

			vars_->add(*old_vars);
			tmp_vars_->add(*old_tmp_vars_);

			vars_->disallow_new_keys(type_->is_strict());
			tmp_vars_->disallow_new_keys(type_->is_strict());

			std::vector<variant> props = property_data_;
			property_data_.clear();
			
			for(auto i = type_->properties().begin(); i != type_->properties().end(); ++i) {
				if(i->second.storage_slot < 0) {
					continue;
				}

				get_property_data(i->second.storage_slot) = deep_copy_variant(i->second.default_value);
			}

			for(auto i = old_type->properties().begin(); i != old_type->properties().end(); ++i) {
				if(i->second.storage_slot < 0 || i->second.storage_slot >= props.size() || props[i->second.storage_slot] == i->second.default_value) {
					continue;
				}

				auto j = type_->properties().find(i->first);
				if(j == type_->properties().end() || j->second.storage_slot < 0) {
					continue;
				}

				get_property_data(j->second.storage_slot) = props[i->second.storage_slot];
			}

			//set the animation to the default animation for the new type.
			set_frame(type_->default_frame().id());
			//std::cerr << "SET TYPE WHEN CHANGING TO '" << type_->id() << "'\n";
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
		if(value.is_string()) {
			set_frame(value.as_string());
		} else if(value.is_map()) {
			frame_ptr f(new frame(value));
			if(type_->use_image_for_collisions()) {
				f->set_image_as_solid();
			}
			set_frame(*f);
		} else {
			set_frame(*value.convert_to<frame>());
		}
		break;
	
	case CUSTOM_OBJECT_X1:
	case CUSTOM_OBJECT_X: {
		const int start_x = centi_x();
		set_x(value.as_int());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
		}

		break;
	}
	
	case CUSTOM_OBJECT_Y1:
	case CUSTOM_OBJECT_Y: {
		const int start_y = centi_y();
		set_y(value.as_int());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_y(start_y);
		}

		break;
	}

	case CUSTOM_OBJECT_X2: {
		const int start_x = centi_x();
		const int current_x = solid_rect().w() ? solid_rect().x2() :
		                                         x() + current_frame().width();
		const int delta_x = value.as_int() - current_x;
		set_x(x() + delta_x);
		if(entity_collides(level::current(), *this, MOVE_NONE) &&
		   entity_in_current_level(this)) {
			set_centi_x(start_x);
		}
		break;
	}

	case CUSTOM_OBJECT_Y2: {
		const int start_y = centi_y();
		const int current_y = solid_rect().h() ? solid_rect().y2() :
		                                         y() + current_frame().height();
		const int delta_y = value.as_int() - current_y;
		set_y(y() + delta_y);
		if(entity_collides(level::current(), *this, MOVE_NONE) &&
		   entity_in_current_level(this)) {
			set_centi_y(start_y);
		}
		break;
	}
	
	case CUSTOM_OBJECT_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		const int start_x = centi_x();
		const int start_y = centi_y();
		set_x(value[0].as_int());
		set_y(value[1].as_int());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
			set_centi_y(start_y);
		}

		break;
	}

	case CUSTOM_OBJECT_Z:
	case CUSTOM_OBJECT_ZORDER:
		zorder_ = value.as_int();
		break;
		
	case CUSTOM_OBJECT_ZSUB_ORDER:
		zsub_order_ = value.as_int();
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
		entity_ptr e(value.try_convert<entity>());
		set_parent(e, parent_pivot_);
		break;
	}

	case CUSTOM_OBJECT_PIVOT: {
		set_parent(parent_, value.as_string());
		break;
	}
	
	case CUSTOM_OBJECT_MID_X:
	case CUSTOM_OBJECT_MIDPOINT_X: {
		//midpoint is, unlike IMG_MID or SOLID_MID, meant to be less-rigorous, but more convenient; it default to basing the "midpoint" on solidity, but drops down to using img_mid if there is no solidity.  The rationale is that generally it doesn't matter which it comes from, and our form of failure (which is silent and returns just x1) is sneaky and can be very expensive because it can take a long time to realize that the value returned actually means the object doesn't have a midpoint for that criteria.  If you need to be rigorous, always use IMG_MID and SOLID_MID.
		const int start_x = centi_x();
			
		const int solid_diff_x = solid_rect().x() - x();
		const int current_x = solid_rect().w() ? (x() + solid_diff_x + solid_rect().w()/2) : x() + current_frame().width()/2;

		const int xdiff = current_x - x();
		set_pos(value.as_int() - xdiff, y());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
		}
		break;
	}

	case CUSTOM_OBJECT_MID_Y:
	case CUSTOM_OBJECT_MIDPOINT_Y: {
		const int start_y = centi_y();
		
		const int solid_diff_y = solid_rect().y() - y();
		const int current_y = solid_rect().h() ? (y() + solid_diff_y + solid_rect().h()/2) : y() + current_frame().height()/2;

		const int ydiff = current_y - y();
		set_pos(x(), value.as_int() - ydiff);
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_y(start_y);
		}
		break;
	}

	case CUSTOM_OBJECT_MID_XY:
	case CUSTOM_OBJECT_MIDPOINT_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set midpoint_xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		const int start_x = centi_x();
		const int solid_diff_x = solid_rect().x() - x();
		const int current_x = solid_rect().w() ? (x() + solid_diff_x + solid_rect().w()/2) : x() + current_frame().width()/2;
		const int xdiff = current_x - x();

		const int start_y = centi_y();
		const int solid_diff_y = solid_rect().y() - y();
		const int current_y = solid_rect().h() ? (y() + solid_diff_y + solid_rect().h()/2) : y() + current_frame().height()/2;
		const int ydiff = current_y - y();

		set_pos(value[0].as_int() - xdiff, value[1].as_int() - ydiff);
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
			set_centi_y(start_y);
		}
		break;
	}

	case CUSTOM_OBJECT_SOLID_MID_X: {
		const int start_x = centi_x();
		const int solid_diff = solid_rect().x() - x();
		const int current_x = x() + solid_diff + solid_rect().w()/2;
		const int xdiff = current_x - x();
		set_pos(value.as_int() - xdiff, y());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
		}
		break;
	}
			
	case CUSTOM_OBJECT_SOLID_MID_Y: {
		const int start_y= centi_y();
		const int solid_diff = solid_rect().y() - y();
		const int current_y = y() + solid_diff + solid_rect().h()/2;
		const int ydiff = current_y - y();
		set_pos(x(), value.as_int() - ydiff);
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_y(start_y);
		}
		break;
	}

	case CUSTOM_OBJECT_SOLID_MID_XY: {
		const int start_x = centi_x();
		const int solid_diff_x = solid_rect().x() - x();
		const int current_x = x() + solid_diff_x + solid_rect().w()/2;
		const int xdiff = current_x - x();
		const int start_y= centi_y();
		const int solid_diff_y = solid_rect().y() - y();
		const int current_y = y() + solid_diff_y + solid_rect().h()/2;
		const int ydiff = current_y - y();
		set_pos(value[0].as_int() - xdiff, value[1].as_int() - ydiff);
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
			set_centi_y(start_y);
		}
		break;
	}

	case CUSTOM_OBJECT_IMG_MID_X: {
		const int start_x = centi_x();
		const int current_x = x() + current_frame().width()/2;
		const int xdiff = current_x - x();
		set_pos(value.as_int() - xdiff, y());
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
		}
		break;
	}
		
	case CUSTOM_OBJECT_IMG_MID_Y: {
		const int start_y = centi_y();
		const int current_y = y() + current_frame().height()/2;
		const int ydiff = current_y - y();
		set_pos(x(), value.as_int() - ydiff);
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_y(start_y);
		}
		break;
	}
		
	case CUSTOM_OBJECT_IMG_MID_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set midpoint_xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		const int start_x = centi_x();
		const int current_x = x() + current_frame().width()/2;
		const int xdiff = current_x - x();
		const int start_y = centi_y();
		const int current_y = y() + current_frame().height()/2;
		const int ydiff = current_y - y();
		set_pos(value[0].as_int() - xdiff, value[1].as_int() - ydiff);
		if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
			set_centi_x(start_x);
			set_centi_y(start_y);
		}
		break;
	}
			
	case CUSTOM_OBJECT_CYCLE:
		cycle_ = value.as_int();
		break;

	case CUSTOM_OBJECT_FACING:
		set_face_right(value.as_int() > 0);
		break;
	
	case CUSTOM_OBJECT_UPSIDE_DOWN:
		set_upside_down(value.as_int() > 0);
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
		max_hitpoints_ = value.as_int() - type_->hitpoints();
		if(hitpoints_ > type_->hitpoints() + max_hitpoints_) {
			hitpoints_ = type_->hitpoints() + max_hitpoints_;
		}
		break;

	case CUSTOM_OBJECT_VELOCITY_X:
		velocity_x_ = value.as_int();
		break;
	
	case CUSTOM_OBJECT_VELOCITY_Y:
		velocity_y_ = value.as_int();
		break;
	
	case CUSTOM_OBJECT_VELOCITY_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set velocity_xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		velocity_x_ = value[0].as_int();
		velocity_y_ = value[1].as_int();
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
		velocity_x_ = (xval*1000).as_int();
		velocity_y_ = (yval*1000).as_int();
		break;
	}
	case CUSTOM_OBJECT_ACCEL_X:
		accel_x_ = value.as_int();
		break;

	case CUSTOM_OBJECT_ACCEL_Y:
		accel_y_ = value.as_int();
		break;

	case CUSTOM_OBJECT_ACCEL_XY: {
		ASSERT_LOG(value.is_list() && value.num_elements() == 2, "set accel_xy value of object to a value in incorrect format ([x,y] expected): " << value.to_debug_string());
		accel_x_ = value[0].as_int();
		accel_y_ = value[1].as_int();
		break;
	}

	case CUSTOM_OBJECT_GRAVITY_SHIFT:
		gravity_shift_ = value.as_int();
		break;

	case CUSTOM_OBJECT_PLATFORM_MOTION_X:
		set_platform_motion_x(value.as_int());
		break;

	case CUSTOM_OBJECT_ROTATE:
	case CUSTOM_OBJECT_ROTATE_Z:
		rotate_z_ = value.as_decimal();
		break;
	case CUSTOM_OBJECT_ROTATE_X:
		rotate_x_ = value.as_decimal();
		break;
	case CUSTOM_OBJECT_ROTATE_Y:
		rotate_y_ = value.as_decimal();
		break;

	case CUSTOM_OBJECT_RED:
		make_draw_color();
		draw_color_->buf()[0] = truncate_to_char(value.as_int());
		break;
	
	case CUSTOM_OBJECT_GREEN:
		make_draw_color();
		draw_color_->buf()[1] = truncate_to_char(value.as_int());
		break;
	
	case CUSTOM_OBJECT_BLUE:
		make_draw_color();
		draw_color_->buf()[2] = truncate_to_char(value.as_int());
		break;

	case CUSTOM_OBJECT_ALPHA:
		make_draw_color();
		draw_color_->buf()[3] = truncate_to_char(value.as_int());
		break;

	case CUSTOM_OBJECT_TEXT_ALPHA:
		if(!text_) {
			set_text("", "default", 10, false);
		}

		text_->alpha = value.as_int();
		break;

	case CUSTOM_OBJECT_BRIGHTNESS:
		make_draw_color();
		draw_color_->buf()[0] = value.as_int();
		draw_color_->buf()[1] = value.as_int();
		draw_color_->buf()[2] = value.as_int();
		break;
	
	case CUSTOM_OBJECT_DISTORTION:
		distortion_ = value.try_convert<graphics::raster_distortion>();
		break;
	
	case CUSTOM_OBJECT_CURRENT_GENERATOR:
		set_current_generator(value.try_convert<current_generator>());
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
			tags_ = new game_logic::map_formula_callable;
			for(int n = 0; n != value.num_elements(); ++n) {
				tags_->add(value[n].as_string(), variant(1));
			}
		}

		break;

#if defined(USE_SHADERS)
	case CUSTOM_OBJECT_SHADER: {
		using namespace gles2;
		if(value.is_map()) {
			shader_.reset(new shader_program(value));
		} else {
			shader_.reset(value.try_convert<shader_program>());
		}
		break;
	}
	case CUSTOM_OBJECT_EFFECTS: {
		using namespace gles2;
		effects_.clear();
		if(value.is_list()) {
			for(size_t n = 0; n < value.num_elements(); ++n) {
				if(value[n].is_map()) {
					effects_.push_back(new shader_program(value[n]));
				} else {
					effects_.push_back(shader_program_ptr(value[n].try_convert<shader_program>()));
				}
			}
		} else if(value.is_map()) {
			effects_.push_back(new shader_program(value));
		} else {
			effects_.push_back(shader_program_ptr(value.try_convert<shader_program>()));
			ASSERT_LOG(effects_.size() > 0, "Couldn't convert type to shader");
		}
		break;
	}
#endif

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
	
	case CUSTOM_OBJECT_CLIP_AREA:
		if(value.is_list() && value.num_elements() == 4) {
			clip_area_.reset(new rect(value[0].as_int(), value[1].as_int(), value[2].as_int(), value[3].as_int()));
		} else {
			ASSERT_LOG(value.is_null(), "BAD CLIP AREA: " << value.to_debug_string());
			clip_area_.reset();
		}

		break;

	case CUSTOM_OBJECT_ALWAYS_ACTIVE:
		always_active_ = value.as_bool();
		break;
			
	case CUSTOM_OBJECT_VARIATIONS:
		handle_event("reset_variations");
		current_variation_.clear();
		if(value.is_list()) {
			for(int n = 0; n != value.num_elements(); ++n) {
				current_variation_.push_back(value[n].as_string());
			}
		} else if(value.is_string()) {
			current_variation_.push_back(value.as_string());
		}

		if(current_variation_.empty()) {
			type_ = base_type_;
		} else {
			type_ = base_type_->get_variation(current_variation_);
		}

		calculate_solid_rect();
		handle_event("set_variations");
		break;
	
	case CUSTOM_OBJECT_ATTACHED_OBJECTS: {
		std::vector<entity_ptr> v;
		for(int n = 0; n != value.num_elements(); ++n) {
			entity* e = value[n].try_convert<entity>();
			if(e) {
				v.push_back(entity_ptr(e));
			}

			//this will initialize shaders and such, which is
			//desired for attached objects
			e->add_to_level();
		}

		set_attached_objects(v);
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

		set_collide_dimensions(solid, weak);
		break;
	}

	case CUSTOM_OBJECT_LIGHTS: {
		lights_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			light* p = value[n].try_convert<light>();
			if(p) {
				lights_.push_back(light_ptr(p));
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

		const unsigned int old_solid = solid_dimensions();
		const unsigned int old_weak = weak_solid_dimensions();
		set_solid_dimensions(solid, weak);
		collision_info collide_info;
		if(entity_in_current_level(this) && entity_collides(level::current(), *this, MOVE_NONE, &collide_info)) {
			set_solid_dimensions(old_solid, old_weak);
			ASSERT_EQ(entity_collides(level::current(), *this, MOVE_NONE), false);

			game_logic::map_formula_callable* callable(new game_logic::map_formula_callable(this));
			callable->add("collide_with", variant(collide_info.collide_with.get()));
			game_logic::formula_callable_ptr callable_ptr(callable);

			handle_event(OBJECT_EVENT_CHANGE_SOLID_DIMENSIONS_FAIL, callable);
		}

		break;
	}

	case CUSTOM_OBJECT_X_SCHEDULE: {
		if(position_schedule_.get() == NULL) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->x_pos.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			position_schedule_->x_pos.push_back(value[n].as_int());
		}
		break;
	}
	case CUSTOM_OBJECT_Y_SCHEDULE: {
		if(position_schedule_.get() == NULL) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->y_pos.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			position_schedule_->y_pos.push_back(value[n].as_int());
		}
		break;
	}
	case CUSTOM_OBJECT_ROTATION_SCHEDULE: {
		if(position_schedule_.get() == NULL) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->rotation.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			position_schedule_->rotation.push_back(value[n].as_decimal());
		}
		break;
	}

	case CUSTOM_OBJECT_SCHEDULE_SPEED: {
		if(position_schedule_.get() == NULL) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->speed = value.as_int();

		break;
	}

	case CUSTOM_OBJECT_SCHEDULE_EXPIRES: {
		if(position_schedule_.get() == NULL) {
			position_schedule_.reset(new PositionSchedule);
			position_schedule_->base_cycle = cycle_;
		}

		position_schedule_->expires = true;
		break;
	}

	case CUSTOM_OBJECT_PLATFORM_AREA: {
		if(value.is_null()) {
			platform_area_.reset();
			platform_solid_info_ = const_solid_info_ptr();
			calculate_solid_rect();
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
			platform_offsets_.push_back(value[n].as_int());
		}
		break;
	}

	case CUSTOM_OBJECT_USE_ABSOLUTE_SCREEN_COORDINATES: {
		use_absolute_screen_coordinates_ = value.as_bool();
		break;
	}

	case CUSTOM_OBJECT_WIDGETS:
	case CUSTOM_OBJECT_WIDGET_LIST: {
		std::vector<gui::widget_ptr> w;
		clear_widgets();
		if(value.is_list()) {
			foreach(const variant& v, value.as_list()) {
				w.push_back(widget_factory::create(v, this));
			}
		} else {
			w.push_back(widget_factory::create(value, this));
		}
		add_widgets(&w);
		break;
	}

	case CUSTOM_OBJECT_MOUSEOVER_DELAY: {
		set_mouseover_delay(value.as_int());
		break;
	}

	case CUSTOM_OBJECT_MOUSEOVER_AREA: {
		set_mouse_over_area(rect(value));
		break;
	}

	case CUSTOM_OBJECT_TRUEZ: {
		set_truez(value.as_bool());
		break;
	}
	case CUSTOM_OBJECT_TX: {
		set_tx(value.as_decimal().as_float());
		break;
	}
	case CUSTOM_OBJECT_TY: {
		set_ty(value.as_decimal().as_float());
		break;
	}
	case CUSTOM_OBJECT_TZ: {
		set_tz(value.as_decimal().as_float());
		break;
	}

	case CUSTOM_OBJECT_CTRL_USER_OUTPUT: {
		controls::set_user_ctrl_output(value);
		break;
	}

#if defined(USE_BOX2D)
	case CUSTOM_OBJECT_BODY: {
		//if(body_) {
		//	box2d::world::our_world_ptr()->destroy_body(body_);
		//}
		body_.reset(new box2d::body(value));
		body_->finish_loading(this);
		break;
	}
#endif

	case CUSTOM_OBJECT_PAUSED: {
		paused_ = value.as_bool();
		break;
	}

	case CUSTOM_OBJECT_CUSTOM_DRAW: {
		if(value.is_null()) {
			custom_draw_.reset();
		}

		std::vector<frame::CustomPoint>* v = new std::vector<frame::CustomPoint>;

		custom_draw_.reset(v);

		std::vector<GLfloat> positions;

		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_decimal() || value[n].is_int()) {
				positions.push_back(GLfloat(value[n].as_decimal().as_float()));
			} else if(value[n].is_list()) {
				for(int index = 0; index != value[n].num_elements(); index += 2) {
					ASSERT_LOG(value[n].num_elements() - index >= 2, "ILLEGAL VALUE TO custom_draw: " << value.to_debug_string() << ", " << n << ", " << index << "/" << value[n].num_elements());

					ASSERT_LOG(v->size() < positions.size(), "ILLEGAL VALUE TO custom_draw -- not enough positions for number of offsets: " << value.to_debug_string() << " " << v->size() << " VS " << positions.size());
					const GLfloat pos = positions[v->size()];

					v->push_back(frame::CustomPoint());
					v->back().pos = pos;
					v->back().offset = point(value[n][index].as_int(), value[n][index + 1].as_int());
				}
			}
		}

		ASSERT_LOG(v->size() >= 3, "ILLEGAL VALUE TO custom_draw: " << value.to_debug_string());

		std::vector<frame::CustomPoint> draw_order;
		int n1 = 0, n2 = v->size() - 1;
		while(n1 <= n2) {
			draw_order.push_back((*v)[n1]);
			if(n2 > n1) {
				draw_order.push_back((*v)[n2]);
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
			foreach(const variant& v, value.as_list()) {
				custom_draw_uv_.push_back(v.as_decimal().as_float());
			}
		}

		break;
	}

	case CUSTOM_OBJECT_XY_ARRAY: {
		if(value.is_null()) {
			custom_draw_xy_.clear();
		} else {
			custom_draw_xy_.clear();
			foreach(const variant& v, value.as_list()) {
				custom_draw_xy_.push_back(v.as_decimal().as_float());
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
			const GLfloat y = GLfloat(ypos)/GLfloat(ydim-1);
			const GLfloat y2 = GLfloat(ypos+1)/GLfloat(ydim-1);
			for(int xpos = 0; xpos < xdim; ++xpos) {
				const GLfloat x = GLfloat(xpos)/GLfloat(xdim-1);

				if(xpos == 0 && ypos > 0) {
					custom_draw_uv_.push_back(x);
					custom_draw_uv_.push_back(y);
				}

				custom_draw_uv_.push_back(x);
				custom_draw_uv_.push_back(y);
				custom_draw_uv_.push_back(x);
				custom_draw_uv_.push_back(y2);

				if(xpos == xdim-1 && ypos != ydim-2) {
					custom_draw_uv_.push_back(x);
					custom_draw_uv_.push_back(y2);
				}
			}
		}

		custom_draw_xy_ = custom_draw_uv_;
		break;
	}

	case CUSTOM_OBJECT_DRAW_PRIMITIVES: {
#if defined(USE_SHADERS)
		draw_primitives_.clear();
		for(int n = 0; n != value.num_elements(); ++n) {
			if(value[n].is_callable()) {
				boost::intrusive_ptr<graphics::draw_primitive> obj(value[n].try_convert<graphics::draw_primitive>());
				ASSERT_LOG(obj.get() != NULL, "BAD OBJECT PASSED WHEN SETTING draw_primitives");
				draw_primitives_.push_back(obj);
			} else if(!value[n].is_null()) {
				draw_primitives_.push_back(graphics::draw_primitive::create(value[n]));
			}
		}
		break;
#else
		break;
#endif
	}

	default:
		if(slot >= type_->slot_properties_base() && (size_t(slot - type_->slot_properties_base()) < type_->slot_properties().size())) {
			const custom_object_type::property_entry& e = type_->slot_properties()[slot - type_->slot_properties_base()];
			ASSERT_LOG(!e.const_value, "Attempt to set const property: " << debug_description() << "." << e.id);
			if(e.setter) {

				if(e.set_type) {
					ASSERT_LOG(e.set_type->match(value), "Setting " << debug_description() << "." << e.id << " to illegal value " << value.write_json() << " of type " << get_variant_type_from_value(value)->to_string() << " expected type " << e.set_type->to_string());
				}

				active_property_scope scope(*this, e.storage_slot, &value);
				variant value = e.setter->execute(*this);
				execute_command(value);
			} else if(e.storage_slot >= 0) {
				get_property_data(e.storage_slot) = value;
			} else {
				ASSERT_LOG(false, "Attempt to set const property: " << debug_description() << "." << e.id);
			}

			if(!properties_requiring_dynamic_initialization_.empty()) {
				std::vector<int>::iterator itor = std::find(properties_requiring_dynamic_initialization_.begin(), properties_requiring_dynamic_initialization_.end(), slot - type_->slot_properties_base());
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
		case CUSTOM_OBJECT_PLAYER_CTRL_KEYS:
		case CUSTOM_OBJECT_PLAYER_CTRL_MICE:
		case CUSTOM_OBJECT_PLAYER_CTRL_TILT:
		case CUSTOM_OBJECT_PLAYER_CTRL_X:
		case CUSTOM_OBJECT_PLAYER_CTRL_Y:
		case CUSTOM_OBJECT_PLAYER_CTRL_REVERSE_AB:
		case CUSTOM_OBJECT_PLAYER_CONTROL_SCHEME:
		case CUSTOM_OBJECT_PLAYER_VERTICAL_LOOK:
		case CUSTOM_OBJECT_PLAYER_CONTROL_LOCK:
			set_player_value_by_slot(slot, value);

		break;
	}
}

void custom_object::set_frame(const std::string& name)
{
	set_frame(type_->get_frame(name));
}

void custom_object::set_frame(const frame& new_frame)
{
	const std::string& name = new_frame.id();
	const std::string previous_animation = frame_name_;

	const bool changing_anim = name != frame_name_;

	//fire an event to say that we're leaving the current frame.
	if(frame_ && changing_anim) {
		handle_event(frame_->leave_event_id());
	}

	const int start_x = feet_x();
	const int start_y = feet_y();

	frame_.reset(&new_frame);
	calculate_solid_rect();
	++current_animation_id_;

	const int diff_x = feet_x() - start_x;
	const int diff_y = feet_y() - start_y;

	if(type_->adjust_feet_on_animation_change()) {
		move_centipixels(-diff_x*100, -diff_y*100);
	}

	set_frame_no_adjustments(new_frame);

	frame_->play_sound(this);

	if(entity_collides(level::current(), *this, MOVE_NONE) && entity_in_current_level(this)) {
		game_logic::map_formula_callable* callable(new game_logic::map_formula_callable);
		callable->add("previous_animation", variant(previous_animation));
		game_logic::formula_callable_ptr callable_ptr(callable);
		static int change_animation_failure_recurse = 0;
		ASSERT_LOG(change_animation_failure_recurse < 5, "OBJECT " << type_->id() << " FAILS TO RESOLVE ANIMATION CHANGE FAILURES");
		++change_animation_failure_recurse;
		handle_event(OBJECT_EVENT_CHANGE_ANIMATION_FAILURE, callable);
		handle_event("change_animation_failure_" + frame_name_, callable);
		--change_animation_failure_recurse;
		ASSERT_LOG(destroyed() || !entity_collides(level::current(), *this, MOVE_NONE),
		  "Object '" << type_->id() << "' has different solid areas when changing from frame " << previous_animation << " to " << frame_name_ << " and doesn't handle it properly");
	}

	handle_event(OBJECT_EVENT_ENTER_ANIM);
	handle_event(frame_->enter_event_id());
}

rect custom_object::draw_rect() const
{
	if(draw_area_) {
		return rect(x(), y(), draw_area_->w()*2, draw_area_->h()*2);
	} else {
		return rect(x(), y(), frame_->width(), frame_->height());
	}
}

void custom_object::set_frame_no_adjustments(const std::string& name)
{
	set_frame_no_adjustments(type_->get_frame(name));
}

void custom_object::set_frame_no_adjustments(const frame& new_frame)
{
	frame_.reset(&new_frame);
	frame_name_ = new_frame.id();
	time_in_frame_ = 0;
	if(frame_->velocity_x() != INT_MIN) {
		velocity_x_ = frame_->velocity_x() * (face_right() ? 1 : -1);
	}

	if(frame_->velocity_y() != INT_MIN) {
		velocity_y_ = frame_->velocity_y();
	}

	if(frame_->accel_x() != INT_MIN) {
		accel_x_ = frame_->accel_x();
	}
	
	if(frame_->accel_y() != INT_MIN) {
		accel_y_ = frame_->accel_y();
	}

	calculate_solid_rect();
}

void custom_object::die()
{
	hitpoints_ = 0;
	handle_event(OBJECT_EVENT_DIE);

#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active(false);
	}
#endif
}

void custom_object::die_with_no_event()
{
	hitpoints_ = 0;

#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active(false);
	}
#endif
}


bool custom_object::is_active(const rect& screen_area) const
{
	if(controls::num_players() > 1) {
		//in multiplayer, make all objects always active
		//TODO: review this behavior
		return true;
	}

	if(always_active()) {
		return true;
	}

	if(type_->goes_inactive_only_when_standing() && !is_standing(level::current())) {
		return true;
	}

	if(activation_area_) {
		return rects_intersect(*activation_area_, screen_area);
	}

	if(text_) {
		const rect text_area(x(), y(), text_->dimensions.w(), text_->dimensions.h());
		if(rects_intersect(screen_area, text_area)) {
			return true;
		}
	}

	const rect& area = frame_rect();
	if(draw_area_) {
		rect draw_area(area.x(), area.y(), draw_area_->w()*2, draw_area_->h()*2);
		return rects_intersect(draw_area, screen_area);
	}
	
	if(parallax_scale_millis_.get() != NULL) {
		if(parallax_scale_millis_->first != 1000 || parallax_scale_millis_->second != 1000){
			const int diffx = ((parallax_scale_millis_->first - 1000)*screen_area.x())/1000;
			const int diffy = ((parallax_scale_millis_->second - 1000)*screen_area.y())/1000;
			rect screen(screen_area.x() - diffx, screen_area.y() - diffy,
						screen_area.w(), screen_area.h());
			const rect& area = frame_rect();
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

bool custom_object::move_to_standing(level& lvl, int max_displace)
{
	int start_y = y();
	const bool result = move_to_standing_internal(lvl, max_displace);
	if(!result || entity_collides(level::current(), *this, MOVE_NONE)) {
		set_pos(x(), start_y);
		return false;
	}

	return result;
}

bool custom_object::move_to_standing_internal(level& lvl, int max_displace)
{
	int start_y = y();
	//descend from the initial-position (what the player was at in the prev level) until we're standing
	for(int n = 0; n != max_displace; ++n) {
		if(is_standing(lvl)) {
			
			if(n == 0) {  //if we've somehow managed to be standing on the very first frame, try to avoid the possibility that this is actually some open space underground on a cave level by scanning up till we reach the surface.
				for(int n = 0; n != max_displace; ++n) {
					set_pos(x(), y() - 1);
					if(!is_standing(lvl)) {
						set_pos(x(), y() + 1);
						
						if(y() < lvl.boundaries().y()) {
							//we are too high, out of the level. Move the
							//character down, under the solid, and then
							//call this function again to move them down
							//to standing on the solid below.
							for(int n = 0; n != max_displace; ++n) {
								set_pos(x(), y() + 1);
								if(!is_standing(lvl)) {
									return move_to_standing_internal(lvl, max_displace);
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
		
		set_pos(x(), y() + 1);
	}
	
	set_pos(x(), start_y);
	return false;
}


bool custom_object::dies_on_inactive() const
{
	return type_->dies_on_inactive();
}

bool custom_object::always_active() const
{
	return always_active_ || type_->always_active();
}

bool custom_object::body_harmful() const
{
	return type_->body_harmful();
}

bool custom_object::body_passthrough() const
{
	return type_->body_passthrough();
}

const frame& custom_object::icon_frame() const
{
	return type_->default_frame();
}

entity_ptr custom_object::clone() const
{
	entity_ptr res(new custom_object(*this));
	res->set_distinct_label();
	return res;
}

entity_ptr custom_object::backup() const
{
	if(type_->stateless()) {
		return entity_ptr(const_cast<custom_object*>(this));
	}

	entity_ptr res(new custom_object(*this));
	return res;
}

bool custom_object::handle_event(const std::string& event, const formula_callable* context)
{
	return handle_event(get_object_event_id(event), context);
}

bool custom_object::handle_event_delay(int event, const formula_callable* context)
{
	return handle_event_internal(event, context, false);
}

namespace {
void run_expression_for_edit_and_continue(boost::function<bool()> fn, bool* success, bool* res)
{
	*success = false;
	*res = fn();
	*success = true;
}
}

bool custom_object::handle_event(int event, const formula_callable* context)
{
	if(preferences::edit_and_continue()) {
		try {
			const_custom_object_type_ptr type_back = type_;
			const_custom_object_type_ptr base_type_back = base_type_;
			assert_edit_and_continue_fn_scope scope(boost::function<void()>(boost::bind(&custom_object::handle_event_internal, this, event, context, true)));
			return handle_event_internal(event, context);
		} catch(validation_failure_exception& e) {
			return true;
		}
	} else {
		try {
			return handle_event_internal(event, context);
		} catch(validation_failure_exception& e) {
			if(level::current().in_editor()) {
				return true;
			}

			throw e;
		}
	}
}

namespace {
struct die_event_scope {
	int event_;
	int& flag_;
	die_event_scope(int event, int& flag) : event_(event), flag_(flag) {
		if(event_ == OBJECT_EVENT_DIE) {
			++flag_;
		}
	}

	~die_event_scope() {
		if(event_ == OBJECT_EVENT_DIE) {
			--flag_;
		}
	}
};
}

bool custom_object::handle_event_internal(int event, const formula_callable* context, bool execute_commands_now)
{
	if(paused_) {
		return false;
	}

	const die_event_scope die_scope(event, currently_handling_die_event_);
	if(hitpoints_ <= 0 && !currently_handling_die_event_) {
		return false;
	}

#ifndef NO_EDITOR
	if(event != OBJECT_EVENT_ANY && (size_t(event) < event_handlers_.size() && event_handlers_[OBJECT_EVENT_ANY] || type_->get_event_handler(OBJECT_EVENT_ANY))) {
		game_logic::map_formula_callable* callable = new game_logic::map_formula_callable;
		variant v(callable);

		callable->add("event", variant(get_object_event_str(event)));

		handle_event_internal(OBJECT_EVENT_ANY, callable, true);
	}
#endif

	const game_logic::formula* handlers[2];
	int nhandlers = 0;

	if(size_t(event) < event_handlers_.size() && event_handlers_[event]) {
		handlers[nhandlers++] = event_handlers_[event].get();
	}

	const game_logic::formula* type_handler = type_->get_event_handler(event).get();
	if(type_handler != NULL) {
		handlers[nhandlers++] = type_handler;
	}

	if(!nhandlers) {
		return false;
	}

	swallow_mouse_event_ = false;
	backup_callable_stack_scope callable_scope(&backup_callable_stack_, context);

	for(int n = 0; n != nhandlers; ++n) {
		const game_logic::formula* handler = handlers[n];

#ifndef DISABLE_FORMULA_PROFILER
		formula_profiler::custom_object_event_frame event_frame = { type_.get(), event, false };
		event_call_stack.push_back(event_frame);
#endif

		++events_handled_per_second;

		variant var;
		
		try {
			formula_profiler::instrument instrumentation("FFL");
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
			if(execute_commands_now) {
				formula_profiler::instrument instrumentation("COMMANDS");
				result = execute_command(var);
			} else {
				delayed_commands_.push_back(var);
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

void custom_object::resolve_delayed_events()
{
	if(delayed_commands_.empty()) {
		return;
	}

	try {
		foreach(const variant& v, delayed_commands_) {
			execute_command(v);
		}
	} catch(validation_failure_exception&) {
	}

	delayed_commands_.clear();
}

bool custom_object::execute_command(const variant& var)
{
	bool result = true;
	if(var.is_null()) { return result; }
	if(var.is_list()) {
		const int num_elements = var.num_elements();
		for(int n = 0; n != num_elements; ++n) {
			result = execute_command(var[n]) && result;
		}
	} else {
		game_logic::command_callable* cmd = var.try_convert<game_logic::command_callable>();
		if(cmd != NULL) {
			cmd->run_command(*this);
		} else {
			custom_object_command_callable* cmd = var.try_convert<custom_object_command_callable>();
			if(cmd != NULL) {
				cmd->run_command(level::current(), *this);
			} else {
				entity_command_callable* cmd = var.try_convert<entity_command_callable>();
				if(cmd != NULL) {
					cmd->run_command(level::current(), *this);
				} else {
					swallow_object_command_callable* cmd = var.try_convert<swallow_object_command_callable>();
					if(cmd) {
						result = false;
					} else {
						swallow_mouse_command_callable* cmd = var.try_convert<swallow_mouse_command_callable>();
						if(cmd) {
							swallow_mouse_event_ = true;
						} else {
							ASSERT_LOG(false, "COMMAND WAS EXPECTED, BUT FOUND: " << var.to_debug_string() << "\nFORMULA INFO: " << output_formula_error_info() << "\n");
						}
					}
				}
			}
		}
	}

	return result;
}

int custom_object::slope_standing_on(int range) const
{
	if(!is_standing(level::current())) {
		return 0;
	}

	const int forward = face_right() ? 1 : -1;
	const int xpos = feet_x();
	int ypos = feet_y();


	for(int n = 0; !level::current().standable(xpos, ypos) && n != 10; ++n) {
		++ypos;
	}

	if(range == 1) {
		if(level::current().standable(xpos + forward, ypos - 1) &&
		   !level::current().standable(xpos - forward, ypos)) {
			return 45;
		}

		if(!level::current().standable(xpos + forward, ypos) &&
		   level::current().standable(xpos - forward, ypos - 1)) {
			return -45;
		}

		return 0;
	} else {
		if(!is_standing(level::current())) {
			return 0;
		}

		int y1 = find_ground_level(level::current(), xpos + forward*range, ypos, range+1);
		int y2 = find_ground_level(level::current(), xpos - forward*range, ypos, range+1);
		while((y1 == INT_MIN || y2 == INT_MIN) && range > 0) {
			y1 = find_ground_level(level::current(), xpos + forward*range, ypos, range+1);
			y2 = find_ground_level(level::current(), xpos - forward*range, ypos, range+1);
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

void custom_object::make_draw_color()
{
	if(!draw_color_.get()) {
		draw_color_.reset(new graphics::color_transform(draw_color()));
	}
}

const graphics::color_transform& custom_object::draw_color() const
{
	if(draw_color_.get()) {
		return *draw_color_;
	}

	static const graphics::color_transform white(0xFF, 0xFF, 0xFF, 0xFF);
	return white;
}

game_logic::const_formula_ptr custom_object::get_event_handler(int key) const
{
	if(size_t(key) < event_handlers_.size()) {
		return event_handlers_[key];
	} else {
		return game_logic::const_formula_ptr();
	}
}

void custom_object::set_event_handler(int key, game_logic::const_formula_ptr f)
{
	if(size_t(key) >= event_handlers_.size()) {
		event_handlers_.resize(key+1);
	}

	event_handlers_[key] = f;
}

bool custom_object::can_interact_with() const
{
	return can_interact_with_;
}

std::string custom_object::debug_description() const
{
	return type_->id();
}

namespace {
bool map_variant_entities(variant& v, const std::map<entity_ptr, entity_ptr>& m)
{
	if(v.is_list()) {
		for(int n = 0; n != v.num_elements(); ++n) {
			variant var = v[n];
			if(map_variant_entities(var, m)) {
				std::vector<variant> new_values;
				for(int i = 0; i != n; ++i) {
					new_values.push_back(v[i]);
				}

				new_values.push_back(var);
				for(size_t i = n+1; i < v.num_elements(); ++i) {
					var = v[i];
					map_variant_entities(var, m);
					new_values.push_back(var);
				}

				v = variant(&new_values);
				return true;
			}
		}
	} else if(v.try_convert<entity>()) {
		entity* e = v.try_convert<entity>();
		std::map<entity_ptr, entity_ptr>::const_iterator i = m.find(entity_ptr(e));
		if(i != m.end()) {
			v = variant(i->second.get());
			return true;
		} else {
			entity_ptr back = e->backup();
			v = variant(back.get());
			return true;
		}
	}

	return false;
}

void do_map_entity(entity_ptr& e, const std::map<entity_ptr, entity_ptr>& m)
{
	if(e) {
		std::map<entity_ptr, entity_ptr>::const_iterator i = m.find(e);
		if(i != m.end()) {
			e = i->second;
		}
	}
}
}

void custom_object::map_entities(const std::map<entity_ptr, entity_ptr>& m)
{
	do_map_entity(last_hit_by_, m);
	do_map_entity(standing_on_, m);
	do_map_entity(parent_, m);

	foreach(variant& v, vars_->values()) {
		map_variant_entities(v, m);
	}

	foreach(variant& v, tmp_vars_->values()) {
		map_variant_entities(v, m);
	}

	foreach(variant& v, property_data_) {
		map_variant_entities(v, m);
	}
}

void custom_object::cleanup_references()
{
	last_hit_by_.reset();
	standing_on_.reset();
	parent_.reset();
	foreach(variant& v, vars_->values()) {
		v = variant();
	}

	foreach(variant& v, tmp_vars_->values()) {
		v = variant();
	}

	foreach(variant& v, property_data_) {
		v = variant();
	}
}

void custom_object::extract_gc_object_references(std::vector<gc_object_reference>& v)
{
	extract_gc_object_references(last_hit_by_, v);
	extract_gc_object_references(standing_on_, v);
	extract_gc_object_references(parent_, v);
	foreach(variant& var, vars_->values()) {
		extract_gc_object_references(var, v);
	}

	foreach(variant& var, tmp_vars_->values()) {
		extract_gc_object_references(var, v);
	}

	foreach(variant& var, property_data_) {
		extract_gc_object_references(var, v);
	}

	gc_object_reference visitor;
	visitor.owner = this;
	visitor.target = NULL;
	visitor.from_variant = NULL;
	visitor.visitor.reset(new game_logic::formula_callable_visitor);
	foreach(gui::widget_ptr w, widgets_) {
		w->perform_visit_values(*visitor.visitor);
	}

	foreach(game_logic::formula_callable_suspended_ptr ptr, visitor.visitor->pointers()) {
		if(dynamic_cast<const custom_object*>(ptr->value())) {
			ptr->destroy_ref();
		}
	}

	v.push_back(visitor);
}

void custom_object::extract_gc_object_references(entity_ptr& e, std::vector<gc_object_reference>& v)
{
	if(!e) {
		return;
	}

	v.resize(v.size()+1);
	gc_object_reference& ref = v.back();
	ref.owner = this;
	ref.target = e.get();
	ref.from_variant = NULL;
	ref.from_ptr = &e;

	e.reset();
}

void custom_object::extract_gc_object_references(variant& var, std::vector<gc_object_reference>& v)
{
	if(var.is_callable()) {
		if(var.try_convert<entity>()) {
			v.resize(v.size()+1);
			gc_object_reference& ref = v.back();
			ref.owner = this;
			ref.target = var.try_convert<entity>();
			ref.from_variant = &var;
			ref.from_ptr = NULL;

			var = variant();
		}
	} else if(var.is_list()) {
		for(int n = 0; n != var.num_elements(); ++n) {
			extract_gc_object_references(*var.get_index_mutable(n), v);
		}
	} else if(var.is_map()) {
		foreach(variant k, var.get_keys().as_list()) {
			extract_gc_object_references(*var.get_attr_mutable(k), v);
		}
	}
}

void custom_object::restore_gc_object_reference(gc_object_reference ref)
{
	if(ref.visitor) {
		foreach(game_logic::formula_callable_suspended_ptr ptr, ref.visitor->pointers()) {
			ptr->restore_ref();
		}
	} else if(ref.from_variant) {
		*ref.from_variant = variant(ref.target);
	} else {
		ref.from_ptr->reset(ref.target);
	}
}

void custom_object::add_particle_system(const std::string& key, const std::string& type)
{
	particle_systems_[key] = type_->get_particle_system_factory(type)->create(*this);
	particle_systems_[key]->set_type(type);
}

void custom_object::remove_particle_system(const std::string& key)
{
	particle_systems_.erase(key);
}

void custom_object::set_text(const std::string& text, const std::string& font, int size, int align)
{
	text_.reset(new custom_object_text);
	text_->text = text;
	text_->font = graphical_font::get(font);
	text_->size = size;
	text_->align = align;
	text_->alpha = 255;
	ASSERT_LOG(text_->font, "UNKNOWN FONT: " << font);
	text_->dimensions = text_->font->dimensions(text_->text, size);
}

bool custom_object::boardable_vehicle() const
{
	return type_->is_vehicle() && driver_.get() == NULL;
}

void custom_object::boarded(level& lvl, const entity_ptr& player)
{
	if(!player) {
		return;
	}

	player->board_vehicle();

	if(player->is_human()) {
		playable_custom_object* new_player(new playable_custom_object(*this));
		new_player->driver_ = player;

		lvl.add_player(new_player);

		new_player->get_player_info()->swap_player_state(*player->get_player_info());
		lvl.remove_character(this);
	} else {
		driver_ = player;
		lvl.remove_character(player);
	}
}

void custom_object::unboarded(level& lvl)
{
	if(velocity_x() > 100) {
		driver_->set_face_right(false);
	}

	if(velocity_x() < -100) {
		driver_->set_face_right(true);
	}

	if(is_human()) {
		custom_object* vehicle(new custom_object(*this));
		vehicle->driver_ = entity_ptr();
		lvl.add_character(vehicle);

		lvl.add_player(driver_);

		driver_->unboard_vehicle();

		driver_->get_player_info()->swap_player_state(*get_player_info());
	} else {
		lvl.add_character(driver_);
		driver_->unboard_vehicle();
		driver_ = entity_ptr();
	}
}

void custom_object::board_vehicle()
{
}

void custom_object::unboard_vehicle()
{
}

void custom_object::set_blur(const blur_info* blur)
{
	if(blur) {
		if(blur_) {
			blur_->copy_settings(*blur); 
		} else {
			blur_.reset(new blur_info(*blur));
		}
	} else {
		blur_.reset();
	}
}

void custom_object::set_sound_volume(const int sound_volume)
{
	sound::change_volume(this, sound_volume);
	sound_volume_ = sound_volume;
}

bool custom_object::allow_level_collisions() const
{
	return type_->static_object() || !type_->collides_with_level();
}

void custom_object::set_platform_area(const rect& area)
{
	if(area.w() <= 0 || area.h() <= 0) {
		platform_area_.reset(new rect(area));
		platform_solid_info_ = const_solid_info_ptr();
	} else {
		platform_area_.reset(new rect(area));
		platform_solid_info_ = solid_info::create_platform(area);
	}

	calculate_solid_rect();
}

void custom_object::shift_position(int x, int y)
{
	entity::shift_position(x, y);
	if(standing_on_prev_x_ != INT_MIN) {
		standing_on_prev_x_ += x;
	}

	if(standing_on_prev_y_ != INT_MIN) {
		standing_on_prev_y_ += y;
	}

	if(position_schedule_.get() != NULL) {
		foreach(int& xpos, position_schedule_->x_pos) {
			xpos += x;
		}

		foreach(int& ypos, position_schedule_->y_pos) {
			ypos += y;
		}
	}

	if(activation_area_.get() != NULL) {
		activation_area_.reset(new rect(activation_area_->x() + x,
		                                activation_area_->y() + y,
										activation_area_->w(),
										activation_area_->h()));
	}
}

bool custom_object::appears_at_difficulty(int difficulty) const
{
	return (min_difficulty_ == -1 || difficulty >= min_difficulty_) &&
	       (max_difficulty_ == -1 || difficulty <= max_difficulty_);
}

void custom_object::set_parent(entity_ptr e, const std::string& pivot_point)
{
	parent_ = e;
	parent_pivot_ = pivot_point;

	const point pos = parent_position();

	if(parent_.get() != NULL) {
        const int parent_facing_sign = parent_->face_right() ? 1 : -1;
        relative_x_ = parent_facing_sign * (x() - pos.x);
        relative_y_ = (y() - pos.y);
    }
        
	parent_prev_x_ = pos.x;
	parent_prev_y_ = pos.y;
    
	if(parent_.get() != NULL) {
		parent_prev_facing_ = parent_->face_right();
	}
}

int custom_object::parent_depth(bool* has_human_parent, int cur_depth) const
{
	if(!parent_ || cur_depth > 10) {
		if(has_human_parent) {
			*has_human_parent = is_human() != NULL;
		}
		return cur_depth;
	}

	return parent_->parent_depth(has_human_parent, cur_depth+1);
}

bool custom_object::editor_force_standing() const
{
	return type_->editor_force_standing();
}

game_logic::const_formula_callable_definition_ptr custom_object::get_definition() const
{
	return type_->callable_definition();
}

rect custom_object::platform_rect_at(int xpos) const
{
	if(platform_offsets_.empty()) {
		return platform_rect();
	}

	rect area = platform_rect();
	if(xpos < area.x() || xpos >= area.x() + area.w()) {
		return area;
	}

	if(platform_offsets_.size() == 1) {
		return rect(area.x(), area.y() + platform_offsets_[0], area.w(), area.h());
	}

	const int pos = (xpos - area.x())*1024;
	const int seg_width = (area.w()*1024)/(platform_offsets_.size()-1);
	const size_t segment = pos/seg_width;
	ASSERT_LT(segment, platform_offsets_.size()-1);

	const int partial = pos%seg_width;

	const int offset = (partial*platform_offsets_[segment+1] + (seg_width-partial)*platform_offsets_[segment])/seg_width;
	return rect(area.x(), area.y() + offset, area.w(), area.h());
}

int custom_object::platform_slope_at(int xpos) const
{
	if(platform_offsets_.size() <= 1) {
		return 0;
	}

	rect area = platform_rect();
	if(xpos < area.x() || xpos >= area.x() + area.w()) {
		return 0;
	}

	const int pos = (xpos - area.x())*1024;
	const int dx = (area.w()*1024)/(platform_offsets_.size()-1);
	const size_t segment = pos/dx;
	ASSERT_LT(segment, platform_offsets_.size()-1);

	const int dy = (platform_offsets_[segment+1] - platform_offsets_[segment])*1024;

	return (dy*45)/dx;
}

bool custom_object::solid_platform() const
{
	return type_->solid_platform();
}

point custom_object::parent_position() const
{
	if(parent_.get() == NULL) {
		return point(0,0);
	}

	return parent_->pivot(parent_pivot_);
}

void custom_object::update_type(const_custom_object_type_ptr old_type,
                                const_custom_object_type_ptr new_type)
{
	if(old_type != base_type_) {
		return;
	}

	base_type_ = new_type;
	if(current_variation_.empty()) {
		type_ = base_type_;
	} else {
		type_ = base_type_->get_variation(current_variation_);
	}

	game_logic::formula_variable_storage_ptr old_vars = vars_;

	vars_.reset(new game_logic::formula_variable_storage(type_->variables()));
	vars_->set_object_name(debug_description());
	foreach(const std::string& key, old_vars->keys()) {
		const variant old_value = old_vars->query_value(key);
		std::map<std::string, variant>::const_iterator old_type_value =
		    old_type->variables().find(key);
		if(old_type_value == old_type->variables().end() ||
		   old_type_value->second != old_value) {
			vars_->mutate_value(key, old_value);
		}
	}

	old_vars = tmp_vars_;

	tmp_vars_.reset(new game_logic::formula_variable_storage(type_->tmp_variables()));
	tmp_vars_->set_object_name(debug_description());
	foreach(const std::string& key, old_vars->keys()) {
		const variant old_value = old_vars->query_value(key);
		std::map<std::string, variant>::const_iterator old_type_value =
		    old_type->tmp_variables().find(key);
		if(old_type_value == old_type->tmp_variables().end() ||
		   old_type_value->second != old_value) {
			tmp_vars_->mutate_value(key, old_value);
		}
	}

	vars_->disallow_new_keys(type_->is_strict());
	tmp_vars_->disallow_new_keys(type_->is_strict());

	if(type_->has_frame(frame_name_)) {
		frame_.reset(&type_->get_frame(frame_name_));
	}

	std::map<std::string, particle_system_ptr> systems;
	systems.swap(particle_systems_);
	for(std::map<std::string, particle_system_ptr>::const_iterator i = systems.begin(); i != systems.end(); ++i) {
		add_particle_system(i->first, i->second->type());
	}

#if defined(USE_SHADERS)
	shader_.reset(new_type->shader() ? new gles2::shader_program(*new_type->shader()) : NULL);
	if(shader_) {
		shader_->init(this);
	}

	effects_.clear();
	for(size_t n = 0; n < new_type->effects().size(); ++n) {
		effects_.push_back(new gles2::shader_program(*new_type->effects()[n]));
		effects_.back()->init(this);
	}
#endif

#if defined(USE_LUA)
	if(!type_->get_lua_source().empty()) {
	//	lua_ptr_.reset(new lua::lua_context());
	}
	init_lua();
#endif

	handle_event("type_updated");
}

std::vector<variant> custom_object::get_variant_widget_list() const
{
	std::vector<variant> v;
	for(widget_list::iterator it = widgets_.begin(); it != widgets_.end(); ++it) {
		v.push_back(variant(it->get()));
	}
	return v;
}

void custom_object::add_widget(const gui::widget_ptr& w)
{ 
	widgets_.insert(w); 
}

void custom_object::add_widgets(std::vector<gui::widget_ptr>* widgets) 
{
	widgets_.clear();
	std::copy(widgets->begin(), widgets->end(), std::inserter(widgets_, widgets_.end()));
}

void custom_object::clear_widgets() 
{ 
	widgets_.clear(); 
}

void custom_object::remove_widget(gui::widget_ptr w)
{
	widget_list::iterator it = widgets_.find(w);
	ASSERT_LOG(it != widgets_.end(), "Tried to erase widget not in list.");
	widgets_.erase(it);
}

bool custom_object::handle_sdl_event(const SDL_Event& event, bool claimed)
{
	SDL_Event ev(event);
	if(event.type == SDL_MOUSEMOTION) {
		ev.motion.x -= x();
		ev.motion.y -= y();
		if(use_absolute_screen_coordinates_) {
			ev.motion.x -= adjusted_draw_position_.x;
			ev.motion.y -= adjusted_draw_position_.y;
		}
	} else if(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
		ev.button.x -= x();
		ev.button.y -= y();
		if(use_absolute_screen_coordinates_) {
			ev.button.x -= adjusted_draw_position_.x;
			ev.button.y -= adjusted_draw_position_.y;
		}
	}

	widget_list w = widgets_;
	widget_list::const_reverse_iterator ritor = w.rbegin();
	while(ritor != w.rend()) {
		claimed |= (*ritor++)->process_event(ev, claimed);
	}
	return claimed;
}

game_logic::formula_ptr custom_object::create_formula(const variant& v)
{
	return game_logic::formula_ptr(new game_logic::formula(v, &get_custom_object_functions_symbol_table()));
}

gui::const_widget_ptr custom_object::get_widget_by_id(const std::string& id) const
{
	foreach(const gui::widget_ptr& w, widgets_) {
		gui::widget_ptr wx = w->get_widget_by_id(id);
		if(wx) {
			return wx;
		}
	}
	return gui::const_widget_ptr();
}

gui::widget_ptr custom_object::get_widget_by_id(const std::string& id)
{
	foreach(const gui::widget_ptr& w, widgets_) {
		gui::widget_ptr wx = w->get_widget_by_id(id);
		if(wx) {
			return wx;
		}
	}
	return gui::widget_ptr();
}

void custom_object::add_to_level()
{
	entity::add_to_level();
	standing_on_.reset();
#if defined(USE_BOX2D)
	if(body_) {
		body_->set_active();
	}
#endif
#if defined(USE_SHADERS)
	if(shader_) {
		shader_->init(this);
	}
	for(size_t n = 0; n < effects_.size(); ++n) {
		effects_[n]->init(this);
	}
#endif
}

BENCHMARK(custom_object_spike) {
	static level* lvl = NULL;
	if(!lvl) {	
		lvl = new level("test.cfg");
		static variant v(lvl);
		lvl->finish_loading();
		lvl->set_as_current_level();
	}
	BENCHMARK_LOOP {
		custom_object* obj = new custom_object("chain_base", 0, 0, false);
		variant v(obj);
		obj->handle_event(OBJECT_EVENT_CREATE);
	}
}

int custom_object::events_handled_per_second = 0;

BENCHMARK_ARG(custom_object_get_attr, const std::string& attr)
{
	static custom_object* obj = new custom_object("ant_black", 0, 0, false);
	BENCHMARK_LOOP {
		obj->query_value(attr);
	}
}

BENCHMARK_ARG_CALL(custom_object_get_attr, easy_lookup, "x");
BENCHMARK_ARG_CALL(custom_object_get_attr, hard_lookup, "xxxx");

BENCHMARK_ARG(custom_object_handle_event, const std::string& object_event)
{
	std::string::const_iterator i = std::find(object_event.begin(), object_event.end(), ':');
	ASSERT_LOG(i != object_event.end(), "custom_object_event_handle argument must have a colon seperator: " << object_event);
	std::string obj_type(object_event.begin(), i);
	std::string event_name(i+1, object_event.end());
	static level* lvl = new level("titlescreen.cfg");
	lvl->set_as_current_level();
	static custom_object* obj = new custom_object(obj_type, 0, 0, false);
	obj->set_level(*lvl);
	const int event_id = get_object_event_id(event_name);
	BENCHMARK_LOOP {
		obj->handle_event(event_id);
	}
}

BENCHMARK_ARG_CALL(custom_object_handle_event, ant_non_exist, "ant_black:blahblah");

BENCHMARK_ARG_CALL_COMMAND_LINE(custom_object_handle_event);
