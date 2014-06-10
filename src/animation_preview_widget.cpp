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

#ifndef NO_EDITOR
#include <boost/bind.hpp>
#include <deque>

#include "kre/Canvas.hpp"
#include "kre/ClipScope.hpp"
#include "kre/Texture.hpp"

#include "animation_preview_widget.hpp"
#include "button.hpp"
#include "formatter.hpp"
#include "input.hpp"
#include "pathfinding.hpp"
#include "solid_map.hpp"

namespace 
{
	using namespace KRE;
	const unsigned char RedBorder[] = {0xf9, 0x30, 0x3d};
	const unsigned char BackgroundColor[] = {0x6f, 0x6d, 0x51};

	bool is_pixel_border(const SurfacePtr& s, int x, int y)
	{
		if(x < 0 || y < 0 || x >= s->width() || y >= s->height()) {
			return false;
		}

		const unsigned char* pixel = reinterpret_cast<const unsigned char*>(s->Pixels()) + y*s->row_pitch() + x*4;
		for(int n = 0; n != 3; ++n) {
			if(pixel[n] != RedBorder[n]) {
				return false;
			}
		}

		return true;
	}

	bool is_pixel_alpha(const SurfacePtr& s, const point& p)
	{
		const unsigned char* pixel = reinterpret_cast<const unsigned char*>(s->Pixels()) + p.y*s->row_pitch() + p.x*4;
		if(pixel[3] == 0) {
			return true;
		}
		for(int n = 0; n != 3; ++n) {
			if(pixel[n] != BackgroundColor[n]) {
				return false;
			}
		}

		return true;
	}

	rect get_border_rect(const SurfacePtr& s, int x, int y)
	{
		int w = 0, h = 0;
		while(is_pixel_border(s, x + w + 1, y)) {
			++w;
		}

		while(is_pixel_border(s, x, y + h + 1) &&
			  is_pixel_border(s, x + w, y + h + 1)) {
			++h;
		}

		if(w == 0 || h == 0) {
			return rect();
		}

		return rect(x+1, y+1, w-1, h-1);
	}

	int path_cost_fn(const SurfacePtr& s, const point& p1, const point& p2) {
		bool a1 = is_pixel_alpha(s, p1);
		bool a2 = is_pixel_alpha(s, p2);
		if(a1 && a2) return 2;
		else if(a1 ^ a2) return 1;
		return 0;
	}

