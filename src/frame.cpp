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
#include <assert.h>

#include <algorithm>
#include <array>
#include <iostream>

#include <boost/lexical_cast.hpp>

#include "DisplayDevice.hpp"
#include "TextureUtils.hpp"
#include "WindowManager.hpp"

#include "asserts.hpp"
#include "frame.hpp"
#include "level.hpp"
#include "module.hpp"
#include "object_events.hpp"
#include "preferences.hpp"
#include "rectangle_rotator.hpp"
#include "solid_map.hpp"
#include "sound.hpp"
#include "string_utils.hpp"
#include "surface_cache.hpp"
#include "surface_palette.hpp"
#include "TextureObject.hpp"
#include "variant_utils.hpp"

PREF_FLOAT(global_frame_scale, 2.0, "Sets the global frame scales for all frames in all animations");

namespace 
{
	std::set<Frame*>& palette_frames() {
		static std::set<Frame*>* instance = new std::set<Frame*>;
		return *instance;
	}

	static unsigned int current_palette_mask = 0;

	static const glm::vec3 z_axis(0, 0, 1.0f);
}

void Frame::buildPatterns(variant obj_variant)
{
	if(!obj_variant["animation"].is_list()) {
		return;
	}

	static const std::string ImagesPath = "./images/";

	std::vector<variant> items = obj_variant["animation"].as_list();
	for(variant item : items) {
		variant pattern = item["image_pattern"];
		if(!pattern.is_string()) {
			continue;
		}

		const std::string path = ImagesPath + pattern.as_string();

		std::string dir;
		std::vector<std::string> files;
		module::get_files_matching_wildcard(path, &dir, &files);

		assert(dir.size() > ImagesPath.size() && std::equal(ImagesPath.begin(), ImagesPath.end(), dir.c_str()));

		dir.erase(dir.begin(), dir.begin() + ImagesPath.size());

		ASSERT_LOG(files.empty() == false, pattern.debug_location() << ": Could not find any images matching path: " << pattern.as_string());

		std::sort(files.begin(), files.end());

		std::vector<KRE::SurfacePtr> surfaces;
		for(const std::string& fname : files) {
			surfaces.emplace_back(KRE::Surface::create(dir + "/" + fname));

			ASSERT_LOG(surfaces.back()->width() == surfaces.front()->width() &&
			           surfaces.back()->height() == surfaces.front()->height(),
					   pattern.debug_location() << ": All images in image pattern must be the same size: " << fname);
			ASSERT_LOG(surfaces.back()->width() <= 2048 && surfaces.back()->height() <= 2048, "Image too large: " << fname);
		}

		int frames_per_row = static_cast<int>(files.size());
		int total_width = surfaces.front()->width() * static_cast<int>(surfaces.size());
		int total_height = surfaces.front()->height();
		while(total_width > 2048) {
			frames_per_row = frames_per_row/2 + frames_per_row%2;
			total_width /= 2;
			total_height *= 2;
		}

		ASSERT_LOG(total_height <= 2048, pattern.debug_location() << ": Animation too large: cannot fit in 2048x2048: " << pattern.as_string());

		const unsigned texture_width = KRE::next_power_of_two(total_width);
		const unsigned texture_height = KRE::next_power_of_two(total_height);

		KRE::SurfacePtr sheet = KRE::Surface::create(texture_width, texture_height, 32, 0, 0, 0, 0xff);

		for(int n = 0; n != surfaces.size(); ++n) {
			const int xframe = n%frames_per_row;
			const int yframe = n/frames_per_row;
			auto src = surfaces[n];
			src->setBlendMode(KRE::Surface::BlendMode::BLEND_MODE_NONE);
			unsigned sw = surfaces.front()->width();
			unsigned sh = surfaces.front()->height();
			sheet->blitTo(src, rect(xframe*sw, yframe*sh, sw, sh));
		}

		// Create uncached texture from surface.
		auto tex = KRE::Texture::createTexture(sheet);

		boost::intrusive_ptr<TextureObject> tex_obj(new TextureObject(tex));

		std::vector<variant> area;
		area.emplace_back(variant(0));
		area.emplace_back(variant(0));
		area.emplace_back(variant(surfaces.front()->width()-1));
		area.emplace_back(variant(surfaces.front()->height()-1));

		item.add_attr_mutation(variant("fbo"), variant(tex_obj.get()));
		item.add_attr_mutation(variant("image"), variant("fbo"));
		item.add_attr_mutation(variant("rect"), variant(&area));
		item.add_attr_mutation(variant("frames_per_row"), variant(frames_per_row));
		item.add_attr_mutation(variant("frames"), variant(static_cast<int>(surfaces.size())));
		item.add_attr_mutation(variant("pad"), variant(0));
	}
}

