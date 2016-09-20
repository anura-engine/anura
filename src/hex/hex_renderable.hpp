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

#pragma once

#include "AttributeSet.hpp"
#include "Blittable.hpp"
#include "SceneNode.hpp"
#include "SceneObject.hpp"

#include "hex_fwd.hpp"
#include "hex_renderable_fwd.hpp"
#include "rect_renderable.hpp"

namespace hex
{
	class MapNode : public KRE::SceneNode
	{
	public:
		explicit MapNode(std::weak_ptr<KRE::SceneGraph> sg, const variant& node);
		void update(int width, int height, const std::vector<HexObject>& tiles);
		static MapNodePtr create(std::weak_ptr<KRE::SceneGraph> sg, const variant& node);
	private:
		void notifyNodeAttached(std::weak_ptr<SceneNode> parent) override;
		
		std::vector<MapLayerPtr> layers_;
		std::shared_ptr<RectRenderable> rr_;

		bool changed_;

		MapNode() = delete;
		MapNode(const MapNode&) = delete;
		void operator=(const MapNode&) = delete;
	};
	
	class MapLayer : public KRE::SceneObject
	{
	public:
		MapLayer();
		virtual ~MapLayer() {}
		void updateAttributes(std::vector<KRE::vertex_texcoord>* attrs);
		void clearAttributes() { attr_->clear(); }
	private:
		std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> attr_;
	};

	class AnimatedMapLayer : public MapLayer
	{
	public:
		AnimatedMapLayer();
		void preRender(const KRE::WindowPtr& wnd) override;
		void addAnimationSeq(const std::vector<std::string>& frames, const point& hex_pos);
		void setAnimationTiming(int frame_time) { timing_ = frame_time; }
		void setCrop(const rect& r) { crop_rect_ = r; }
		void setBCO(const point& b, const point& c, const point& o) { base_ = b; center_ = c; offset_ = o; }
	private:
		struct AnimFrame
		{
			AnimFrame(const rect& a, std::vector<int>& b) : area(a), borders(b) {}
			rect area;
			std::vector<int> borders;
		};
		std::map<point, std::vector<AnimFrame>> frames_;
		rect crop_rect_;
		int timing_;
		int current_frame_pos_;
		point base_;
		point center_;
		point offset_;
		std::shared_ptr<KRE::Blittable> mask_;
		rectf alpha_uv_;
	};

	struct MapRenderParams
	{
		MapRenderParams() : map_layer(nullptr), coords(), tiles() {}
		std::shared_ptr<MapLayer> map_layer;
		std::vector<KRE::vertex_texcoord> coords;
		std::vector<const HexObject*> tiles;
	};
}
