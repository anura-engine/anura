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
#include <memory>
#include <set>
#include <stack>

#include "kre/SceneNode.hpp"

#include "blur.hpp"
#include "custom_object_type.hpp"
#include "draw_primitive.hpp"
#include "draw_scene.hpp"
#include "entity.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_visitor.hpp"
#include "formula_variable_storage.hpp"
#include "frame.hpp"
#include "light.hpp"
#include "particle_system.hpp"
#include "variant.hpp"
#include "widget.hpp"

struct CollisionInfo;
class Level;
struct CustomObjectText;

class CustomObject : public Entity, public KRE::SceneNode
{
public:
	static const std::string* current_debug_error();
	static void reset_current_debug_error();

	static std::set<CustomObject*>& getAll();
	static std::set<CustomObject*>& getAll(const std::string& type);
	static void init();

	static void run_garbage_collection();

	explicit CustomObject(variant node);
	CustomObject(const std::string& type, int x, int y, bool face_right);
	CustomObject(const CustomObject& o);
	virtual ~CustomObject();

	void validate_properties();

	bool isA(const std::string& type) const;

	//finishLoading(): called when a level finishes loading all objects,
	//and allows us to do any final setup such as finding our parent.
	void finishLoading(Level* lvl);
	virtual variant write() const;
	virtual void draw(int x, int y) const;
	virtual void drawLater(int x, int y) const;
	virtual void drawGroup() const;
	virtual void process(Level& lvl);
	virtual void construct();
	virtual bool createObject();
	void setLevel(Level& lvl) { }

	void check_initialized();

	int parallaxScaleMillisX() const {
		if(parallax_scale_millis_.get() == NULL){
			return type_->parallaxScaleMillisX();
		}else{
			return parallax_scale_millis_->first;
		}
	}
	int parallaxScaleMillisY() const {
		if(parallax_scale_millis_.get() == NULL){
			return type_->parallaxScaleMillisY();
		}else{
			return parallax_scale_millis_->second;
		}
	}

	
	virtual int zorder() const;
	virtual int zSubOrder() const;

	virtual int velocityX() const;
	virtual int velocityY() const;
	virtual int mass() const { return type_->mass(); }

	int getTeleportOffsetX() const { return type_->getTeleportOffsetX(); }
	int getTeleportOffsetY() const { return type_->getTeleportOffsetY(); }
	bool hasNoMoveToStanding() const { return type_->hasNoMoveToStanding(); };
	bool hasReverseGlobalVerticalZordering() const { return type_->hasReverseGlobalVerticalZordering(); };

	bool hasFeet() const;

	
	virtual bool isStandable(int x, int y, int* friction=NULL, int* traction=NULL, int* adjust_y=NULL) const;

	virtual bool destroyed() const;
	virtual bool pointCollides(int x, int y) const;
	virtual bool rectCollides(const rect& r) const;

	virtual const Frame& getCurrentFrame() const { return *frame_; }

	void setFrame(const std::string& name);
	void setFrame(const Frame& new_frame);

	virtual rect getDrawRect() const;

	//bare setting of the frame without adjusting position/checking solidity
	//etc etc.
	void setFrameNoAdjustments(const std::string& name);
	void setFrameNoAdjustments(const Frame& new_frame);
	void die();
	void dieWithNoEvent();
	virtual bool isActive(const rect& screen_area) const;
	bool diesOnInactive() const;
	bool isAlwaysActive() const;
	bool moveToStanding(Level& lvl, int max_displace=10000);

	bool isBodyHarmful() const;
	bool isBodyPassthrough() const;

	int getTimeInFrame() const { return time_in_frame_; }

	FormulaCallable* vars() { return vars_.get(); }
	const FormulaCallable* vars() const { return vars_.get(); }

	int cycle() const { return cycle_; }

	int getSurfaceFriction() const;
	int getSurfaceTraction() const;

	variant getChild(const std::string& key) const {
		return type_->getChild(key);
	}

	const Frame& getIconFrame() const;

	virtual EntityPtr clone() const;
	virtual EntityPtr backup() const;

	game_logic::ConstFormulaPtr getEventHandler(int key) const;
	void setEventHandler(int, game_logic::ConstFormulaPtr f);

	bool canInteractWith() const;

	std::string getDebugDescription() const;

	void mapEntities(const std::map<EntityPtr, EntityPtr>& m);
	void cleanup_references();

	void addParticleSystem(const std::string& key, const std::string& type);
	void remove_ParticleSystem(const std::string& key);

	virtual int getHitpoints() const { return hitpoints_; }

	void setText(const std::string& text, const std::string& font, int size, int align);

	virtual bool boardableVehicle() const;

