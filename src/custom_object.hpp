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

#include "anura_shader.hpp"
#include "blur.hpp"
#include "custom_object_type.hpp"
#include "draw_primitive_fwd.hpp"
#include "draw_scene.hpp"
#include "entity.hpp"
#include "ffl_dom_fwd.hpp"
#include "formula.hpp"
#include "formula_callable.hpp"
#include "formula_callable_visitor.hpp"
#include "formula_variable_storage.hpp"
#include "frame.hpp"
#include "light.hpp"
#include "particle_system.hpp"
#include "widget.hpp"

#include "particle_system_proxy.hpp"

struct CollisionInfo;
class Level;
struct CustomObjectText;

//Construct one of these managers in a draw layer
//to make it so objects within that layer will
//attempt to optimize their drawing. Batch drawing
//will be used where possible and the objects will
//try to reuse stencils and other settings.
struct CustomObjectDrawZOrderManager
{
	CustomObjectDrawZOrderManager();
	~CustomObjectDrawZOrderManager();

	bool disabled_;
};

class CustomObject : public Entity
{
public:
	static const std::string* currentDebugError();
	static void resetCurrentDebugError();

	static std::set<CustomObject*>& getAll();
	static std::set<CustomObject*>& getAll(const std::string& type);
	static void init();

	static void run_garbage_collection();

	explicit CustomObject(variant node);
	CustomObject(const std::string& type, int x, int y, bool face_right, bool deferInitProperties=false);
	CustomObject(const CustomObject& o);
	virtual ~CustomObject();

	ConstCustomObjectTypePtr getType() const { return type_; }

	void validate_properties() override;

	bool isA(const std::string& type) const;
	bool isA(int type_index) const;

	//finishLoading(): called when a level finishes loading all objects,
	//and allows us to do any final setup such as finding our parent.
	void finishLoading(Level* lvl) override;
	virtual variant write() const override;
	virtual void draw(int xx, int yy) const override;
	virtual void drawLater(int x, int y) const override;
	virtual void drawGroup() const override;
	virtual void process(Level& lvl) override;
	virtual void construct();
	virtual bool createObject() override;
	void setLevel(Level& lvl) { }

	void checkInitialized();

	int parallaxScaleMillisX() const override {
		if(parallax_scale_millis_ == nullptr){
			return type_->parallaxScaleMillisX();
		}else{
			return parallax_scale_millis_->first;
		}
	}
	int parallaxScaleMillisY() const override {
		if(parallax_scale_millis_ == nullptr){
			return type_->parallaxScaleMillisY();
		}else{
			return parallax_scale_millis_->second;
		}
	}
	
	virtual int velocityX() const override;
	virtual int velocityY() const override;
	virtual int mass() const override { return type_->mass(); }

	int getTeleportOffsetX() const override { return type_->getTeleportOffsetX(); }
	int getTeleportOffsetY() const override { return type_->getTeleportOffsetY(); }
	bool hasNoMoveToStanding() const override { return type_->hasNoMoveToStanding(); };
	bool hasReverseGlobalVerticalZordering() const override { return type_->hasReverseGlobalVerticalZordering(); };

	bool hasFeet() const override;

	virtual bool isStandable(int x, int y, int* friction=nullptr, int* traction=nullptr, int* adjust_y=nullptr) const override;

	virtual bool destroyed() const override;
	virtual bool pointCollides(int x, int y) const override;
	virtual bool rectCollides(const rect& r) const override;

	virtual const Frame& getCurrentFrame() const override { return *frame_; }

	void setFrame(const std::string& name);
	void setFrame(const Frame& new_frame);

	virtual rect getDrawRect() const override;

	//bare setting of the frame without adjusting position/checking solidity
	//etc etc.
	void setFrameNoAdjustments(const std::string& name);
	void setFrameNoAdjustments(const Frame& new_frame);
	void die();
	void dieWithNoEvent() override;
	virtual bool isActive(const rect& screen_area) const override;
	bool diesOnInactive() const override;
	bool isAlwaysActive() const override;
	bool moveToStanding(Level& lvl, int max_displace=10000) override;

