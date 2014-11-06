/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

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
#include "graphics.hpp"

#include "asserts.hpp"
#include "camera.hpp"
#include "fbo.hpp"
#include "foreach.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "raster_distortion.hpp"
#include "rectangle_rotator.hpp"
#include "texture_frame_buffer.hpp"

#include "SDL.h"
#include "SDL_syswm.h"

#include <boost/shared_array.hpp>
#include <iostream>
#include <cmath>
#include <stack>

// Defined in video_selections.cpp
extern int g_vsync;

namespace graphics
{
	/* unavoidable global variable to store global clip
	 rectangle changes */
	std::vector<boost::shared_array<GLint> > clip_rectangles;
	
	std::vector<GLfloat>& global_vertex_array()
	{
		static std::vector<GLfloat> v;
		return v;
	}
	
	std::vector<GLfloat>& global_texcoords_array()
	{
		static std::vector<GLfloat> v;
		return v;
	}

	std::vector<GLbyte>& global_vertex_color_array()
	{
		static std::vector<GLbyte> v;
		return v;
	}
	
	namespace {
		struct draw_detection_rect {
			rect area;
			char* buf;
		};
		
		std::vector<draw_detection_rect> draw_detection_rects_;
		rect draw_detection_rect_;
		char* draw_detection_buf_;
		
		std::vector<const raster_distortion*> distortions_;
	}
	