	rect get_border_rect_heuristic_search(const SurfacePtr& s, int ox, int oy, int max_cost) 
	{
		int x1 = INT_MAX, y1 = INT_MAX, x2 = INT_MIN, y2 = INT_MIN;
		if(ox < 0 || oy < 0 || ox >= s->width() || oy >= s->height()) {
			return rect::from_coordinates(ox,oy,ox+1,oy+1);
		}
		const rect r(0, 0, s->width(), s->height());

		if(is_pixel_alpha(s, point(ox, oy))) {
			return rect::from_coordinates(ox,oy,ox+1,oy+1);
		}

		typedef pathfinding::graph_node<point, int> graph_node;
		typedef graph_node::graph_node_ptr graph_node_ptr;
		typedef std::map<point, graph_node_ptr> graph_node_list;
		graph_node_list node_list;
		std::deque<graph_node_ptr> open_list;
		typedef std::pair<point, bool> reachable_node;
		std::vector<reachable_node> reachable;

		bool searching = true;
		try {
			graph_node_ptr current = graph_node_ptr(new graph_node(point(ox, oy)));
			current->set_cost(0, 0);
			current->set_on_open_list(true);
			open_list.push_back(current);
			node_list[point(ox, oy)] = current;

			while(searching && !open_list.empty()) {
				current = open_list.front(); open_list.pop_front();
				current->set_on_open_list(false);
				current->set_on_closed_list(true);
				if(current->G() <= max_cost) {
					reachable.push_back(reachable_node(current->get_node_value(), 
						is_pixel_alpha(s, current->get_node_value())));
				}
				for(auto& p : pathfinding::get_neighbours_from_rect(current->get_node_value(), 1, 1, r)) {
					graph_node_list::const_iterator neighbour_node = node_list.find(p);
					int g_cost = path_cost_fn(s, p, current->get_node_value()) + current->G();
					if(neighbour_node == node_list.end()) {
						graph_node_ptr new_node = graph_node_ptr(new graph_node(point(p.x, p.y)));
							new_node->set_parent(current);
							new_node->set_cost(g_cost, 0);
							new_node->set_on_open_list(true);
							node_list[p] = new_node;
							if(g_cost > max_cost) {
								new_node->set_on_closed_list(true);
							} else {
								new_node->set_on_open_list(true);
								open_list.push_back(new_node);
							}
					} else if(neighbour_node->second->on_closed_list() || neighbour_node->second->on_open_list()) {
						if(g_cost < neighbour_node->second->G()) {
							neighbour_node->second->G(g_cost);
							neighbour_node->second->set_parent(current);
						}
					} else {
						throw "Path error node on list, but not on open or closed lists";
					}
				}
			}
		} catch(...) {
			std::cerr << "get_border_rect_heuristic_search(): Caught exception" << std::endl;
		}

		for(const reachable_node& rn : reachable) {
			if(rn.second) {
				if(rn.first.x < x1) x1 = rn.first.x;
				if(rn.first.x > x2) x2 = rn.first.x;
				if(rn.first.y < y1) y1 = rn.first.y;
				if(rn.first.y > y2) y2 = rn.first.y;
			}
		}

		std::cerr << "CALC RECT " << x1 << "," << y1 << "," << x2 << "," << y2 << std::endl;
		//std::cerr << "PIXEL: 0x" << std::hex << int(pixel[0]) << ",0x" << int(pixel[1]) << ",0x" << int(pixel[2]) << ",0x" << int(pixel[3]) << std::endl;
		return rect::from_coordinates(x1, y1, x2, y2);
	}

	rect get_border_rect_around_loc(const SurfacePtr& s, int ox, int oy)
	{
		int x = ox, y = oy;
		std::cerr << "SEARCHING FOR BORDER AROUND " << x << "," << y << "\n";
		while(y >= 0 && !is_pixel_border(s, x, y)) {
			--y;
		}

		while(x >= 0 && is_pixel_border(s, x, y)) {
			--x;
		}

		++x;

		std::cerr << "STEPPED TO " << x << "," << y << "\n";

		if(y >= 0 && is_pixel_border(s, x, y)) {
			std::cerr << "RETURNING " << get_border_rect(s, x, y) << "\n";
			return get_border_rect(s, x, y);
		} else {
			std::cerr << "TRYING HEURISTIC SEARCH AROUND " << ox << "," << oy << std::endl;
			return get_border_rect_heuristic_search(s, ox, oy, 10);
		}
	}

