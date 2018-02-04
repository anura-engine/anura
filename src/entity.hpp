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

#include <cstdint>
#include <string>

#include "boost/intrusive_ptr.hpp"

#include "geometry.hpp"

#include "controls.hpp"
#include "current_generator.hpp"
#include "editor_variable_info.hpp"
#include "entity_fwd.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition_fwd.hpp"
#include "formula_fwd.hpp"
#include "frame.hpp"
#include "light.hpp"
#include "solid_map_fwd.hpp"
#include "wml_formula_callable.hpp"
#include "variant.hpp"

class character;
class Frame;
class Level;
class pc_character;
class PlayerInfo;

typedef ffl::IntrusivePtr<character> CharacterPtr;

class Entity : public game_logic::WmlSerializableFormulaCallable
{
public:
	static EntityPtr build(variant node);
	explicit Entity(variant node);
	Entity(int x, int y, bool face_right);
	virtual ~Entity() {}

	virtual void validate_properties() {}
	virtual void addToLevel();

	virtual void finishLoading(Level*) {}
	virtual variant write() const = 0;
	virtual void setupDrawing() const {}
	virtual void draw(int x, int y) const = 0;
	virtual void drawLater(int x, int y) const = 0;
	virtual void drawGroup() const = 0;
	PlayerInfo* getPlayerInfo() { return isHuman(); }
	const PlayerInfo* getPlayerInfo() const { return isHuman(); }
	virtual const PlayerInfo* isHuman() const { return nullptr; }
	virtual PlayerInfo* isHuman() { return nullptr; }
	virtual void process(Level& lvl);
	virtual bool executeCommand(const variant& var) override = 0;

	const std::string& label() const { return label_; }
	void setLabel(const std::string& lb) { label_ = lb; }
	void setDistinctLabel();

	virtual void shiftPosition(int x, int y) { x_ += x*100; y_ += y*100; prev_feet_x_ += x; prev_feet_y_ += y; calculateSolidRect(); }
	
	void setPos(const point& p) { x_ = p.x*100; y_ = p.y*100; calculateSolidRect(); }
	void setPos(int x, int y) { x_ = x*100; y_ = y*100; calculateSolidRect(); }
	void setX(int x) { x_ = x*100; calculateSolidRect(); }
	void setY(int y) { y_ = y*100; calculateSolidRect(); }

	void setCentiX(int x) { x_ = x; calculateSolidRect(); }
	void setCentiY(int y) { y_ = y; calculateSolidRect(); }

	int x() const { return x_/100 - (x_ < 0 && x_%100 ? 1 : 0); }
	int y() const { return y_/100 - (y_ < 0 && y_%100 ? 1 : 0); }

	virtual int parallaxScaleMillisX() const { return 1000;}
	virtual int parallaxScaleMillisY() const { return 1000;}
		
	int zorder() const { return zorder_; }
	int zSubOrder() const { return zsub_order_; }

	void setZOrder(int z) { zorder_ = z; }
	void setZSubOrder(int z) { zsub_order_ = z; }

	public:

	virtual const std::pair<int,int>* parallaxScaleMillis() const { return 0; }

	int centiX() const { return x_; }
	int centiY() const { return y_; }

	virtual int velocityX() const { return 0; }
	virtual int velocityY() const { return 0; }

	int group() const { return group_; }
	void setGroup(int group) { group_ = group; }

	virtual bool isStandable(int x, int y, int* friction=nullptr, int* traction=nullptr, int* adjust_y=nullptr) const { return false; }

	virtual bool destroyed() const = 0;

	virtual int getSurfaceFriction() const { return 0; }
	virtual int getSurfaceTraction() const { return 0; }

	virtual bool pointCollides(int x, int y) const = 0;
	virtual bool rectCollides(const rect& r) const = 0;
	const SolidInfo* platform() const { return platform_.get(); }
	const SolidInfo* solid() const { return solid_.get(); }
	const rect& solidRect() const { return solid_rect_; }
	const rect& frameRect() const { return frame_rect_; }
	rect platformRect() const { return platform_rect_; }
	virtual rect platformRectAt(int xpos) const { return platformRect(); }
	virtual int platformSlopeAt(int xpos) const { return 0; }
	virtual bool isSolidPlatform() const { return false; }
	rect getBodyRect() const;
	rect getHitRect() const;
	point getMidpoint() const;