Frame::Frame(variant node)
   : id_(node["id"].as_string()),
     variant_id_(id_),
	 doc_(node),
     enter_event_id_(get_object_event_id("enter_" + id_ + "_anim")),
	 end_event_id_(get_object_event_id("end_" + id_ + "_anim")),
	 leave_event_id_(get_object_event_id("leave_" + id_ + "_anim")),
	 processEvent_id_(get_object_event_id("process_" + id_)),
	 solid_(SolidInfo::create(node)),
	 platform_(SolidInfo::createPlatform(node)),
     collide_rect_(node.has_key("collide") ? rect(node["collide"]) :
	               rect(node["collide_x"].as_int(),
                        node["collide_y"].as_int(),
                        node["collide_w"].as_int(),
                        node["collide_h"].as_int())),
	 hit_rect_(node.has_key("hit") ? rect(node["hit"]) :
	               rect(node["hit_x"].as_int(),
				        node["hit_y"].as_int(),
				        node["hit_w"].as_int(),
				        node["hit_h"].as_int())),
	 platform_rect_(node.has_key("platform") ? rect(node["platform"]) :
	                rect(node["platform_x"].as_int(),
	                     node["platform_y"].as_int(),
	                     node["platform_w"].as_int(), 1)),
	 platform_x_(0),
	 platform_y_(0),
	 platform_w_(0),
	 img_rect_(node.has_key("rect") ? rect(node["rect"]) :
	           rect(node["x"].as_int(),
	                node["y"].as_int(),
	                node["w"].as_int(),
	                node["h"].as_int())),
	 feet_x_(node["feet_x"].as_int(img_rect_.w()/2)),
	 feet_y_(node["feet_y"].as_int(img_rect_.h()/2)),
	 accel_x_(node["accel_x"].as_int(std::numeric_limits<int>::min())),
	 accel_y_(node["accel_y"].as_int(std::numeric_limits<int>::min())),
	 velocity_x_(node["velocity_x"].as_int(std::numeric_limits<int>::min())),
	 velocity_y_(node["velocity_y"].as_int(std::numeric_limits<int>::min())),
	 nframes_(node["frames"].as_int(1)),
	 nframes_per_row_(node["frames_per_row"].as_int(-1)),
	 frame_time_(node["duration"].as_int(-1)),
	 reverse_frame_(node["reverse"].as_bool()),
	 play_backwards_(node["play_backwards"].as_bool()),
	 scale_(node["scale"].as_float(static_cast<float>(g_global_frame_scale))),
	 pad_(node["pad"].as_int()),
	 rotate_(node["rotate"].as_int()),
	 blur_(node["blur"].as_int()),
	 rotate_on_slope_(node["rotate_on_slope"].as_bool()),
	 damage_(node["damage"].as_int()),
	 sounds_(node["sound"].is_list() ? node["sound"].as_list_string() : util::split(node["sound"].as_string_default())),
	 force_no_alpha_(node["force_no_alpha"].as_bool(false)),
	 no_remove_alpha_borders_(node["no_remove_alpha_borders"].as_bool(node.has_key("fbo"))),
	 collision_areas_inside_frame_(true),
	 current_palette_(-1), 
	 blit_target_(node)
{
	blit_target_.setCentre(KRE::Blittable::Centre::TOP_LEFT);
	// We override any scale value set on the frame since we handle that ourselves.
	blit_target_.setScale(1.0f, 1.0f);

	if(node.has_key("image")) {
		auto res = KRE::Texture::findImageNames(node["image"]);
		ASSERT_LOG(res.size() > 0 && !res[0].empty(), "No valid filenames for texture found in: " << node["image"].to_debug_string());
		image_ = res[0];
	} else {
		ASSERT_LOG(false, "No 'image' attribute found.");
	}

	std::vector<std::string> palettes = parse_variant_list_or_csv_string(node["palettes"]);
	unsigned palettes_bitmap = 0;
	for(const std::string& p : palettes) {
		int id = graphics::get_palette_id(p);
		palettes_recognized_.emplace_back(id);
	}

	KRE::TexturePtr fbo_texture;

	if(node.has_key("fbo")) {
		fbo_texture = node["fbo"].convert_to<TextureObject>()->texture();
		blit_target_.setTexture(fbo_texture);
		if(node.has_key("blend")) {
			blit_target_.setBlendMode(KRE::BlendMode(node["blend"]));
		} else {
			blit_target_.setBlendMode(KRE::BlendModeConstants::BM_SRC_ALPHA, KRE::BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);
		}

		if(node.has_key("blend_equation")) {
			blit_target_.setBlendEquation(KRE::BlendEquation(node));
		}
	} else if(node.has_key("image")) {
		blit_target_.setTexture(graphics::get_palette_texture(image_, node["image"], palettes_recognized_));
	}

	if(palettes_recognized_.empty() == false) {
		palette_frames().insert(this);
		if(current_palette_mask) {
			setPalettes(current_palette_mask);
		}
	}

	std::vector<std::string> hit_frames = util::split(node["hit_frames"].as_string_default());
	for(const std::string& f : hit_frames) {
		hit_frames_.emplace_back(boost::lexical_cast<int>(f));
	}

	const std::string& events = node["events"].as_string_default();
	if(!events.empty()) {
		//events are in the format time0:time1:...:timen:event0,time0:time1:...:timen:event1,...
		std::vector<std::string> event_vector = util::split(events);
		std::map<int, std::string> event_map;
		for(const std::string& e : event_vector) {
			std::vector<std::string> time_event = util::split(e, ':');
			if(time_event.size() < 2) {
				continue;
			}

			const std::string& event = time_event.back();

			for(unsigned int n = 0; n < time_event.size() - 1; ++n) {
				const int time = atoi(time_event[n].c_str());
				event_map[time] = event;
			}
		}

		typedef std::pair<int,std::string> event_pair;
		for(const event_pair& p : event_map) {
			event_frames_.emplace_back(p.first);
			event_names_.emplace_back(p.second);
		}
	}

	static const std::string AreaPostfix = "_area";
	for(const variant_pair& val : node.as_map()) {
		const std::string& attr = val.first.as_string();
		if(attr.size() <= AreaPostfix.size() || std::equal(AreaPostfix.begin(), AreaPostfix.end(), attr.end() - AreaPostfix.size()) == false || attr == "solid_area" ||attr == "platform_area") {
			continue;
		}

		const std::string area_id = std::string(attr.begin(), attr.end() - AreaPostfix.size());

		variant value = val.second;

		bool solid = false;
		rect r;
		if(value.is_null()) {
			continue;
		} else if(value.is_string() && value.as_string() == "all") {
			r = rect(0, 0, width(), height());
		} else if(value.is_list()) {
			std::vector<int> v;
			for(const variant& var : value.as_list()) {
				if(var.is_int()) {
					v.emplace_back(var.as_int());
				} else if(var.is_string() && var.as_string() == "solid") {
					solid = true;
				} else if(var.is_string() && var.as_string() == "all") {
					r = rect(0, 0, width(), height());
				} else {
					ASSERT_LOG(false, "Unrecognized attribute for '" << attr << "': " << value.to_debug_string());
				}
			}

			if(v.empty() == false) {
				r = rect(v);
				r = rectf(r.x()*scale_, r.y()*scale_, r.w()*scale_, r.h()*scale_).as_type<int>();
			}
		}

		CollisionArea area = { area_id, r, solid };
		collision_areas_.emplace_back(area);

		if(solid && (r.x() < 0 || r.y() < 0 || r.x2() > width() || r.y2() > height())) {
			collision_areas_inside_frame_ = false;
		}
	}

	if(node.has_key("frame_info")) {
		std::vector<int> values = node["frame_info"].as_list_int();
		int num_values = static_cast<int>(values.size());

		ASSERT_GT(num_values, 0);
		ASSERT_EQ(num_values%8, 0);
		ASSERT_LE(num_values, 1024);
		const int* i = &values[0];
		const int* i2 = &values[0] + num_values;
		while(i != i2) {
			FrameInfo info;
			info.x_adjust = *i++;
			info.y_adjust = *i++;
			info.x2_adjust = *i++;
			info.y2_adjust = *i++;
			const int x = *i++;
			const int y = *i++;
			const int w = *i++;
			const int h = *i++;
			info.area = rect(x, y, w, h);
			frames_.emplace_back(info);
			ASSERT_EQ(intersection_rect(info.area, rect(0, 0, static_cast<int>(blit_target_.getTexture()->surfaceWidth()), static_cast<int>(blit_target_.getTexture()->surfaceHeight()))), info.area);
			ASSERT_EQ(w + (info.x_adjust + info.x2_adjust), img_rect_.w());
			ASSERT_EQ(h + (info.y_adjust + info.y2_adjust), img_rect_.h());

		}

		ASSERT_EQ(frames_.size(), nframes_);

		buildAlphaFromFrameInfo();
	} else {
		buildAlpha();
	}

	for(const variant_pair& value : node.as_map()) {
		static const std::string PivotPrefix = "pivot_";
		const std::string& attr = value.first.as_string();
		if(attr.size() > PivotPrefix.size() && std::equal(PivotPrefix.begin(), PivotPrefix.end(), attr.begin())) {
			PivotSchedule schedule;
			schedule.name = std::string(attr.begin() + PivotPrefix.size(), attr.end());

			std::vector<int> values = value.second.as_list_int();

			ASSERT_LOG(values.size()%2 == 0, "PIVOT POINTS IN INCORRECT FORMAT, ODD NUMBER OF INTEGERS");
			const int num_points = static_cast<int>(values.size())/2;

			int repeat = std::max<int>(1, (nframes_*frame_time_)/std::max<int>(1, num_points));
			for(int n = 0; n != num_points; ++n) {
				point p(values[n*2], values[n*2+1]);
				for(int m = 0; m != repeat; ++m) {
					schedule.points.emplace_back(p);
				}
			}

			if(reverse_frame_) {
				std::vector<point> v = schedule.points;
				std::reverse(v.begin(), v.end());
				schedule.points.insert(schedule.points.end(), v.begin(), v.end());
			}

			if(schedule.points.empty() == false) {
				pivots_.emplace_back(schedule);
			}
		}
	}

	//by default once we've used an fbo texture we clear surfaces
	//from it as generally fbo textures don't need their surfaces
	//anymore after that.
	if(fbo_texture && node["clear_fbo"].as_bool(true)) {
		fbo_texture->clearSurfaces();
	}

	// Need to do stuff with co-ordinates here I think.
}