	bool find_full_animation(const SurfacePtr& s, const rect& r, int* pad, int* num_frames, int* frames_per_row)
	{
		const int x = r.x() + r.w()/2;
		const int y = r.y() + r.h()/2;

		int next_x = x + r.w();
		if(next_x >= s->width()) {
			std::cerr << "FAIL FIND " << next_x << " >= " << s->width() << "\n";
			return false;
		}

		rect next_rect = get_border_rect_around_loc(s, next_x, y);
		std::cerr << "NEXT RECT: " << next_rect << " VS " << r << "\n";
		if(next_rect.w() != r.w() || next_rect.h() != r.h()) {
			return false;
		}

		*pad = next_rect.x() - r.x2();
		*num_frames = 2;

		std::vector<rect> rect_row;
		rect_row.push_back(r);
		rect_row.push_back(next_rect);

		std::cerr << "SETTING... " << get_border_rect_around_loc(s, next_x + r.w() + *pad, y) << " VS " << rect(next_rect.x() + next_rect.w() + *pad, y, r.w(), r.h()) << "\n";

		while(next_x + r.w() + *pad < s->width() && get_border_rect_around_loc(s, next_x + r.w() + *pad, y) == rect(next_rect.x() + next_rect.w() + *pad, r.y(), r.w(), r.h())) {
				std::cerr << "ITER\n";
			*num_frames += 1;
			next_x += r.w() + *pad;
			next_rect = rect(next_rect.x() + next_rect.w() + *pad, r.y(), r.w(), r.h());
			rect_row.push_back(next_rect);
		}

		*frames_per_row = *num_frames;

		bool row_valid = true;
		while(row_valid) {
			int index = 0;
			for(rect& r : rect_row) {
				rect next_rect(r.x(), r.y() + r.h() + *pad, r.w(), r.h());
				std::cerr << "MATCHING: " << get_border_rect_around_loc(s, next_rect.x() + next_rect.w()/2, next_rect.y() + next_rect.h()/2) << " VS " << next_rect << "\n";
				if(next_rect.y2() >= s->height() || get_border_rect_around_loc(s, next_rect.x() + next_rect.w()/2, next_rect.y() + next_rect.h()/2) != next_rect) {
					std::cerr << "MISMATCH: " << index << "/" << rect_row.size() << " -- " << get_border_rect_around_loc(s, next_rect.x() + next_rect.w()/2, next_rect.y() + next_rect.h()/2) << " VS " << next_rect << "\n";
					row_valid = false;
					break;
				}

				r = next_rect;
				++index;
			}

			if(row_valid) {
				*num_frames += *frames_per_row;
			}
		}

		return true;
	}

}

namespace gui 
{
	bool AnimationPreviewWidget::is_animation(variant obj)
	{
		return !obj.is_null() && obj["image"].is_string() && !obj["image"].as_string().empty();
	}

	AnimationPreviewWidget::AnimationPreviewWidget(const variant& v, game_logic::FormulaCallable* e) 
		: Widget(v,e), cycle_(0), zoom_label_(NULL), pos_label_(NULL), scale_(0), 
		anchor_x_(-1), anchor_y_(-1), anchor_pad_(-1), has_motion_(false), dragging_sides_bitmap_(0), 
		moving_solid_rect_(false), anchor_solid_x_(-1), anchor_solid_y_(-1)
	{
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");

		if(v.has_key("on_rect_change")) {
			rect_handler_ = boost::bind(&AnimationPreviewWidget::rectHandlerDelegate, this, _1);
			ffl_rect_handler_ = getEnvironment()->createFormula(v["on_rect_change"]);
		}
		if(v.has_key("on_pad_change")) {
			pad_handler_ = boost::bind(&AnimationPreviewWidget::padHandlerDelegate, this, _1);
			ffl_pad_handler_ = getEnvironment()->createFormula(v["on_pad_change"]);
		}
		if(v.has_key("on_frames_change")) {
			num_frames_handler_ = boost::bind(&AnimationPreviewWidget::numFramesHandlerDelegate, this, _1);
			ffl_num_frames_handler_ = getEnvironment()->createFormula(v["on_frames_change"]);
		}
		if(v.has_key("on_frames_per_row_change")) {
			frames_per_row_handler_ = boost::bind(&AnimationPreviewWidget::framesPerRowHandlerDelegate, this, _1);
			ffl_frames_per_row_handler_ = getEnvironment()->createFormula(v["on_frames_per_row_change"]);
		}
		if(v.has_key("on_solid_change")) {
			solid_handler_ = boost::bind(&AnimationPreviewWidget::solidHandlerDelegate, this, _1, _2);
			ffl_solid_handler_ = getEnvironment()->createFormula(v["on_solid_change"]);
		}
	
		try {
			setObject(v);
		} catch(type_error&) {
		} catch(frame::error&) {
		} catch(validation_failure_exception&) {
		} catch(KRE::ImageLoadError&) {
		}
	}

