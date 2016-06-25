#include <algorithm>

#include "asserts.hpp"
#include "scrollable.hpp"

#include "easy_svg.hpp"
#include "WindowManager.hpp"

namespace scrollable
{
	using namespace KRE;

	namespace
	{
		void add_rect(std::vector<vertex_texture_color>* vert, const rect& r, const rectf& t, const Color& c)
		{
			// XXX
			ASSERT_LOG(vert != nullptr, "vert was NULL.");
			glm::u8vec4 color = c.as_u8vec4();
			vert->emplace_back(glm::vec2(r.x1(), r.y1()), glm::vec2(t.x1(), t.y1()), color);
			vert->emplace_back(glm::vec2(r.x2(), r.y1()), glm::vec2(t.x2(), t.y1()), color);
			vert->emplace_back(glm::vec2(r.x1(), r.y2()), glm::vec2(t.x1(), t.y2()), color);

			vert->emplace_back(glm::vec2(r.x2(), r.y1()), glm::vec2(t.x2(), t.y1()), color);
			vert->emplace_back(glm::vec2(r.x1(), r.y2()), glm::vec2(t.x1(), t.y2()), color);
			vert->emplace_back(glm::vec2(r.x2(), r.y2()), glm::vec2(t.x2(), t.y2()), color);
		}
	}

	Scrollbar::Scrollbar(Direction d, change_handler onchange, const rect& loc)
		: SceneObject("Scrollbar"),
		  on_change_(onchange),
		  dir_(d),
		  min_range_(0),
		  max_range_(100),
		  scroll_pos_(0),
		  loc_(loc),
		  up_arrow_area_(),
		  down_arrow_area_(),
		  left_arrow_area_(),
		  right_arrow_area_(),
		  thumb_area_(),
		  visible_(false),
		  thumb_color_(205, 205, 205),
		  thumb_selected_color_(95, 95, 95),
		  thumb_mouseover_color_(166, 166, 166),
		  background_color_(240, 240, 240),
		  changed_(true),
		  thumb_dragging_(false),
		  thumb_mouseover_(false),
		  vertices_(nullptr)

	{
		setShader(ShaderProgram::getProgram("vtc_shader"));

		init();
	}

	Scrollbar::~Scrollbar()
	{
	}

	void Scrollbar::setScrollPosition(int pos)
	{
		scroll_pos_ = pos;
		if(pos < min_range_) {
			LOG_WARN("Scrollbar::setScrollPosition() setting scroll position outside minimum range: " << pos << " < " << min_range_ << " defaulting to minimum.");
			scroll_pos_ = min_range_;
		}
		if(pos > max_range_) {
			LOG_WARN("Scrollbar::setScrollPosition() setting scroll position outside maximum range: " << pos << " > " << max_range_ << " defaulting to maximum.");
			scroll_pos_ = max_range_;
		}
	}

	void Scrollbar::init()
	{
		// number of different positions that we can scroll to.
		const int range = max_range_ - min_range_ + 1;

		const std::vector<std::string> arrow_files{ 
			"scrollbar-up-arrow.svg", 
			"scrollbar-down-arrow.svg", 
			"scrollbar-left-arrow.svg", 
			"scrollbar-right-arrow.svg", 
			"scrollbar-background.svg",
			"scrollbar-thumb.svg",
		};
		std::vector<point> arrow_sizes{ 
			point(loc_.w(), loc_.w()), 
			point(loc_.w(), loc_.w()), 
			point(loc_.h(), loc_.h()), 
			point(loc_.h(), loc_.h()),
			point(loc_.w(), loc_.w()),
			point(loc_.w(), loc_.w()),
		};
		tex_ = svgs_to_single_texture(arrow_files, arrow_sizes, &tex_coords_);
		tex_->setAddressModes(0, Texture::AddressMode::WRAP, Texture::AddressMode::WRAP);		
		setTexture(tex_);

		if(dir_ == Direction::VERTICAL) {
			up_arrow_area_ = rect(loc_.x(), loc_.y(), loc_.w(), loc_.w());
			down_arrow_area_ = rect(loc_.x(), loc_.y2() - loc_.w(), loc_.w(), loc_.w());

			const int min_length = std::min(loc_.w(), loc_.h() / range);
			thumb_area_ = rect(loc_.x(), static_cast<int>(static_cast<float>(scroll_pos_) / range * loc_.h()) + loc_.y(), loc_.w(), min_length);

		} else {
			left_arrow_area_ = rect(loc_.x(), loc_.y(), loc_.h(), loc_.h());
			right_arrow_area_ = rect(loc_.x2() - loc_.h(), loc_.y(), loc_.h(), loc_.h());

			const int min_length = std::min(loc_.h(), (max_range_ - min_range_) / loc_.w());
			thumb_area_ = rect(scroll_pos_ / range * loc_.h() + loc_.x(), loc_.y(), min_length, loc_.h());
		}

		changed_ = true;
	}

	bool Scrollbar::handleMouseMotion(bool claimed, int x, int y)
	{
		if(!claimed && geometry::pointInRect(point(x,y), loc_)) {
			claimed = true;

			// test in the arrow regions and the thumb region.
		}
		return claimed;
	}

	bool Scrollbar::handleMouseButtonDown(bool claimed, int x, int y, unsigned button)
	{
		if(!claimed && geometry::pointInRect(point(x,y), loc_)) {
			claimed = true;
		}
		return claimed;
	}

	bool Scrollbar::handleMouseButtonUp(bool claimed, int x, int y, unsigned button)
	{
		if(!claimed && geometry::pointInRect(point(x,y), loc_)) {
			claimed = true;
		}
		return claimed;
	}

	void Scrollbar::setLocation(int x, int y)
	{
		loc_.set_xy(x, y);
		init();
	}

	void Scrollbar::setDimensions(int w, int h)
	{
		loc_.set_wh(w, h);
		init();
	}

	void Scrollbar::preRender(const WindowPtr& wm)
	{
		if(changed_) {
			changed_ = false;
			// XXX recalculate attribute sets

			if(vertices_ == nullptr) {
				auto as = DisplayDevice::createAttributeSet(true, false, false);
				as->setDrawMode(DrawMode::TRIANGLES);

				vertices_ = std::make_shared<Attribute<vertex_texture_color>>(AccessFreqHint::DYNAMIC);
				vertices_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 3, AttrFormat::FLOAT, false, sizeof(vertex_texture_color), offsetof(vertex_texture_color, vertex)));
				vertices_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texture_color), offsetof(vertex_texture_color, texcoord)));
				vertices_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, false, sizeof(vertex_texture_color), offsetof(vertex_texture_color, color)));

				as->addAttribute(vertices_);
				addAttributeSet(as);
			}

			std::vector<vertex_texture_color> vtc;
			vtc.reserve(4 * 6);

			add_rect(&vtc, loc_, tex_coords_[4], background_color_);
			add_rect(&vtc, thumb_area_, tex_coords_[5], thumb_dragging_ ? thumb_selected_color_ : thumb_mouseover_ ? thumb_mouseover_color_  : thumb_color_);
			add_rect(&vtc, dir_ == Direction::VERTICAL ? up_arrow_area_ : left_arrow_area_, dir_ == Direction::VERTICAL ? tex_coords_[0] : tex_coords_[2], Color::colorWhite());
			add_rect(&vtc, dir_ == Direction::VERTICAL ? down_arrow_area_ : right_arrow_area_, dir_ == Direction::VERTICAL ? tex_coords_[1] : tex_coords_[3], Color::colorWhite());

			getAttributeSet().back()->setCount(vtc.size());
			vertices_->update(&vtc);
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