Frame::~Frame()
{
	if(palettes_recognized_.empty() == false) {
		palette_frames().erase(this);
	}
}

variant Frame::write() const
{
	return doc_;

	/* --if we decide we want to write this out instead of just saving the
	     doc this is what it might look like.
	variant_builder builder;

	builder.add("id", id_);

	builder.add("collide", collide_rect_.write());
	builder.add("hit", hit_rect_.write());
	builder.add("platform", platform_rect_.write());
	builder.add("rect", img_rect_.write());

	if(feet_x_ != img_rect_.w()/2) {
		builder.add("feet_x", feet_x_);
	}

	if(feet_y_ != img_rect_.h()/2) {
		builder.add("feet_y", feet_y_);
	}

	if(accel_x_ != std::numeric_limits<int>::min()) {
		builder.add("accel_x", accel_x_);
	}
	
	if(accel_y_ != std::numeric_limits<int>::min()) {
		builder.add("accel_y", accel_y_);
	}

	if(velocity_x_ != std::numeric_limits<int>::min()) {
		builder.add("velocity_x", velocity_x_);
	}
	
	if(velocity_y_ != std::numeric_limits<int>::min()) {
		builder.add("velocity_y", velocity_y_);
	}

	builder.add("frames", nframes_);
	builder.add("frames_per_row", nframes_per_row_);
	builder.add("duration", frame_time_);
	builder.add("reverse", reverse_frame_);
	builder.add("play_backwards", play_backwards_);
	builder.add("scale", scale_);
	builder.add("pad", pad_);
	builder.add("rotate", rotate_);
	builder.add("blur", blur_);
	builder.add("rotate_on_slope", rotate_on_slope_);
	builder.add("damage", damage_);
	builder.add("sounds", sounds_);
	builder.add("force_no_alpha", force_no_alpha_);
	builder.add("no_remove_alpha_borders", no_remove_alpha_borders_);

	if(image_.empty()) {
	} else {
		builder.add("image", image_);
	}

	return builder.build();
	*/
}