	AnimationPreviewWidget::AnimationPreviewWidget(variant obj) : cycle_(0), zoom_label_(NULL), pos_label_(NULL), scale_(0), anchor_x_(-1), anchor_y_(-1), anchor_pad_(-1), has_motion_(false), dragging_sides_bitmap_(0), moving_solid_rect_(false), anchor_solid_x_(-1), anchor_solid_y_(-1)
	{
		setEnvironment();
		setObject(obj);
	}

	void AnimationPreviewWidget::init()
	{
		widgets_.clear();

		Button* b = new Button("+", boost::bind(&AnimationPreviewWidget::zoomIn, this));
		b->setLoc(x() + 10, y() + height() - b->height() - 5);
		widgets_.push_back(WidgetPtr(b));
		b = new Button("-", boost::bind(&AnimationPreviewWidget::zoomOut, this));
		b->setLoc(x() + 40, y() + height() - b->height() - 5);
		widgets_.push_back(WidgetPtr(b));

		zoom_label_ = new Label("Zoom: 100%");
		zoom_label_->setLoc(b->x() + b->width() + 10, b->y());
		widgets_.push_back(WidgetPtr(zoom_label_));

		pos_label_ = new Label("");
		pos_label_->setLoc(zoom_label_->x() + zoom_label_->width() + 8, zoom_label_->y());
		widgets_.push_back(WidgetPtr(pos_label_));

		b = new Button("Reset", boost::bind(&AnimationPreviewWidget::resetRect, this));
		b->setLoc(pos_label_->x() + pos_label_->width() + 58, y() + height() - b->height() - 5);
		widgets_.push_back(WidgetPtr(b));
	}

	void AnimationPreviewWidget::setObject(variant obj)
	{
		if(obj == obj_) {
			return;
		}

		frame* f = new frame(obj);

		obj_ = obj;
		frame_.reset(f);
		cycle_ = 0;
	}

	void AnimationPreviewWidget::process()
	{
	}

