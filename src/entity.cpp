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

#include <iostream>
#include <limits.h>

#include "Canvas.hpp"

#include "custom_object.hpp"
#include "debug_console.hpp"
#include "entity.hpp"
#include "level.hpp"
#include "playable_custom_object.hpp"
#include "preferences.hpp"
#include "rectangle_rotator.hpp"
#include "solid_map.hpp"
#include "variant_utils.hpp"

Entity::Entity(variant node)
  : WmlSerializableFormulaCallable(node["_uuid"].is_string() ? read_uuid(node["_uuid"].as_string()) : generate_uuid()),
    x_(node["x"].as_int()*100),
    y_(node["y"].as_int()*100),
	anchorx_(-1),
	anchory_(-1),
	prev_feet_x_(std::numeric_limits<int>::min()), prev_feet_y_(std::numeric_limits<int>::min()),
	last_move_x_(0), last_move_y_(0),
	zorder_(0), zsub_order_(0),
	face_right_(node["face_right"].as_bool(true)),
	upside_down_(node["upside_down"].as_bool(false)),
	rotate_z_(node["rotate"].as_decimal()),
	group_(node["group"].as_int(-1)),
    id_(-1), respawn_(node["respawn"].as_bool(true)),
	solid_dimensions_(0), collide_dimensions_(0),
	weak_solid_dimensions_(0), weak_collide_dimensions_(0),
	platform_motion_x_(node["platform_motion_x"].as_int()),
	mouse_over_entity_(false), being_dragged_(false), mouse_button_state_(0),
	mouseover_delay_(0), mouseover_trigger_cycle_(std::numeric_limits<int>::max()),
	true_z_(false), tx_(node["x"].as_decimal().as_float()), ty_(node["y"].as_decimal().as_float()), tz_(0.0f)
{
	if(node.has_key("anchorx")) {
		setAnchorX(node["anchorx"].as_decimal());
	}

	if(node.has_key("anchory")) {
		setAnchorY(node["anchory"].as_decimal());
	}

	for(bool& b : controls_) {
		b = false;
	}
}

Entity::Entity(int x, int y, bool face_right)
  : x_(x*100), y_(y*100), anchorx_(-1), anchory_(-1), prev_feet_x_(std::numeric_limits<int>::min()), prev_feet_y_(std::numeric_limits<int>::min()),
	last_move_x_(0), last_move_y_(0),
	zorder_(0), zsub_order_(0),
    face_right_(face_right), upside_down_(false), group_(-1), id_(-1),
	respawn_(true), solid_dimensions_(0), collide_dimensions_(0),
	weak_solid_dimensions_(0), weak_collide_dimensions_(0),	platform_motion_x_(0), 
	mouse_over_entity_(false), being_dragged_(false), mouse_button_state_(0),
	mouseover_delay_(0), mouseover_trigger_cycle_(std::numeric_limits<int>::max()),
	true_z_(false), tx_(double(x)), ty_(double(y)), tz_(0.0f)
{
	for(bool& b : controls_) {
		b = false;
	}
}

void Entity::setAnchorX(decimal value)
{
	if(value < 0) {
		anchorx_ = -1;
	} else {
		anchorx_ = (value*1000).as_int();
	}
}

void Entity::setAnchorY(decimal value)
{
	if(value < 0) {
		anchory_ = -1;
	} else {
		anchory_ = (value*1000).as_int();
	}
}

decimal Entity::getAnchorX() const
{
	if(anchorx_ == -1) {
		return decimal::from_int(-1);
	}

	return decimal::from_int(anchorx_)/1000;
}

decimal Entity::getAnchorY() const
{
	if(anchory_ == -1) {
		return decimal::from_int(-1);
	}

	return decimal::from_int(anchory_)/1000;
}

void Entity::addToLevel()
{
	last_move_x_ = last_move_y_ = 0;
	prev_feet_x_ = prev_feet_y_ = std::numeric_limits<int>::min();
	prev_platform_rect_ = rect();
	calculateSolidRect();
}

EntityPtr Entity::build(variant node)
{
	if(node["is_human"].as_bool()) {
		return EntityPtr(new PlayableCustomObject(node));
	} else {
		return EntityPtr(new CustomObject(node));
	}
}

bool Entity::hasFeet() const
{
	return solid() != nullptr;
}