void Frame::setPalettes(unsigned int palettes)
{
	int npalette = -1;
	for(auto palette : palettes_recognized_) {
		if((1 << palette) & palettes) {
			npalette = palette;
			break;
		}
	}
	blit_target_.getTexture()->setPalette(palettes == 0 ? -1 : npalette);
	//LOG_DEBUG("Set palette " << npalette << " on " << blit_target_.getTexture()->id() << " from selection: " << std::hex << palettes << ", " << graphics::get_palette_name(npalette));
}

void Frame::setColorPalette(unsigned int palettes)
{
	LOG_DEBUG("Frame::setColorPalette: " << palettes);
	current_palette_mask = palettes;
	for(auto i : palette_frames()) {
		i->setPalettes(palettes);
	}
}

void Frame::setImageAsSolid()
{
	solid_ = SolidInfo::createFromTexture(blit_target_.getTexture(), img_rect_);
}

void Frame::playSound(const void* object) const
{
	if (sounds_.empty() == false){
		int randomNum = rand()%sounds_.size();  //like a 1d-size die
		if(sounds_[randomNum].empty() == false) {
			sound::play(sounds_[randomNum], object);
		}
	}
}

void Frame::buildAlphaFromFrameInfo()
{
	if(!blit_target_.getTexture()) {
		return;
	}

	alpha_.resize(nframes_*img_rect_.w()*img_rect_.h(), true);
	for(int n = 0; n < nframes_; ++n) {
		const rect& area = frames_[n].area;
		int dst_index = frames_[n].y_adjust*img_rect_.w()*nframes_ + n*img_rect_.w() + frames_[n].x_adjust;
		for(int y = 0; y != area.h(); ++y) {
			ASSERT_INDEX_INTO_VECTOR(dst_index, alpha_);
			std::vector<bool>::iterator dst = alpha_.begin() + dst_index;

			ASSERT_LT(area.x(), static_cast<int>(blit_target_.getTexture()->surfaceWidth()));
			ASSERT_LE(area.x() + area.w(), static_cast<int>(blit_target_.getTexture()->surfaceWidth()));
			ASSERT_LT(area.y() + y, static_cast<int>(blit_target_.getTexture()->surfaceHeight()));
			std::vector<bool>::const_iterator src = blit_target_.getTexture()->getFrontSurface()->getAlphaRow(area.x(), area.y() + y);

			std::copy(src, src + area.w(), dst);
			
			dst_index += img_rect_.w()*nframes_;
		}
	}

	if(force_no_alpha_) {
		const auto nsize = alpha_.size();
		alpha_.clear();
		alpha_.resize(nsize, false);
		return;
	}
}

