#include "AttributeSet.hpp"
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
	opaques_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::SHORT, false, sizeof(tile_corner), offsetof(tile_corner, vertex)));
	opaques_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(tile_corner), offsetof(tile_corner, uv)));
	ab->addAttribute(opaques_);	
	ab->setDrawMode(DrawMode::TRIANGLES);
	addAttributeSet(ab);

	auto tab = DisplayDevice::createAttributeSet(true, false, false);
	transparent_ = std::make_shared<Attribute<tile_corner>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
	transparent_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::SHORT, false, sizeof(tile_corner), offsetof(tile_corner, vertex)));
	transparent_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(tile_corner), offsetof(tile_corner, uv)));
	tab->addAttribute(transparent_);

	tab->setDrawMode(DrawMode::TRIANGLES);
	addAttributeSet(tab);
}

void LayerBlitInfo::setVertices(std::vector<tile_corner>* op, std::vector<tile_corner>* tr)
{
	//LOG_DEBUG("Adding " << op->size() << " opaque vertices");
	if(op != nullptr) {
		getAttributeSet()[0]->setCount(op->size());
		opaques_->update(op);
	}
	//LOG_DEBUG("Adding " << tr->size() << " transparent vertices");
	if(tr != nullptr) {
		getAttributeSet()[1]->setCount(tr->size());
		transparent_->update(tr);
	}
}
