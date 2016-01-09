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
#include "SceneNode.hpp"
#include "SceneObject.hpp"
#include "SceneObjectCallable.hpp"

#include "hex_fwd.hpp"
#include "hex_renderable_fwd.hpp"

namespace hex
{
	class MaskNode : public graphics::SceneObjectCallable
	{
	public:
		explicit MaskNode(const variant& node);
		void setLocs(const std::vector<int>& locs);

		static MaskNodePtr create(const variant& node);

		void process();

		void setRenderTarget(KRE::RenderTargetPtr rt) { rt_ = rt; }
		KRE::RenderTargetPtr getRenderTarget() const { return rt_; }

	private:
		void update();

		std::string id_;
		std::shared_ptr<KRE::Attribute<KRE::vertex_texcoord>> attr_;

		std::vector<int> locs_;
		bool changed_;

		KRE::RenderTargetPtr rt_;

		DECLARE_CALLABLE(MaskNode);

		MaskNode() = delete;
		MaskNode(const MaskNode&) = delete;
		void operator=(const MaskNode&) = delete;
	};
}
