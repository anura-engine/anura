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
#include <deque>
#include <limits>

#include "Canvas.hpp"
#include "ClipScope.hpp"
#include "Texture.hpp"

#include "animation_preview_widget.hpp"
#include "button.hpp"
#include "formatter.hpp"
#include "input.hpp"
#include "logger.hpp"
#include "pathfinding.hpp"
#include "solid_map.hpp"

// A lot in this code scares me. the raw pointers needs changed to intrusive.

namespace 
{
	using namespace KRE;
	const unsigned char RedBorder[] = {0x3d, 0x30, 0xf9};
	const unsigned char BackgroundColor[] = {0x51, 0x6d, 0x6f};

	bool is_pixel_border(const SurfacePtr& s, int x, int y)
	{
		if(x < 0 || y < 0 || x >= s->width() || y >= s->height()) {
			return false;
		}

		const unsigned char* pixel = reinterpret_cast<const unsigned char*>(s->pixels()) + y*s->rowPitch() + x*4;
		for(int n = 0; n != 3; ++n) {
			if(pixel[n] != RedBorder[n]) {
				return false;
			}
		}

		return true;
	}

	bool is_pixel_alpha(const SurfacePtr& s, const point& p)
	{
		const unsigned char* pixel = reinterpret_cast<const unsigned char*>(s->pixels()) + p.y*s->rowPitch() + p.x*4;
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
		int x1 = std::numeric_limits<int>::max();
		int y1 = std::numeric_limits<int>::max();
		int x2 = std::numeric_limits<int>::min();
		int y2 = std::numeric_limits<int>::min();
		if(ox >= s->width() || oy >= s->height()) {
			return rect(ox,oy,2,2);
		}
		const rect r(0, 0, s->width(), s->height());

		if(is_pixel_alpha(s, point(ox, oy))) {
			return rect(ox,oy,2,2);
		}

		typedef pathfinding::GraphNode<point, int> graph_node;
		typedef graph_node::GraphNodePtr graph_node_ptr;
		typedef std::map<point, graph_node_ptr> graph_node_list;
		graph_node_list node_list;
		std::deque<graph_node_ptr> open_list;
		typedef std::pair<point, bool> reachable_node;
		std::vector<reachable_node> reachable;

		bool searching = true;
		try {
			graph_node_ptr current = graph_node_ptr(new graph_node(point(ox, oy)));
			current->setCost(0, 0);
			current->setOnOpenList(true);
			open_list.push_back(current);
			node_list[point(ox, oy)] = current;

			while(searching && !open_list.empty()) {
				current = open_list.front(); open_list.pop_front();
				current->setOnOpenList(false);
				current->setOnClosedList(true);
				if(current->G() <= max_cost) {
					reachable.push_back(reachable_node(current->getNodeValue(), 
						is_pixel_alpha(s, current->getNodeValue())));
				}
				for(auto& p : pathfinding::get_neighbours_from_rect(current->getNodeValue(), 1, 1, r)) {
					graph_node_list::const_iterator neighbour_node = node_list.find(p);
					int g_cost = path_cost_fn(s, p, current->getNodeValue()) + current->G();
					if(neighbour_node == node_list.end()) {
						graph_node_ptr new_node = graph_node_ptr(new graph_node(point(p.x, p.y)));
							new_node->setParent(current);
							new_node->setCost(g_cost, 0);
							new_node->setOnOpenList(true);
							node_list[p] = new_node;
							if(g_cost > max_cost) {
								new_node->setOnClosedList(true);
							} else {
								new_node->setOnOpenList(true);
								open_list.push_back(new_node);
							}
					} else if(neighbour_node->second->isOnClosedList() || neighbour_node->second->isOnOpenList()) {
						if(g_cost < neighbour_node->second->G()) {
							neighbour_node->second->G(g_cost);
							neighbour_node->second->setParent(current);
						}
					} else {
						throw "Path error node on list, but not on open or closed lists";
					}
				}
			}
		} catch(...) {
			LOG_INFO("get_border_rect_heuristic_search(): Caught exception");
		}

		for(const reachable_node& rn : reachable) {
			if(rn.second) {
				if(rn.first.x < x1) x1 = rn.first.x;
				if(rn.first.x > x2) x2 = rn.first.x;
				if(rn.first.y < y1) y1 = rn.first.y;
				if(rn.first.y > y2) y2 = rn.first.y;
			}
		}

		LOG_INFO("CALC RECT " << x1 << "," << y1 << "," << x2 << "," << y2);
		return rect::from_coordinates(x1, y1, x2, y2);
	}