	void blit_texture(const texture& tex, int x, int y, GLfloat rotate)
	{
		if(!tex.valid()) {
			return;
		}

		x &= preferences::xypos_draw_mask;
		y &= preferences::xypos_draw_mask;
		
		int h = tex.height();
		int w = tex.width();
		const int w_odd = w % 2;
		const int h_odd = h % 2;
		h /= 2;
		w /= 2;
		
		glPushMatrix();
		
		glTranslatef(x+w,y+h,0.0);
		glRotatef(rotate,0.0,0.0,1.0);
		
		tex.set_as_current_texture();
		
		GLfloat varray[] = {
			(GLfloat)-w, (GLfloat)-h,
			(GLfloat)-w, (GLfloat)h+h_odd,
			(GLfloat)w+w_odd, (GLfloat)-h,
			(GLfloat)w+w_odd, (GLfloat)h+h_odd
		};
		GLfloat tcarray[] = {
			texture::get_coord_x(0.0), texture::get_coord_y(0.0),
			texture::get_coord_x(0.0), texture::get_coord_y(1.0),
			texture::get_coord_x(1.0), texture::get_coord_y(0.0),
			texture::get_coord_x(1.0), texture::get_coord_y(1.0)
		};
#if defined(USE_SHADERS)
		gles2::active_shader()->prepare_draw();
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0, tcarray);
#else
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glTexCoordPointer(2, GL_FLOAT, 0, tcarray);
#endif
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		glPopMatrix();
	}
	
	namespace {
		
		//function which marks the draw detection buffer with pixels drawn.
		void detect_draw(const texture& tex, int x, int y, int orig_w, int orig_h, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
		{
			if(draw_detection_rects_.empty()) {
				return;
			}
			
			rect draw_rect(x, y, std::abs(orig_w), std::abs(orig_h));
			
			foreach(const draw_detection_rect& detect, draw_detection_rects_) {
				if(rects_intersect(draw_rect, detect.area)) {
					rect r = intersection_rect(draw_rect, detect.area);
					for(int ypos = r.y(); ypos != r.y2(); ++ypos) {
						for(int xpos = r.x(); xpos != r.x2(); ++xpos) {
							const GLfloat u = (GLfloat(draw_rect.x2() - xpos)*x1 + GLfloat(xpos - draw_rect.x())*x2)/GLfloat(draw_rect.w());
							const GLfloat v = (GLfloat(draw_rect.y2() - ypos)*y1 + GLfloat(ypos - draw_rect.y())*y2)/GLfloat(draw_rect.h());
							const int texture_x = u*tex.width();
							const int texture_y = v*tex.height();
							ASSERT_GE(texture_x, 0);
							ASSERT_GE(texture_y, 0);
							ASSERT_LOG(texture_x < tex.width(), texture_x << " < " << tex.width() << " " << r.x() << " " << r.x2() << " " << xpos << " x: " << x1 << " x2: " << x2 << " u: " << u << "\n");
							ASSERT_LT(texture_x, tex.width());
							ASSERT_LT(texture_y, tex.height());
							const bool alpha = tex.is_alpha(texture_x, texture_y);
							if(!alpha) {
								const int buf_x = xpos - detect.area.x();
								const int buf_y = ypos - detect.area.y();
								const int buf_index = buf_y*detect.area.w() + buf_x;
								ASSERT_LOG(buf_index >= 0, xpos << ", " << ypos << " -> " << buf_x << ", " << buf_y << " -> " << buf_index << " in " << detect.area << "\n");
								ASSERT_GE(buf_index, 0);
								ASSERT_LT(buf_index, detect.area.w()*detect.area.h());
								detect.buf[buf_index] = true;
							}
						}
					}
				}
			}
		}
		
		void blit_texture_internal(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
		{
			if(!tex.valid()) {
				return;
			}
			
			const int w_odd = w % 2;
			const int h_odd = h % 2;
			
			w /= 2;
			h /= 2;
			glPushMatrix();
			tex.set_as_current_texture();
			glTranslatef(x+abs(w),y+abs(h),0.0);
			glRotatef(rotate,0.0,0.0,1.0);
			GLfloat varray[] = {
				(GLfloat)-w, (GLfloat)-h,
				(GLfloat)-w, (GLfloat)h+h_odd,
				(GLfloat)w+w_odd, (GLfloat)-h,
				(GLfloat)w+w_odd, (GLfloat)h+h_odd
			};
			GLfloat tcarray[] = {
				texture::get_coord_x(x1), texture::get_coord_y(y1),
				texture::get_coord_x(x1), texture::get_coord_y(y2),
				texture::get_coord_x(x2), texture::get_coord_y(y1),
				texture::get_coord_x(x2), texture::get_coord_y(y2)
			};
#if defined(USE_SHADERS)
			gles2::active_shader()->prepare_draw();
			gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
			gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0, tcarray);
#else
			glVertexPointer(2, GL_FLOAT, 0, varray);
			glTexCoordPointer(2, GL_FLOAT, 0, tcarray);
#endif
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			glPopMatrix();
		}
		
		void blit_texture_with_distortion(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2, const raster_distortion& distort)
		{
			const rect& area = distort.area();
			if(x < area.x()) {
				const int new_x = area.x();
				const GLfloat new_x1 = (x1*(x + w - new_x) + x2*(new_x - x))/w;
				
				blit_texture(tex, x, y, new_x - x, h, rotate, x1, y1, new_x1, y2);
				
				x1 = new_x1;
				w -= new_x - x;
				x = new_x;
			}
			
			if(y < area.y()) {
				const int new_y = area.y();
				const GLfloat new_y1 = (y1*(y + h - new_y) + y2*(new_y - y))/h;
				
				blit_texture(tex, x, y, w, new_y - y, rotate, x1, y1, x2, new_y1);
				
				y1 = new_y1;
				h -= new_y - y;
				y = new_y;
			}
			
			if(x + w > area.x2()) {
				const int new_w = area.x2() - x;
				const int new_xpos = x + new_w;
				const GLfloat new_x2 = (x1*(x + w - new_xpos) + x2*(new_xpos - x))/w;
				
				blit_texture(tex, new_xpos, y, x + w - new_xpos, h, rotate, new_x2, y1, x2, y2);
				
				x2 = new_x2;
				w = new_w;
			}
			
			if(y + h > area.y2()) {
				const int new_h = area.y2() - y;
				const int new_ypos = y + new_h;
				const GLfloat new_y2 = (y1*(y + h - new_ypos) + y2*(new_ypos - y))/h;
				
				blit_texture(tex, x, new_ypos, w, y + h - new_ypos, rotate, x1, new_y2, x2, y2);
				
				y2 = new_y2;
				h = new_h;
			}
			
			tex.set_as_current_texture();
			
			const int xdiff = distort.granularity_x();
			const int ydiff = distort.granularity_y();
			for(int xpos = 0; xpos < w; xpos += xdiff) {
				const int xbegin = x + xpos;
				const int xend = std::min<int>(x + w, xbegin + xdiff);
				
				const GLfloat u1 = (x1*(x+w - xbegin) + x2*(xbegin - x))/w;
				const GLfloat u2 = (x1*(x+w - xend) + x2*(xend - x))/w;
				for(int ypos = 0; ypos < h; ypos += ydiff) {
					const int ybegin = y + ypos;
					const int yend = std::min<int>(y + h, ybegin + ydiff);
					
					const GLfloat v1 = (y1*(y+h - ybegin) + y2*(ybegin - y))/h;
					const GLfloat v2 = (y1*(y+h - yend) + y2*(yend - y))/h;
					
					GLfloat points[8] = { (GLfloat)xbegin, (GLfloat)ybegin, (GLfloat)xend, (GLfloat)ybegin, (GLfloat)xbegin, (GLfloat)yend, (GLfloat)xend, (GLfloat)yend };
					GLfloat uv[8] = { u1, v1, u2, v1, u1, v2, u2, v2 };
					
					for(int n = 0; n != 4; ++n) {
						distort.distort_point(&points[n*2], &points[n*2 + 1]);
					}
					
#if defined(USE_SHADERS)
					gles2::active_shader()->prepare_draw();
					gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, points);
					gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0, uv);
#else
					glVertexPointer(2, GL_FLOAT, 0, points);
					glTexCoordPointer(2, GL_FLOAT, 0, uv);
#endif
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				}
			}
		}
		
		int blit_texture_translate_x = 0;
		int blit_texture_translate_y = 0;
		
	}  // namespace
	
	distortion_translation::distortion_translation()
	: x_(0), y_(0)
	{
	}
	
	distortion_translation::~distortion_translation()
	{
		if(x_ || y_) {
			foreach(const raster_distortion* distort, distortions_) {
				rect r = distort->area();
				r = rect(r.x() + x_, r.y() + y_, r.w(), r.h());
				const_cast<raster_distortion*>(distort)->set_area(r);
			}
		}
	}
	
	void distortion_translation::translate(int x, int y)
	{
		x_ += x;
		y_ += y;
		
		foreach(const raster_distortion* distort, distortions_) {
			rect r = distort->area();
			r = rect(r.x() - x, r.y() - y, r.w(), r.h());
			const_cast<raster_distortion*>(distort)->set_area(r);
		}
	}
	
	void blit_texture(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
	{
		x &= preferences::xypos_draw_mask;
		y &= preferences::xypos_draw_mask;

		if(w < 0) {
			std::swap(x1, x2);
			w *= -1;
		}
		
		if(h < 0) {
			std::swap(y1, y2);
			h *= -1;
		}
		
		detect_draw(tex, x, y, w, h, x1, y1, x2, y2);
		
		for(std::vector<const raster_distortion*>::const_iterator i = distortions_.begin(); i != distortions_.end() && rotate == 0.0; ++i) {
			const raster_distortion& distort = **i;
			if(rects_intersect(rect(x, y, w, h), distort.area())) {
				blit_texture_with_distortion(tex, x, y, w, h, rotate, x1, y1, x2, y2, distort);
				return;
			}
		}
		blit_texture_internal(tex, x, y, w, h, rotate, x1, y1, x2, y2);
	}

namespace {
const texture* blit_current_texture;
std::vector<GLfloat> blit_tcqueue;
std::vector<GLshort> blit_vqueue;
}

void queue_blit_texture(const texture& tex, int x, int y, int w, int h,
                        GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	x &= preferences::xypos_draw_mask;
	y &= preferences::xypos_draw_mask;

	if(&tex != blit_current_texture) {
		flush_blit_texture();
		blit_current_texture = &tex;
	}

	x1 = tex.translate_coord_x(x1);
	y1 = tex.translate_coord_y(y1);
	x2 = tex.translate_coord_x(x2);
	y2 = tex.translate_coord_y(y2);

	if(w < 0) {
		std::swap(x1, x2);
		w *= -1;
	}
		
	if(h < 0) {
		std::swap(y1, y2);
		h *= -1;
	}
	
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y2);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y2);
	
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y + h);
}

