#include <map>

#include "asserts.hpp"
#include "fbo_scene.hpp"
#include "cairo.hpp"
#include "filesystem.hpp"
#include "module.hpp"

namespace graphics
{

namespace {
bool initialize_gtype() {
	g_type_init();
	return true;
}
}

cairo_context::cairo_context(int w, int h)
  : width_(w), height_(h)
{
	static const bool init = initialize_gtype();

	surface_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_ = cairo_create(surface_);
}

cairo_context::~cairo_context()
{
	cairo_destroy(cairo_);
	cairo_surface_destroy(surface_);
}

cairo_t* cairo_context::get() const
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

void cairo_context::render_svg(const std::string& fname)
{
	GError *error = NULL;

	static std::map<std::string, RsvgHandle*> cache;

	RsvgHandle* handle = NULL;

	auto itor = cache.find(fname);
	if(itor == cache.end()) {
		std::string real_fname = module::map_file(fname);
		ASSERT_LOG(sys::file_exists(real_fname), "Could not find svg file: " << fname);

		handle = rsvg_handle_new_from_file(real_fname.c_str(), &error);
		ASSERT_LOG(error == NULL, "SVG rendering error: " << error->message);
	} else {
		handle = itor->second;
	}

	rsvg_handle_render_cairo(handle, cairo_);

	cairo_status_t status = cairo_status(cairo_);
	ASSERT_LOG(status == 0, "SVG rendering error: " << cairo_status_to_string(status));
}

void cairo_context::write_png(const std::string& fname) 
{
	cairo_surface_write_to_png(surface_, fname.c_str());
}

cairo_matrix_saver::cairo_matrix_saver(cairo_context& ctx) : ctx_(ctx)
{
	cairo_save(ctx_.get());
}

cairo_matrix_saver::~cairo_matrix_saver()
{
	cairo_restore(ctx_.get());
}

namespace
{

typedef std::function<void(cairo_context&, const std::vector<variant>&)> CairoOp;
class cairo_op : public game_logic::formula_callable
{
public:
	cairo_op(CairoOp fn, const std::vector<variant>& args) : fn_(fn), args_(args)
	{}

	void execute(cairo_context& context) {
		fn_(context, args_);
	}
private:
	DECLARE_CALLABLE(cairo_op);
	
	CairoOp fn_;
	std::vector<variant> args_;
};

BEGIN_DEFINE_CALLABLE_NOBASE(cairo_op)
END_DEFINE_CALLABLE(cairo_op)

#define BEGIN_CAIRO_FN(a,b) BEGIN_DEFINE_FN(a,b "->builtin cairo_op") std::vector<variant> fn_args; for(int i = 0; i < NUM_FN_ARGS; ++i) { fn_args.push_back(FN_ARG(i)); } return variant(new cairo_op([](cairo_context& context, const std::vector<variant>& args) {

#define END_CAIRO_FN }, fn_args)); END_DEFINE_FN

}

cairo_callable::cairo_callable()
{}

BEGIN_DEFINE_CALLABLE_NOBASE(cairo_callable)
BEGIN_CAIRO_FN(save, "()")
	cairo_save(context.get());
END_CAIRO_FN

BEGIN_CAIRO_FN(restore, "()")
	cairo_restore(context.get());
END_CAIRO_FN

BEGIN_CAIRO_FN(translate, "(decimal,decimal)")
	cairo_scale(context.get(), args[0].as_decimal().as_float(), args[1].as_decimal().as_float());
END_CAIRO_FN

BEGIN_CAIRO_FN(scale, "(decimal,decimal)")
	cairo_scale(context.get(), args[0].as_decimal().as_float(), args[1].as_decimal().as_float());
END_CAIRO_FN

BEGIN_CAIRO_FN(rotate, "(decimal)")
	cairo_rotate(context.get(), args[0].as_decimal().as_float());
END_CAIRO_FN

BEGIN_CAIRO_FN(draw_svg, "(string)")
	context.render_svg(args[0].as_string());
END_CAIRO_FN

BEGIN_DEFINE_FN(render, "(int, int, [builtin cairo_op]) ->object")
	cairo_context context(FN_ARG(0).as_int(), FN_ARG(1).as_int());

	std::vector<variant> ops = FN_ARG(2).as_list();
	for(variant op : ops) {
		op.convert_to<cairo_op>()->execute(context);
	}

	return variant(new texture_object(context.write()));
END_DEFINE_FN

END_DEFINE_CALLABLE(cairo_callable)

}