	rect get_border_rect_around_loc(const SurfacePtr& s, int ox, int oy)
	{
		int x = ox, y = oy;
		LOG_INFO("SEARCHING FOR BORDER AROUND " << x << "," << y);
		while(y >= 0 && !is_pixel_border(s, x, y)) {
			--y;
		}

		while(x >= 0 && is_pixel_border(s, x, y)) {
			--x;
		}

		++x;

		LOG_INFO("STEPPED TO " << x << "," << y);

		if(y >= 0 && is_pixel_border(s, x, y)) {
			LOG_INFO("RETURNING " << get_border_rect(s, x, y));
			return get_border_rect(s, x, y);
		} else {
			LOG_INFO("TRYING HEURISTIC SEARCH AROUND " << ox << "," << oy);
			return get_border_rect_heuristic_search(s, ox, oy, 10);
		}
	}

	bool find_full_animation(const SurfacePtr& s, const rect& r, int* pad, int* num_frames, int* frames_per_row)
	{
		const int x = r.x() + r.w()/2;
		const int y = r.y() + r.h()/2;

		int next_x = x + r.w();
		if(next_x >= static_cast<int>(s->width())) {
			LOG_INFO("FAIL FIND " << next_x << " >= " << s->width());
			return false;
		}

		rect next_rect = get_border_rect_around_loc(s, next_x, y);
		LOG_INFO("NEXT RECT: " << next_rect << " VS " << r);
		if(next_rect.w() != r.w() || next_rect.h() != r.h()) {
			return false;
		}

		*pad = next_rect.x() - r.x2();
		*num_frames = 2;

		std::vector<rect> rect_row;
		rect_row.push_back(r);
		rect_row.push_back(next_rect);

		auto new_r = get_border_rect_around_loc(s, next_x + r.w() + *pad, y);
		auto old_r = rect(next_rect.x() + next_rect.w() + *pad, y, r.w(), r.h());
		LOG_INFO("SETTING... " << new_r << " VS " << old_r);

		while(next_x + r.w() + *pad < static_cast<int>(s->width()) && new_r == old_r) {
				LOG_INFO("ITER");
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
				auto rr = get_border_rect_around_loc(s, next_rect.x() + next_rect.w()/2, next_rect.y() + next_rect.h()/2);
				LOG_INFO("MATCHING: " << rr << " VS " << next_rect);
				if(next_rect.y2() >= static_cast<int>(s->height()) || rr != next_rect) {
					LOG_INFO("MISMATCH: " << index << "/" << rect_row.size() << " -- " << rr << " VS " << next_rect);
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
		: Widget(v,e), 
		  cycle_(0), 
		  zoom_label_(nullptr), 
		  pos_label_(nullptr), 
		  scale_(0), 
		  anchor_x_(-1), 
		  anchor_y_(-1), 
		  anchor_pad_(-1), 
		  has_motion_(false), 
		  dragging_sides_bitmap_(0), 
		  moving_solid_rect_(false), 
		  anchor_solid_x_(-1), 
		  anchor_solid_y_(-1)
	{
		ASSERT_LOG(getEnvironment() != 0, "You must specify a callable environment");

		using namespace std::placeholders;

		if(v.has_key("on_rect_change")) {
			rect_handler_ = std::bind(&AnimationPreviewWidget::rectHandlerDelegate, this, _1);
			ffl_rect_handler_ = getEnvironment()->createFormula(v["on_rect_change"]);
		}
		if(v.has_key("on_pad_change")) {
			pad_handler_ = std::bind(&AnimationPreviewWidget::padHandlerDelegate, this, _1);
			ffl_pad_handler_ = getEnvironment()->createFormula(v["on_pad_change"]);
		}
		if(v.has_key("on_frames_change")) {
			num_frames_handler_ = std::bind(&AnimationPreviewWidget::numFramesHandlerDelegate, this, _1);
			ffl_num_frames_handler_ = getEnvironment()->createFormula(v["on_frames_change"]);
		}
		if(v.has_key("on_frames_per_row_change")) {
			frames_per_row_handler_ = std::bind(&AnimationPreviewWidget::framesPerRowHandlerDelegate, this, _1);
			ffl_frames_per_row_handler_ = getEnvironment()->createFormula(v["on_frames_per_row_change"]);
		}
		if(v.has_key("on_solid_change")) {
			solid_handler_ = std::bind(&AnimationPreviewWidget::solidHandlerDelegate, this, _1, _2);
			ffl_solid_handler_ = getEnvironment()->createFormula(v["on_solid_change"]);
		}
	
		try {
			setObject(v);
		} catch(type_error&) {
		} catch(Frame::Error&) {
		} catch(validation_failure_exception&) {
		} catch(KRE::ImageLoadError&) {
		}
	}

	AnimationPreviewWidget::AnimationPreviewWidget(variant obj) 
		: cycle_(0), 
		  zoom_label_(nullptr), 
		  pos_label_(nullptr), 
		  scale_(0), 
		  anchor_x_(-1), 
		  anchor_y_(-1), 
		  anchor_pad_(-1), 
		  has_motion_(false), 
		  dragging_sides_bitmap_(0), 
		  moving_solid_rect_(false), 
		  anchor_solid_x_(-1), 
		  anchor_solid_y_(-1)
	{
		setEnvironment();
		setObject(obj);
	}

	AnimationPreviewWidget::AnimationPreviewWidget(const AnimationPreviewWidget& a)
		: obj_(),
		  frame_(nullptr),
		  cycle_(a.cycle_), 
		  widgets_(a.widgets_),
		  zoom_label_(a.zoom_label_), 
		  pos_label_(a.pos_label_), 
		  scale_(a.scale_), 
		  src_rect_(a.src_rect_),
		  dst_rect_(a.dst_rect_),
		  anchor_x_(a.anchor_x_), 
		  anchor_y_(a.anchor_y_),
		  anchor_area_(a.anchor_area_),
		  anchor_pad_(a.anchor_pad_), 
		  has_motion_(a.has_motion_), 
		  locked_focus_(a.locked_focus_),
		  dragging_sides_bitmap_(a.dragging_sides_bitmap_), 
		  solid_rect_(a.solid_rect_),
		  moving_solid_rect_(a.moving_solid_rect_), 
		  anchor_solid_x_(a.anchor_solid_x_), 
		  anchor_solid_y_(a.anchor_solid_y_),
		  rect_handler_(a.rect_handler_),
		  pad_handler_(a.pad_handler_),
		  num_frames_handler_(a.num_frames_handler_),
		  frames_per_row_handler_(a.frames_per_row_handler_),
		  solid_handler_(a.solid_handler_),
		  ffl_rect_handler_(a.ffl_rect_handler_),
		  ffl_pad_handler_(a.ffl_pad_handler_),
		  ffl_num_frames_handler_(a.ffl_num_frames_handler_),
		  ffl_frames_per_row_handler_(a.ffl_frames_per_row_handler_),
		  ffl_solid_handler_(a.ffl_solid_handler_)
	{
		setObject(a.obj_);
	}

	void AnimationPreviewWidget::init()
	{
		widgets_.clear();

		Button* b = new Button("+", std::bind(&AnimationPreviewWidget::zoomIn, this));
		b->setLoc(x() + 10, y() + height() - b->height() - 5);
		widgets_.push_back(WidgetPtr(b));
		b = new Button("-", std::bind(&AnimationPreviewWidget::zoomOut, this));
		b->setLoc(x() + 40, y() + height() - b->height() - 5);
		widgets_.push_back(WidgetPtr(b));

		zoom_label_ = new Label("Zoom: 100%");
		zoom_label_->setLoc(b->x() + b->width() + 10, b->y());
		widgets_.push_back(WidgetPtr(zoom_label_));

		pos_label_ = new Label("");
		pos_label_->setLoc(zoom_label_->x() + zoom_label_->width() + 8, zoom_label_->y());
		widgets_.push_back(WidgetPtr(pos_label_));

		b = new Button("Reset", std::bind(&AnimationPreviewWidget::resetRect, this));
		b->setLoc(pos_label_->x() + pos_label_->width() + 58, y() + height() - b->height() - 5);
		widgets_.push_back(WidgetPtr(b));
	}

	void AnimationPreviewWidget::setObject(variant obj)
	{
		if(obj == obj_) {
			return;
		}

		Frame* f = new Frame(obj);

		obj_ = obj;
		frame_.reset(f);
		cycle_ = 0;
	}


	void AnimationPreviewWidget::handleProcess()
	{
		for(auto w : widgets_) {
			w->process();
		}
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
			KRE::ClipScope::Manager clip_scope(image_area, canvas->getCamera());

			const bool view_locked = mouse_buttons && locked_focus_.w()*locked_focus_.h() > 0;

			rect focus_area;
			if(frame_->numFramesPerRow() == 0) {
				focus_area = rect();
			} else {
				focus_area = rect(frame_->area().x(), frame_->area().y(),
					  (frame_->area().w() + frame_->pad())*frame_->numFramesPerRow(),
					  (frame_->area().h() + frame_->pad())*(frame_->numFrames()/frame_->numFramesPerRow() + (frame_->numFrames()%frame_->numFramesPerRow() ? 1 : 0)));
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

			const int show_width = static_cast<int>(image_area.w()/scale);
			const int show_height = static_cast<int>(image_area.h()/scale);

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
			if(x2 > static_cast<int>(image_texture->width())) {
				x1 -= (x2 - image_texture->width());
				x2 = image_texture->width();
				if(x1 < 0) {
					x1 = 0;
				}
			}

			if(y2 > static_cast<int>(image_texture->height())) {
				y1 -= (y2 - image_texture->height());
				y2 = image_texture->height();
				if(y1 < 0) {
					y1 = 0;
				}
			}

			int xpos = image_area.x();
			int ypos = image_area.y();

			src_rect_ = rect(x1, y1, x2 - x1, y2 - y1);
			dst_rect_ = rect(xpos, ypos, static_cast<int>((x2-x1)*scale), static_cast<int>((y2-y1)*scale));

			canvas->blitTexture(image_texture, src_rect_, 0, dst_rect_);

			if(!mouse_buttons) {
				dragging_sides_bitmap_ = 0;
			}

			for(int n = 0; n != frame_->numFrames(); ++n) {
				const int row = n/frame_->numFramesPerRow();
				const int col = n%frame_->numFramesPerRow();
				const int x = static_cast<int>(xpos - x1*scale + (frame_->area().x() + col*(frame_->area().w()+frame_->pad()))*scale);
				const int y = static_cast<int>(ypos - y1*scale + (frame_->area().y() + row*(frame_->area().h()+frame_->pad()))*scale);
				const rect box(x, y, static_cast<int>(frame_->area().w()*scale), static_cast<int>(frame_->area().h()*scale));
				{
					KRE::Color color(255, 255, n == 0 ? 0 : 255, frame_->frameNumber(cycle_) == n ? 0xFF : 0x88);
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
				
					if(rect_chosen || (!(dragging_sides_bitmap_&TOP_SIDE) && pointInRect(point(mousex, mousey), rect(box.x(), box.y2()-4, box.w(), 9)))) {
						dragging_sides_bitmap_ |= BOTTOM_SIDE;
						canvas->drawSolidRect(rect(box.x(), box.y2()-1, box.w(), 2), KRE::Color::colorRed());
					}

					if(rect_chosen || pointInRect(point(mousex, mousey), rect(box.x()-4, box.y(), 9, box.h()))) {
						dragging_sides_bitmap_ |= LEFT_SIDE;
						canvas->drawSolidRect(rect(box.x()-1, box.y(), 2, box.h()), KRE::Color::colorRed());
					}
				
					if(rect_chosen || (!(dragging_sides_bitmap_&LEFT_SIDE) && pointInRect(point(mousex, mousey), rect(box.x2()-4, box.y(), 9, box.h())))) {
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

				int xpos1 = static_cast<int>(xpos - x1*scale + p1.x*scale);
				int xpos2 = static_cast<int>(xpos - x1*scale + p2.x*scale);
				int ypos1 = static_cast<int>(ypos - y1*scale + p1.y*scale);
				int ypos2 = static_cast<int>(ypos - y1*scale + p2.y*scale);
			
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

		const int framex = static_cast<int>(preview_area.x() + (preview_area.w() - frame_->width()*scale)/2);
		const int framey = static_cast<int>(preview_area.y() + (preview_area.h() - frame_->height()*scale)/2);
		frame_->draw(nullptr, framex, framey, true, false, cycle_, 0, scale);
		if(++cycle_ >= frame_->duration()) {
			cycle_ = 0;
		}

		solid_rect_ = rect();

		ConstSolidInfoPtr solid = frame_->solid();
		if(solid && solid->area().w()*solid->area().h() > 0) {
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

		const int x = static_cast<int>(src_rect_.x() + (src_rect_.w() + 1.0)*xpos);
		const int y = static_cast<int>(src_rect_.y() + (src_rect_.h() + 1.0)*ypos);
	
		return point(x, y);
	}

	bool AnimationPreviewWidget::handleEvent(const SDL_Event& event, bool claimed)
	{
		for(WidgetPtr w : widgets_) {
			claimed = w->processEvent(point(0,0), event, claimed) || claimed;
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
				}

				if(dragging_sides_bitmap_&RIGHT_SIDE) {
					x2 += delta_x;
				}

				if(dragging_sides_bitmap_&TOP_SIDE) {
					y1 += delta_y;
				}

				if(dragging_sides_bitmap_&BOTTOM_SIDE) {
					y2 += delta_y;
				}

				if(x1 > x2 - 1 && (dragging_sides_bitmap_&LEFT_SIDE)) {
					x1 = x2 - 1;
				}

				if(x2 < x1 + 1 && (dragging_sides_bitmap_&RIGHT_SIDE)) {
					x2 = x1 + 1;
				}

				if(y1 > y2 - 1 && (dragging_sides_bitmap_&TOP_SIDE)) {
					y1 = y2 - 1;
				}

				if(y2 < y1 + 1 && (dragging_sides_bitmap_&BOTTOM_SIDE)) {
					y2 = y1 + 1;
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
				claimed = true;
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

			if(pointInRect(p, rect(x(), y(), width(), height()))) {
				claimed = true;
			}
		} else if(event.type == SDL_MOUSEBUTTONUP && anchor_x_ != -1) {

			const SDL_MouseButtonEvent& e = event.button;
			point anchor(anchor_x_, anchor_y_);
			point p(e.x, e.y);
			if(anchor == p && !has_motion_) {
				claimed = claimMouseEvents();
				p = mousePointToImageLoc(p);

				auto surf = KRE::Surface::create(obj_["image"].as_string(), KRE::SurfaceFlags::NO_ALPHA_FILTER|KRE::SurfaceFlags::NO_CACHE);

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
								LOG_INFO("SETTING NUM FRAMES TO " << num_frames);
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

			if(pointInRect(p, rect(x(), y(), width(), height()))) {
				claimed = true;
			}
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

	void AnimationPreviewWidget::setRectHandler(std::function<void(rect)> handler)
	{
		rect_handler_ = handler;
	}

	void AnimationPreviewWidget::setPadHandler(std::function<void(int)> handler)
	{
		pad_handler_ = handler;
	}

	void AnimationPreviewWidget::setNumFramesHandler(std::function<void(int)> handler)
	{
		num_frames_handler_ = handler;
	}

	void AnimationPreviewWidget::setFramesPerRowHandler(std::function<void(int)> handler)
	{
		frames_per_row_handler_ = handler;
	}

	void AnimationPreviewWidget::setSolidHandler(std::function<void(int,int)> handler)
	{
		solid_handler_ = handler;
	}

	void AnimationPreviewWidget::rectHandlerDelegate(rect r)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("new_rect", r.write());
			variant value = ffl_rect_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("AnimationPreviewWidget::rectHandlerDelegate() called without environment!");
		}
	}

	void AnimationPreviewWidget::padHandlerDelegate(int pad)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("new_pad", variant(pad));
			variant value = ffl_pad_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("AnimationPreviewWidget::padHandlerDelegate() called without environment!");
		}
	}

	void AnimationPreviewWidget::numFramesHandlerDelegate(int frames)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("new_frames", variant(frames));
			variant value = ffl_num_frames_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("AnimationPreviewWidget::numFramesHandlerDelegate() called without environment!");
		}
	}

	void AnimationPreviewWidget::framesPerRowHandlerDelegate(int frames_per_row)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("new_frames_per_row", variant(frames_per_row));
			variant value = ffl_frames_per_row_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("AnimationPreviewWidget::framesPerRowHandlerDelegate() called without environment!");
		}
	}

	void AnimationPreviewWidget::solidHandlerDelegate(int x, int y)
	{
		using namespace game_logic;
		if(getEnvironment()) {
			MapFormulaCallablePtr callable = MapFormulaCallablePtr(new MapFormulaCallable(getEnvironment()));
			callable->add("new_solidx", variant(x));
			callable->add("new_solidy", variant(y));
			variant value = ffl_solid_handler_->execute(*callable);
			getEnvironment()->executeCommand(value);
		} else {
			LOG_ERROR("AnimationPreviewWidget::solidHandlerDelegate() called without environment!");
		}
	}

	WidgetPtr AnimationPreviewWidget::clone() const
	{
		AnimationPreviewWidget* ap = new AnimationPreviewWidget(*this);
		ap->init();
		return WidgetPtr(ap);
	}

	BEGIN_DEFINE_CALLABLE(AnimationPreviewWidget, Widget)
		DEFINE_FIELD(object, "any")
			return obj.obj_;
		DEFINE_SET_FIELD
			try {
				obj.setObject(value);
			} catch(type_error&) {
			} catch(Frame::Error&) {
			} catch(validation_failure_exception&) {
			} catch(KRE::ImageLoadError&) {
			}
	END_DEFINE_CALLABLE(AnimationPreviewWidget)

}

#endif // !NO_EDITOR