	void AnimationPreviewWidget::handleDraw() const
	{
		auto canvas = KRE::Canvas::getInstance();

		int mousex, mousey;
		int mouse_buttons = input::sdl_get_mouse_state(&mousex, &mousey);

		canvas->drawSolidRect(rect(x(),y(),width(),height()), KRE::Color(0,0,0,196));
		rect image_area(x(), y(), (width()*3)/4, height() - 30);

		KRE::TexturePtr image_texture = Texture::createTexture(obj_["image"].as_string() /* no_filter -- default is a strip spritesheet annotations filter */);

		if(image_texture) {
			auto cs = KRE::ClipScope::create(image_area);

			const bool view_locked = mouse_buttons && locked_focus_.w()*locked_focus_.h();

			rect focus_area;
			if(frame_->num_frames_per_row() == 0) {
				focus_area = rect();
			} else {
				focus_area = rect(frame_->area().x(), frame_->area().y(),
					  (frame_->area().w() + frame_->pad())*frame_->num_frames_per_row(),
					  (frame_->area().h() + frame_->pad())*(frame_->num_frames()/frame_->num_frames_per_row() + (frame_->num_frames()%frame_->num_frames_per_row() ? 1 : 0)));
			}

			if(view_locked) {
				focus_area = locked_focus_;
			} else {
				locked_focus_ = focus_area;
			}

			float scale = 2.0;
			for(int n = 0; n != abs(scale_); ++n) {
				scale *= (scale_ < 0 ? 0.5f : 2.0f);
			}

			if(!view_locked) {
				while(image_area.w()*scale*2.0f < image_area.w() &&
					  image_area.h()*scale*2.0f < image_area.h()) {
					scale *= 2.0f;
					scale_++;
					update_zoom_label();
				}
		
				while(focus_area.w()*scale > image_area.w() ||
					  focus_area.h()*scale > image_area.h()) {
					scale *= 0.5f;
					scale_--;
					update_zoom_label();
				}
			}

			const int show_width = image_area.w()/scale;
			const int show_height = image_area.h()/scale;

			int x1 = focus_area.x() + (focus_area.w() - show_width)/2;
			int y1 = focus_area.y() + (focus_area.h() - show_height)/2;
			if(x1 < 0) {
				x1 = 0;
			}

			if(y1 < 0) {
				y1 = 0;
			}

			int x2 = x1 + show_width;
			int y2 = y1 + show_height;
			if(x2 > image_texture->Width()) {
				x1 -= (x2 - image_texture->Width());
				x2 = image_texture->Width();
				if(x1 < 0) {
					x1 = 0;
				}
			}

			if(y2 > image_texture->Height()) {
				y1 -= (y2 - image_texture->Height());
				y2 = image_texture->Height();
				if(y1 < 0) {
					y1 = 0;
				}
			}

			int xpos = image_area.x();
			int ypos = image_area.y();

			src_rect_ = rect(x1, y1, x2 - x1, y2 - y1);
			dst_rect_ = rect(xpos, ypos, (x2-x1)*scale, (y2-y1)*scale);

			canvas->blitTexture(image_texture, src_rect_, 0, dst_rect_);

			if(!mouse_buttons) {
				dragging_sides_bitmap_ = 0;
			}

			for(int n = 0; n != frame_->num_frames(); ++n) {
				const int row = n/frame_->num_frames_per_row();
				const int col = n%frame_->num_frames_per_row();
				const int x = xpos - x1*scale + (frame_->area().x() + col*(frame_->area().w()+frame_->pad()))*scale;
				const int y = ypos - y1*scale + (frame_->area().y() + row*(frame_->area().h()+frame_->pad()))*scale;
				const rect box(x, y, frame_->area().w()*scale, frame_->area().h()*scale);
				{
					KRE::Color color(255, 255, n == 0 ? 0 : 255, frame_->frame_number(cycle_) == n ? 0xFF : 0x88);
					canvas->drawHollowRect(box, color);
				}

				if(n == 0 && !mouse_buttons) {
					bool rect_chosen = false;
					if(box.w() > 10 && box.h() > 10) {
						rect_chosen = pointInRect(point(mousex, mousey), rect(box.x()+5, box.y()+5, box.w()-10, box.h()-10));
					}

					if(rect_chosen || pointInRect(point(mousex, mousey), rect(box.x(), box.y()-4, box.w(), 9))) {
						dragging_sides_bitmap_ |= TOP_SIDE;
						canvas->drawSolidRect(rect(box.x(), box.y()-1, box.w(), 2), KRE::Color::colorRed());
					}
				
					if(rect_chosen || !(dragging_sides_bitmap_&TOP_SIDE) && pointInRect(point(mousex, mousey), rect(box.x(), box.y2()-4, box.w(), 9))) {
						dragging_sides_bitmap_ |= BOTTOM_SIDE;
						canvas->drawSolidRect(rect(box.x(), box.y2()-1, box.w(), 2), KRE::Color::colorRed());
					}

					if(rect_chosen || pointInRect(point(mousex, mousey), rect(box.x()-4, box.y(), 9, box.h()))) {
						dragging_sides_bitmap_ |= LEFT_SIDE;
						canvas->drawSolidRect(rect(box.x()-1, box.y(), 2, box.h()), KRE::Color::colorRed());
					}
				
					if(rect_chosen || (!dragging_sides_bitmap_&LEFT_SIDE) && pointInRect(point(mousex, mousey), rect(box.x2()-4, box.y(), 9, box.h()))) {
						dragging_sides_bitmap_ |= RIGHT_SIDE;
						canvas->drawSolidRect(rect(box.x2()-1, box.y(), 2, box.h()), KRE::Color::colorRed());
					}
				} else if(n != 0 && !mouse_buttons) {
					if(pointInRect(point(mousex, mousey), box)) {
						dragging_sides_bitmap_ = PADDING;
						canvas->drawSolidRect(box, KRE::Color(255,255,0,128));
					}
				}
			}

			if(anchor_x_ != -1 && !dragging_sides_bitmap_ &&
			   input::sdl_get_mouse_state(&mousex, &mousey) &&
			   pointInRect(point(mousex, mousey), dst_rect_)) {
				const point p1 = mousePointToImageLoc(point(mousex, mousey));
				const point p2 = mousePointToImageLoc(point(anchor_x_, anchor_y_));

				int xpos1 = xpos - x1*scale + p1.x*scale;
				int xpos2 = xpos - x1*scale + p2.x*scale;
				int ypos1 = ypos - y1*scale + p1.y*scale;
				int ypos2 = ypos - y1*scale + p2.y*scale;
			
				if(xpos2 < xpos1) {
					std::swap(xpos1, xpos2);
				}

				if(ypos2 < ypos1) {
					std::swap(ypos1, ypos2);
				}
			
				canvas->drawHollowRect(rect(xpos1, ypos1, xpos2 - xpos1, ypos2 - ypos1), KRE::Color::colorWhite());
			}
		}

		rect preview_area(x() + (width()*3)/4, y(), width()/4, height());
		float scale = 1.0;
		while(false && (frame_->width()*scale > preview_area.w() || frame_->height()*scale > preview_area.h())) {
			scale *= 0.5;
		}

		const int framex = preview_area.x() + (preview_area.w() - frame_->width()*scale)/2;
		const int framey = preview_area.y() + (preview_area.h() - frame_->height()*scale)/2;
		frame_->draw(framex, framey, true, false, cycle_, 0, scale);
		if(++cycle_ >= frame_->duration()) {
			cycle_ = 0;
		}

		solid_rect_ = rect();

		const_solid_info_ptr solid = frame_->solid();
		if(solid && solid->area().w()*solid->area().h()) {
			const rect area = solid->area();
			solid_rect_ = rect(framex + area.x(), framey + area.y(), area.w(), area.h());
			canvas->drawSolidRect(solid_rect_, KRE::Color(255, 255, 255, 64));
		}

		for(ConstWidgetPtr w : widgets_) {
			w->draw();
		}
	}

