/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <boost/intrusive_ptr.hpp>

#include "filesystem.hpp"
#include "formula.hpp"
#include "TextureObject.hpp"


TextureObject::TextureObject(KRE::TexturePtr texture)
	: texture_(texture),
	  binding_point_(0)
{
}

BEGIN_DEFINE_CALLABLE_NOBASE(TextureObject)

DEFINE_FIELD(id, "int")
	return variant(obj.texture()->id());

DEFINE_FIELD(binding_point, "int")
	return variant(obj.binding_point_);
DEFINE_SET_FIELD
	obj.binding_point_ = value.as_int();

BEGIN_DEFINE_FN(save, "(string) ->commands")
	using namespace game_logic;
	using namespace KRE;
	Formula::failIfStaticContext();

	std::string fname = FN_ARG(0).as_string();

	std::string path_error;
	if(!sys::is_safe_write_path(fname, &path_error)) {
		ASSERT_LOG(false, "Illegal filename to save to: " << fname << " -- " << path_error);
	}

	boost::intrusive_ptr<const TextureObject> ptr(&obj);

	return variant(new FnCommandCallable([=]() {
		auto t = ptr->texture();
		ASSERT_LOG(t, "Could not get texture");
		auto s = t->getFrontSurface();
		ASSERT_LOG(s != nullptr, "Could not get surface from texture");
		s->savePng(fname);
		LOG_INFO("Saved image to " << fname);
	}));
END_DEFINE_FN

END_DEFINE_CALLABLE(TextureObject)

/* XXX Decide what to do about this unseemly mess. -- ideally the reason for it's existance will be tidied up elsewhere
KRE::TexturePtr render_fbo(const rect& area, const std::vector<EntityPtr> objects)
{
	const controls::control_backup_scope ctrl_backup;

	const int tex_width = graphics::texture::allows_npot() ? area.w() : graphics::texture::next_power_of_2(area.w());
	const int tex_height = graphics::texture::allows_npot() ? area.h() : graphics::texture::next_power_of_2(area.h());
	GLint video_framebuffer_id = 0;
	glGetIntegerv(EXT_MACRO(GL_FRAMEBUFFER_BINDING), &video_framebuffer_id);

	GLuint texture_id = 0;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	GLuint framebuffer_id = 0;
	EXT_CALL(glGenFramebuffers)(1, &framebuffer_id);
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), framebuffer_id);

	// attach the texture to FBO color attachment point
	EXT_CALL(glFramebufferTexture2D)(EXT_MACRO(GL_FRAMEBUFFER), EXT_MACRO(GL_COLOR_ATTACHMENT0),
							  GL_TEXTURE_2D, texture_id, 0);

	// check FBO status
	GLenum status = EXT_CALL(glCheckFramebufferStatus)(EXT_MACRO(GL_FRAMEBUFFER));
	ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_UNSUPPORTED));
	ASSERT_EQ(status, EXT_MACRO(GL_FRAMEBUFFER_COMPLETE));

	glViewport(0, 0, area.w(), area.h());

	level_ptr lvl(new level("empty.cfg"));
	foreach(const entity_ptr& e, objects) {
	lvl->add_character(e);
	lvl->add_draw_character(e);
	}

	lvl->set_boundaries(area);
	screen_position pos;
	pos.x = area.x()*100;
	pos.y = area.y()*100;
	{
	preferences::ScreenDimensionOverrideScope dim_scope(area.w(), area.h(), area.w(), area.h());
	lvl->process();
	lvl->process_draw();
	foreach(const entity_ptr& e, objects) {
	lvl->add_draw_character(e);
	}
	render_scene(*lvl, pos);
	}

	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), video_framebuffer_id);
	glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());

	graphics::texture result(texture_id, tex_width, tex_height);
	graphics::surface surf = result.get_surface();
	assert(surf.get());

	//The surface is upside down, so flip it before returning it.
	unsigned char* data = reinterpret_cast<unsigned char*>(surf->pixels);
	const int nbytesperpixel = surf->format->BytesPerPixel;
	const int nbytesperrow = nbytesperpixel*surf->w;
	std::vector<unsigned char> tmp_row(nbytesperrow);
	unsigned char* tmp = &tmp_row[0];
	for(int n = 0; n < surf->h/2; ++n) {
	unsigned char* a = data + nbytesperrow*n;
	unsigned char* b = data + nbytesperrow*(surf->h-n-1);

	memcpy(tmp, a, nbytesperrow);
	memcpy(a, b, nbytesperrow);
	memcpy(b, tmp, nbytesperrow);
	}

	return graphics::texture::get_no_cache(surf);
}
*/
