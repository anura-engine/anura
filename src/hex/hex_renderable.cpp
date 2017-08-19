/*
	Copyright (C) 2014-2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <set>

#include "hex_helper.hpp"
#include "hex_loader.hpp"
#include "hex_map.hpp"
#include "hex_renderable.hpp"
#include "hex_tile.hpp"

#include "Shaders.hpp"
#include "SceneGraph.hpp"
#include "StencilSettings.hpp"
#include "WindowManager.hpp"

#include "random.hpp"
#include "profile_timer.hpp"

namespace hex
{
	using namespace KRE;

	const int g_hex_tile_size = 72;

	namespace 
	{
		rng::Seed hex_tile_seed;
	}

	SceneNodeRegistrar<MapNode> psc_register("hex_map");

	MapNode::MapNode(std::weak_ptr<KRE::SceneGraph> sg, const variant& node)
		: SceneNode(sg, node),
		  layers_(),
		  changed_(false)
	{
	}

	MapNodePtr MapNode::create(std::weak_ptr<KRE::SceneGraph> sg, const variant& node)
	{
		return std::make_shared<MapNode>(sg, node);
	}

	void MapNode::notifyNodeAttached(std::weak_ptr<SceneNode> parent)
	{
		for(auto& layer : layers_) {
			attachObject(layer);
		}
		attachObject(rr_);
	}

	point calculate_position(int w, int h, const std::vector<int>& borders, const point& base, const point& center, const point& offset, const point& hex_pixel_pos)
	{
		point p = hex_pixel_pos + offset + center; // + base;
		if(center.x != 0 || center.y != 0) {
			p.x -= w / 2;
			p.y -= h / 2;
			if(!borders.empty()) {
				p.x -= (borders[0] + borders[2]) / 2;
				p.y -= (borders[1] + borders[3]) / 2;
			}
		}

		if(!borders.empty()) {
			p.x += borders[0];
			p.y += borders[1];
		}
		return p;
	}

	point add_tex_coords(std::vector<KRE::vertex_texcoord>* coords, const rectf& uv, int w, int h, const std::vector<int>& borders, const point& base, const point& center, const point& offset, const point& hex_pixel_pos)
	{
		point p = calculate_position(w, h, borders, base, center, offset, hex_pixel_pos);
		const float vx1 = static_cast<float>(p.x);
		const float vy1 = static_cast<float>(p.y);
		const float vx2 = static_cast<float>(p.x + w);
		const float vy2 = static_cast<float>(p.y + h);

		coords->emplace_back(glm::vec2(vx1, vy1), glm::vec2(uv.x1(), uv.y1()));
		coords->emplace_back(glm::vec2(vx2, vy1), glm::vec2(uv.x2(), uv.y1()));
		coords->emplace_back(glm::vec2(vx2, vy2), glm::vec2(uv.x2(), uv.y2()));

		coords->emplace_back(glm::vec2(vx2, vy2), glm::vec2(uv.x2(), uv.y2()));
		coords->emplace_back(glm::vec2(vx1, vy1), glm::vec2(uv.x1(), uv.y1()));
		coords->emplace_back(glm::vec2(vx1, vy2), glm::vec2(uv.x1(), uv.y2()));
		return p;
	}

	void MapNode::update(int width, int height, const std::vector<HexObject>& tiles)
	{
		layers_.clear();
		clear();

		/*rr_.reset(new RectRenderable);
		const point p1 = get_pixel_pos_from_tile_pos_evenq(1, 1, g_hex_tile_size) + point(0, g_hex_tile_size / 2);
		const point p2 = get_pixel_pos_from_tile_pos_evenq(width-2, height-2, g_hex_tile_size) + point(0, g_hex_tile_size / 2);
		rr_->update(p1.x, p1.y, p2.x, p2.y, Color::colorWhite());
		rr_->setOrder(999999);
		attachObject(rr_);*/

		std::map<std::pair<int,int>, std::pair<MapLayerPtr, std::vector<KRE::vertex_texcoord>>> map_layers;
		for(auto& hex : tiles) {
			auto images = hex.getImages();
			for(auto& img : images) {
				rect area;
				std::vector<int> borders;
				auto tex = get_terrain_texture(img.name, &area, &borders);
				if(!img.crop.empty()) {
					area = rect(area.x1() + img.crop.x1(), area.y1() + img.crop.y1(), img.crop.w(), img.crop.h());
				}
				if(img.is_animated) {
					auto& layer = map_layers[std::make_pair(img.layer,tex->id())];
					std::shared_ptr<AnimatedMapLayer> aml = std::dynamic_pointer_cast<AnimatedMapLayer>(layer.first);
					if(aml == nullptr) {
						aml = std::make_shared<AnimatedMapLayer>();
					}
					aml->setTexture(tex);
					aml->addAnimationSeq(img.animation_frames, get_pixel_pos_from_tile_pos_evenq(hex.getPosition(), g_hex_tile_size));
					aml->setAnimationTiming(img.animation_timing);
					aml->setCrop(img.crop);
					aml->setColor(1.0f, 1.0f, 1.0f, img.opacity);
					aml->setBCO(img.base, img.center, img.offset);
					
					layer.first = aml;
				} else {
					if(tex) {
						auto& layer = map_layers[std::make_pair(img.layer,tex->id())];
						if(layer.first == nullptr) {
							layer.first.reset(new MapLayer);
						}

						layer.first->setTexture(tex);
						add_tex_coords(&layer.second, 
							tex->getTextureCoords(0, area), 
							area.w(), 
							area.h(), 
							borders, 
							img.base, 
							img.center, 
							img.offset,
							get_pixel_pos_from_tile_pos_evenq(hex.getPosition(), 
							g_hex_tile_size));
						layer.first->setColor(1.0f, 1.0f, 1.0f, img.opacity);
					}
				}
			}
		}

		for(auto& layer : map_layers) {
			layer.second.first->updateAttributes(&layer.second.second);
			layer.second.first->setOrder(layer.first.first + layer.first.second + 1000);
			layer.second.first->setBlendMode(BlendModeConstants::BM_ONE, BlendModeConstants::BM_ONE_MINUS_SRC_ALPHA);
			layers_.emplace_back(layer.second.first);
			attachObject(layer.second.first);
		}
	}

	MapLayer::MapLayer()
		: SceneObject("hex::MapLayer"),
		  attr_(nullptr)
	{
		setShader(ShaderProgram::getSystemDefault());

		//auto as = DisplayDevice::createAttributeSet(true, false ,true);
		auto as = DisplayDevice::createAttributeSet(true, false, false);
		as->setDrawMode(DrawMode::TRIANGLES);

		attr_ = std::make_shared<Attribute<vertex_texcoord>>(AccessFreqHint::STATIC);
		attr_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
		attr_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));

		as->addAttribute(attr_);
		addAttributeSet(as);
	}
	
	void MapLayer::updateAttributes(std::vector<KRE::vertex_texcoord>* attrs)
	{
		attr_->update(attrs);
	}

	AnimatedMapLayer::AnimatedMapLayer()
		: frames_(),
		  crop_rect_(),
		  timing_(100),
		  current_frame_pos_(0),
		  mask_(nullptr),
		  alpha_uv_()
	{
	}

	void AnimatedMapLayer::preRender(const KRE::WindowPtr& wnd)
	{
		static int last_check_time = -1;
		bool update_anim = false;
		if(last_check_time == -1) {
			last_check_time = profile::get_tick_time();
			update_anim = true;
		} else {
			int current_tick = profile::get_tick_time();
			if(current_tick - last_check_time >= timing_) {
				last_check_time = current_tick;
				update_anim = true;
			}
		}

		if(update_anim) {
			if(mask_ == nullptr) {
				std::vector<int> borders;
				rect area;
				auto tex = get_terrain_texture("alphamask", &area, &borders);
				alpha_uv_ = tex->getTextureCoords(0, area);
				mask_.reset(new Blittable(tex));
				auto shader = ShaderProgram::getSystemDefault()->clone();
				shader->setUniformDrawFunction([](ShaderProgramPtr shader) { 
					shader->setUniformValue(shader->getDiscardUniform(), 1);
				});
				mask_->setShader(shader);
			}

			std::vector<KRE::vertex_texcoord> vtx;
			std::vector<KRE::vertex_texcoord> mask_vtx;
			auto tex = getTexture();
			for(auto it = frames_.cbegin(); it != frames_.cend(); ++it) {
				const auto& f = *it;
				const auto& pos = f.first;
				const auto& frame = f.second;
				rect area = frame[current_frame_pos_ % frame.size()].area;
				if(!crop_rect_.empty()) {
					area = rect(area.x1() + crop_rect_.x1(), area.y1() + crop_rect_.y1(), crop_rect_.w(), crop_rect_.h());
				}
				point p = add_tex_coords(&vtx, 
					tex->getTextureCoords(0, area),
					area.w(),
					area.h(),
					frame[current_frame_pos_ % frame.size()].borders, 
					base_, 
					center_, 
					offset_,
					pos);
				if(it != frames_.cbegin()) {
					mask_vtx.emplace_back(glm::vec2(p.x, p.y), glm::vec2(alpha_uv_.x1(), alpha_uv_.y1())); // degenerate
				}
				mask_vtx.emplace_back(glm::vec2(p.x, p.y), glm::vec2(alpha_uv_.x1(), alpha_uv_.y1()));
				mask_vtx.emplace_back(glm::vec2(p.x + area.w(), p.y), glm::vec2(alpha_uv_.x2(), alpha_uv_.y1()));
				mask_vtx.emplace_back(glm::vec2(p.x, p.y + area.h()), glm::vec2(alpha_uv_.x1(), alpha_uv_.y2()));
				mask_vtx.emplace_back(glm::vec2(p.x + area.w(), p.y + area.h()), glm::vec2(alpha_uv_.x2(), alpha_uv_.y2()));
				auto next_it = it; ++next_it;
				if(next_it != frames_.cend()) {
					mask_vtx.emplace_back(glm::vec2(p.x + area.w(), p.y + area.h()), glm::vec2(alpha_uv_.x2(), alpha_uv_.y2())); // degenerate
				}
			}

			clearAttributes();
			updateAttributes(&vtx);

			mask_->update(&mask_vtx);
			setClipSettings(get_stencil_mask_settings(), mask_);

			++current_frame_pos_;
		}
	}

	void AnimatedMapLayer::addAnimationSeq(const std::vector<std::string>& frames, const point& hex_pos)
	{
		std::vector<AnimFrame> new_frames;
		new_frames.reserve(frames.size());
		for(const auto& frame : frames) {
			rect area;
			std::vector<int> borders;
			auto tex = get_terrain_texture(frame, &area, &borders);
			new_frames.emplace_back(area, borders);
		}
		frames_.emplace(hex_pos, new_frames);
	}
}