	bool isBodyHarmful() const override;
	bool isBodyPassthrough() const override;

	int getTimeInFrame() const override{ return time_in_frame_; }

	FormulaCallable* vars() override { return vars_.get(); }
	const FormulaCallable* vars() const override { return vars_.get(); }

	int cycle() const { return cycle_; }

	int getSurfaceFriction() const override;
	int getSurfaceTraction() const override;

	variant getChild(const std::string& key) const {
		return type_->getChild(key);
	}

	const Frame& getIconFrame() const override;

	virtual EntityPtr clone() const override;
	virtual EntityPtr backup() const override;

	game_logic::ConstFormulaPtr getEventHandler(int key) const override;
	void setEventHandler(int, game_logic::ConstFormulaPtr f) override;

	bool canInteractWith() const override;

	std::string getDebugDescription() const override;

	void mapEntities(const std::map<EntityPtr, EntityPtr>& m) override;
	void cleanup_references() override;

	void addParticleSystem(const std::string& key, const std::string& type);
	void removeParticleSystem(const std::string& key);

	virtual int getHitpoints() const override { return hitpoints_; }

	void setText(const std::string& text, const std::string& font, int size, int align);

	virtual bool boardableVehicle() const override;

	virtual void boarded(Level& lvl, const EntityPtr& player) override;
	virtual void unboarded(Level& lvl) override;

	virtual void boardVehicle() override;
	virtual void unboardVehicle() override;

	void set_driver_position();

	virtual bool useAbsoluteScreenCoordinates() const override { return use_absolute_screen_coordinates_; }

	virtual int getCurrentAnimationId() const override { return current_animation_id_; }

	virtual bool handle_sdl_event(const SDL_Event& event, bool claimed);
#ifndef NO_EDITOR
	virtual ConstEditorEntityInfoPtr getEditorInfo() const override;
#endif // !NO_EDITOR

	virtual bool handleEvent(const std::string& event, const FormulaCallable* context=nullptr) override;
	virtual bool handleEvent(int event, const FormulaCallable* context=nullptr) override;
	virtual bool handleEventDelay(int event, const FormulaCallable* context=nullptr) override;

	virtual void resolveDelayedEvents() override;

	virtual bool serializable() const override;

	void setSoundVolume(float volume, float nseconds=0.0) override;
	
	bool executeCommand(const variant& var) override;
	bool executeCommandOrFn(const variant& var);

	virtual game_logic::FormulaPtr createFormula(const variant& v) override;

	bool allowLevelCollisions() const override;

	//statistic on how many FFL events are handled every second.
	static int events_handled_per_second;

	const std::vector<LightPtr>& lights() const override { return lights_; }
	void swapLights(std::vector<LightPtr>& lights) override { lights_.swap(lights); }

	void shiftPosition(int x, int y) override;

	bool appearsAtDifficulty(int difficulty) const override;

	int getMinDifficulty() const { return min_difficulty_; }
	int getMaxDifficulty() const { return max_difficulty_; }

	void setDifficultyRange(int min, int max) { min_difficulty_ = min; max_difficulty_ = max; }

	void updateType(ConstCustomObjectTypePtr old_type,
	                 ConstCustomObjectTypePtr new_type);

	void addWidget(const gui::WidgetPtr& w);
	void addWidgets(std::vector<gui::WidgetPtr>* widgets);
	void clearWidgets();
	void removeWidget(gui::WidgetPtr w);
	gui::WidgetPtr getWidgetById(const std::string& id);
	gui::ConstWidgetPtr getWidgetById(const std::string& id) const;
	std::vector<variant> getVariantWidgetList() const;
	bool getClipArea(rect* clip_area) const override;

	struct AnimatedMovement {
		std::string name;

