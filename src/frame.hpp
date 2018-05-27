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

#include <string>
#include <vector>

#include "Blittable.hpp"

#include "anura_shader.hpp"
#include "formula.hpp"
#include "solid_map_fwd.hpp"
#include "variant.hpp"
#include <glm/glm.hpp>


class Frame : public game_logic::FormulaCallable
{
public:
	//exception thrown when there's a loading error.
	struct Error {};

	struct CollisionArea {
		std::string name;
		rect area;

		//if this flag is set, then the entire area is considered to
		//collide, rather than just the pixels that have non-zero alpha.
		bool no_alpha_check;
	};

	static void buildPatterns(variant obj_variant);

	static void setColorPalette(unsigned int palettes);

	explicit Frame(variant node);
	~Frame();

	variant write() const;

	//ID of the frame. Not unique, but is the name of the element the frame
	//came from. Useful to tell what kind of frame it is.
	const std::string& id() const { return id_; }
	const variant& variantId() const { return variant_id_; }
	const std::string& getImageName() const { return image_; }

	//play a sound. 'object' is just the address of the object playing the
	//sound, useful if the sound is later cancelled.
	void playSound(const void* object=nullptr) const;
	bool isAlpha(int x, int y, int time, bool face_right) const;

	//Low level interface to alpha information.
	std::vector<bool>::const_iterator getAlphaItor(int x, int y, int time, bool face_right) const;
	const std::vector<bool>& getAlphaBuf() const { return alpha_; }

	void draw(graphics::AnuraShaderPtr shader, int x, int y, bool face_right=true, bool upside_down=false, int time=0, float rotate=0) const;
	void draw(graphics::AnuraShaderPtr shader, int x, int y, bool face_right, bool upside_down, int time, float rotate, float scale) const;
	void draw(graphics::AnuraShaderPtr shader, int x, int y, const rect& area, bool face_right=true, bool upside_down=false, int time=0, float rotate=0) const;

	struct CustomPoint {
		CustomPoint() : pos(0) {}
		float pos;
		point offset;
	};

	void drawCustom(graphics::AnuraShaderPtr shader, int x, int y, const std::vector<CustomPoint>& points, const rect* area, bool face_right, bool upside_down, int time, float rotate) const;
	void drawCustom(graphics::AnuraShaderPtr shader, int x, int y, const float* xy, const float* uv, int nelements, bool face_right, bool upside_down, int time, float rotate, int cycle) const;

	struct BatchDrawItem {
		const Frame* frame;
		int x, y;
		bool face_right;
		bool upside_down;
		int time;
		float rotate;
		float scale;
	};

	static void drawBatch(graphics::AnuraShaderPtr shader, const BatchDrawItem* i1, const BatchDrawItem* i2);

	void setImageAsSolid();
	ConstSolidInfoPtr solid() const { return solid_; }
	ConstSolidInfoPtr platform() const { return platform_; }
	int collideX() const { return static_cast<int>(collide_rect_.x()*scale_); }
	int collideY() const { return static_cast<int>(collide_rect_.y()*scale_); }
	int collideW() const { return static_cast<int>(collide_rect_.w()*scale_); }
	int collideH() const { return static_cast<int>(collide_rect_.h()*scale_); }
	int hitX() const { return static_cast<int>(hit_rect_.x()*scale_); }
	int hitY() const { return static_cast<int>(hit_rect_.y()*scale_); }
	int hitW() const { return static_cast<int>(hit_rect_.w()*scale_); }
	int hitH() const { return static_cast<int>(hit_rect_.h()*scale_); }
	int platformX() const { return static_cast<int>(platform_rect_.x()*scale_); }
	int platformY() const { return static_cast<int>(platform_rect_.y()*scale_); }
	int platformW() const { return static_cast<int>(platform_rect_.w()*scale_); }
	bool hasPlatform() const { return platform_rect_.w() > 0; }
	int getFeetX() const { return static_cast<int>(feet_x_*scale_); }
	int getFeetY() const { return static_cast<int>(feet_y_*scale_); }
	int accelX() const { return accel_x_; }
	int accelY() const { return accel_y_; }
	int velocityX() const { return velocity_x_; }
	int velocityY() const { return velocity_y_; }
	int width() const { return static_cast<int>(img_rect_.w()*scale_); }
	int height() const { return static_cast<int>(img_rect_.h()*scale_); }
	int duration() const;
	bool hit(int time_in_frame) const;
	KRE::TexturePtr img() const { return blit_target_.getTexture(); }
	const rect& area() const { return img_rect_; }
	int numFrames() const { return nframes_; }
	int numFramesPerRow() const { return nframes_per_row_ > 0 && nframes_per_row_ < nframes_ ? nframes_per_row_ : nframes_; }
	int pad() const { return pad_; }
	int blur() const { return blur_; }
	bool rotateOnSlope() const { return rotate_on_slope_; }
	int damage() const { return damage_; }

