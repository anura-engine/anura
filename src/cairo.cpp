#include "cairo.hpp"

namespace graphics
{

cairo_context::cairo_context(int w, int h)
  : width_(w), height_(h)
{
	surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_ = cairo_create(surface_);
}

cairo_context::~cairo_context()
{
	cairo_destroy(cairo_);
	cairo_surface_destroy(surface_);
}

cairo_t* cairo_context::get()
{
	return cairo_;
}

surface cairo_context::get_surface() const
{
	unsigned char* data = cairo_image_surface_get_data(surface_);
	surface result(SDL_CreateRGBSurface(0, width_, height_, 32, SURFACE_MASK));
	memcpy(result->pixels, data, width_*height_*4);
	return result;
}

graphics::texture cairo_context::write() const
{
	std::vector<surface> s;
	s.push_back(get_surface());
	return graphics::texture(s);
}

}
