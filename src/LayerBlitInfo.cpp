#include "DisplayDevice.hpp"
#include "LayerBlitInfo.hpp"

LayerBlitInfo::LayerBlitInfo()
	: KRE::SceneObject("layer_blit_info"),
	  xbase_(0),
	  ybase_(0),
	  initialised_(false)
{
	using namespace KRE;

	auto ab = DisplayDevice::createAttributeSet(true, false, false);
	opaques_ = std::make_shared<Attribute<tile_corner>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
	opaques_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::UNSIGNED_SHORT, false, sizeof(tile_corner), offsetof(tile_corner, vertex)));
	opaques_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(tile_corner), offsetof(tile_corner, uv)));
	ab->addAttribute(opaques_);

	transparent_ = std::make_shared<Attribute<tile_corner>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
	transparent_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::UNSIGNED_SHORT, false, sizeof(tile_corner), offsetof(tile_corner, vertex)));
	transparent_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(tile_corner), offsetof(tile_corner, uv)));
	ab->addAttribute(transparent_);

	ab->setDrawMode(DrawMode::TRIANGLES);
	addAttributeSet(ab);
}

void LayerBlitInfo::addTextureToList(KRE::TexturePtr tex)
{
	tex_list_.emplace_back(tex);
}

void LayerBlitInfo::setVertices(std::vector<tile_corner>* op, std::vector<tile_corner>* tr)
{
	opaques_->update(op);
	transparent_->update(tr);
}
