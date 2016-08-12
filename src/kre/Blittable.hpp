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

#include "geometry.hpp"
#include "AttributeSet.hpp"
#include "Texture.hpp"
#include "SceneObject.hpp"

namespace KRE
{
	// This is basically a helper class that lets you blit a 
	// texture to the screen in what should be a relatively 
	// optimised manner.
	class Blittable : public SceneObject
	{
	public:
		enum class Centre {
			MIDDLE,
			TOP_LEFT,
			TOP_RIGHT,
			BOTTOM_LEFT,
			BOTTOM_RIGHT,
			MANUAL,
		};
		Blittable();
		explicit Blittable(const variant& node);
		explicit Blittable(const TexturePtr& tex);
		virtual ~Blittable();

		template<typename T>
		void setDrawRect(const geometry::Rect<T>& r) {
			draw_rect_ = r.template as_type<float>();
			changed_ = true;
		}

		void preRender(const WindowPtr& wm) override;

		Centre getCentre() const { return centre_; }
		void setCentre(Centre c);
		const pointf& getCentreCoords() const { return centre_offset_; }
		template<typename T>
		void setCentreCoords(const geometry::Point<T>& p) {
			centre_offset_ = p;
			centre_ = Centre::MANUAL;
			changed_ = true;
		}
		void update(std::vector<vertex_texcoord>* queue);
		void setDrawMode(DrawMode mode);
		void setMirrorHoriz(bool mirrorh) { horizontal_mirrored_ = mirrorh; changed_ = true; }
		void setMirrorVert(bool mirrorv) { vertical_mirrored_ = mirrorv; changed_ = true; }
	protected:
		void setChanged() const { changed_ = true; }
	private:
		void init();
		virtual void onTextureChanged() override { changed_ = true; }

		std::shared_ptr<Attribute<vertex_texcoord>> attribs_;
		rectf draw_rect_;
		pointf centre_offset_;
		Centre centre_;
		mutable bool changed_;
		bool horizontal_mirrored_;
		bool vertical_mirrored_;
	};
}
