#include <algorithm>

#include "asserts.hpp"
#include "profile_timer.hpp"
#include "scrollable.hpp"

#include "easy_svg.hpp"
#include "WindowManager.hpp"

namespace scrollable
{
	using namespace KRE;

	namespace
	{
		static const std::vector<std::string> arrow_files{ 
			"scrollbar-up-arrow.svg", 
			"scrollbar-down-arrow.svg", 
			"scrollbar-left-arrow.svg", 
			"scrollbar-right-arrow.svg", 
			"scrollbar-background.svg",
			"scrollbar-thumb.svg",
		};
		static std::vector<point> arrow_sizes{ 
			point(64, 64), 
			point(64, 64), 
			point(64, 64), 
			point(64, 64),
			point(64, 64),
			point(64, 64),
		};

		void add_rect(std::vector<vertex_texcoord>* vert, const rect& r, const rectf& t)
		{
			ASSERT_LOG(vert != nullptr, "vert was NULL.");
			vert->emplace_back(glm::vec2(r.x1(), r.y1()), glm::vec2(t.x1(), t.y1()));
			vert->emplace_back(glm::vec2(r.x2(), r.y1()), glm::vec2(t.x2(), t.y1()));
			vert->emplace_back(glm::vec2(r.x1(), r.y2()), glm::vec2(t.x1(), t.y2()));

			vert->emplace_back(glm::vec2(r.x2(), r.y1()), glm::vec2(t.x2(), t.y1()));
			vert->emplace_back(glm::vec2(r.x1(), r.y2()), glm::vec2(t.x1(), t.y2()));
			vert->emplace_back(glm::vec2(r.x2(), r.y2()), glm::vec2(t.x2(), t.y2()));
		}

		KRE::TexturePtr get_scrollbar_texture(std::vector<rectf>* tc)
		{
			static KRE::TexturePtr tex;
			static std::vector<rectf> tex_coords;
			if(tex == nullptr) {
				tex = svgs_to_single_texture(arrow_files, arrow_sizes, &tex_coords);
				tex->setAddressModes(0, Texture::AddressMode::WRAP, Texture::AddressMode::WRAP);		
			}
			if(tc != nullptr) {
				*tc = tex_coords;
			}
			return tex;
		}

		template<typename D=float>
		inline D ease_in(D t, D p1, D p2, D d) 
		{
			D c = p2 - p1;
			t /= d;
			t *= t;
			return t * c + p1;
		}

		template<typename D=float>
		inline D ease_out(D t, D p1, D p2, D d) 
		{
			D c = p2 - p1;
			t /= d;
			t *= (t - D(2));
			return -t * c + p1;
		}

	}

	Scrollbar::Scrollbar(Direction d, change_handler onchange, const rect& loc, const point& offset)
		: SceneObject("Scrollbar"),
		  on_change_(onchange),
		  dir_(d),
		  min_range_(0),
		  max_range_(100),
		  scroll_pos_(0),
		  page_size_(),
		  line_size_(),
		  loc_(loc),
		  up_arrow_area_(),
		  down_arrow_area_(),
		  left_arrow_area_(),
		  right_arrow_area_(),
		  thumb_area_(),
		  background_loc_(),
		  visible_(false),
		  thumb_color_(192, 192, 192),
		  thumb_selected_color_(128, 128, 128),
		  thumb_mouseover_color_(224, 224, 224),
		  background_color_(96, 96, 96),
		  vertices_arrows_(nullptr),
		  vertices_background_(nullptr),
		  vertices_thumb_(nullptr),
		  attr_arrows_(nullptr),
		  attr_background_(nullptr),
		  attr_thumb_(nullptr),
		  changed_(true),
		  thumb_dragging_(false),
		  thumb_mouseover_(false),
		  thumb_update_(false),
		  mouse_in_scrollbar_(false),
		  drag_start_position_(),
		  offset_(offset),
		  fade_enabled_(false),
		  fade_triggered_(false),
		  fade_in_time_(0.5),
		  fade_out_time_(0.5),
		  transition_(0),
		  start_time_(0),
		  fade_out_start_(0),
		  fade_in_on_mouseenter_(false),
		  fade_out_on_mouseleave_(false),
		  fading_in_(true),
		  start_alpha_(255),
		  alpha_(255)
	{
		setTexture(get_scrollbar_texture(nullptr));

		init();
	}

	Scrollbar::~Scrollbar()
	{
	}

