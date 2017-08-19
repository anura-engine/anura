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

#include "intrusive_ptr.hpp"

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

DEFINE_FIELD(width, "int")
	return variant(obj.texture()->surfaceWidth());

DEFINE_FIELD(height, "int")
	return variant(obj.texture()->surfaceHeight());

DEFINE_FIELD(binding_point, "int")
	return variant(obj.binding_point_);
DEFINE_SET_FIELD
	obj.binding_point_ = value.as_int();

BEGIN_DEFINE_FN(clear_surfaces, "() ->commands")
	ffl::IntrusivePtr<const TextureObject> ptr(&obj);
	return variant(new game_logic::FnCommandCallable("texture::clear_surfaces", [ptr]() {
		auto t = ptr->texture();
		ASSERT_LOG(t, "Could not get texture");
		t->clearSurfaces();
	}));
END_DEFINE_FN

BEGIN_DEFINE_FN(bind, "() ->commands")
	ffl::IntrusivePtr<const TextureObject> ptr(&obj);
	return variant(new game_logic::FnCommandCallable("texture::bind", [ptr]() {
		auto t = ptr->texture();
		ASSERT_LOG(t, "Could not get texture");
		t->bind();
	}));
END_DEFINE_FN

BEGIN_DEFINE_FN(save, "(string) ->commands")
	using namespace game_logic;
	using namespace KRE;
	Formula::failIfStaticContext();

	std::string fname = FN_ARG(0).as_string();

	std::string path_error;
	if(!sys::is_safe_write_path(fname, &path_error)) {
		ASSERT_LOG(false, "Illegal filename to save to: " << fname << " -- " << path_error);
	}

	ffl::IntrusivePtr<const TextureObject> ptr(&obj);

	return variant(new FnCommandCallable("texture::save", [=]() {
		auto t = ptr->texture();
		ASSERT_LOG(t, "Could not get texture");
		//auto s = t->getFrontSurface();
		auto s = t->extractTextureToSurface();
		ASSERT_LOG(s != nullptr, "Could not get surface from texture");
		s->savePng(fname);
		LOG_INFO("Saved image to " << fname);
	}));
END_DEFINE_FN

END_DEFINE_CALLABLE(TextureObject)