	virtual const Frame& getIconFrame() const = 0;
	virtual const Frame& getCurrentFrame() const = 0;

	virtual rect getDrawRect() const = 0;

	bool isAlpha(int xpos, int ypos) const;

	virtual bool hasFeet() const;
	int getFeetX() const;
	int getFeetY() const;

	int getLastMoveX() const;
	int getLastMoveY() const;

	void setPlatformMotionX(int value);
	int getPlatformMotionX() const;
	int mapPlatformPos(int xpos) const;

	bool isFacingRight() const { return face_right_; }
	virtual void setFacingRight(bool facing);

	bool isUpsideDown() const { return upside_down_; }
	virtual void setUpsideDown(bool facing);

	decimal rotate_z_;
	decimal getRotateZ() const { return rotate_z_; }
	void setRotateZ(decimal new_rotate_z) { rotate_z_ = new_rotate_z; }
	void setRotateZ(float new_rotate_z);
	
	virtual decimal getDrawScale() const { return decimal(1.0); };
	virtual void setDrawScale(float new_scale);

	int getFaceDir() const { return isFacingRight() ? 1 : -1; }

	virtual bool isBodyHarmful() const { return true; }

	virtual int getTimeInFrame() const = 0;

	virtual int getTeleportOffsetX() const { return 0; }
	virtual int getTeleportOffsetY() const { return 0; }
	virtual bool hasNoMoveToStanding() const { return 0; }
	virtual bool hasReverseGlobalVerticalZordering() const { return 0; }

	virtual EntityPtr standingOn() const = 0;

	virtual void dieWithNoEvent() = 0;
	virtual bool isActive(const rect& screen_area) const = 0;
	virtual bool diesOnInactive() const { return false; } 
	virtual bool isAlwaysActive() const { return false; } 
	
	virtual FormulaCallable* vars() { return nullptr; }
	virtual const FormulaCallable* vars() const { return nullptr; }

	virtual bool isBodyPassthrough() const { return false; }

	//the number of pixels up or down to adjust the scroll position if this
	//object is focused.
	virtual int verticalLook() const { return 0; }

	void setId(int id) { id_ = id; }
	int getId() const { return id_; }

	bool respawn() const { return respawn_; }

	virtual bool boardableVehicle() const { return false; }
	virtual void boarded(Level& lvl, const EntityPtr& player) {}
	virtual void unboarded(Level& lvl) {}

	virtual void boardVehicle() {}
	virtual void unboardVehicle() {}

	virtual void setSoundVolume(float volume, float nseconds=0.0) = 0;
	virtual int weight() const { return 1; }
	
	virtual int mass() const = 0;

	void drawDebugRects() const;

#ifndef NO_EDITOR
	virtual ConstEditorEntityInfoPtr getEditorInfo() const { return ConstEditorEntityInfoPtr(); }
#endif // !NO_EDITOR

	virtual EntityPtr clone() const { return EntityPtr(); }
	virtual EntityPtr backup() const = 0;

	virtual void generateCurrent(const Entity& target, int* velocity_x, int* velocity_y) const;

	virtual game_logic::ConstFormulaPtr getEventHandler(int key) const { return game_logic::ConstFormulaPtr(); }
	virtual void setEventHandler(int, game_logic::ConstFormulaPtr f) { return; }

	virtual bool handleEvent(const std::string& id, const FormulaCallable* context=nullptr) { return false; }
	virtual bool handleEvent(int id, const FormulaCallable* context=nullptr) { return false; }
	virtual bool handleEventDelay(int id, const FormulaCallable* context=nullptr) { return false; }
	virtual void resolveDelayedEvents() = 0;

	//function which returns true if this object can be 'interacted' with.
	//i.e. if the player ovelaps with the object and presses up if they will
	//talk to or enter the object.
	virtual bool canInteractWith() const { return false; }

	virtual bool serializable() const { return true; }

	virtual std::string getDebugDescription() const = 0;

	//a function call which tells us to get any references to other entities
	//that we hold, and map them according to the mapping given. This is useful
	//when we back up an entire level and want to make references match.
	virtual void mapEntities(const std::map<EntityPtr, EntityPtr>& m) {}
	virtual void cleanup_references() {}