void queue_blit_texture(const texture& tex, int x, int y, int w, int h, GLfloat rotate,
						GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	x &= preferences::xypos_draw_mask;
	y &= preferences::xypos_draw_mask;
	
	if(&tex != blit_current_texture) {
		flush_blit_texture();
		blit_current_texture = &tex;
	}
	
	x1 = tex.translate_coord_x(x1);
	y1 = tex.translate_coord_y(y1);
	x2 = tex.translate_coord_x(x2);
	y2 = tex.translate_coord_y(y2);
	
	if(w < 0) {
		std::swap(x1, x2);
		w *= -1;
	}
	
	if(h < 0) {
		std::swap(y1, y2);
		h *= -1;
	}
	
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y2);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y2);
	
	
	
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y + h);

	rect r(x,y,w,h);
	GLshort* varray = &blit_vqueue[blit_vqueue.size()-8];
	rotate_rect(x+(w/2), y+(h/2), rotate, varray); 

}

void queue_blit_texture_3d(const texture& tex, 
	GLfloat x, GLfloat y, GLfloat z, 
	int w, int h, 
	GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	x1 = tex.translate_coord_x(x1);
	y1 = tex.translate_coord_y(y1);
	x2 = tex.translate_coord_x(x2);
	y2 = tex.translate_coord_y(y2);
	
	if(w < 0) {
		std::swap(x1, x2);
		w *= -1;
	}
	
	if(h < 0) {
		std::swap(y1, y2);
		h *= -1;
	}
	
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y2);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y2);
	
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(z);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(z);
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(z);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(z);
}

