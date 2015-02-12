#include "DisplayDevice.hpp"
#include "LayerBlitInfo.hpp"

LayerBlitInfo::LayerBlitInfo()
	: KRE::SceneObject("layer_blit_info"),
	  xbase_(0),
	  ybase_(0),
	  initialised_(false)
{
	using namespace KRE;

	auto ab = DisplayDevice::createAttributeSet(false, false, false);
	auto pc = std::make_shared<Attribute<vertex_color>>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
	pc->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
	pc->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));
	ab->addAttribute(pc);
	ab->setDrawMode(DrawMode::TRIANGLES);
	addAttributeSet(ab);
}