		//animation_values is a multiple of animation_slots size.
		//animation_slots represents the values being set for each frame
		//in the animation. animation_values contains all the data for
		//all the frames.
		std::vector<int> animation_slots;
		std::vector<variant> animation_values;

		int pos;

		variant on_begin, on_process, on_complete;

		std::vector<std::pair<variant,variant>> follow_on;

		AnimatedMovement() : pos(0)
		{}

		int getAnimationFrames() const { return static_cast<int>(animation_values.size()/animation_slots.size()); }
	};

	void addAnimatedMovement(variant attr, variant options);
	void setAnimatedSchedule(std::shared_ptr<AnimatedMovement> movement);
	void cancelAnimatedSchedule(const std::string& name);

	bool hasOwnDraw() const { return true; }

	void createParticles(const variant& node);

	xhtml::DocumentObjectPtr getDocument() const { return document_; }

	virtual int mouseDragThreshold(int default_value) const override;
protected:
	//components of per-cycle process() that can be done even on
	//static objects.
	void staticProcess(Level& lvl);

	virtual void control(const Level& lvl) override;
	int getValueSlot(const std::string& key) const override;
	variant getValue(const std::string& key) const override;
	variant getValueBySlot(int slot) const override;
	void setValue(const std::string& key, const variant& value) override;
	void setValueBySlot(int slot, const variant& value) override;

	virtual variant getPlayerValueBySlot(int slot) const;
	virtual void setPlayerValueBySlot(int slot, const variant& value);

	//function which indicates if the object wants to walk up or down stairs.
	//-1 = up stairs, 0 = no change, 1 = down stairs
	virtual int walkUpOrDownStairs() const { return 0; }

	bool isUnderwater() const {
		return was_underwater_;
	}

	const std::pair<int,int>* parallaxScaleMillis() const override { return parallax_scale_millis_.get(); }

	enum class STANDING_STATUS { NOT_STANDING, BACK_FOOT, FRONT_FOOT };
	STANDING_STATUS isStanding(const Level& lvl, CollisionInfo* info=nullptr) const;

	void setParent(EntityPtr e, const std::string& pivot_point);

	virtual int parentDepth(bool* has_human_parent=nullptr, int cur_depth=0) const override;

	virtual bool editorForceStanding() const override;

	virtual game_logic::ConstFormulaCallableDefinitionPtr getDefinition() const override;

	EntityPtr standingOn() const override { return standing_on_; }
	virtual void addToLevel() override;

	virtual rect platformRectAt(int xpos) const override;
	virtual int platformSlopeAt(int xpos) const override;

	virtual bool isSolidPlatform() const override;

	virtual void beingRemoved() override;
	virtual void beingAdded() override;

	//set up an animation schedule. values.size() should be a multiple of
	//slots.size().

	bool editorOnly() const override { return editor_only_; }

	void initDeferredProperties();

protected:
	void surrenderReferences(GarbageCollector* collector) override;

private:
	void initProperties(bool defer=false);
	void initProperty(const CustomObjectType::PropertyEntry& e);
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

	std::string debugObjectName() const override;

	bool moveToStandingInternal(Level& lvl, int max_displace);

	void processFrame();

	ConstSolidInfoPtr calculateSolid() const override;
	ConstSolidInfoPtr calculatePlatform() const override;

	virtual void getInputs(std::vector<game_logic::FormulaInput>* inputs) const override;

	int slopeStandingOn(int range) const;

	int previous_y_;

	variant custom_type_;
	ConstCustomObjectTypePtr type_; //the type after variations are applied
	ConstCustomObjectTypePtr base_type_; //the type without any variation
	std::vector<std::string> current_variation_;
	ffl::IntrusivePtr<const Frame> frame_;
	std::string frame_name_;
	int time_in_frame_;
	int time_in_frame_delta_;

	decimal velocity_x_, velocity_y_;
	decimal accel_x_, accel_y_;
	int gravity_shift_;

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
    
	std::unique_ptr<std::pair<int, int>> parallax_scale_millis_;