void flush_blit_texture_3d()
{
	if(!blit_current_texture) {
		return;
	}
	blit_current_texture->set_as_current_texture();
#if defined(USE_SHADERS)
	gles2::active_shader()->shader()->vertex_array(3, GL_SHORT, 0, 0, &blit_vqueue.front());
	gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0,  &blit_tcqueue.front());
#else
	glVertexPointer(3, GL_SHORT, 0, &blit_vqueue.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &blit_tcqueue.front());
#endif
	glDrawArrays(GL_TRIANGLE_STRIP, 0, blit_tcqueue.size()/2);

	blit_current_texture = NULL;
	blit_tcqueue.clear();
	blit_vqueue.clear();
}


void flush_blit_texture()
{
	if(!blit_current_texture) {
		return;
	}

	blit_current_texture->set_as_current_texture();
#if defined(USE_SHADERS)
	gles2::active_shader()->prepare_draw();
	gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &blit_vqueue.front());
	gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0,  &blit_tcqueue.front());
#else
	glVertexPointer(2, GL_SHORT, 0, &blit_vqueue.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &blit_tcqueue.front());
#endif
	glDrawArrays(GL_TRIANGLE_STRIP, 0, blit_tcqueue.size()/2);

	blit_current_texture = NULL;
	blit_tcqueue.clear();
	blit_vqueue.clear();
}

void blit_queue::clear()
{
	texture_ = 0;
	vertex_.clear();
	uv_.clear();
}

void blit_queue::do_blit() const
{
	if(vertex_.empty()) {
		return;
	}

	texture::set_current_texture(texture_);

#if defined(USE_SHADERS)
	gles2::active_shader()->prepare_draw();
	gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &vertex_.front());
	gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0,  &uv_.front());
#else
	glVertexPointer(2, GL_SHORT, 0, &vertex_.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &uv_.front());
#endif
	glDrawArrays(GL_TRIANGLE_STRIP, 0, uv_.size()/2);
}

void blit_queue::do_blit_range(short begin, short end) const
{
	if(vertex_.empty()) {
		return;
	}

	texture::set_current_texture(texture_);

#if defined(USE_SHADERS)
	gles2::active_shader()->prepare_draw();
	gles2::active_shader()->shader()->vertex_array(2, GL_SHORT, 0, 0, &vertex_[begin]);
	gles2::active_shader()->shader()->texture_array(2, GL_FLOAT, 0, 0,  &uv_[begin]);
#else
	glVertexPointer(2, GL_SHORT, 0, &vertex_[begin]);
	glTexCoordPointer(2, GL_FLOAT, 0, &uv_[begin]);
#endif
	glDrawArrays(GL_TRIANGLE_STRIP, 0, (end - begin)/2);
}

