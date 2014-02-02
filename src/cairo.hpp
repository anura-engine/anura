#ifndef CAIRO_HPP_INCLUDED
#define CAIRO_HPP_INCLUDED

#include <cairo/cairo.h>

#include "surface.hpp"
#include "texture.hpp"

namespace graphics
{

// Basic glue class between cairo and Anura's texture/surface formats.
// Use a cairo_context when you want to use vector graphics to render
// a texture. Create the cairo_context, use get() to get out the cairo_t*
// and then perform whatever cairo draw calls you want. Then call write()
// to extract a graphics::texture for use in Anura.
class cairo_context
{
public:
	cairo_context(int w, int h);
	~cairo_context();

	cairo_t* get();
	surface get_surface() const;
	graphics::texture write() const;
private:
	cairo_context(const cairo_context&);
	void operator=(const cairo_context&);

	cairo_surface_t* surface_;
	cairo_t* cairo_;
	int width_, height_;
};

}

#endif
