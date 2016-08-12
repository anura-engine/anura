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
#include "hex_mask.hpp"
#include "hex_object.hpp"
#include "hex_renderable.hpp"
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
  : graphics::SceneObjectCallable(node), id_(node["id"].as_string_default("")), changed_(false)
{
	std::vector<int> v;
	if(node.has_key("locs")) {
		for(auto item : node["locs"].as_list()) {
			for(int i : item.as_list_int()) {
				v.push_back(i);
			}
		}
	}

	setLocs(v);
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

void MaskNode::setLocs(const std::vector<int>& locs)
{
	locs_ = locs;
	changed_ = true;
}

void MaskNode::update()
{
	clearAttributeSets();

	setShader(KRE::ShaderProgram::getSystemDefault());

	auto as = KRE::DisplayDevice::createAttributeSet(true, false, false);
	as->setDrawMode(KRE::DrawMode::TRIANGLES);


	attr_ = std::make_shared<Attribute<vertex_texcoord>>(AccessFreqHint::DYNAMIC);
	attr_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
	attr_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));

	as->addAttribute(attr_);
	addAttributeSet(as);

	TileTypePtr mask = TileType::factory("mask");
	ASSERT_LOG(mask.get(), "No 'mask' hex type");

	setTexture(mask->getTexture());

	std::vector<KRE::vertex_texcoord> coords;

	for(int i = 0; i < static_cast<int>(locs_.size())-1; i += 2) {
		mask->render(locs_[i], locs_[i+1], &coords);
	}

	attr_->update(coords);
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
	for(int i = 0; i < static_cast<int>(obj.locs_.size())-1; i += 2) {
		std::vector<variant> v;
		v.push_back(variant(obj.locs_[i]));
		v.push_back(variant(obj.locs_[i+1]));
		res.push_back(variant(&v));
	}

	return variant(&res);
DEFINE_SET_FIELD
	std::vector<int> v;
	for(auto item : value.as_list()) {
		for(int i : item.as_list_int()) {
			v.push_back(i);
		}
	}

	obj.setLocs(v);

END_DEFINE_CALLABLE(MaskNode);

}