int Entity::getFeetX() const
{
	if(solid_) {
		const int diff = solid_->area().x() + solid_->area().w()/2;
		return isFacingRight() ? x() + diff : x() + getCurrentFrame().width() - diff;
	}

	if(anchorx_ != -1) {
		return x() + (getCurrentFrame().area().w()*getCurrentFrame().scale()*anchorx_)/1000;
	}

	return isFacingRight() ? x() + getCurrentFrame().getFeetX() : x() + getCurrentFrame().width() - getCurrentFrame().getFeetX();
}

int Entity::getFeetY() const
{
	if(solid_) {
		return isUpsideDown() ?
				solidRect().y2() + 1 :
		       y() + solid_->area().y() + solid_->area().h();
	}
	if(anchory_ != -1) {
		return y() + (getCurrentFrame().area().h()*getCurrentFrame().scale()*anchory_)/1000;
	}
	return y() + getCurrentFrame().getFeetY();
}

int Entity::getLastMoveX() const
{
	return last_move_x_;
}

int Entity::getLastMoveY() const
{
	return last_move_y_;
}

void Entity::setPlatformMotionX(int value)
{
	platform_motion_x_ = value;
}

int Entity::mapPlatformPos(int xpos) const
{
	if(platform_rect_.w() > 0 && platform_rect_.h() > 0 && xpos >= prev_platform_rect_.x() && xpos < prev_platform_rect_.x() + prev_platform_rect_.w()) {
		const int proportion = xpos - prev_platform_rect_.x();
		int maps_to = (1024*proportion*platform_rect_.w())/prev_platform_rect_.w();
		if(maps_to%1024 >= 512) {
			maps_to = platform_rect_.x() + maps_to/1024 + 1;
		} else {
			maps_to = platform_rect_.x() + maps_to/1024;
		}


		return maps_to - xpos - (getFeetX() - prev_feet_x_);
	}

	return 0;
}

int Entity::getPlatformMotionX() const
{
	return platform_motion_x_;
}

void Entity::process(Level& lvl)
{
	if(prev_feet_x_ != std::numeric_limits<int>::min()) {
		last_move_x_ = getFeetX() - prev_feet_x_;
		last_move_y_ = getFeetY() - prev_feet_y_;
	}
	prev_feet_x_ = getFeetX();
	prev_feet_y_ = getFeetY();
	prev_platform_rect_ = platform_rect_;
}

void Entity::setFacingRight(bool facing)
{
	if(facing == face_right_) {
		return;
	}
	const int start_x = getFeetX();
	face_right_ = facing;
	const int delta_x = getFeetX() - start_x;
	x_ -= delta_x*100;
	assert(getFeetX() == start_x);

	calculateSolidRect();
}

void Entity::setUpsideDown(bool facing)
{
	const int start_y = solid_rect_.y();
	upside_down_ = facing;
	calculateSolidRect();

	const int delta_y = solid_rect_.y() - start_y;
	y_ -= delta_y*100;
	calculateSolidRect();

	assert(start_y == solid_rect_.y());
}

void Entity::setRotateZ(float new_rotate_z)
{
	rotate_z_ = variant(new_rotate_z).as_decimal();
}

void Entity::setDrawScale(float new_scale)
{
	//Why is this function is needed here? What does it do?
}

void Entity::calculateSolidRect()
{
	const Frame& f = getCurrentFrame();

	frame_rect_ = rect(x(), y(), f.width(), f.height());
	
	solid_ = calculateSolid();
	if(solid_) {
		const rect& area = solid_->area();

		int xpos;
		if(isFacingRight()) {
			xpos = x() + area.x();
		} else {
			xpos = x() + f.width() - area.x() - area.w();
		}

		int ypos;
		if(isUpsideDown()) {
			ypos = y() + f.height() - area.y() - area.h();
		} else {
			ypos = y() + area.y();
		}

		solid_rect_ = rect(xpos, ypos, area.w(), area.h());
	} else {
		solid_rect_ = rect();
	}

	platform_ = calculatePlatform();
	if(platform_) {
		const int delta_y = getLastMoveY();
		const rect& area = platform_->area();
		
		if(area.empty()) {
			platform_rect_ = rect();
		} else {
			if(delta_y < 0) {
				platform_rect_ = rect(x() + area.x(), y() + area.y(), area.w(), area.h() - delta_y);
			} else {
				platform_rect_ = rect(x() + area.x(), y() + area.y(), area.w(), area.h());
			}
		}
	} else {
		platform_rect_ = rect();
	}
}