	void Scrollbar::setScrollPosition(int pos)
	{
		scroll_pos_ = pos;
		if(scroll_pos_ < min_range_) {
			//LOG_WARN("Scrollbar::setScrollPosition() setting scroll position outside minimum range: " << pos << " < " << min_range_ << " defaulting to minimum.");
			scroll_pos_ = min_range_;
		}
		if(scroll_pos_ > max_range_) {
			//LOG_WARN("Scrollbar::setScrollPosition() setting scroll position outside maximum range: " << pos << " > " << max_range_ << " defaulting to maximum.");
			scroll_pos_ = max_range_;
		}
		computeThumbPosition();

		if(on_change_) {
			on_change_(scroll_pos_);
		}
	}

	void Scrollbar::setOnChange(change_handler onchange) 
	{ 
		on_change_ = onchange;
		if(on_change_) {
			on_change_(scroll_pos_);
		}
	}

	void Scrollbar::init()
	{
		// number of different positions that we can scroll to.
		const int range = max_range_ - min_range_ + 1;

		if(dir_ == Direction::VERTICAL) {
			up_arrow_area_ = rect(loc_.x(), loc_.y(), loc_.w(), loc_.w());
			down_arrow_area_ = rect(loc_.x(), loc_.y2() - loc_.w(), loc_.w(), loc_.w());

			background_loc_ = rect(loc_.x(), loc_.y() + up_arrow_area_.h(), loc_.w(), loc_.h() - down_arrow_area_.h() - up_arrow_area_.h());
		} else {
			left_arrow_area_ = rect(loc_.x(), loc_.y(), loc_.h(), loc_.h());
			right_arrow_area_ = rect(loc_.x2() - loc_.h(), loc_.y(), loc_.h(), loc_.h());

			background_loc_ = rect(loc_.x() + left_arrow_area_.w(), loc_.y(), loc_.w() - right_arrow_area_.w() - left_arrow_area_.w(), loc_.h());
		}
		computeThumbPosition();

		changed_ = true;
	}

	void Scrollbar::computeThumbPosition()
	{
		const int range = max_range_ - min_range_ + 1;
		if(dir_ == Direction::VERTICAL) {
			const int min_length = std::max(loc_.w(), (loc_.h() - up_arrow_area_.h() - down_arrow_area_.h()) / range);
			const int y_loc = std::min(std::max(static_cast<int>(static_cast<float>(scroll_pos_) / range * background_loc_.h()) + background_loc_.y() - min_length/2, background_loc_.y()),
				background_loc_.y2() - min_length);
			thumb_area_ = rect(loc_.x(), y_loc, loc_.w(), min_length);
		} else {
			const int min_length = std::max(loc_.h(), (loc_.w() - up_arrow_area_.w() - down_arrow_area_.w()) / range);
			const int x_loc = std::min(std::max(static_cast<int>(static_cast<float>(scroll_pos_) / range * background_loc_.w()) + background_loc_.x1() - min_length/2, background_loc_.x1()),
				background_loc_.x2() - min_length);
			thumb_area_ = rect(x_loc, loc_.y(), min_length, loc_.h());
		}
		thumb_update_ = true;
	}

	void Scrollbar::updateColors()
	{
		transition_ = profile::get_tick_time();
		if(fade_enabled_ && fade_triggered_) {
			float delta = static_cast<float>(transition_ - start_time_)/1000.0f;
			if(fading_in_) {
				// p1 should be current alpha value
				alpha_ = static_cast<int>(255.f * ease_in(delta, start_alpha_/255.f, 1.0f, fade_in_time_));
				if(delta >= fade_in_time_) {
					fade_triggered_ = false;
				}
			} else {
				if(fade_out_start_ == 0 ||  transition_ > fade_out_start_) {
					fade_out_start_ = 0;
					// p1 should be current alpha value
					alpha_ = static_cast<int>(255.f * ease_out(delta, start_alpha_ / 255.f, 0.0f, fade_out_time_));
					if(delta >= fade_out_time_) {
						fade_triggered_ = false;
					}
				}
			}
			alpha_ = std::max(0, std::min(255, alpha_));
		}
		if(thumb_dragging_ || mouse_in_scrollbar_) {
			alpha_ = 255;
		}
		
		if(attr_background_) {
			Color c = background_color_;
			if(fade_enabled_) {
				c.setAlpha(alpha_);
			}
			attr_background_->setColor(c);
		}
		if(attr_arrows_) {
			Color c(Color::colorWhite());
			if(fade_enabled_) {
				c.setAlpha(alpha_);
			}
			attr_arrows_->setColor(c);
		}
		if(attr_thumb_) {
			Color c = thumb_dragging_ ? thumb_selected_color_ : thumb_mouseover_ ? thumb_mouseover_color_  : thumb_color_;
			if(fade_enabled_) {
				c.setAlpha(alpha_);
			}
			attr_thumb_->setColor(c);
		}
	}

