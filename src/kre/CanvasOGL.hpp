/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "AlignedAllocator.hpp"
#include "Canvas.hpp"

namespace KRE
{
	class CanvasOGL : public Canvas, public AlignedAllocator16
	{
	public:
		CanvasOGL();
		virtual ~CanvasOGL();

		// Blit's a texture from co-ordinates given in src to the screen co-ordinates dst
		virtual void blitTexture(const TexturePtr& tex, const rect& src, float rotation, const rect& dst, const Color& color, CanvasBlitFlags flags) const override;
		virtual void blitTexture(const TexturePtr& tex, const std::vector<vertex_texcoord>& vtc, float rotation, const Color& color) override;
		// Blit a texture to the given co-ordinates on the display. Assumes the whole texture is being used.
		void blitTexture(const TexturePtr& tex, float rotation, const rect& dst, const Color& color) const;
		void blitTexture(const TexturePtr& tex, float rotation, int x, int y, const Color& color) const;

		void drawSolidRect(const rect& r, const Color& fill_color, const Color& stroke_color, float rotate=0) const override;
		void drawSolidRect(const rect& r, const Color& fill_color, float rotate=0) const override;
		void drawHollowRect(const rect& r, const Color& stroke_color, float rotate=0) const override;
		void drawLine(const point& p1, const point& p2, const Color& color) const override;
		void drawLines(const std::vector<glm::vec2>& varray, float line_width, const Color& color) const override;
		void drawLines(const std::vector<glm::vec2>& varray, float line_width, const std::vector<glm::u8vec4>& carray) const override;
		void drawLineStrip(const std::vector<glm::vec2>& points, float line_width, const Color& color) const override;
		void drawLineLoop(const std::vector<glm::vec2>& varray, float line_width, const Color& color) const override;
		void drawLine(const pointf& p1, const pointf& p2, const Color& color) const override;
		// Draw filled polygon (i.e. triangle fan) using given color	
		void drawPolygon(const std::vector<glm::vec2>& points, const Color& color) const override;

		void drawSolidCircle(const point& centre, float radius, const Color& color) const override;
		void drawSolidCircle(const point& centre, float radius, const std::vector<glm::u8vec4>& color) const override;
		void drawSolidCircle(const pointf& centre, float radius, const Color& color) const override;
		void drawSolidCircle(const pointf& centre, float radius, const std::vector<glm::u8vec4>& color) const override;

		void drawHollowCircle(const point& centre, float outer_radius, float inner_radius, const Color& color) const override;
		void drawHollowCircle(const pointf& centre, float outer_radius, float inner_radius, const Color& color) const override;

		void drawPoints(const std::vector<glm::vec2>& points, float radius, const Color& color=Color::colorWhite()) const override;

		static CanvasPtr getInstance();
	private:
		DISALLOW_COPY_AND_ASSIGN(CanvasOGL);
		void handleDimensionsChanged() override;
	};
}