rect Entity::getBodyRect() const
{
	const Frame& f = getCurrentFrame();

	const int ypos = y() + (isUpsideDown() ? (f.height() - (f.collideY() + f.collideH())) : f.collideY());
	return rect(isFacingRight() ? x() + f.collideX() : x() + f.width() - f.collideX() - f.collideW(),
	            ypos, f.collideW(), f.collideH());
}

rect Entity::getHitRect() const
{
	const Frame& f = getCurrentFrame();
	const std::vector<Frame::CollisionArea>& areas = f.getCollisionAreas();
	for(const Frame::CollisionArea& a : areas) {
		if(a.name == "attack") {
			return calculateCollisionRect(f, a);
		}
	}

	return rect();
}

rect Entity::calculateCollisionRect(const Frame& f, const Frame::CollisionArea& a) const
{
	const rect& r = a.area;
	rect result(isFacingRight() ? x() + r.x() : x() + f.width() - r.x() - r.w(), y() + r.y(), r.w(), r.h());

	const int rotation = currentRotation();
	if(rotation != 0) {
		const int r_center_x = result.x() + result.w()/2;
		const int r_center_y = result.y() + result.h()/2;

		const int center_x = x() + f.width()/2;
		const int center_y = y() + f.height()/2;

		point p = rotate_point_around_origin_with_offset(r_center_x, r_center_y, (rotation*3.14159f)/180.0f, center_x, center_y);
		result = rect(result.x() + p.x - r_center_x, result.y() + p.y - r_center_y, result.w(), result.h());
	}

	return result;
}

point Entity::getMidpoint() const
{
	if(solid()) {
		const rect r = solidRect();
		return point(r.x() + r.w()/2, r.y() + r.h()/2);
	}

	const Frame& f = getCurrentFrame();
	return point(x() + f.width()/2, y() + f.height()/2);
}

bool Entity::isAlpha(int xpos, int ypos) const
{
	return getCurrentFrame().isAlpha(xpos - x(), ypos - y(), getTimeInFrame(), isFacingRight());
}

void Entity::drawDebugRects() const
{
	if(preferences::show_debug_hitboxes() == false) {
		return;
	}

	const float radians_to_degrees = 57.29577951308232087f;

	float rotation = currentRotation()/radians_to_degrees;

	auto wnd = KRE::WindowManager::getMainWindow();

	const rect& body = solidRect();
	if(body.w() > 0 && body.h() > 0) {
		RectRenderable body_rr(true, true);
		body_rr.update(body, rotation, KRE::Color(255,255,255,0xaa));
		wnd->render(&body_rr);
	}

	rect platform = platformRect();
	if(platform.w() > 0) {
		if(platform.h() < 2) {
			platform = rect(platform.x(), platform.y(), platform.w(), 2);
		}
		RectRenderable platform_rr(true, true);
		platform_rr.update(platform, rotation, KRE::Color(0,255,0,0xaa));
		wnd->render(&platform_rr);
	}

	const rect& hit = getHitRect();
	if(hit.w() > 0 && hit.h() > 0) {
		RectRenderable hit_rr(true, true);
		hit_rr.update(body, rotation, KRE::Color(255,0,0,0xaa));
		wnd->render(&hit_rr);
	}

	RectRenderable feet_rr(true, true);
	feet_rr.update(getFeetX() - 1, getFeetY() - 1, 3, 3, KRE::Color(255,255,255,0xaa));
	wnd->render(&feet_rr);

	const Frame& f = getCurrentFrame();
	for(auto area : f.getCollisionAreas()) {
		if(area.name == "attack") {
			const rect r = calculateCollisionRect(f, area);
			RectRenderable rr(true, true);
			rr.update(r, rotation, KRE::Color(255,0,0,0xaa));
			wnd->render(&rr);
		}
	}
}

void Entity::generateCurrent(const Entity& target, int* velocity_x, int* velocity_y) const
{
	if(current_generator_) {
		const rect& my_rect = getBodyRect();
		const rect& target_rect = target.getBodyRect();
		current_generator_->generate(my_rect.mid_x(), my_rect.mid_y(),
		                             target_rect.mid_x(), target_rect.mid_y(), target.mass(),
		                             velocity_x, velocity_y);
	}
}

static const int EndAnimationScheduledCommand = -20000000;

