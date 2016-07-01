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

#include "hex_logical_tiles.hpp"
#include "hex_object.hpp"
#include "hex_renderable.hpp"
#include "hex_tile.hpp"

#include "Shaders.hpp"
#include "SceneGraph.hpp"
#include "WindowManager.hpp"

#include "random.hpp"

namespace hex
{
	using namespace KRE;

	namespace 
	{
		rng::Seed hex_tile_seed;
	}

	SceneNodeRegistrar<MapNode> psc_register("hex_map");

	MapNode::MapNode(std::weak_ptr<KRE::SceneGraph> sg, const variant& node)
		: SceneNode(sg, node),
		  layers_(),
		  overlay_(),
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
		for(auto& o : overlay_) {
			attachObject(o);
		}
	}

	namespace 
	{
		struct OverlayHelper {
			OverlayHelper() : x_(nullptr), obj_(nullptr), tex_(nullptr) {}
			explicit OverlayHelper(const Alternate* x, const HexObject* obj, const KRE::TexturePtr& tex) : x_(x), obj_(obj), tex_(tex) {}
			const Alternate* x_;
			const HexObject* obj_;
			KRE::TexturePtr tex_;
		};
	}

	void MapNode::update(int width, int height, const std::vector<HexObject>& tiles)
	{
		layers_.clear();
		clear();

		int max_tile_id = logical::Tile::getMaxTileId();

		std::vector<MapRenderParams> sorted_map;
		sorted_map.resize(max_tile_id);

		for(auto& t : tiles) {
			MapRenderParams& param = sorted_map[t.tile()->numeric_id()];
			param.tiles.emplace_back(&t);
			param.map_layer = std::make_shared<MapLayer>();
		}

		size_t base_order = 0;
		for(auto& layer : sorted_map) {
			if(layer.tiles.empty() || layer.map_layer == nullptr) {
				continue;
			}

			auto& new_layer = layer.map_layer;
			int height = layer.tiles.front()->logical_tile()->getHeight() << 9;
			new_layer->setOrder(base_order + height);
			new_layer->setTexture(layer.tiles.front()->tile()->getTexture());
			++base_order;
		
			for(auto hex_obj : layer.tiles) {
				hex_obj->render(&layer.coords);
			}
		}

		for(auto& layer : sorted_map) {
			if(layer.map_layer == nullptr) {
				continue;
			}
			for(auto hex_obj : layer.tiles) {
				hex_obj->renderAdjacent(&sorted_map);
			}
		}

		for(auto& layer : sorted_map) {
			if(layer.map_layer == nullptr) {
				continue;
			}
			auto& new_layer = layer.map_layer;
			layers_.emplace_back(new_layer);
			attachObject(new_layer);
			new_layer->updateAttributes(&layer.coords);
		}

		// Create a map of overlays based on the same texture.
		std::map<unsigned int, std::vector<OverlayHelper>> overlay_map;
		rng::set_seed(hex_tile_seed);
		for(auto& t : tiles) {
			for(auto& tag : t.logical_tile()->getTags()) {
				std::string tag_name = tag;
				std::string tag_sub = "default";
				auto pos = tag.find('^');
				if(pos != std::string::npos) {
					tag_name = tag.substr(0, pos);
					tag_sub = tag.substr(pos + 1);
				}
				auto ov = Overlay::getOverlay(tag_name);
				unsigned int tex_id = ov->getTexture()->id();
				auto it = overlay_map.find(tex_id);
				overlay_map[tex_id].emplace_back(&ov->getAlternative(tag_sub), &t, ov->getTexture());
			}
		}

		base_order = 0x100000;
		overlay_.clear();
		for(const auto& om : overlay_map) {
			auto new_layer = std::make_shared<MapLayer>();
			new_layer->setTexture(om.second.front().tex_);
			std::vector<KRE::vertex_texcoord> coords;
			for(auto& ov : om.second) {
				new_layer->setOrder(base_order);
				ov.obj_->renderOverlay(*ov.x_, ov.tex_, &coords);
			}
			new_layer->updateAttributes(&coords);
			attachObject(new_layer);
			++base_order;
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
	
	void MapLayer::preRender(const KRE::WindowPtr& wnd)
	{
	}

	void MapLayer::updateAttributes(std::vector<KRE::vertex_texcoord>* attrs)
	{
		attr_->update(attrs);
	}
}