	float scale() const { return scale_; }

	const std::string* getEvent(int time_in_frame) const;

	const std::vector<CollisionArea>& getCollisionAreas() const { return collision_areas_; }
	bool hasCollisionAreasInsideFrame() const { return collision_areas_inside_frame_; }

	int enterEventId() const { return enter_event_id_; }
	int endEventId() const { return end_event_id_; }
	int leaveEventId() const { return leave_event_id_; }
	int processEventId() const { return processEvent_id_; }

	struct FrameInfo {
		FrameInfo() : x_adjust(0), y_adjust(0), x2_adjust(0), y2_adjust(0), draw_rect_init(false)
		{}
		int x_adjust, y_adjust, x2_adjust, y2_adjust;
		rect area;

		mutable bool draw_rect_init;
		mutable rectf draw_rect;
	};

	const std::vector<FrameInfo>& frameLayout() const { return frames_; }

	point pivot(const std::string& name, int time_in_frame) const;
	int frameNumber(int time_in_frame) const;

	const std::vector <std::string>& getSounds() const { return sounds_; }

	void SetNeedsSerialization(bool b) { needs_serialization_ = b; }
	bool GetNeedsSerialization() const { return needs_serialization_;  }
private:
	DECLARE_CALLABLE(Frame);

	void getRectInTexture(int time, const FrameInfo*& info) const;
	void getRectInFrameNumber(int nframe, const FrameInfo*& info) const;

	void surrenderReferences(GarbageCollector* collector) override;
	std::string id_, image_;

	//ID as a variant, useful to be able to get a variant of the ID
	//very efficiently.
	variant variant_id_;

	//the document fragment this was created from.
	variant doc_;

	//ID's used to signal events that occur on this animation.
	int enter_event_id_, end_event_id_, leave_event_id_, processEvent_id_;
	ConstSolidInfoPtr solid_, platform_;
	rect collide_rect_;
	rect hit_rect_;
	rect img_rect_;

	std::vector<FrameInfo> frames_;

	rect platform_rect_;
	std::vector<int> hit_frames_;
	int platform_x_, platform_y_, platform_w_;
	int feet_x_, feet_y_;
	int accel_x_, accel_y_;
	int velocity_x_, velocity_y_;
	int nframes_;
	int nframes_per_row_;
	int frame_time_;
	bool reverse_frame_;
	bool play_backwards_;
	float scale_;
	int pad_;
	int rotate_;
	int blur_;
	bool rotate_on_slope_;
	int damage_;

	std::vector<int> event_frames_;
	std::vector<std::string> event_names_;
	std::vector <std::string> sounds_;

	std::vector<CollisionArea> collision_areas_;
	bool collision_areas_inside_frame_;

	void buildAlphaFromFrameInfo();
	void buildAlpha();
	std::vector<bool> alpha_;
	bool allow_wrapping_;
	bool force_no_alpha_;

	bool no_remove_alpha_borders_;

	//the animation was created dynamically and should be serialized with objects
	bool needs_serialization_;

	std::vector<int> palettes_recognized_;
	int current_palette_;

	struct PivotSchedule {
		std::string name;
		std::vector<point> points;
	};

	std::vector<PivotSchedule> pivots_;

	void setPalettes(unsigned int palettes);

	mutable KRE::Blittable blit_target_;
};

typedef ffl::IntrusivePtr<Frame> FramePtr;