	void Scrollbar::triggerFadeIn()
	{
		if(!fade_triggered_ && !fading_in_) {
			fade_triggered_ = true;
			fading_in_ = true;
			start_alpha_ = alpha_;
			transition_ = 0;
			start_time_ = profile::get_tick_time();
			fade_out_start_ = 0;
		}
	}

    void Scrollbar::triggerFadeOut()
	{
		if(!fade_triggered_ && fading_in_) {
			fade_triggered_ = true;
			fading_in_ = false;
			start_alpha_ = alpha_;
			transition_ = 0;
			fade_out_start_ = profile::get_tick_time() + 750;
			start_time_ = fade_out_start_;
		}
	}

	void Scrollbar::setLocation(int x, int y)
	{
		loc_.set_x(x);
		loc_.set_y(y);
		init();
	}

	void Scrollbar::setDimensions(int w, int h)
	{
		loc_.set_wh(w, h);
		init();
	}

	void Scrollbar::setRect(const rect& r)
	{
		loc_ = r;
		init();
	}

	void Scrollbar::enableFade(float in_time, float out_time, bool in_on_mouseenter, bool out_on_mouseleave)
	{
		fade_enabled_ = true;
		transition_ = 0;
		fade_in_time_ = in_time;
		fade_out_time_ = out_time;
		fade_in_on_mouseenter_ = in_on_mouseenter;
		fade_out_on_mouseleave_ = out_on_mouseleave;
	}