	point AnimationPreviewWidget::mousePointToImageLoc(const point& p) const
	{
		const double xpos = double(p.x - dst_rect_.x())/double(dst_rect_.w());
		const double ypos = double(p.y - dst_rect_.y())/double(dst_rect_.h());

		const int x = src_rect_.x() + (double(src_rect_.w()) + 1.0)*xpos;
		const int y = src_rect_.y() + (double(src_rect_.h()) + 1.0)*ypos;
	
		return point(x, y);
	}

	bool AnimationPreviewWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		for(WidgetPtr w : widgets_) {
			claimed = w->processEvent(event, claimed) || claimed;
		}

		if(event.type == SDL_MOUSEBUTTONUP) {
			moving_solid_rect_ = false;
		}

		if(event.type == SDL_MOUSEMOTION) {
			has_motion_ = true;
			const SDL_MouseMotionEvent& e = event.motion;
			if(moving_solid_rect_) {
				if(solid_handler_) {
					const int x = e.x/2;
					const int y = e.y/2;

					solid_handler_(x - anchor_solid_x_, y - anchor_solid_y_);

					anchor_solid_x_ = x;
					anchor_solid_y_ = y;
				}

				return claimed;
			}

			point p(e.x, e.y);
			if(pointInRect(p, dst_rect_)) {
				p = mousePointToImageLoc(p);
				pos_label_->setText(formatter() << p.x << "," << p.y);
			}

			if(e.state && dragging_sides_bitmap_) {
				int delta_x = e.x - anchor_x_;
				int delta_y = e.y - anchor_y_;

				if(scale_ >= 0) {
					for(int n = 0; n < scale_+1; ++n) {
						delta_x >>= 1;
						delta_y >>= 1;
					}
				}

				int x1 = anchor_area_.x();
				int x2 = anchor_area_.x2();
				int y1 = anchor_area_.y();
				int y2 = anchor_area_.y2();

				if(dragging_sides_bitmap_&LEFT_SIDE) {
					x1 += delta_x;
					if(x1 > x2 - 1) {
						x1 = x2 - 1;
					}
				}

				if(dragging_sides_bitmap_&RIGHT_SIDE) {
					x2 += delta_x;
					if(x2 < x1 + 1) {
						x2 = x1 + 1;
					}
				}

				if(dragging_sides_bitmap_&TOP_SIDE) {
					y1 += delta_y;
					if(y1 > y2 - 1) {
						y1 = y2 - 1;
					}
				}

				if(dragging_sides_bitmap_&BOTTOM_SIDE) {
					y2 += delta_y;
					if(y2 < y1 + 1) {
						y2 = y1 + 1;
					}
				}

				const int width = x2 - x1;
				const int height = y2 - y1;

				rect area(x1, y1, width, height);
				if(area != frame_->area() && rect_handler_) {
					rect_handler_(area);
				}

				if(dragging_sides_bitmap_&PADDING && pad_handler_) {
					const int new_pad = anchor_pad_ + delta_x;
					if(new_pad != frame_->pad()) {
						pad_handler_(new_pad);
					}
				}
			}
		} else if(event.type == SDL_MOUSEBUTTONDOWN) {
			moving_solid_rect_ = false;

			const SDL_MouseButtonEvent& e = event.button;
			const point p(e.x, e.y);
			anchor_area_ = frame_->area();
			anchor_pad_ = frame_->pad();
			has_motion_ = false;
			if(pointInRect(p, dst_rect_)) {
				claimed = claimMouseEvents();
				anchor_x_ = e.x;
				anchor_y_ = e.y;
			} else {
				anchor_x_ = anchor_y_ = -1;

				if(pointInRect(p, solid_rect_)) {
					moving_solid_rect_ = claimMouseEvents();
					anchor_solid_x_ = e.x/2;
					anchor_solid_y_ = e.y/2;
				}
			}
		} else if(event.type == SDL_MOUSEBUTTONUP && anchor_x_ != -1) {

			const SDL_MouseButtonEvent& e = event.button;
			point anchor(anchor_x_, anchor_y_);
			point p(e.x, e.y);
			if(anchor == p && !has_motion_) {
				claimed = claimMouseEvents();
				p = mousePointToImageLoc(p);
				graphics::surface surf = graphics::surface_cache::get(obj_["image"].as_string());
				std::vector<graphics::surface> surf_key;
				surf_key.push_back(surf);
				if(surf) {
					surf = graphics::texture::build_surface_from_key(surf_key, surf->w, surf->h);
				}

				if(surf) {
					rect area = get_border_rect_around_loc(surf, p.x, p.y);
					if(area.w() > 0) {
						if(rect_handler_) {
							rect_handler_(area);
						}

						int pad = frame_->pad();
						int num_frames = 1;
						int frames_per_row = 1;
						if(find_full_animation(surf, area, &pad, &num_frames, &frames_per_row)) {
							if(pad_handler_) {
								pad_handler_(pad);
							}

							if(num_frames_handler_) {
								std::cerr << "SETTING NUM FRAMES TO " << num_frames << "\n";
								num_frames_handler_(num_frames);
							}

							if(frames_per_row_handler_) {
								frames_per_row_handler_(frames_per_row);
							}
						}
					}
				}
			} else if(!dragging_sides_bitmap_ && pointInRect(anchor, dst_rect_) && pointInRect(p, dst_rect_)) {
				claimed = claimMouseEvents();

				anchor = mousePointToImageLoc(anchor);
				p = mousePointToImageLoc(p);

				int x1 = anchor.x;
				int y1 = anchor.y;
				int x2 = p.x;
				int y2 = p.y;

				if(x2 < x1) {
					std::swap(x1, x2);
				}

				if(y2 < y1) {
					std::swap(y1, y2);
				}

				rect area(x1, y1, x2 - x1, y2 - y1);

				if(rect_handler_) {
					rect_handler_(area);
				}
			}

			anchor_x_ = anchor_y_ = -1;
		}