	int hitpoints_, max_hitpoints_;
	bool was_underwater_;

	bool has_feet_;

	int invincible_;

	bool use_absolute_screen_coordinates_;
	
	float sound_volume_;

	game_logic::ConstFormulaPtr next_animation_formula_;

	game_logic::FormulaVariableStoragePtr vars_, tmp_vars_;
	game_logic::MapFormulaCallablePtr tags_;

	void ensure_property_data_init(int slot) const {
		if(property_init_deferred_.empty() == false) {
			for(auto i = property_init_deferred_.begin(); i != property_init_deferred_.end(); ++i) {
				auto p = *i;
				if(p->storage_slot == slot) {
					auto mutable_this = const_cast<CustomObject*>(this);

					auto itor = mutable_this->property_init_deferred_.begin();
					itor += i - property_init_deferred_.begin();

					mutable_this->property_init_deferred_.erase(itor);
					mutable_this->initProperty(*p);
					return;
				}
			}
		}
	}
	variant& get_property_data(int slot) { ensure_property_data_init(slot); if(property_data_.size() <= size_t(slot)) { property_data_.resize(slot+1); } return property_data_[slot]; }
	variant get_property_data(int slot) const { ensure_property_data_init(slot); if(property_data_.size() <= size_t(slot)) { return variant(); } return property_data_[slot]; }
	std::vector<variant> property_data_;

	//A list of properties which have their initialization *deferred*. This is so properties with an init:
	//can wait until all the other fields are populated. If an attempt is made to access one of these
	//properties it will be initialized on the spot. Otherwise all deferred properties are initialized
	//at the end of construction.
	std::vector<const CustomObjectType::PropertyEntry*> property_init_deferred_;
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
	decimal getDrawScale() const override { return getValueBySlot(CUSTOM_OBJECT_SCALE).as_decimal(); };
	
	void setDrawScale(float new_scale) override {
		setValueBySlot(CUSTOM_OBJECT_SCALE, variant(new_scale));
	}
	
	std::shared_ptr<rect> draw_area_, activation_area_, clip_area_;
	bool clip_area_absolute_;
	int activation_border_;
	
	bool can_interact_with_;

	std::map<std::string, ParticleSystemPtr> particle_systems_;

	typedef std::shared_ptr<CustomObjectText> CustomObjectTextPtr;
	CustomObjectTextPtr text_;

	EntityPtr driver_;

	std::vector<ffl::IntrusivePtr<BlurObject> > blur_objects_;

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

	std::vector<std::shared_ptr<AnimatedMovement>> animated_movement_;

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

	std::shared_ptr<const std::vector<Frame::CustomPoint>> custom_draw_;

	std::vector<float> custom_draw_xy_;
	std::vector<float> custom_draw_uv_;

	void set_platform_area(const rect& area);

	std::vector<int> platform_offsets_;

	bool editor_only_;
	bool collides_with_level_;

	bool handleEventInternal(int event, const FormulaCallable* context, bool executeCommands_now=true);
	std::vector<variant> delayed_commands_;

	int currently_handling_die_event_;

	typedef std::set<gui::WidgetPtr, gui::WidgetSortZOrder> widget_list;
	widget_list widgets_;

	rect previous_water_bounds_;

	mutable screen_position adjusted_draw_position_;

	std::vector<graphics::DrawPrimitivePtr> draw_primitives_;

	bool paused_;

	std::vector<int> properties_requiring_dynamic_initialization_;

	// for lua integration
#if defined(USE_LUA)
	void init_lua();
	std::unique_ptr<lua::LuaContext> lua_ptr_;
	std::unique_ptr<lua::CompiledChunk> lua_chunk_;
#endif

	graphics::AnuraShaderPtr shader_;
	unsigned int shader_flags_;
	std::vector<graphics::AnuraShaderPtr> effects_shaders_;

	// new particles systems
	graphics::ParticleSystemContainerProxyPtr particles_;

	xhtml::DocumentObjectPtr document_;
};