void Frame::buildAlpha()
{
	ASSERT_LOG(nframes_ < 1024, "Animation has too many frames");
	frames_.resize(nframes_);
	if(!blit_target_.getTexture()) {
		return;
	}

	const size_t bufsize = nframes_*img_rect_.w()*img_rect_.h();
	ASSERT_LOG(bufsize < size_t(8192*8192), "Animation is unreasonably large");

	alpha_.resize(bufsize, true);

	for(int n = 0; n < nframes_; ++n) {
		const int current_col = (nframes_per_row_ > 0) ? (n% nframes_per_row_) : n;
		const int current_row = (nframes_per_row_ > 0) ? (n/nframes_per_row_) : 0;
		const int xbase = img_rect_.x() + current_col*(img_rect_.w()+pad_);
		const int ybase = img_rect_.y() + current_row*(img_rect_.h()+pad_);

		if(xbase < 0 || ybase < 0 
			|| xbase + img_rect_.w() > blit_target_.getTexture()->surfaceWidth()
			|| ybase + img_rect_.h() > blit_target_.getTexture()->surfaceHeight()) {
			LOG_INFO("IMAGE RECT FOR FRAME '" << id_ << "' #" << n << ": " << img_rect_.x() << " + " << current_col << " * (" << img_rect_.w() << "+" << pad_ << ") IS INVALID: " << xbase << ", " << ybase << ", " << (xbase + img_rect_.w()) << ", " << (ybase + img_rect_.h()) << " / " << blit_target_.getTexture()->surfaceWidth() << "," << blit_target_.getTexture()->surfaceHeight());
			LOG_INFO("IMAGE_NAME: " << image_ << ", Name from texture: " << blit_target_.getTexture()->getFrontSurface()->getName());
			throw Error();
		}

		/*if(!blit_target_.getTexture()->getFrontSurface()) {
			auto& f = frames_[n];
			f.area = rect(xbase, ybase, img_rect_.w(), img_rect_.h());
			continue;
		}*/

		for(int y = 0; y != img_rect_.h(); ++y) {
			const int dst_index = y*img_rect_.w()*nframes_ + n*img_rect_.w();
			ASSERT_INDEX_INTO_VECTOR(dst_index, alpha_);

			std::vector<bool>::iterator dst = alpha_.begin() + dst_index;

			if(!blit_target_.getTexture()->getFrontSurface()) {
				no_remove_alpha_borders_ = true;
				std::fill(dst, dst + img_rect_.w(), false);
			} else {
				std::vector<bool>::const_iterator src = blit_target_.getTexture()->getFrontSurface()->getAlphaRow(xbase, ybase + y);
				std::copy(src, src + img_rect_.w(), dst);
			}
		}

		//now calculate if the actual frame we should be using for drawing
		//is smaller than the outer rectangle, so we can save on drawing space
		auto& f = frames_[n];
		f.area = rect(xbase, ybase, img_rect_.w(), img_rect_.h());

		if(no_remove_alpha_borders_) {
			continue;
		}
		
		int top;
		for(top = 0; top != img_rect_.h(); ++top) {
			const std::vector<bool>::const_iterator a = blit_target_.getTexture()->getFrontSurface()->getAlphaRow(xbase, ybase + top);
			if(std::find(a, a + img_rect_.w(), false) != a + img_rect_.w()) {
				break;
			}
		}

		int bot;
		for(bot = img_rect_.h(); bot > 0; --bot) {
			const std::vector<bool>::const_iterator a = blit_target_.getTexture()->getFrontSurface()->getAlphaRow(xbase, ybase + bot-1);
			if(std::find(a, a + img_rect_.w(), false) != a + img_rect_.w()) {
				break;
			}
		}

		int left;
		for(left = 0; left < img_rect_.w(); ++left) {
			std::vector<bool>::const_iterator a = blit_target_.getTexture()->getFrontSurface()->getAlphaRow(xbase + left, ybase);

			bool has_opaque = false;
			for(int n = 0; n != img_rect_.h(); ++n) {
				if(!*a) {
					has_opaque = true;
				}
				if(n+1 != img_rect_.h()) {
					a += blit_target_.getTexture()->surfaceWidth();
				}
			}

			if(has_opaque) {
				break;
			}
		}

		int right;
		for(right = img_rect_.w(); right > 0; --right) {
			std::vector<bool>::const_iterator a = blit_target_.getTexture()->getFrontSurface()->getAlphaRow(xbase + right-1, ybase);

			bool has_opaque = false;
			for(int n = 0; n != img_rect_.h(); ++n) {
				if(!*a) {
					has_opaque = true;
				}

				if(n+1 != img_rect_.h()) {
					a += blit_target_.getTexture()->surfaceWidth();
				}
			}

			if(has_opaque) {
				break;
			}
		}

		if(right < left) {
			right = left;
		}

		if(bot < top) {
			bot = top;
		}

		f.x_adjust = left;
		f.y_adjust = top;
		f.x2_adjust = img_rect_.w() - right;
		f.y2_adjust = img_rect_.h() - bot;
		f.area = rect(xbase + left, ybase + top, right - left, bot - top);
		ASSERT_EQ(f.area.w() + f.x_adjust + f.x2_adjust, img_rect_.w());
		ASSERT_EQ(f.area.h() + f.y_adjust + f.y2_adjust, img_rect_.h());
	}

	if(force_no_alpha_) {
		const auto nsize = alpha_.size();
		alpha_.clear();
		alpha_.resize(nsize, false);
		return;
	}
}

bool Frame::isAlpha(int x, int y, int time, bool face_right) const
{
	std::vector<bool>::const_iterator itor = getAlphaItor(x, y, time, face_right);
	if(itor == alpha_.end()) {
		return true;
	} else {
		return *itor;
	}
}

std::vector<bool>::const_iterator Frame::getAlphaItor(int x, int y, int time, bool face_right) const
{
	if(alpha_.empty()) {
		return alpha_.end();
	}

	if(face_right == false) {
		x = width() - x - 1;
	}

	if(x < 0 || y < 0 || x >= width() || y >= height()) {
		return alpha_.end();
	}

	x = static_cast<int>(x / scale_);
	y = static_cast<int>(y / scale_);

	const int nframe = frameNumber(time);
	x += nframe*img_rect_.w();
	
	const int index = y*img_rect_.w()*nframes_ + x;
	ASSERT_INDEX_INTO_VECTOR(index, alpha_);
	return alpha_.begin() + index;
}

