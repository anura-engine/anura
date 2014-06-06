/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "Color.hpp"
#include "Geometry.hpp"
#include "Material.hpp"
#include "Util.hpp"
#include "VGraph.hpp"

namespace KRE
{
	class Canvas;
	typedef std::shared_ptr<Canvas> CanvasPtr;

	// A 2D canvas class for drawing on. Not in the renderable pipelines.
	// Canvas writes are done in the order in the code.
	// Intended for making things like UI's.
	class Canvas
	{
	public:
		virtual ~Canvas();

		void SetDimensions(unsigned w, unsigned h);

		unsigned Width() const { return width_; }
		unsigned Height() const { return height_; }

		// Blit's a texture from co-ordinates given in src to the screen co-ordinates dst
		virtual void BlitTexture(const TexturePtr& tex, const rect& src, float rotation, const rect& dst, const Color& color) = 0;

		// Blit's a material from internal co-ordinates to destination screen co-ordinates.
		virtual void BlitTexture(const MaterialPtr& mat, float rotation, const rect& dst, const Color& color) = 0;

		//void DrawVectorContext(const Vector::ContextPtr& context);
		static CanvasPtr GetInstance();
	protected:
		Canvas();
	private:
		DISALLOW_COPY_AND_ASSIGN(Canvas);
		unsigned width_;
		unsigned height_;
		virtual void HandleDimensionsChanged() = 0;
	};
}