bool blit_queue::merge(const blit_queue& q, short begin, short end)
{
	if(vertex_.empty()) {
		texture_ = q.texture_;
		vertex_.insert(vertex_.end(), q.vertex_.begin()+begin, q.vertex_.begin()+end);
		uv_.insert(uv_.end(), q.uv_.begin()+begin, q.uv_.begin()+end);
		return true;
	}

	if(texture_ != q.texture_) {
		return false;
	}

	repeat_last();
	vertex_.push_back(q.vertex_[begin]);
	vertex_.push_back(q.vertex_[begin+1]);
	uv_.push_back(q.uv_[begin]);
	uv_.push_back(q.uv_[begin+1]);

	vertex_.insert(vertex_.end(), q.vertex_.begin()+begin, q.vertex_.begin()+end);
	uv_.insert(uv_.end(), q.uv_.begin()+begin, q.uv_.begin()+end);

	return true;
}
	
	void set_draw_detection_rect(const rect& rect, char* buf)
	{
		draw_detection_rect new_rect = { rect, buf };
		draw_detection_rects_.push_back(new_rect);
	}
	
	void clear_draw_detection_rect()
	{
		draw_detection_rects_.clear();
	}
	
	void add_raster_distortion(const raster_distortion* distortion)
	{
//TODO: distortions currently disabled
//		distortion->next_cycle();
//		distortions_.push_back(distortion);
	}
	
	void remove_raster_distortion(const raster_distortion* distortion)
	{
//		distortions_.erase(std::remove(distortions_.begin(), distortions_.end(), distortion), distortions_.end());
	}
	
	void clear_raster_distortion()
	{
		distortions_.clear();
	}
	
	void draw_rect(const SDL_Rect& r, const SDL_Color& color,
				   unsigned char alpha)
	{
		GLfloat varray[] = {
			(GLfloat)r.x, (GLfloat)r.y,
			(GLfloat)r.x+r.w, (GLfloat)r.y,
			(GLfloat)r.x, (GLfloat)r.y+r.h,
			(GLfloat)r.x+r.w, (GLfloat)r.y+r.h
		};
#if defined(USE_SHADERS)
		glColor4ub(color.r,color.g,color.b,alpha);
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glColor4f(1.0, 1.0, 1.0, 1.0);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r,color.g,color.b,alpha);
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		//glRecti(r.x,r.y,r.x+r.w,r.y+r.h);
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}
	
	void draw_rect(const rect& r, const graphics::color& color)
	{
		GLfloat varray[] = {
			(GLfloat)r.x(), (GLfloat)r.y(),
			(GLfloat)r.x()+r.w(), (GLfloat)r.y(),
			(GLfloat)r.x(), (GLfloat)r.y()+r.h(),
			(GLfloat)r.x()+r.w(), (GLfloat)r.y()+r.h()
		};
#if defined(USE_SHADERS)
		glColor4ub(color.r(),color.g(),color.b(),color.a());
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glColor4f(1.0, 1.0, 1.0, 1.0);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r(),color.g(),color.b(),color.a());
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		//glRecti(r.x(),r.y(),r.x()+r.w(),r.y()+r.h());
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}
	
	
	void draw_hollow_rect(const SDL_Rect& r, const SDL_Color& color,
						  unsigned char alpha)
	{
		GLfloat varray[] = {
			(GLfloat)r.x, (GLfloat)r.y,
			(GLfloat)r.x + r.w, (GLfloat)r.y,
			(GLfloat)r.x + r.w, (GLfloat)r.y + r.h,
			(GLfloat)r.x, (GLfloat)r.y + r.h
		};
#if defined(USE_SHADERS)
		glColor4ub(color.r, color.g, color.b, alpha);
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4f(1.0, 1.0, 1.0, 1.0);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r, color.g, color.b, alpha);
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}

	void draw_hollow_rect(const rect& r, const graphics::color& color)
	{
		GLfloat varray[] = {
			(GLfloat)r.x(), (GLfloat)r.y(),
			(GLfloat)r.x() + r.w(), (GLfloat)r.y(),
			(GLfloat)r.x() + r.w(), (GLfloat)r.y() + r.h(),
			(GLfloat)r.x(), (GLfloat)r.y() + r.h()
		};
#if defined(USE_SHADERS)
		glColor4ub(color.r(), color.g(), color.b(), color.a());
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4f(1.0, 1.0, 1.0, 1.0);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r(), color.g(), color.b(), color.a());
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}


	void draw_circle(int x, int y, int radius)
	{
		static std::vector<GLfloat> varray;
		varray.clear();
		varray.push_back(x);
		varray.push_back(y);
		for(double angle = 0; angle < 3.1459*2.0; angle += 0.1) {
			const double xpos = x + radius*cos(angle);
			const double ypos = y + radius*sin(angle);
			varray.push_back(xpos);
			varray.push_back(ypos);
		}

		//repeat the first coordinate to complete the circle.
		varray.push_back(varray[2]);
		varray.push_back(varray[3]);

#if defined(USE_SHADERS)
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray.front());
		glDrawArrays(GL_TRIANGLE_FAN, 0, varray.size()/2);