void Frame::draw(graphics::AnuraShaderPtr shader, int x, int y, bool face_right, bool upside_down, int time, float rotate) const
{
	rect old_src_rect = blit_target_.getTexture()->getSourceRect();

	const FrameInfo* info = nullptr;
	getRectInTexture(time, info);

	x += static_cast<int>((face_right ? info->x_adjust : info->x2_adjust) * scale_);
	y += static_cast<int>(info->y_adjust * scale_);
	int w = static_cast<int>(info->area.w() * scale_);
	int h = static_cast<int>(info->area.h() * scale_);
	//if(x & preferences::xypos_draw_mask) {
	//	--w;
	//}
	//if(h & preferences::xypos_draw_mask) {
	//	--h;
	//}
	//x &= preferences::xypos_draw_mask;
	//y &= preferences::xypos_draw_mask;

	if(shader) {
		shader->setDrawArea(rect(x, y, w, h));
		shader->setSpriteArea(blit_target_.getTexture()->getSourceRectNormalised());
		blit_target_.setShader(shader->getShader());
	}

	auto wnd = KRE::WindowManager::getMainWindow();
	blit_target_.setCentre(KRE::Blittable::Centre::MIDDLE);
	blit_target_.setPosition(x + w/2, y + h/2);
	blit_target_.setRotation(rotate, z_axis);
	blit_target_.setDrawRect(rect(0, 0, w, h));
	blit_target_.setMirrorHoriz(upside_down);
	blit_target_.setMirrorVert(!face_right);
	blit_target_.preRender(wnd);
	wnd->render(&blit_target_);

	blit_target_.getTexture()->setSourceRect(0, old_src_rect);
}

void Frame::draw(graphics::AnuraShaderPtr shader, int x, int y, bool face_right, bool upside_down, int time, float rotate, float scale) const
{
	rect old_src_rect = blit_target_.getTexture()->getSourceRect();

	const FrameInfo* info = nullptr;
	getRectInTexture(time, info);

	x += static_cast<int>((face_right ? info->x_adjust : info->x2_adjust) * scale_);
	y += static_cast<int>(info->y_adjust * scale_);
	const int w = static_cast<int>(info->area.w() * scale_);
	const int h = static_cast<int>(info->area.h() * scale_);
	//x &= preferences::xypos_draw_mask;
	//y &= preferences::xypos_draw_mask;

	//adjust x,y to accomodate scaling so that we scale from the center.
	//const int width_delta = static_cast<int>(img_rect_.w() * scale_ * scale - img_rect_.w() * scale_);
	//const int height_delta = static_cast<int>(img_rect_.h() * scale_ * scale - img_rect_.h() * scale_);
	//x -= width_delta/2;
	//y -= height_delta/2;

	if(shader) {
		shader->setDrawArea(rect(x, y, w, h));
		shader->setSpriteArea(blit_target_.getTexture()->getSourceRectNormalised());
		blit_target_.setShader(shader->getShader());
	}

	auto wnd = KRE::WindowManager::getMainWindow();
	blit_target_.setCentre(KRE::Blittable::Centre::MIDDLE);
	blit_target_.setPosition(x + w/2, y + h/2);
	blit_target_.setRotation(rotate, z_axis);
	blit_target_.setScale(scale, scale);
	blit_target_.setDrawRect(rect(0, 0, w, h));
	blit_target_.setMirrorHoriz(upside_down);
	blit_target_.setMirrorVert(!face_right);
	blit_target_.preRender(wnd);
	wnd->render(&blit_target_);
	blit_target_.setScale(1.0f, 1.0f);

	blit_target_.getTexture()->setSourceRect(0, old_src_rect);
}

void Frame::draw(graphics::AnuraShaderPtr shader, int x, int y, const rect& area, bool face_right, bool upside_down, int time, float rotate) const
{
	rect old_src_rect = blit_target_.getTexture()->getSourceRect();

	const FrameInfo* info = nullptr;
	getRectInTexture(time, info);

	const int x_adjust = area.x();
	const int y_adjust = area.y();
	const int w_adjust = area.w() - img_rect_.w();
	const int h_adjust = area.h() - img_rect_.h();

	const int w = static_cast<int>(info->area.w() * scale_ + w_adjust * scale_);
	const int h = static_cast<int>(info->area.h() * scale_ + h_adjust * scale_);

	const rect src_rect = blit_target_.getTexture()->getSourceRect();

	if(shader) {
		shader->setDrawArea(rect(x, y, w, h));
		shader->setSpriteArea(blit_target_.getTexture()->getSourceRectNormalised());
		blit_target_.setShader(shader->getShader());
	}

	auto wnd = KRE::WindowManager::getMainWindow();
	blit_target_.setCentre(KRE::Blittable::Centre::MIDDLE);
	blit_target_.setPosition(x + w/2, y + h/2);
	blit_target_.setRotation(rotate, z_axis);
	blit_target_.setDrawRect(rect(0, 0, w, h));
	blit_target_.getTexture()->setSourceRect(0, rect(src_rect.x() + x_adjust, src_rect.y() + y_adjust, src_rect.w() + x_adjust + w_adjust, src_rect.h() + y_adjust + h_adjust));
	blit_target_.setMirrorHoriz(upside_down);
	blit_target_.setMirrorVert(!face_right);
	blit_target_.preRender(wnd);
	wnd->render(&blit_target_);

	blit_target_.getTexture()->setSourceRect(0, old_src_rect);
}