	void addEndAnimCommand(variant cmd);
	std::vector<variant> popEndAnimCommands();

	void addScheduledCommand(int cycle, variant cmd);
	std::vector<variant> popScheduledCommands(bool* is_debug=nullptr);

	virtual void saveGame() {}

	virtual EntityPtr driver() { return EntityPtr(); }
	virtual ConstEntityPtr driver() const { return ConstEntityPtr(); }

	virtual bool moveToStanding(Level& lvl, int max_displace=10000) { return false; }
	virtual int getHitpoints() const { return 1; }
	virtual int getMaxHitpoints() const { return 1; }

	void setControlStatusUser(const variant& v) { controls_user_ = v; }
	void setControlStatus(const std::string& key, bool value);
	void setControlStatus(controls::CONTROL_ITEM ctrl, bool value) { controls_[ctrl] = value; }
	void clearControlStatus() { for(int n = 0; n != controls::NUM_CONTROLS; ++n) { controls_[n] = false; } }

	virtual bool enter() const { return false; }

	virtual void setInvisible(bool value) {}
	virtual void recordStatsMovement() {}

	virtual EntityPtr saveCondition() const { return EntityPtr(); }
	virtual void respawnPlayer() {}

	virtual int getCurrentAnimationId() const { return 0; }

	virtual void setLevel(Level* lvl) {}

	unsigned int getSolidDimensions() const { return solid_dimensions_; }
	unsigned int getCollideDimensions() const { return collide_dimensions_; }

	unsigned int getWeakSolidDimensions() const { return weak_solid_dimensions_; }
	unsigned int getWeakCollideDimensions() const { return weak_collide_dimensions_; }

	void setAttachedObjects(const std::vector<EntityPtr>& v);

	virtual bool allowLevelCollisions() const { return false; }

	virtual const std::vector<LightPtr>& lights() const = 0;
	virtual void swapLights(std::vector<LightPtr>& lights) = 0;

	point pivot(const std::string& name, bool reverse_facing=false) const;

	virtual bool appearsAtDifficulty(int difficulty) const = 0;

	virtual int parentDepth(bool* has_human_parent=nullptr, int cur_depth=0) const { return 0; }

	virtual bool editorForceStanding() const = 0;

	void setSpawnedBy(const std::string& key);
	const std::string& wasSpawnedBy() const;

	bool isMouseOverEntity() const { return mouse_over_entity_; }
	void setMouseOverEntity(bool val=true) { mouse_over_entity_=val; }
	void setMouseButtons(uint8_t buttons) { mouse_button_state_ = buttons; }
	uint8_t getMouseButtons() const { return mouse_button_state_; }
	bool isBeingDragged() const { return being_dragged_; }
	void setBeingDragged(bool val=true) { being_dragged_ = val; }
	virtual int mouseDragThreshold(int value) const { return value; }
	virtual bool getClipArea(rect* clipArea) const = 0;
	void setMouseOverArea(const rect& area);
	const rect& getMouseOverArea() const;

	virtual game_logic::ConstFormulaCallableDefinitionPtr getDefinition() const = 0;

	virtual bool createObject() = 0;

	virtual bool useAbsoluteScreenCoordinates() const = 0;

	virtual void beingRemoved();
	virtual void beingAdded() = 0;

	int getMouseoverDelay() const { return mouseover_delay_; }
	void setMouseoverDelay(int dly) { mouseover_delay_ = dly; }
	unsigned getMouseoverTriggerCycle() const { return mouseover_trigger_cycle_; }
	void setMouseoverTriggerCycle(unsigned cyc) { mouseover_trigger_cycle_ = cyc; }

	rect calculateCollisionRect(const Frame& f, const Frame::CollisionArea& a) const;

	void setAnchorX(decimal value);
	void setAnchorY(decimal value);

	decimal getAnchorX() const;
	decimal getAnchorY() const;

	//Get the slot for a named attribute so we can quickly look it
	//up using queryValueBySlot()
	virtual int getValueSlot(const std::string& key) const = 0;

	int currentRotation() const { return rotate_z_.as_int(); }

protected:
	virtual ConstSolidInfoPtr calculateSolid() const = 0;
	virtual ConstSolidInfoPtr calculatePlatform() const = 0;
	void calculateSolidRect();

