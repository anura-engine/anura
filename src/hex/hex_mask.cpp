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

#include "hex.hpp"
#include "hex_helper.hpp"
#include "hex_mask.hpp"
#include "hex_tile.hpp"

#include "Shaders.hpp"
#include "SceneGraph.hpp"
#include "TextureObject.hpp"
#include "variant.hpp"
#include "WindowManager.hpp"

#include "kre/RenderTarget.hpp"

namespace hex
{
	using namespace KRE;

	MaskNode::MaskNode(const variant& node)
	  : graphics::SceneObjectCallable(node), 
		id_(node["id"].as_string_default("")), 
		attr_(nullptr),
		locs_(),
		changed_(false),
		rt_(nullptr),
		area_(),
		uv_()
	{
		std::vector<point> v;
		if(node.has_key("locs")) {
			for(auto item : node["locs"].as_list()) {
				v.emplace_back(item);
			}
		}

		setLocs(&v);

		setShader(KRE::ShaderProgram::getSystemDefault());

		auto as = KRE::DisplayDevice::createAttributeSet(true, false, false);
		as->setDrawMode(KRE::DrawMode::TRIANGLE_STRIP);

		attr_ = std::make_shared<Attribute<vertex_texcoord>>(AccessFreqHint::DYNAMIC);
		attr_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
		attr_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));

		as->addAttribute(attr_);
		addAttributeSet(as);

		std::vector<int> borders;
		auto tex = get_terrain_texture("alphamask", &area_, &borders);
		ASSERT_LOG(tex != nullptr, "No texture for value 'alphamask'.");
		setTexture(tex);
		uv_ = tex->getTextureCoords(0, area_);
	}

	MaskNodePtr MaskNode::create(const variant& node)
	{
		return MaskNodePtr(new MaskNode(node));
	}

	void MaskNode::process()
	{
		if(changed_) {
			update();
			changed_ = false;
		}
	}

	void MaskNode::setLocs(std::vector<point>* locs)
	{
		locs_.swap(*locs);
		changed_ = true;
	}

	void MaskNode::update()
	{
		std::vector<KRE::vertex_texcoord> coords;

		for(auto it = locs_.cbegin(); it != locs_.cend(); ++it) {
			auto& loc = *it;
			point p = get_pixel_pos_from_tile_pos_evenq(loc, g_hex_tile_size);

			if(it != locs_.cbegin()) {
				// degenerate
				coords.emplace_back(glm::vec2(p.x, p.y), glm::vec2(uv_.x1(), uv_.y1()));
			}
			coords.emplace_back(glm::vec2(p.x, p.y), glm::vec2(uv_.x1(), uv_.y1()));
			coords.emplace_back(glm::vec2(p.x + area_.w(), p.y), glm::vec2(uv_.x2(), uv_.y1()));
			coords.emplace_back(glm::vec2(p.x, p.y + area_.h()), glm::vec2(uv_.x1(), uv_.y2()));
			coords.emplace_back(glm::vec2(p.x + area_.w(), p.y + area_.h()), glm::vec2(uv_.x2(), uv_.y2()));
			if((it + 1) != locs_.cend()) {
				// degenerate
				coords.emplace_back(glm::vec2(p.x + area_.w(), p.y + area_.h()), glm::vec2(uv_.x2(), uv_.y2()));
			}
		}

		getAttributeSet().back()->setCount(coords.size());
		attr_->update(&coords);
	}

	BEGIN_DEFINE_CALLABLE(MaskNode, graphics::SceneObjectCallable);
	DEFINE_FIELD(id, "string")
		return variant(obj.id_);
	DEFINE_FIELD(texture, "null|builtin texture_object")
		if(obj.rt_.get()) {
			return variant(new TextureObject(obj.rt_->getTexture()));
		} else {
			return variant();
		}
	DEFINE_FIELD(locs, "[[int,int]]")
		std::vector<variant> res;
		for(const auto& loc : obj.locs_) {
			variant v = loc.write();
			res.emplace_back(v);
		}
		return variant(&res);
	DEFINE_SET_FIELD
		std::vector<point> v;
		for(auto item : value.as_list()) {
			v.emplace_back(item);
		}
		obj.setLocs(&v);

	END_DEFINE_CALLABLE(MaskNode);
}