void Entity::addEndAnimCommand(variant cmd)
{
	scheduled_commands_.push_back(ScheduledCommand(EndAnimationScheduledCommand, cmd));
}

std::vector<variant> Entity::popEndAnimCommands()
{
	std::vector<variant> result;
	auto i = scheduled_commands_.begin();
	while(i != scheduled_commands_.end()) {
		if(i->t == EndAnimationScheduledCommand) {
			result.push_back(i->cmd);
			i = scheduled_commands_.erase(i);
		} else {
			++i;
		}
	}
	return result;
}

void Entity::addScheduledCommand(int cycle, variant cmd)
{
	scheduled_commands_.push_back(ScheduledCommand(cycle, cmd));
	if(debug_console::isExecutingDebugConsoleCommand()) {
		scheduled_commands_.back().is_debug = true;
	}
}

std::vector<variant> Entity::popScheduledCommands(bool* is_debug)
{
	std::vector<variant> result;
	std::vector<ScheduledCommand>::iterator i = scheduled_commands_.begin();
	while(i != scheduled_commands_.end()) {
		if(i->t != EndAnimationScheduledCommand && --(i->t) <= 0) {
			if(is_debug && i->is_debug) {
				*is_debug = true;
			}

			result.push_back(i->cmd);
			i = scheduled_commands_.erase(i);
		} else {
			++i;
		}
	}

	return result;
}

void Entity::setCurrentGenerator(CurrentGenerator* generator)
{
	current_generator_ = CurrentGeneratorPtr(generator);
}

void Entity::setAttachedObjects(const std::vector<EntityPtr>& v)
{
	if(v != attached_objects_) {
		attached_objects_ = v;
	}
}

bool Entity::moveCentipixels(int dx, int dy)
{
	int start_x = x();
	int start_y = y();
	x_ += dx;
	y_ += dy;
	if(x() != start_x || y() != start_y) {
		calculateSolidRect();
		return true;
	} else {
		return false;
	}
}

void Entity::setDistinctLabel()
{
	//generate a random label for the object
	char buf[64];
	sprintf(buf, "_%x", rand());
	setLabel(buf);
}

void Entity::setControlStatus(const std::string& key, bool value)
{
	static const std::vector<std::string> keys { "up", "down", "left", "right", "attack", "jump" };
	const auto it = std::find(keys.begin(), keys.end(), key);
	if(it == keys.end()) {
		return;
	}

	const auto index = it - keys.begin();
	controls_[index] = value;
}

void Entity::readControls(int cycle)
{
	PlayerInfo* info = getPlayerInfo();
	if(info) {
		info->readControls(cycle);
	}
}

point Entity::pivot(const std::string& name, bool reverse_facing) const
{
	const Frame& f = getCurrentFrame();
	if(name == "") {
		return getMidpoint();
	}

	bool facing_right = isFacingRight();
	if(reverse_facing) {
		facing_right = !facing_right;
	}

	const point pos = f.pivot(name, getTimeInFrame());
	if(facing_right) {
		return point(x() + pos.x, y() + pos.y);
	} else {
		return point(x() + f.width() - pos.x, y() + pos.y);
	}
}

void Entity::setSpawnedBy(const std::string& key)
{
	spawned_by_ = key;
}

const std::string& Entity::wasSpawnedBy() const
{
	return spawned_by_;
}

void Entity::setMouseOverArea(const rect& area)
{
	mouse_over_area_ = area;
}

const rect& Entity::getMouseOverArea() const
{
	return mouse_over_area_;
}

void Entity::beingRemoved()
{
	scheduled_commands_.clear();
}

bool zorder_compare(const EntityPtr& a, const EntityPtr& b)
{
	return EntityZOrderCompare()(a, b);
}

EntityZOrderCompare::EntityZOrderCompare() : reverse_(Level::current().player() && Level::current().player()->hasReverseGlobalVerticalZordering())
{
}

void Entity::surrenderReferences(GarbageCollector* collector)
{
	collector->surrenderVariant(&controls_user_, "CONTROLS_USER");

	for(ScheduledCommand& cmd : scheduled_commands_) {
		collector->surrenderVariant(&cmd.cmd, "SCHEDULED_CMD");
	}

	for(EntityPtr& attachment : attached_objects_) {
		collector->surrenderPtr(&attachment, "ATTACHED");
	}

}