#else
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, &varray.front());
		glDrawArrays(GL_TRIANGLE_FAN, 0, varray.size()/2);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
	}

	namespace {
	struct stencil_buffer_settings {
		bool enabled;
		GLint mask;
		GLenum func;
		GLint ref;
		GLuint ref_mask;

		GLenum sfail, dpfail, dppass;
	};

	std::stack<stencil_buffer_settings> stencil_buffer_stack;

	}

	stencil_scope::stencil_scope(bool enabled, int mask, GLenum func, GLenum ref, GLenum ref_mask, GLenum sfail, GLenum dpfail, GLenum dppass)
	  : init_(true)
	{
		stencil_buffer_settings settings = { enabled, mask, func, ref, ref_mask, sfail, dpfail, dppass };
		stencil_buffer_stack.push(settings);
		apply_settings();
	}

	stencil_scope::~stencil_scope() {
		if(init_) {
			revert_settings();
		}
	}

	void stencil_scope::apply_settings() {
		assert(!stencil_buffer_stack.empty());
		const stencil_buffer_settings& settings = stencil_buffer_stack.top();
		if(settings.enabled) {
			glEnable(GL_STENCIL_TEST);
		} else {
			glDisable(GL_STENCIL_TEST);
		}

		glStencilMask(settings.mask);
		glStencilFunc(settings.func, settings.ref, settings.ref_mask);
		glStencilOp(settings.sfail, settings.dpfail, settings.dppass);
	}
	
	void stencil_scope::revert_settings() {
		assert(!stencil_buffer_stack.empty());
		stencil_buffer_stack.pop();
		if(stencil_buffer_stack.empty()) {
			glDisable(GL_STENCIL_TEST);
			glStencilMask(0x0);
		} else {
			apply_settings();
		}
	}

	clip_scope* current_clip_scope = NULL;

	clip_scope::clip_scope(const SDL_Rect& r)
	  : parent_(current_clip_scope), area_(r)
	{
		glGetFloatv(GL_MODELVIEW_MATRIX, matrix_);
		apply(r);
		current_clip_scope = this;
	};
	
	void clip_scope::apply(const SDL_Rect& r)
	{
		{
		stencil_scope stencil_settings(true, 0x01, GL_NEVER, 0x01, 0xff, GL_REPLACE, GL_KEEP, GL_KEEP);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glClear(GL_STENCIL_BUFFER_BIT);
		
		GLfloat varray[] = {
			(GLfloat)r.x, (GLfloat)r.y,
			(GLfloat)r.x+r.w, (GLfloat)r.y,
			(GLfloat)r.x, (GLfloat)r.y+r.h,
			(GLfloat)r.x+r.w, (GLfloat)r.y+r.h
		};
#if defined(USE_SHADERS)
		glColor4f(1.0f,1.0f,1.0f,1.0f);
		gles2::manager gles2_manager(gles2::get_simple_shader());
		gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, varray);
 		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#else
		glColor4ub(255, 255, 255, 255);
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
#endif
		}

		stencil_.reset(new stencil_scope(true, 0x0, GL_EQUAL, 0x1, 0x1, GL_KEEP, GL_KEEP, GL_KEEP));

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	void clip_scope::reapply()
	{
		glPushMatrix();
		glLoadMatrixf(matrix_);
		apply(area_);
		glPopMatrix();
	}
	
	clip_scope::~clip_scope() {
		stencil_.reset();
		if(parent_) {
			parent_->reapply();
		}

		current_clip_scope = parent_;
	}
	
	namespace {
		int zoom_level = 1;
	}
	
	int screen_width()
	{
		return preferences::virtual_screen_width()*zoom_level;
	}
	
	int screen_height()
	{
		return preferences::virtual_screen_height()*zoom_level;
	}
	
	void zoom_in()
	{
		--zoom_level;
		if(zoom_level < 1) {
			zoom_level = 1;
		}
	}
	
	void zoom_out()
	{
		++zoom_level;
		if(zoom_level > 5) {
			zoom_level = 5;
		}
	}
	
	void zoom_default()
	{
		zoom_level = 1;
	}

}