void Frame::drawCustom(graphics::AnuraShaderPtr shader, int x, int y, const std::vector<CustomPoint>& points, const rect* area, bool face_right, bool upside_down, int time, float rotation) const
{
	KRE::Blittable blit;
	blit.setTexture(blit_target_.getTexture()->clone());
	rect old_src_rect = blit_target_.getTexture()->getSourceRect();

	const FrameInfo* info = nullptr;
	getRectInTexture(time, info);
	rectf rf = blit_target_.getTexture()->getSourceRectNormalised();

	std::array<float, 4> r = { rf.x1(), rf.y1(), rf.x2(), rf.y2() };

	x += static_cast<int>((face_right ? info->x_adjust : info->x2_adjust) * scale_);
	y += static_cast<int>(info->y_adjust * scale_);
	int w = static_cast<int>(info->area.w() * scale_);
	int h = static_cast<int>(info->area.h() * scale_);

	if(!face_right) {
		std::swap(r[0], r[2]);
	}

	if(upside_down) {
		std::swap(r[1], r[3]);
	}

	if(area != nullptr) {
		const int x_adjust = area->x();
		const int y_adjust = area->y();
		const int w_adjust = area->w() - img_rect_.w();
		const int h_adjust = area->h() - img_rect_.h();

		r[0] += blit_target_.getTexture()->translateCoordW(0, x_adjust);
		r[1] += blit_target_.getTexture()->translateCoordH(0, y_adjust);
		r[2] += blit_target_.getTexture()->translateCoordW(0, x_adjust + w_adjust);
		r[3] += blit_target_.getTexture()->translateCoordH(0, y_adjust + h_adjust);

		w += static_cast<int>(w_adjust * scale_);
		h += static_cast<int>(h_adjust * scale_);
	}

	std::vector<KRE::vertex_texcoord> queue;

	const float center_x = x + static_cast<float>(w)/2.0f;
	const float center_y = y + static_cast<float>(h)/2.0f;

	blit.setPosition(center_x, center_y);
	blit.setRotation(rotation, z_axis);

	if(shader) {
		shader->setDrawArea(rect(x, y, w, h));
		shader->setSpriteArea(blit.getTexture()->getSourceRectNormalised());
		blit.setShader(shader->getShader());
	}

	for(const CustomPoint& p : points) {
		float pos = p.pos;

		if(pos > 4.0) {
			pos = 4.0;
		}

		int side = static_cast<int>(pos);
		float f = pos - static_cast<float>(side);
		if(side >= 4) {
			side = 0;
		}

		float xpos, ypos;
		float u, v;
		switch(side) {
		case 0:
			u = r[0] + (r[2] - r[0]) * f;
			v = r[1];
			xpos = static_cast<float>(x) + static_cast<float>(w) * f;
			ypos = static_cast<float>(y);
			break;
		case 2:
			u = r[2] - (r[2] - r[0]) * f;
			v = r[3];
			xpos = static_cast<float>(x + w) - static_cast<float>(w) * f;
			ypos = static_cast<float>(y + h);
			break;
		case 1:
			u = r[2];
			v = r[1] + (r[3] - r[1]) * f;
			xpos = static_cast<float>(x + w);
			ypos = static_cast<float>(y) + static_cast<float>(h) * f;
			break;
		case 3:
			u = r[0];
			v = r[3] - (r[3] - r[1]) * f;
			xpos = static_cast<float>(x);
			ypos = static_cast<float>(y + h) - static_cast<float>(h) * f;
			break;
		default:
			ASSERT_LOG(false, "ILLEGAL CUSTOM FRAME POSITION: " << side);
			break;
		}

		xpos += static_cast<float>(p.offset.x);
		ypos += static_cast<float>(p.offset.y);

		queue.emplace_back(glm::vec2(xpos - center_x, ypos - center_y), glm::vec2(u, v));
	}

	ASSERT_LOG(queue.size() > 2, "ILLEGAL CUSTOM BLIT: " << queue.size());

	auto wnd = KRE::WindowManager::getMainWindow();
	blit.update(&queue);
	wnd->render(&blit);
	blit_target_.getTexture()->setSourceRect(0, old_src_rect);
}

PREF_BOOL(debug_custom_draw, false, "Show debug visualization of custom drawing");