		return claimed;
	}

	void AnimationPreviewWidget::zoomIn()
	{
		++scale_;
		update_zoom_label();
	}

	void AnimationPreviewWidget::zoomOut()
	{
		--scale_;
		update_zoom_label();
	}

	void AnimationPreviewWidget::resetRect()
	{
		if(rect_handler_) {
			rect_handler_(rect(0,0,0,0));
		}
	}

	void AnimationPreviewWidget::update_zoom_label() const
	{
		if(zoom_label_) {
			int percent = 100;
			for(int n = 0; n != abs(scale_); ++n) {
				if(scale_ > 0) {
					percent *= 2;
				} else {
					percent /= 2;
				}
			}

			zoom_label_->setText(formatter() << "Zoom: " << percent << "%");
		}
	}

	void AnimationPreviewWidget::setRectHandler(boost::function<void(rect)> handler)
	{
		rect_handler_ = handler;
	}

	void AnimationPreviewWidget::setPadHandler(boost::function<void(int)> handler)
	{
		pad_handler_ = handler;
	}

	void AnimationPreviewWidget::setNumFramesHandler(boost::function<void(int)> handler)
	{
		num_frames_handler_ = handler;
	}

	void AnimationPreviewWidget::setFramesPerRowHandler(boost::function<void(int)> handler)
	{
		frames_per_row_handler_ = handler;
	}

	void AnimationPreviewWidget::setSolidHandler(boost::function<void(int,int)> handler)
	{
		solid_handler_ = handler;
	}

	void AnimationPreviewWidget::rectHandlerDelegate(rect r)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			map_FormulaCallablePtr callable = map_FormulaCallablePtr(new map_FormulaCallable(getEnvironment()));
			callable->add("new_rect", r.write());
			variant value = ffl_rect_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "AnimationPreviewWidget::rectHandlerDelegate() called without environment!" << std::endl;
		}
	}

	void AnimationPreviewWidget::padHandlerDelegate(int pad)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			map_FormulaCallablePtr callable = map_FormulaCallablePtr(new map_FormulaCallable(getEnvironment()));
			callable->add("new_pad", variant(pad));
			variant value = ffl_pad_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "AnimationPreviewWidget::padHandlerDelegate() called without environment!" << std::endl;
		}
	}

	void AnimationPreviewWidget::numFramesHandlerDelegate(int frames)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			map_FormulaCallablePtr callable = map_FormulaCallablePtr(new map_FormulaCallable(getEnvironment()));
			callable->add("new_frames", variant(frames));
			variant value = ffl_num_frames_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "AnimationPreviewWidget::numFramesHandlerDelegate() called without environment!" << std::endl;
		}
	}

	void AnimationPreviewWidget::framesPerRowHandlerDelegate(int frames_per_row)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			map_FormulaCallablePtr callable = map_FormulaCallablePtr(new map_FormulaCallable(getEnvironment()));
			callable->add("new_frames_per_row", variant(frames_per_row));
			variant value = ffl_frames_per_row_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "AnimationPreviewWidget::framesPerRowHandlerDelegate() called without environment!" << std::endl;
		}
	}

	void AnimationPreviewWidget::solidHandlerDelegate(int x, int y)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			map_FormulaCallablePtr callable = map_FormulaCallablePtr(new map_FormulaCallable(getEnvironment()));
			callable->add("new_solidx", variant(x));
			callable->add("new_solidy", variant(y));
			variant value = ffl_solid_handler_->execute(*callable);
			getEnvironment()->createFormula(value);
		} else {
			std::cerr << "AnimationPreviewWidget::solidHandlerDelegate() called without environment!" << std::endl;
		}
	}

	BEGIN_DEFINE_CALLABLE(AnimationPreviewWidget, Widget)
		DEFINE_FIELD(object, "any")
			return obj.obj_;
		DEFINE_SET_FIELD
			try {
				obj.setObject(value);
			} catch(type_error&) {
			} catch(frame::error&) {
			} catch(validation_failure_exception&) {
			} catch(KRE::ImageLoadError&) {
			}
	END_DEFINE_CALLABLE(AnimationPreviewWidget)

}

#endif // !NO_EDITOR