	bool controlStatus(controls::CONTROL_ITEM ctrl) const { return controls_[ctrl]; }
	variant controlStatusUser() const { return controls_user_; }
	void readControls(int cycle);

	void setCurrentGenerator(CurrentGenerator* generator);

	void setRespawn(bool value) { respawn_ = value; }

	//move the entity by a number of centi pixels. Returns true if its
	//position is changed.
	bool moveCentipixels(int dx, int dy);

	void setSolidDimensions(unsigned int dim, unsigned int weak) { solid_dimensions_ = dim; weak_solid_dimensions_ = dim|weak; }
	void setCollideDimensions(unsigned int dim, unsigned int weak) { collide_dimensions_ = dim; weak_collide_dimensions_ = dim|weak; }

	const std::vector<EntityPtr>& attachedObjects() const { return attached_objects_; }

	virtual void control(const Level& lvl) = 0;

	variant serializeToWml() const override { return write(); }

	int getPrevFeetX() const { return prev_feet_x_; }
	int getPrevFeetY() const { return prev_feet_y_; }

	virtual bool editorOnly() const { return false; }

protected:
	void surrenderReferences(GarbageCollector* collector) override;

private:

	std::string label_;

	int x_, y_;

	//'anchor' values -- override 'feet' values.
	int anchorx_, anchory_;

	int prev_feet_x_, prev_feet_y_;
	int last_move_x_, last_move_y_;

	int zorder_, zsub_order_;

	bool face_right_;
	bool upside_down_;

	//the entity group the entity is in.
	int group_;

	int id_;

	bool respawn_;

	bool mouse_over_entity_;
	uint8_t mouse_button_state_;
	bool being_dragged_;

	unsigned int solid_dimensions_, collide_dimensions_;
	unsigned int weak_solid_dimensions_, weak_collide_dimensions_;

	CurrentGeneratorPtr current_generator_;

	struct ScheduledCommand
	{
		ScheduledCommand(int nt=0, const variant& c=variant()) : cmd(c), t(nt), is_debug(false)
		{}
		variant cmd;
		int t;
		bool is_debug;
	};

	std::vector<ScheduledCommand> scheduled_commands_;

	bool controls_[controls::NUM_CONTROLS];	
	variant controls_user_;

	//attached objects are objects which are also drawn with this object.
	//attached objects should generally NOT be present in the level, and are
	//NOT processed independently of this object.
	std::vector<EntityPtr> attached_objects_;

	//caches of commonly queried rects.
	rect solid_rect_, frame_rect_, platform_rect_, prev_platform_rect_;
	ConstSolidInfoPtr solid_;
	ConstSolidInfoPtr platform_;

	int platform_motion_x_;

	std::string spawned_by_;

	int mouseover_delay_;
	unsigned mouseover_trigger_cycle_;
	rect mouse_over_area_;

	bool true_z_;
	double tx_, ty_, tz_;
};

bool zorder_compare(const EntityPtr& e1, const EntityPtr& e2);	
struct EntityZOrderCompare
{

	bool reverse_;
	EntityZOrderCompare();
	bool operator()(const EntityPtr& a, const EntityPtr& b) const {
	if(reverse_){
		return a->zorder() < b->zorder() ||
			(a->zorder() == b->zorder() && a->zSubOrder() < b->zSubOrder()) ||
			(a->zorder() == b->zorder() && a->zSubOrder() == b->zSubOrder() && a->getMidpoint().y < b->getMidpoint().y) ||
			(a->zorder() == b->zorder() && a->zSubOrder() == b->zSubOrder() && a->getMidpoint().y == b->getMidpoint().y && a.get() < b.get());		
	}
		return a->zorder() < b->zorder() ||
		(a->zorder() == b->zorder() && a->zSubOrder() < b->zSubOrder()) ||
		(a->zorder() == b->zorder() && a->zSubOrder() == b->zSubOrder() && a->getMidpoint().y > b->getMidpoint().y) ||
		(a->zorder() == b->zorder() && a->zSubOrder() == b->zSubOrder() && a->getMidpoint().y == b->getMidpoint().y && a.get() > b.get());
	}
};