void Frame::drawCustom(graphics::AnuraShaderPtr shader, int x, int y, const float* xy, const float* uv, int nelements, bool face_right, bool upside_down, int time, float rotation, int cycle) const
{
	rect old_src_rect = blit_target_.getTexture()->getSourceRect();

	const FrameInfo* info = nullptr;
	getRectInTexture(time, info);
	rectf rf = blit_target_.getTexture()->getSourceRectNormalised();

	std::array<float, 4> r = { rf.x1(), rf.y1(), rf.x2(), rf.y2() };
	
	x += static_cast<int>((face_right ? info->x_adjust : info->x2_adjust) * scale_);
	y += static_cast<int>(info->y_adjust * scale_);
	int w = static_cast<int>(info->area.w() * scale_);
	int h = static_cast<int>(info->area.h() * scale_);

	if(!face_right) {
		std::swap(r[0], r[2]);
	}

	if(upside_down) {
		std::swap(r[1], r[3]);
	}

	std::vector<KRE::vertex_texcoord> queue;
	KRE::Blittable blit;

	blit.setTexture(blit_target_.getTexture());
	blit.setRotation(rotation, z_axis);

	for(int n = 0; n < nelements; ++n) {
		queue.emplace_back(glm::vec2(x + w*xy[0], y + h*xy[1]), glm::vec2(r[0] + (r[2] - r[0]) * uv[0], r[1] + (r[3] - r[1]) * uv[1]));
		xy += 2;
		uv += 2;
	}

	blit.getAttributeSet().back()->setCount(queue.size());
	blit.update(&queue);
	if(shader) {
		shader->setDrawArea(rect(x, y, w, h));
		shader->setSpriteArea(rectf::from_coordinates(r[0], r[1], r[2], r[3]));
		shader->setCycle(cycle);
		blit.setShader(shader->getShader());
	}

	auto wnd = KRE::WindowManager::getMainWindow();
	wnd->render(&blit);

	if(g_debug_custom_draw) {
		static auto tex = KRE::Texture::createTexture("white2x2.png");
		blit.setTexture(tex);
		blit.setDrawMode(KRE::DrawMode::LINE_STRIP);
		wnd->render(&blit);
	}
	blit_target_.getTexture()->setSourceRect(0, old_src_rect);
}

void Frame::getRectInTexture(int time, const FrameInfo*& info) const
{
	//picks out a single frame to draw from a whole animation, based on time
	getRectInFrameNumber(frameNumber(time), info);
}

void Frame::getRectInFrameNumber(int nframe, const FrameInfo*& info_result) const
{
	const FrameInfo& info = frames_[nframe];
	info_result = &info;

	if(info.draw_rect_init) {
		blit_target_.getTexture()->setSourceRect(0, info.area);
		info.draw_rect = blit_target_.getTexture()->getSourceRectNormalised();
		return;
	}

	blit_target_.getTexture()->setSourceRect(0, info.area);
	info.draw_rect = blit_target_.getTexture()->getSourceRectNormalised();
	info.draw_rect_init = true;
}

int Frame::duration() const
{
	return (nframes_ + (reverse_frame_ ? nframes_ : 0))*frame_time_;
}

bool Frame::hit(int time_in_frame) const
{
	if(hit_frames_.empty()) {
		return false;
	}

	return std::find(hit_frames_.begin(), hit_frames_.end(), frameNumber(time_in_frame)) != hit_frames_.end();
}

int Frame::frameNumber(int time) const
{
	if(play_backwards_){
		int frame_num = nframes_-1;
		if(frame_time_ > 0 && nframes_ >= 1) {
			if(time >= duration()) {
				if(reverse_frame_){
					frame_num = nframes_-1;
				}else{	
					frame_num = 0;
				}
			} else {
				frame_num = nframes_-1 - time/frame_time_;
			}
			
			//if we are in reverse now
			if(frame_num < 0) {
				frame_num = -frame_num - 1;
			}
		}
		
		return frame_num;
	} else {
		int frame_num = 0;
		if(frame_time_ > 0 && nframes_ >= 1) {
			if(time >= duration()) {
				frame_num = nframes_-1;
			} else {
				frame_num = time/frame_time_;
			}
			
			//if we are in reverse now
			if(frame_num >= nframes_) {
				frame_num = nframes_ - 1 - (frame_num - nframes_);
			}
		}
		
		return frame_num;
	}
}

const std::string* Frame::getEvent(int time_in_frame) const
{
	if(event_frames_.empty()) {
		return nullptr;
	}

	std::vector<int>::const_iterator i = std::find(event_frames_.begin(), event_frames_.end(), time_in_frame);
	if(i == event_frames_.end()) {
		return nullptr;
	}

	return &event_names_[i - event_frames_.begin()];
}

point Frame::pivot(const std::string& name, int time_in_frame) const
{
	if(time_in_frame < 0) {
		return point(getFeetX(),getFeetY());
	}

	for(const PivotSchedule& s : pivots_) {
		if(s.name != name) {
			continue;
		}

		if(static_cast<unsigned>(time_in_frame) >= s.points.size()) {
			return s.points.back();
		}

		return s.points[time_in_frame];
	}

	return point(getFeetX(),getFeetY()); //default is to pivot around feet.
}

BEGIN_DEFINE_CALLABLE_NOBASE(Frame)
	DEFINE_FIELD(id, "string")
		return obj.variantId();
	DEFINE_FIELD(image, "string")
		return variant(obj.getImageName());
	DEFINE_FIELD(duration, "int")
		return variant(obj.frame_time_);
	DEFINE_FIELD(total_animation_time, "int")
		return variant(obj.duration());
	DEFINE_FIELD(width, "int")
		return variant(obj.width());
	DEFINE_FIELD(height, "int")
		return variant(obj.height());
END_DEFINE_CALLABLE(Frame)
