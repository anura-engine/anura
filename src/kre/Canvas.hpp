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

#include <stack>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "CameraObject.hpp"
#include "Color.hpp"
#include "geometry.hpp"
#include "ModelMatrixScope.hpp"
#include "Texture.hpp"
#include "Util.hpp"
#include "VGraph.hpp"
#include "SceneUtil.hpp"
#include "WindowManager.hpp"

namespace KRE
{
	class Canvas;
	typedef std::shared_ptr<Canvas> CanvasPtr;

	enum class CanvasBlitFlags {
		NONE				= 0,
		FLIP_HORIZONTAL		= 1,
		FLIP_VERTICAL		= 2,
	};

	inline CanvasBlitFlags operator|(CanvasBlitFlags lhs, CanvasBlitFlags rhs) {
		return static_cast<CanvasBlitFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
	}
	
	inline bool operator&(CanvasBlitFlags lhs, CanvasBlitFlags rhs) {
		return (static_cast<int>(lhs) & static_cast<int>(rhs)) == static_cast<int>(rhs);
	}

	// A 2D canvas class for drawing on. Not in the renderable pipelines.
	// Canvas writes are done in the order in the code.
	// Intended for making things like UI's.
	class Canvas
	{
	public:
		virtual ~Canvas();

		void setDimensions(unsigned w, unsigned h);

		unsigned width() const { return width_; }
		unsigned height() const { return height_; }

		// Blit's a texture from co-ordinates given in src to the screen co-ordinates dst
		virtual void blitTexture(const TexturePtr& tex, const rect& src, float rotation, const rect& dst, const Color& color=Color::colorWhite(), CanvasBlitFlags flags=CanvasBlitFlags::NONE) const = 0;
		virtual void blitTexture(const TexturePtr& tex, const std::vector<vertex_texcoord>& vtc, float rotation, const Color& color=Color::colorWhite()) = 0;
		// Blit a texture to the given co-ordinates on the display. Assumes the whole texture is being used.
		void blitTexture(const TexturePtr& tex, float rotation, const rect& dst, const Color& color=Color::colorWhite()) const;
		void blitTexture(const TexturePtr& tex, float rotation, int x, int y, const Color& color=Color::colorWhite()) const;

		virtual void drawSolidRect(const rect& r, const Color& fill_color, const Color& stroke_color, float rotate=0) const = 0;
		virtual void drawSolidRect(const rect& r, const Color& fill_color, float rotate=0) const = 0;
		virtual void drawHollowRect(const rect& r, const Color& stroke_color, float rotate=0) const = 0;
		virtual void drawLine(const point& p1, const point& p2, const Color& color) const = 0;
		virtual void drawLines(const std::vector<glm::vec2>& varray, float line_width, const Color& color=Color::colorWhite()) const = 0;
		virtual void drawLines(const std::vector<glm::vec2>& varray, float line_width, const std::vector<glm::u8vec4>& carray) const = 0;
		virtual void drawLineStrip(const std::vector<glm::vec2>& points, float line_width, const Color& color) const = 0;
		virtual void drawLineLoop(const std::vector<glm::vec2>& varray, float line_width, const Color& color) const = 0;
		virtual void drawLine(const pointf& p1, const pointf& p2, const Color& color) const = 0;
		// Draw filled polygon (i.e. triangle fan) using given color	
		// Should add a version taking fill and stroke color.
		virtual void drawPolygon(const std::vector<glm::vec2>& points, const Color& color=Color::colorWhite()) const = 0;

		virtual void drawSolidCircle(const point& centre, float radius, const Color& color=Color::colorWhite()) const = 0;
		virtual void drawSolidCircle(const point& centre, float radius, const std::vector<glm::u8vec4>& color) const = 0;
		virtual void drawSolidCircle(const pointf& centre, float radius, const Color& color=Color::colorWhite()) const = 0;
		virtual void drawSolidCircle(const pointf& centre, float radius, const std::vector<glm::u8vec4>& color) const = 0;

		virtual void drawHollowCircle(const point& centre, float outer_radius, float inner_radius, const Color& color=Color::colorWhite()) const = 0;
		virtual void drawHollowCircle(const pointf& centre, float outer_radius, float inner_radius, const Color& color=Color::colorWhite()) const = 0;

		virtual void drawPoints(const std::vector<glm::vec2>& points, float radius, const Color& color=Color::colorWhite()) const = 0;

		void drawVectorContext(const Vector::ContextPtr& context);

		static CanvasPtr getInstance();

		struct ColorManager
		{
			ColorManager(const Color& color) : canvas_(KRE::Canvas::getInstance()) {
				canvas_->color_stack_.push(color);
			}
			~ColorManager() {
				canvas_->color_stack_.pop();
			}
			CanvasPtr canvas_;
		};

		struct CameraScope
		{
			CameraScope(CameraPtr cam) 
				: canvas_(Canvas::getInstance()), 
				  saved_cam_(canvas_->camera_)
			{
				canvas_->camera_ = cam;
			}
			~CameraScope()
			{
				canvas_->camera_ = saved_cam_;
			}
			CanvasPtr canvas_;
			CameraPtr saved_cam_;
		};

		struct ShaderScope
		{
			ShaderScope(ShaderProgramPtr shader) : canvas_(KRE::Canvas::getInstance()) {
				canvas_->shader_stack_.push(shader);
			}
			~ShaderScope()
			{
				canvas_->shader_stack_.pop();
			}
			CanvasPtr canvas_;
		};

		const Color getColor() const {
			if(color_stack_.empty()) {
				return Color::colorWhite();
			}
			return color_stack_.top();
		}

		WindowPtr getWindow() const;
		void setWindow(WindowPtr wnd);

		const glm::mat4& getPVMatrix() const { return pv_; }

		const CameraPtr& getCamera() const { return camera_; }

		ShaderProgramPtr getCurrentShader() const {
			if(shader_stack_.empty()) {
				return ShaderProgram::getSystemDefault();
			}
			return shader_stack_.top();
		}
	protected:
		Canvas();
	private:
		DISALLOW_COPY_AND_ASSIGN(Canvas);
		unsigned width_;
		unsigned height_;
		virtual void handleDimensionsChanged() = 0;
		std::stack<Color> color_stack_;
		std::stack<ShaderProgramPtr> shader_stack_;
		mutable glm::mat4 model_matrix_;
		mutable bool model_changed_;
		std::weak_ptr<Window> window_;
		int size_change_key_;
		CameraPtr camera_;
		glm::mat4 pv_;
	};

	// Helper function to generate a color wheel between the given hue values.
	// Generated color values can be passed as input to the appropriate drawSolidCircle() version.
	void generate_color_wheel(int num_points, std::vector<glm::u8vec4>* color_array, const Color& centre=Color::colorWhite(), float start_hue=0.0f, float end_hue=1.0f);
}