	virtual void boarded(Level& lvl, const EntityPtr& player);
	virtual void unboarded(Level& lvl);

	virtual void boardVehicle();
	virtual void unboardVehicle();

	void set_driver_position();

	virtual bool useAbsoluteScreenCoordinates() const { return use_absolute_screen_coordinates_; }

	virtual int getCurrentAnimationId() const { return current_animation_id_; }

	virtual bool handle_sdl_event(const SDL_Event& event, bool claimed);
#ifndef NO_EDITOR
	virtual ConstEditorEntityInfoPtr getEditorInfo() const override;
#endif // !NO_EDITOR

	virtual bool handleEvent(const std::string& event, const FormulaCallable* context=NULL);
	virtual bool handleEvent(int event, const FormulaCallable* context=NULL);
	virtual bool handleEventDelay(int event, const FormulaCallable* context=NULL);

	virtual void resolveDelayedEvents();

	virtual bool serializable() const;

	void set_blur(const BlurInfo* blur);
	void setSoundVolume(const int volume);
	void setZSubOrder(const int zsub_order) {zsub_order_ = zsub_order;}
	
	bool executeCommand(const variant& var);

	virtual game_logic::FormulaPtr createFormula(const variant& v);

	bool allowLevelCollisions() const;

	//statistic on how many FFL events are handled every second.
	static int events_handled_per_second;

	const std::vector<LightPtr>& lights() const { return lights_; }
	void swapLights(std::vector<LightPtr>& lights) { lights_.swap(lights); }

	void shiftPosition(int x, int y);

	bool appearsAtDifficulty(int difficulty) const;

	int getMinDifficulty() const { return min_difficulty_; }
	int getMaxDifficulty() const { return max_difficulty_; }

	void setDifficultyRange(int min, int max) { min_difficulty_ = min; max_difficulty_ = max; }

	void updateType(ConstCustomObjectTypePtr old_type,
	                 ConstCustomObjectTypePtr new_type);