	void Scrollbar::preRender(const WindowPtr& wm)
	{
		static std::vector<rectf> texcoords;
		if(texcoords.empty()) {
			get_scrollbar_texture(&texcoords);
		}
		std::vector<vertex_texcoord> vt;

		if(changed_) {
			changed_ = false;

			if(vertices_arrows_ == nullptr) {
				attr_background_ = DisplayDevice::createAttributeSet(true, false, false);
				attr_background_->setDrawMode(DrawMode::TRIANGLES);
				vertices_background_ = std::make_shared<Attribute<vertex_texcoord>>(AccessFreqHint::DYNAMIC);
				vertices_background_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
				vertices_background_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));
				attr_background_->addAttribute(vertices_background_);
				addAttributeSet(attr_background_);

				attr_arrows_ = DisplayDevice::createAttributeSet(false, false, false);
				attr_arrows_->setDrawMode(DrawMode::TRIANGLES);
				vertices_arrows_ = std::make_shared<Attribute<vertex_texcoord>>(AccessFreqHint::DYNAMIC);
				vertices_arrows_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
				vertices_arrows_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));
				attr_arrows_->addAttribute(vertices_arrows_);
				addAttributeSet(attr_arrows_);

				attr_thumb_ = DisplayDevice::createAttributeSet(false, false, false);
				attr_thumb_->setDrawMode(DrawMode::TRIANGLES);
				vertices_thumb_ = std::make_shared<Attribute<vertex_texcoord>>(AccessFreqHint::DYNAMIC);
				vertices_thumb_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
				vertices_thumb_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));
				attr_thumb_->addAttribute(vertices_thumb_);
				addAttributeSet(attr_thumb_);
			}

			add_rect(&vt, background_loc_, texcoords[4]);
			attr_background_->setCount(vt.size());
			vertices_background_->update(&vt);

			vt.clear();
			add_rect(&vt, thumb_area_, texcoords[5]);
			attr_thumb_->setCount(vt.size());
			vertices_thumb_->update(&vt);

			vt.clear();
			add_rect(&vt, dir_ == Direction::VERTICAL ? up_arrow_area_ : left_arrow_area_, dir_ == Direction::VERTICAL ? texcoords[0] : texcoords[2]);
			add_rect(&vt, dir_ == Direction::VERTICAL ? down_arrow_area_ : right_arrow_area_, dir_ == Direction::VERTICAL ? texcoords[1] : texcoords[3]);
			attr_arrows_->setCount(vt.size());
			vertices_arrows_->update(&vt);
		}

		if(thumb_update_) {
			thumb_update_ = false;
			vt.clear();
			add_rect(&vt, thumb_area_, texcoords[5]);
			attr_thumb_->setCount(vt.size());
			vertices_thumb_->update(&vt);
		}
		updateColors();
	}

	bool Scrollbar::handleMouseMotion(bool claimed, const point& mp, unsigned keymod)
	{
		point p = mp - offset_;
		if(!claimed && geometry::pointInRect(p, loc_)) {
			claimed = true;

			mouse_in_scrollbar_ = true;
			if(fade_enabled_ && fade_in_on_mouseenter_ && fade_triggered_ == false) {
				triggerFadeIn();
			}

			// test in the arrow regions and the thumb region.
			if(geometry::pointInRect(p, dir_ == Direction::VERTICAL ? up_arrow_area_ : left_arrow_area_)) {
				// XXX
				thumb_mouseover_ = false;
			} else if (geometry::pointInRect(p, dir_ == Direction::VERTICAL ? down_arrow_area_ : right_arrow_area_)) {
				// XXX
				thumb_mouseover_ = false;
			} else if (geometry::pointInRect(p, thumb_area_)) {
				thumb_mouseover_ = true;
			} else {
				thumb_mouseover_ = false;
			}
		} else {
			mouse_in_scrollbar_ = false;
			thumb_mouseover_ = false;
	
			if(fade_enabled_ && fade_out_on_mouseleave_ && fade_triggered_ == false) {
				triggerFadeOut();
			}
		}

		if(thumb_dragging_) {
			const int range = max_range_ - min_range_ + 1;
			float pos;
			if(dir_ == Direction::VERTICAL) {
				pos = (static_cast<float>(p.y - background_loc_.y1()) / background_loc_.h()) * range;
			} else {
				pos = (static_cast<float>(p.x - background_loc_.x1()) / background_loc_.w()) * range;
			}
			setScrollPosition(static_cast<int>(pos));
		}

		return claimed;
	}

	bool Scrollbar::handleMouseButtonUp(bool claimed, const point& mp, unsigned buttons, unsigned keymod)
	{
		point p = mp - offset_;
		if(!claimed && geometry::pointInRect(p, loc_)) {
			claimed = true;
		}
		if(thumb_dragging_) {
			claimed = true;
			thumb_dragging_ = false;
			SDL_CaptureMouse(SDL_FALSE);
		}
		return claimed;
	}

	bool Scrollbar::handleMouseButtonDown(bool claimed, const point & mp, unsigned buttons, unsigned keymod)
	{
		point p = mp - offset_;
		if(!claimed && geometry::pointInRect(p, loc_)) {
			claimed = true;
			if(geometry::pointInRect(p, dir_ == Direction::VERTICAL ? up_arrow_area_ : left_arrow_area_)) {
				setScrollPosition(scroll_pos_ - getLineSize());
			} else if (geometry::pointInRect(p, dir_ == Direction::VERTICAL ? down_arrow_area_ : right_arrow_area_)) {
				setScrollPosition(scroll_pos_ + getLineSize()); 
			} else if (geometry::pointInRect(p, thumb_area_)) {
				thumb_dragging_ = true;
				drag_start_position_ = p;
				SDL_CaptureMouse(SDL_TRUE);
			} else {
				// mouse down somewhere else on the scrollbar.
				const int range = max_range_ - min_range_ + 1;
				int pos;
				if(dir_ == Direction::VERTICAL) {
					pos = static_cast<int>((static_cast<float>(p.y - background_loc_.y1()) / background_loc_.h()) * range);
				} else {
					pos = static_cast<int>((static_cast<float>(p.x - background_loc_.x1()) / background_loc_.w()) * range);
				}
				if(pos < scroll_pos_) {
					setScrollPosition(scroll_pos_ - getPageSize());
				} else {
					setScrollPosition(scroll_pos_ + getPageSize());
				}
			}
		}
		return claimed;
	}

	bool Scrollbar::handleMouseWheel(bool claimed, const point& mp, const point& delta, int direction)
	{
		point p = mp - offset_;
		if(!claimed && geometry::pointInRect(p, loc_)) {
			claimed = true;

			if(dir_ == Direction::VERTICAL) {
				setScrollPosition(scroll_pos_ - delta.y * getLineSize());
			} else {
				setScrollPosition(scroll_pos_ - delta.x * getLineSize());
			}
		}
		return claimed;
	}

	void Scrollbar::scrollLines(int lines)
	{
		if(dir_ == Direction::VERTICAL) {
			setScrollPosition(scroll_pos_ - lines * getLineSize());
		} else {
			setScrollPosition(scroll_pos_ - lines * getLineSize());
		}
	}

	void Scrollbar::setRange(int minr, int maxr) 
	{ 
		min_range_ = minr; 
		max_range_ = maxr; 
		if(min_range_ > max_range_) {
			LOG_ERROR("Swapping min and max ranges as they do not satisfy the ordering criterion. " << min_range_ << " > " << max_range_);
			std::swap(min_range_, max_range_);
		}
		if(scroll_pos_ < min_range_) {
			scroll_pos_ = min_range_;
		}
		if(scroll_pos_ > max_range_) {
			scroll_pos_ = max_range_;
		}
	}
}
