/*
   Copyright 2014 Kristina Simpson <sweet.kristas@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <sstream>
#include "TexPack.hpp"

namespace KRE
{
	namespace
	{
		static int counter = 0;

		class tex_node
		{
		public:
			tex_node(SurfacePtr surface, const rect& r) : r_(r), surface_(surface) {
				child_[0] = child_[1] = nullptr;
			}
			~tex_node() {
				if(child_[0]) {
					delete child_[0];
				}
				if(child_[1]) {
					delete child_[1];
				}
			}
			std::vector<rect> split_rect_vertically(int h) const {
				std::vector<rect> res;
				res.emplace_back(rect(r_.x(), r_.y(), r_.w(), h));
				res.emplace_back(rect(r_.x(), r_.y()+h, r_.w(), r_.h()-h));
				return res;
			}
			std::vector<rect> split_rect_horizontally(int w) const {
				std::vector<rect> res;
				res.emplace_back(rect(r_.x(), r_.y(), w, r_.h()));
				res.emplace_back(rect(r_.x()+w, r_.y(), r_.w()-w, r_.h()));
				return res;
			}
			void split_node(SurfacePtr surface, const rect& r) {
				ASSERT_LOG(is_leaf(), "Attempt to split non-leaf");
				ASSERT_LOG(can_contain(rect(0, 0, r.w(), r.h())), "Node to small to fit image.");
				if(r.w() == r_.w() && r.h() == r_.h()) {
					surface_ = surface;
				} else {
					if(should_split_vertically(r.w(), r.h())) {
						auto vr = split_rect_vertically(r.h());
						child_[0] = new tex_node(surface, vr[0]);
						child_[1] = new tex_node(surface, vr[1]);
					} else {
						auto hr = split_rect_horizontally(r.w());
						child_[0] = new tex_node(surface, hr[0]);
						child_[1] = new tex_node(surface, hr[1]);
					}
					child_[0]->split_node(surface, r);
				}
			}
			bool grow_node(SurfacePtr surface, const rect& r, int max_w, int max_h) {
				ASSERT_LOG(!is_empty_leaf(), "Attempt to grow empty leaf.");
				if(r_.w() + r.w() > max_w || r_.h() + r.h() > max_h) {
					return false;
				}
				auto tn = new tex_node(surface, r_);
				tn->child_[0] = child_[0];
				tn->child_[1] = child_[1];
				surface_ = nullptr;
				child_[0] = tn;
				if(should_grow_vertically(r.w(), r.h())) {
					child_[1] = new tex_node(surface, rect(r_.x(), r_.y()+r_.h(), r_.w(), r.h()));
					r_ = rect(r_.x(), r_.y(), r_.w(), r_.h() + r.h());
				} else {
					child_[1] = new tex_node(surface, rect(r_.x()+r_.w(), r_.y(), r.w(), r_.h()));
					r_ = rect(r_.x(), r_.y(), r_.w() + r.w(), r_.h());
				}
				child_[1]->split_node(surface, r);
				return true;
			}
			bool should_split_vertically(int w, int h) const {
				if(r_.w() == w) {
					return true;
				} else if(r_.h() == h) {
					return false;
				}
				auto vr = split_rect_vertically(h);
				auto hr = split_rect_horizontally(w);
				return vr[1].perimeter() > hr[1].perimeter();
			}
			bool should_grow_vertically(int w, int h) const {
				bool can_grow_vert = r_.w() >= w;
				bool can_grow_horz = r_.h() >= h;
				ASSERT_LOG(can_grow_vert || can_grow_horz, "Unable to grow any further.");
				if(can_grow_vert && !can_grow_horz) {
					return true;
				}
				if(!can_grow_vert && can_grow_horz) {
					return false;
				}
				return r_.h() + h < r_.w() + w;
			}
			bool is_empty_leaf() const { return is_leaf() && surface_ == nullptr; }
			bool is_leaf() const { return child_[0] == nullptr && child_[1] == nullptr; }
			bool can_contain(const rect& r) const { return r.w() <= r_.w() && r.h() <= r_.h(); }
			
			tex_node* get_left_child() { return child_[0]; }
			
			tex_node* get_right_child() { return child_[1]; }
			
			const rect& get_rect() const { return r_; }
			
			void blit(SurfacePtr dest, std::vector<rect>* rects) {
				if(child_[0]) {
					child_[0]->blit(dest, rects);
				}
				if(surface_) {
					dest->blitToScaled(surface_, rect(r_.x(), r_.y(), r_.w(), r_.h()), r_);
					rects->emplace_back(r_);
				}
				if(child_[1]) {
					child_[1]->blit(dest, rects);
				}
			}
		private:
			rect r_;
			tex_node* child_[2];
			SurfacePtr surface_;
		};

		tex_node* find_empty_leaf(tex_node* tn, SurfacePtr surface, const rect& r) 
		{
			if(tn->is_empty_leaf()) {
				return tn->can_contain(rect(0, 0, r.w(), r.h())) ? tn : nullptr;
			}
			if(tn->is_leaf()) {
				return nullptr;
			}
			auto leaf = find_empty_leaf(tn->get_left_child(), surface, r);
			if(leaf) { 
				return leaf;
			}
			return find_empty_leaf(tn->get_right_child(), surface, r);
		}
	}

	Packer::Packer(const std::vector<SurfaceAreas>& inp, int max_width, int max_height)
	{
		std::vector<tex_node*> root;
		for(auto& img : inp) {
			for(auto& r : img.rects) {
				if(root.empty()) {
					root.emplace_back(new tex_node(img.surface, r));
					root.back()->split_node(img.surface, r);
				}
				auto leaf =	find_empty_leaf(root.back(), img.surface, r);
				if(leaf) {
					leaf->split_node(img.surface, r);
				} else {
					if(!root.back()->grow_node(img.surface, r, max_width, max_height)) {
						root.emplace_back(new tex_node(img.surface, r));
						root.back()->split_node(img.surface, r);
					}
				}
			}
		}

		ASSERT_LOG(root.size() <= 1, "Currently we are limiting things to one surface.");

		// process root
		for(auto& n : root) {
			std::stringstream ss;
			outp_ = Surface::create(n->get_rect().w(), n->get_rect().h(), PixelFormat::PF::PIXELFORMAT_RGBA8888);
			n->blit(outp_, &out_rects_);
			ss << "temp/nn" << counter++ << ".png";
			outp_->savePng(ss.str());
		}

		// delete root.
		for(auto& n : root) {
			delete n;
		}
	}
}