	bool isMouseEventSwallowed() const {return swallow_mouse_event_;}
	void resetMouseEvent() {swallow_mouse_event_ = false;}
	void addWidget(const gui::WidgetPtr& w);
	void addWidgets(std::vector<gui::WidgetPtr>* widgets);
	void clearWidgets();
	void removeWidget(gui::WidgetPtr w);
	gui::WidgetPtr getWidgetById(const std::string& id);
	gui::ConstWidgetPtr getWidgetById(const std::string& id) const;
	std::vector<variant> getVariantWidgetList() const;
	bool getClipArea(rect* clip_area) {
		if(clip_area_ && clip_area) {
			*clip_area = *clip_area_.get();
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

		std::vector<std::pair<variant,variant>> follow_on;

		AnimatedMovement() : pos(0)
		{}

		int getAnimationFrames() const { return animation_values.size()/animation_slots.size(); }
	};

	void addAnimatedMovement(variant attr, variant options);
	void setAnimatedSchedule(std::shared_ptr<AnimatedMovement> movement);
	void cancelAnimatedSchedule(const std::string& name);

protected:
	//components of per-cycle process() that can be done even on
	//static objects.
	void staticProcess(Level& lvl);

	virtual void control(const Level& lvl);
	variant getValue(const std::string& key) const;
	variant getValueBySlot(int slot) const;
	void setValue(const std::string& key, const variant& value);
	void setValueBySlot(int slot, const variant& value);

	virtual variant getPlayerValueBySlot(int slot) const;
	virtual void setPlayerValueBySlot(int slot, const variant& value);

	//function which indicates if the object wants to walk up or down stairs.
	//-1 = up stairs, 0 = no change, 1 = down stairs
	virtual int walkUpOrDownStairs() const { return 0; }

	bool isUnderwater() const {
		return was_underwater_;
	}

	const std::pair<int,int>* parallaxScaleMillis() const { return parallax_scale_millis_.get(); }

	enum class STANDING_STATUS { NOT_STANDING, BACK_FOOT, FRONT_FOOT };
	STANDING_STATUS isStanding(const Level& lvl, CollisionInfo* info=NULL) const;

	void setParent(EntityPtr e, const std::string& pivot_point);

	virtual int parentDepth(bool* has_human_parent=NULL, int cur_depth=0) const;

	virtual bool editorForceStanding() const;

	virtual game_logic::ConstFormulaCallableDefinitionPtr getDefinition() const;

	EntityPtr standingOn() const { return standing_on_; }
	virtual void addToLevel();

	virtual rect platformRectAt(int xpos) const;
	virtual int platformSlopeAt(int xpos) const;

	virtual bool isSolidPlatform() const;

	virtual void beingRemoved();
	virtual void beingAdded();

	//set up an animation schedule. values.size() should be a multiple of
	//slots.size().

private:
	void initProperties();
	CustomObject& operator=(const CustomObject& o);
	struct Accessor;

	struct gc_object_reference {
		Entity* owner;
		Entity* target;
		variant* from_variant;
		EntityPtr* from_ptr;
		std::shared_ptr<game_logic::FormulaCallableVisitor> visitor;
	};

	void extractGcObjectReferences(std::vector<gc_object_reference>& v);
	void extractGcObjectReferences(EntityPtr& e, std::vector<gc_object_reference>& v);
	void extractGcObjectReferences(variant& var, std::vector<gc_object_reference>& v);
	static void restoreGcObjectReference(gc_object_reference ref);

	bool moveToStandingInternal(Level& lvl, int max_displace);

	void processFrame();

	ConstSolidInfoPtr calculateSolid() const;
	ConstSolidInfoPtr calculatePlatform() const;

	virtual void getInputs(std::vector<game_logic::FormulaInput>* inputs) const;

	int slopeStandingOn(int range) const;

	int previous_y_;

	variant custom_type_;
	ConstCustomObjectTypePtr type_; //the type after variations are applied
	ConstCustomObjectTypePtr base_type_; //the type without any variation
	std::vector<std::string> current_variation_;
	boost::intrusive_ptr<const Frame> frame_;
	std::string frame_name_;
	int time_in_frame_;
	int time_in_frame_delta_;

	int velocity_x_, velocity_y_;
	int accel_x_, accel_y_;
	int gravity_shift_;

	virtual int currentRotation() const override;

	decimal rotate_z_;

    void setMidX(int new_mid_x) {
        const int current_x = x() + getCurrentFrame().width()/2;
		const int xdiff = current_x - x();
		setPos(new_mid_x - xdiff, y());
    }
    void setMidY(int new_mid_y) {
		const int current_y = y() + getCurrentFrame().height()/2;
		const int ydiff = current_y - y();
		setPos(x(), new_mid_y - ydiff);
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

	game_logic::ConstFormulaPtr next_animation_formula_;

	game_logic::FormulaVariableStoragePtr vars_, tmp_vars_;
	game_logic::MapFormulaCallablePtr tags_;

	variant& get_property_data(int slot) { if(property_data_.size() <= size_t(slot)) { property_data_.resize(slot+1); } return property_data_[slot]; }
	variant get_property_data(int slot) const { if(property_data_.size() <= size_t(slot)) { return variant(); } return property_data_[slot]; }
	std::vector<variant> property_data_;
	mutable int active_property_;

	//a stack of items that serve as the 'value' parameter, used in
	//property setters.
	mutable std::stack<variant> value_stack_;

	friend class ActivePropertyScope;

	EntityPtr last_hit_by_;
	int last_hit_by_anim_;
	int current_animation_id_;

	int cycle_;

	bool created_;

	//variable which is always set to false on construction, and then the
	//first time process is called will fire the on_load event and set to false
	bool loaded_;

	std::vector<game_logic::ConstFormulaPtr> event_handlers_;

	EntityPtr standing_on_;

	int standing_on_prev_x_, standing_on_prev_y_;

	void make_draw_color();
	const KRE::ColorTransform& draw_color() const;
	std::shared_ptr<KRE::ColorTransform> draw_color_;

	std::shared_ptr<decimal> draw_scale_;
	std::shared_ptr<rect> draw_area_, activation_area_, clip_area_;
	int activation_border_;
	
	bool can_interact_with_;

	std::map<std::string, ParticleSystemPtr> ParticleSystems_;

	typedef std::shared_ptr<CustomObjectText> CustomObjectText_ptr;
	CustomObjectText_ptr text_;

	EntityPtr driver_;

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

	std::vector<LightPtr> lights_;

	std::unique_ptr<rect> platform_area_;
	ConstSolidInfoPtr platform_solid_info_;

	point parent_position() const;

	//storage of the parent object while we're loading the object still.
	variant parent_loading_;

	EntityPtr parent_;
	std::string parent_pivot_;
	int parent_prev_x_, parent_prev_y_;
	bool parent_prev_facing_;
    int relative_x_, relative_y_;

	int min_difficulty_, max_difficulty_;

	std::shared_ptr<const std::vector<Frame::CustomPoint> > custom_draw_;

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

	std::vector<graphics::DrawPrimitivePtr> DrawPrimitives_;

	bool paused_;

	std::vector<int> properties_requiring_dynamic_initialization_;

	// for lua integration
#if defined(USE_LUA)
	void init_lua();
	std::unique_ptr<lua::LuaContext> lua_ptr_;
	std::unique_ptr<lua::CompiledChunk> lua_chunk_;
#endif
};
