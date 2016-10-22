// XXX needs fixing
#if 0
/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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
#include <boost/array.hpp>
#include <boost/shared_array.hpp>
#include "intrusive_ptr.hpp"

#include <assert.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <numeric>
#include <map>
#include <vector>

#include "SDL.h"

#include "asserts.hpp"
#include "border_widget.hpp"
#include "button.hpp"
#include "checkbox.hpp"
#include "color_picker.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "grid_widget.hpp"
#include "gui_section.hpp"
#include "image_widget.hpp"
#include "input.hpp"
#include "json_parser.hpp"
#include "label.hpp"
#include "level_runner.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "slider.hpp"
#include "surface_utils.hpp"
#include "unit_test.hpp"

namespace
{
	struct rgb
	{
		uint8_t r, g, b;
	};

	struct hsv
	{
		uint8_t h, s, v;
	};

	hsv rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b)
	{
		hsv out;
		uint8_t min_color, max_color, delta;

		min_color = std::min(r, std::min(g, b));
		max_color = std::max(r, std::max(g, b));

		delta = max_color - min_color;
		out.v = max_color;
		if(out.v == 0) {
			out.s = 0;
			out.h = 0;
			return out;
		}

		out.s = uint8_t(255.0 * delta / out.v);
		if(out.s == 0) {
			out.h = 0;
			return out;
		}

		if(r == max_color) {
			out.h = uint8_t(43.0 * (g-b)/delta);
		} else if(g == max_color) {
			out.h = 85 + uint8_t(43.0 * (b-r)/delta);
		} else {
			out.h = 171 + uint8_t(43.0 * (r-g)/delta);
		}
		return out;
	}

	rgb hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v)
	{
		rgb out;
		uint8_t region, remainder, p, q, t;

		if(s == 0) {
			out.r = out.g = out.b = v;
		} else {
			region = h / 43;
			remainder = (h - (region * 43)) * 6; 

			p = (v * (255 - s)) >> 8;
			q = (v * (255 - ((s * remainder) >> 8))) >> 8;
			t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

			switch(region)
			{
				case 0:  out.r = v; out.g = t; out.b = p; break;
				case 1:  out.r = q; out.g = v; out.b = p; break;
				case 2:  out.r = p; out.g = v; out.b = t; break;
				case 3:  out.r = p; out.g = q; out.b = v; break;
				case 4:  out.r = t; out.g = p; out.b = v; break;
				default: out.r = v; out.g = p; out.b = q; break;
			}
		}
		return out;
	}

	template<typename T>
	uint8_t clamp_u8(T a)
	{
		if(a < 0) {
			return 0;
		} else if(a > T(255)) {
			return 255;
		}
		return uint8_t(a);
	}

	void calculate_normal(SDL_Surface* s, double gs_param[3])
	{
		ASSERT_LOG(s != nullptr, "FATAL: Invalid surface");

		graphics::setAlpha_for_transparent_colors_in_rgba_surface(s, 0);

		// convert to array of values (from hsv)
		const int sz = s->w * s->h;
		std::vector<hsv> hsv_ary;
		hsv_ary.resize(sz);
		uint8_t* pixels = reinterpret_cast<uint8_t*>(s->pixels);
		for(int n = 0; n != sz; ++n) {
			if(pixels[3] != 0) {
				hsv_ary[n] = rgb_to_hsv(pixels[0], pixels[1], pixels[2]);
			}
			pixels += 4;
		}

		// convert to grayscale
		std::vector<uint8_t> gs_ary;
		gs_ary.resize(sz);
		pixels = reinterpret_cast<uint8_t*>(s->pixels);
		for(int n = 0; n != sz; ++n) {
			if(pixels[3] != 0) {
				gs_ary[n] = clamp_u8(gs_param[0]*pixels[0]+gs_param[1]*pixels[1]+gs_param[2]*pixels[2]);
			}
			pixels += 4;
		}

		// run sobel convolution on data, writing data into vert/horz
		std::vector<uint8_t> vert(sz,0);
		std::vector<uint8_t> horz(sz,0);
		/*for(int m = 1; m != s->h-1; ++m) {
			for(int n = 1; n != s->w-1; ++n) {
				int ndx = m*s->w + n;
				vert[ndx] = clamp_u8(gs_ary[(m-1)*s->w+n-1]*1 + gs_ary[(m-1)*s->w+n]*2 + gs_ary[(m-1)*s->w+n+1]*1 
					- gs_ary[(m+1)*s->w+n-1]*1 - gs_ary[(m+1)*s->w+n]*2 - gs_ary[(m+1)*s->w+n+1]*1);
				horz[ndx] = clamp_u8(gs_ary[(m-1)*s->w+n-1]*1 - gs_ary[(m-1)*s->w+n+1]*1 
					+ gs_ary[(m)*s->w+n-1]*2 - gs_ary[(m)*s->w+n+1]*2 
					+ gs_ary[(m+1)*s->w+n-1]*1 - gs_ary[(m+1)*s->w+n+1]*1);
			}
		}*/
		// emboss [1,1,1][0,0,0][-1,-1,-1]
		const int bias = 128;
		for(int m = 1; m != s->h-1; ++m) {
			for(int n = 1; n != s->w-1; ++n) {
				int ndx = m*s->w + n;
				//vert[ndx] = clamp_u8(gs_ary[(m-1)*s->w+n-1]*2 + gs_ary[ndx]*-1 + gs_ary[(m+1)*s->w+n+1]*-1);
				//horz[ndx] = clamp_u8(gs_ary[(m-1)*s->w+n+1]*2 + gs_ary[ndx]*-1 + gs_ary[(m+1)*s->w+n-1]*-1);
				vert[ndx] = clamp_u8(gs_ary[(m-1)*s->w+n-1]*-1 + gs_ary[(m-1)*s->w+n]*-1 + gs_ary[(m-1)*s->w+n+1]*-1 
					- gs_ary[(m+1)*s->w+n-1]*1 - gs_ary[(m+1)*s->w+n]*1 - gs_ary[(m+1)*s->w+n+1]*1 + bias);
				horz[ndx] = clamp_u8(gs_ary[(m-1)*s->w+n-1]*-1 + gs_ary[(m-1)*s->w+n+1]*1 
					+ gs_ary[(m)*s->w+n-1]*-1 + gs_ary[(m)*s->w+n+1]*1 
					+ gs_ary[(m+1)*s->w+n-1]*-1 + gs_ary[(m+1)*s->w+n+1]*1 + bias);
			}
		}

		pixels = reinterpret_cast<uint8_t*>(s->pixels);
		for(int n = 0; n != sz; ++n) {
			if(pixels[3] != 0) {
				pixels[0] = horz[n];
				pixels[1] = vert[n];
				pixels[2] = clamp_u8(hsv_ary[n].v+bias);
			}
			pixels += 4;
		}
	}

	graphics::surface make_grayscale(const graphics::surface& src, double* gs_param)
	{
		double gs_p[3];
		if(gs_param == nullptr) {
			gs_p[0] = 0.21;
			gs_p[1] = 0.71;
			gs_p[2] = 0.07;
		} else {
			gs_p[0] = gs_param[0];
			gs_p[1] = gs_param[1];
			gs_p[2] = gs_param[2];
		}
		graphics::surface dst = src.clone();
		const uint8_t* s_pixels = reinterpret_cast<const uint8_t*>(src.get()->pixels);
		uint8_t* d_pixels = reinterpret_cast<uint8_t*>(dst.get()->pixels);
		const int sz = src.get()->w * src.get()->h;
		for(int n = 0; n != sz; ++n) {
			if(s_pixels[3] != 0) {
				uint8_t pv = clamp_u8(gs_p[0]*s_pixels[0]+gs_p[1]*s_pixels[1]+gs_p[2]*s_pixels[2]);
				d_pixels[0] = d_pixels[1] = d_pixels[2] = pv;
			}
			s_pixels += 4;
			d_pixels += 4;
		}
		return dst;
	}

	graphics::surface calculate_normal2(const graphics::surface& horz, const graphics::surface& vert)
	{
		ASSERT_LOG(horz.get()->w == vert.get()->w && horz.get()->h == vert.get()->h, "FATAL: NORMAL: Widths or heights don't match");
		graphics::surface res = horz.clone();

		const int sz = horz.get()->w * horz.get()->h;
		uint8_t* pixels = reinterpret_cast<uint8_t*>(res.get()->pixels);
		for(int n = 0; n != sz; ++n) {
			if(pixels[3] != 0) {
				pixels[0] = reinterpret_cast<uint8_t*>(horz.get()->pixels)[n*4];
				pixels[1] = reinterpret_cast<uint8_t*>(vert.get()->pixels)[n*4];
				pixels[2] = clamp_u8(128);
				pixels[3] = 255;
			}
			pixels += 4;
		}
		return res;
	}

	// Generic convolution filter
	// bias is added to all pixels in the output.
	// divisor is the factor to divide the computing pixel value by.
	// for pixels outside of the input range
	//     if clamp == false: use default_color 
    //	   else: use closest pixel value
	// if preserve_alpha: don't saturate the alpha value.
	graphics::surface convolution_filter(const graphics::surface& surf, 
		const std::vector<std::vector<double>>& matrix, 
		int mx, 
		int my, 
		double divisor, 
		double bias, 
		bool preserve_alpha, 
		const graphics::color& default_color, 
		bool clamp)
	{
		graphics::surface res = surf.clone();
		const uint8_t* pixels = reinterpret_cast<const uint8_t*>(surf.get()->pixels);
		uint8_t* res_pixels = reinterpret_cast<uint8_t*>(res.get()->pixels);
		for(int m = 0; m != res->h; ++m) {
			for(int n = 0; n != res->w; ++n) {
				const int ndx = (n + m * res->w) * 4;
				float sum_r = bias;
				float sum_g = bias;
				float sum_b = bias;
				float sum_a = bias;
				for(int j = 0; j != my; ++j) {
					for(int i = 0; i != mx; ++i) {
						int xoffs = i-mx/2;
						int yoffs = j-my/2;

						int quadrant = 0;
						if(n+xoffs < 0) {
							quadrant |= 1;
						} 
						if(n+xoffs >= res->w) {
							quadrant |= 2;
						} 
						if(m+yoffs < 0) { 
							quadrant |= 4;
						} 
						if(m+yoffs >= res->h) {
							quadrant |= 8;
						}
						if(quadrant != 0) {
							if(clamp) {
								sum_r += float(default_color.r()) * matrix[j][i];
								sum_g += float(default_color.g()) * matrix[j][i];
								sum_b += float(default_color.b()) * matrix[j][i];
								sum_a += float(default_color.a()) * matrix[j][i];
								continue;
							}
							if(quadrant & 1) {
								xoffs = 0;
							}
							if(quadrant & 2) {
								xoffs = 0;
							}
							if(quadrant & 4) {
								yoffs = 0;
							}
							if(quadrant & 8) {
								yoffs = 0;
							}
						}
						const int offs = ndx + (xoffs + yoffs*res->w) * 4;

						sum_r += float(pixels[offs+0]) * matrix[j][i];
						sum_g += float(pixels[offs+1]) * matrix[j][i];
						sum_b += float(pixels[offs+2]) * matrix[j][i];
						sum_a += float(pixels[offs+3]) * matrix[j][i];
					}
				}
				res_pixels[ndx+0] = clamp_u8(sum_r / divisor);
				res_pixels[ndx+1] = clamp_u8(sum_g / divisor);
				res_pixels[ndx+2] = clamp_u8(sum_b / divisor);
				if(!preserve_alpha) {
					res_pixels[ndx+3] = clamp_u8(sum_a / divisor);
				}
			}
		}
		return res;
	}

	graphics::surface blur_filter(const graphics::surface& s, int blur_x, int blur_y, int passes)
	{
		// blur filter is special case of a convolution filter with 1.0 co-efficients.
		std::vector<std::vector<double>> blur_matrix;
		blur_matrix.resize(blur_y);
		for(int n = 0; n != blur_y; ++n) {
			blur_matrix[n].resize(blur_x, 1.0);
		}

		graphics::surface res = s.clone();
		for(int ps = 0; ps != passes; ++ps) {
			res = convolution_filter(res, blur_matrix, blur_x, blur_y, double(blur_x)*blur_y, 0, true, graphics::color(0,0,0), true);
		}
		return res;
	}

	graphics::surface alpha_clip(const graphics::surface& src, const graphics::surface& src_with_alpha)
	{
		graphics::surface res = src_with_alpha.clone();
		const int sz = res->w * res->h;
		const uint8_t* s_pixels = reinterpret_cast<const uint8_t*>(src.get()->pixels);
		uint8_t* pixels = reinterpret_cast<uint8_t*>(res.get()->pixels);
		for(int n = 0; n != sz; ++n) {
			if(pixels[3] != 0) {
				pixels[0] = s_pixels[0];
				pixels[1] = s_pixels[1];
				pixels[2] = s_pixels[2];
			}
			pixels += 4;
			s_pixels += 4;
		}
		return res;
	}

	graphics::surface extract_alpha_mask(const graphics::surface& s, 
		const graphics::color& shadow_color, 
		const graphics::color& non_shadow_color)
	{
		graphics::surface mask = s.clone();
		const int sz = mask->w * mask->h;
		uint8_t* pixels = reinterpret_cast<uint8_t*>(mask->pixels);
		for(int n = 0; n != sz; ++n) {
			if(pixels[3] != 255) {
				pixels[0] = shadow_color.r();
				pixels[1] = shadow_color.g();
				pixels[2] = shadow_color.b();
			} else {
				pixels[0] = non_shadow_color.r();
				pixels[1] = non_shadow_color.g();
				pixels[2] = non_shadow_color.b();
			}
			pixels[3] = 255;
			pixels += 4;
		}
		return mask;
	}

	graphics::surface drop_shadow_filter(const graphics::surface& s, 
		const graphics::color& shadow_color, 
		int blur_x, 
		int blur_y, 
		double angle, 
		double distance, 
		double strength,
		bool inner_shadow,
		bool knockout, 
		int passes)
	{
		// 1) clone the new surface and create a mask with a color value of black if there is alpha != 255
		//    else white.
		graphics::surface mask = extract_alpha_mask(s, shadow_color, graphics::color(255,255,255));
		// 2) Scale the surface
		//SDL_Surface* scaled = SDL_CreateRGBSurface(0, mask->w, mask->h, 32, SURFACE_MASK);
		//SDL_FillRect(scaled, nullptr, shadow_color.value());
		//SDL_Rect scaled_rect = {
		//	int(distance/2), int(distance/2), int(mask->w-distance/2), int(mask->h-distance/2)
		//};
		//SDL_BlitScaled(mask, nullptr, scaled, &scaled_rect);

		mask = blur_filter(mask, blur_x, blur_y, passes);

		const int sz = s->w * s->h;
		uint8_t* spixels = reinterpret_cast<uint8_t*>(s->pixels);
		uint8_t* pixels = reinterpret_cast<uint8_t*>(mask->pixels);
		for(int n = 0; n != sz; ++n) {
			if(spixels[3] != 255) {
				pixels[0] = shadow_color.r();
				pixels[1] = shadow_color.g();
				pixels[2] = shadow_color.b();
			}
			pixels += 4;
			spixels += 4;		
		}

		// move the image by angle/distance
		if(distance > 0) {
			// XXX
		}

		return graphics::surface(mask);
	}

	// angle in radians, returns a new surface with the given sobel operator applied
	graphics::surface sobel(const graphics::surface& s, double angle)
	{
		std::vector<std::vector<double>> matrix;
		matrix.resize(3);
		for(int n = 0; n != 3; ++n) {
			matrix[n].resize(3);
		}
		double delta_r = M_PI/4.0;
		matrix[0][0] = cos(angle + 3.0*delta_r);
		matrix[0][1] = cos(angle + 2.0*delta_r);
		matrix[0][2] = cos(angle + delta_r);
		
		matrix[1][0] = cos(delta_r + 4.0*delta_r);
		matrix[1][1] = 0;
		matrix[1][2] = cos(angle);

		matrix[2][0] = cos(angle + 5.0*delta_r);
		matrix[2][1] = cos(angle + 6.0*delta_r);
		matrix[2][2] = cos(angle + 7.0*delta_r);

		return convolution_filter(s, matrix, 3, 3, 1.0, 128.0, true, graphics::color(255,255,255), true);
	}


	// angle in radians, returns a new surface with the given embossing.
	graphics::surface emboss(const graphics::surface& s, double angle)
	{
		std::vector<std::vector<double>> matrix;
		matrix.resize(3);
		for(int n = 0; n != 3; ++n) {
			matrix[n].resize(3);
		}
		double delta_r = M_PI/4.0;
		matrix[0][0] = cos(angle + delta_r);
		matrix[0][1] = cos(angle + 2.0*delta_r);
		matrix[0][2] = cos(angle + 3.0*delta_r);
		
		matrix[1][0] = cos(angle + delta_r);
		matrix[1][1] = 0;
		matrix[1][2] = cos(angle + 4.0*delta_r);

		matrix[2][0] = cos(angle - delta_r);
		matrix[2][1] = cos(angle - 2.0*delta_r);
		matrix[2][2] = cos(angle - 3.0*delta_r);

		return convolution_filter(s, matrix, 3, 3, 1.0, 128.0, true, graphics::color(255,255,255), false);
	}

	// a is the base layer, b is the top layer.
	graphics::surface blend_overlay(const graphics::surface& src_a, const graphics::surface& src_b, double opacity)
	{
		graphics::surface res = src_a.clone();
		const int sz = res->w * res->h;
		const uint8_t* a_pixels = reinterpret_cast<const uint8_t*>(src_a.get()->pixels);
		const uint8_t* b_pixels = reinterpret_cast<const uint8_t*>(src_b.get()->pixels);
		uint8_t* pixels = reinterpret_cast<uint8_t*>(res.get()->pixels);
		for(int n = 0; n != sz; ++n) {
			hsv a_hsv = rgb_to_hsv(a_pixels[0], a_pixels[1], a_pixels[2]);
			if(a_hsv.v < 0.5) {
				pixels[0] = clamp_u8(2*a_pixels[0]*b_pixels[0]);
				pixels[1] = clamp_u8(2*a_pixels[1]*b_pixels[1]);
				pixels[2] = clamp_u8(2*a_pixels[2]*b_pixels[2]);
				pixels[3] = clamp_u8(2*a_pixels[3]*b_pixels[3]);
			} else {
				pixels[0] = 255-clamp_u8(2*(255-a_pixels[0])*(255-b_pixels[0]));
				pixels[1] = 255-clamp_u8(2*(255-a_pixels[1])*(255-b_pixels[1]));
				pixels[2] = 255-clamp_u8(2*(255-a_pixels[2])*(255-b_pixels[2]));
				pixels[3] = 255-clamp_u8(2*(255-a_pixels[3])*(255-b_pixels[3]));
			}

			pixels += 4;
			a_pixels += 4;
			b_pixels += 4;
		}
		return res;
	}

	struct manager
	{
		manager(gles2::program_ptr shader) 
		{
			glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
			glUseProgram(shader->get());
		}
		~manager()
		{
			glUseProgram(old_program);
		}
		GLint old_program;
	};


	class ImageWidget_lighted : public gui::ImageWidget
	{
	public:
		explicit ImageWidget_lighted(graphics::texture tex, graphics::texture tex_normal, int w, int h) 
			: ImageWidget(tex, w, h), tex_normal_(tex_normal) {
			shader_ = gles2::shader_program::get_global("texture_2d_lighted")->shader();
			u_mvp_matrix_ = shader_->get_fixed_uniform("mvp_matrix");

			u_tex_map_ = shader_->get_fixed_uniform("tex_map");
			u_normal_map_ = shader_->get_fixed_uniform("normal_map");

			u_resolution_ = shader_->get_fixed_uniform("resolution");

			lighting_.reset(new graphics::lighting(shader_));
			lighting_->set_light_position(0, glm::vec3(0.5f, 0.5f, 0.07f));
			lighting_->set_ambient_color(0, glm::vec3(1.0f, 1.0f, 1.0f));
			lighting_->set_light_color(0, glm::vec3(1.0f, 0.8f, 0.8f));			
			lighting_->set_ambient_intensity(0, 0.2f);
			lighting_->enable_light_source(0, true);

			set_uniforms();

			setFrameSet("empty_window");
		}
		virtual ~ImageWidget_lighted() {
		}
		void set_uniforms() const {
			manager m(shader_);
			glUniform1i(u_tex_map_, 0);
			glUniform1i(u_normal_map_, 1);
			glUniform2f(u_resolution_, GLfloat(width()), GLfloat(height()));
		}
	protected:
		virtual void handleDraw() const {
			manager m(shader_);

			GLint cur_id = graphics::texture::get_currentTexture();

			if(lighting_) {
				lighting_->set_all_uniforms();
			}

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, tex_normal_.getId());
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tex().getId());

			const int w_odd = width() % 2;
			const int h_odd = height() % 2;
			const int w = width() / 2;
			const int h = height() / 2;

			glPushMatrix();
			glTranslatef((x()+w)&preferences::xypos_draw_mask, (y()+h)&preferences::xypos_draw_mask, 0.0f);
			glUniformMatrix4fv(u_mvp_matrix_, 1, GL_FALSE, glm::value_ptr(gles2::get_mvp_matrix()));

			const GLfloat varray[] = {
				(GLfloat)-w, (GLfloat)-h,
				(GLfloat)-w, (GLfloat)h+h_odd,
				(GLfloat)w+w_odd, (GLfloat)-h,
				(GLfloat)w+w_odd, (GLfloat)h+h_odd
			};

			GLfloat tcx, tcy, tcx2, tcy2;
			if(area().w() == 0) {
				tcx = graphics::texture::get_coord_x(0.0);
				tcy = graphics::texture::get_coord_y(0.0);
				tcx2 = graphics::texture::get_coord_x(1.0);
				tcy2 = graphics::texture::get_coord_y(1.0);
			} else {
				tcx = GLfloat(area().x())/tex().width();
				tcy = GLfloat(area().y())/tex().height();
				tcx2 = GLfloat(area().x2())/tex().width();
				tcy2 = GLfloat(area().y2())/tex().height();
			}

			const GLfloat tcarray[] = {
				tcx, tcy,
				tcx, tcy2,
				tcx2, tcy,
				tcx2, tcy2,
			};
			shader_->vertex_array(2, GL_FLOAT, 0, 0, varray);
			shader_->texture_array(2, GL_FLOAT, 0, 0, tcarray);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			glBindTexture(GL_TEXTURE_2D, cur_id);

			glPopMatrix();
		}

		virtual bool handleEvent(const SDL_Event& event, bool claimed) { 
			if(event.type == SDL_MOUSEMOTION) {
				const SDL_MouseMotionEvent& e = event.motion;
				if(inWidget(e.x, e.y) && lighting_) {
					lighting_->set_light_position(0, glm::vec3(float(e.x-x()), float(e.y-y()), 0.07f));
					claimed = true;
				}
			}
			return claimed; 
		}
	private:
		graphics::texture tex_normal_;
		//GLfloat varray_[8];
		gles2::program_ptr shader_;
		
		GLuint u_resolution_;

		GLuint u_mvp_matrix_;

		GLuint u_tex_map_;
		GLuint u_normal_map_;

		graphics::lighting_ptr lighting_;
	};

	typedef ffl::IntrusivePtr<ImageWidget_lighted> ImageWidget_lighted_ptr;

	class normal_viewer : public gui::dialog
	{
	public:
		explicit normal_viewer(const rect& r, const std::string& fname);
		virtual ~normal_viewer();

		void init();
	private:
		std::string fname_;
		rect area_;

		gui::ImageWidgetPtr img_;
		gui::ImageWidgetPtr normal_;
		gui::ImageWidgetPtr aux_;
		ImageWidget_lighted_ptr output_img_;

		normal_viewer();
		normal_viewer(const normal_viewer&);
	};

	normal_viewer::normal_viewer(const rect& r, const std::string& fname)
		: dialog(r.x(), r.y(), r.w(), r.h()), area_(r), fname_(fname)
	{
		init();
	}

	normal_viewer::~normal_viewer()
	{
	}

	void normal_viewer::init()
	{
		using namespace graphics;
		clear();

		const int sidebar_padding = 0;//(width()*15)/100;
		const int between_padding = 10;

		const int widget_width = width()/2 - sidebar_padding - between_padding*2;
		const int widget_height = height()/2 - between_padding;

		surface s = surface_cache::get_no_cache(fname_);
		texture tex = texture::get_no_cache(s);

		img_.reset(new gui::ImageWidget(tex, widget_width, widget_height));
		addWidget(img_, 0, 0);

		surface s_sobel = s.clone();
		double gs_param[3] = {0.21, 0.71, 0.07};
		calculate_normal(s_sobel.get(), gs_param);
		texture s_tex = texture::get_no_cache(s_sobel);

		graphics::setAlpha_for_transparent_colors_in_rgba_surface(s, 0);
		surface s_aux = drop_shadow_filter(s, graphics::color(0,0,0), 3, 3, 0.0, 3, 1.0, true, false, 3);
		//graphics::surface s_aux = extract_alpha_mask(s, graphics::color(0,0,0), graphics::color(255,255,255));
		//surface s_emb_h = emboss(s_aux, 0);
		s_aux = make_grayscale(s_aux, nullptr);
		surface s_norm = calculate_normal2(emboss(s_aux, 0), emboss(s_aux, 90));
		s_norm = alpha_clip(s_norm, s);
		//surface s_emb = s_aux;
		aux_.reset(new gui::ImageWidget(texture::get_no_cache(s_norm), widget_width, widget_height));
		addWidget(aux_, 0, widget_height + between_padding);

		normal_.reset(new gui::ImageWidget(s_tex, widget_width, widget_height));
		addWidget(normal_, widget_width + between_padding, 0);

		output_img_.reset(new ImageWidget_lighted(tex, texture::get_no_cache(s_norm), widget_width, widget_height));
		addWidget(output_img_, widget_width + between_padding, widget_height + between_padding);
	}

}

UTILITY(calculate_normal_map)
{
	std::deque<std::string> arguments(args.begin(), args.end());

	ASSERT_LOG(arguments.size() <= 1, "Unexpected arguments");

	std::string fname;
	if(arguments.empty() == false) {
		fname = module::map_file(arguments.front());
	}

	ffl::IntrusivePtr<normal_viewer> editor(new normal_viewer(rect(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height()), fname));
	editor->showModal();
}

#endif
