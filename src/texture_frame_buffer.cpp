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
#include "graphics.hpp"

#include "asserts.hpp"
#include "preferences.hpp"
#include "texture.hpp"
#include "texture_frame_buffer.hpp"

#if defined(__ANDROID__)
#if defined(USE_SHADERS)
#include <GLES2/gl2ext.h>
#else
#include <GLES/glext.h>
#endif
#endif

#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA) || defined(TARGET_BLACKBERRY)
#include <EGL/egl.h>
#define glGenFramebuffersOES        preferences::glGenFramebuffersOES
#define glBindFramebufferOES        preferences::glBindFramebufferOES
#define glFramebufferTexture2DOES   preferences::glFramebufferTexture2DOES
#define glCheckFramebufferStatusOES preferences::glCheckFramebufferStatusOES
#endif

//define macros that make it easy to make the OpenGL calls in this file.
#if defined(USE_SHADERS)
#define EXT_CALL(call) call
#define EXT_MACRO(macro) macro
#elif defined(TARGET_OS_HARMATTAN) || TARGET_OS_IPHONE || defined(TARGET_PANDORA) || defined(TARGET_TEGRA) || defined(TARGET_BLACKBERRY) || defined(__ANDROID__)
#define EXT_CALL(call) call##OES
#define EXT_MACRO(macro) macro##_OES
#elif defined(__APPLE__)
#define EXT_CALL(call) call##EXT
#define EXT_MACRO(macro) macro##_EXT
#else
#define EXT_CALL(call) call##EXT
#define EXT_MACRO(macro) macro##_EXT
#endif

namespace texture_frame_buffer {

namespace {
bool supported = false;
GLuint texture_id = 0, texture_id_back = 0;  //ID of the texture which the frame buffer is stored in
GLuint framebuffer_id = 0, framebuffer_id_back = 0; //framebuffer object
GLint video_framebuffer_id = 0; //the original frame buffer object
int frame_buffer_texture_width = 128;
int frame_buffer_texture_height = 128;

void init_internal(int buffer_width, int buffer_height)
{
	// Clear any old errors.
	glGetError();
	frame_buffer_texture_width = buffer_width;
	frame_buffer_texture_height = buffer_height;

#if defined(__native_client__)
	{
		supported = false;
		LOG("FRAME BUFFER OBJECT NOT SUPPORTED");
		return;
	}
#endif

#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA) || defined(TARGET_BLACKBERRY)
	if (glGenFramebuffersOES        != NULL &&
		glBindFramebufferOES        != NULL &&
		glFramebufferTexture2DOES   != NULL &&
		glCheckFramebufferStatusOES != NULL)
	{
		supported = true;
	}
	else
	{
		fprintf(stderr, "FRAME BUFFER OBJECT NOT SUPPORTED\n");
		supported = false;
		return;
	}
#endif
#if defined(__GLEW_H__)
	if(!GLEW_EXT_framebuffer_object)
    {
		fprintf(stderr, "FRAME BUFFER OBJECT NOT SUPPORTED\n");
		supported = false;
		return;
	}
#endif
	fprintf(stderr, "FRAME BUFFER OBJECT IS SUPPORTED\n");
	supported = true;

#ifndef TARGET_TEGRA
	glGetIntegerv(EXT_MACRO(GL_FRAMEBUFFER_BINDING), &video_framebuffer_id);
#endif

	// Clear any current errors.
	GLenum err = glGetError();

	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width(), height(), 0,
             GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);


	// create a framebuffer object
	EXT_CALL(glGenFramebuffers)(1, &framebuffer_id);
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), framebuffer_id);

	// attach the texture to FBO color attachment point
	EXT_CALL(glFramebufferTexture2D)(EXT_MACRO(GL_FRAMEBUFFER), EXT_MACRO(GL_COLOR_ATTACHMENT0),
                          GL_TEXTURE_2D, texture_id, 0);

	// check FBO status
	GLenum status = EXT_CALL(glCheckFramebufferStatus)(EXT_MACRO(GL_FRAMEBUFFER));
	if(status == EXT_MACRO(GL_FRAMEBUFFER_UNSUPPORTED)) {
		std::cerr << "FRAME BUFFER OBJECT NOT SUPPORTED\n";
		supported = false;
		err = glGetError();
	} else {
		ASSERT_EQ(status, EXT_MACRO(GL_FRAMEBUFFER_COMPLETE));
	}

	// switch back to window-system-provided framebuffer
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), video_framebuffer_id);

	err = glGetError();
	ASSERT_EQ(err, GL_NO_ERROR);
}
}

void init(int buffer_width, int buffer_height)
{
	init_internal(buffer_width, buffer_height);
	switch_texture();
	init_internal(buffer_width, buffer_height);
}

void set_framebuffer_id(int framebuffer)
{
	video_framebuffer_id = framebuffer;
}

void switch_texture()
{
	std::swap(texture_id, texture_id_back);
	std::swap(framebuffer_id, framebuffer_id_back);
}

int width() { return frame_buffer_texture_width; }
int height() { return frame_buffer_texture_height; }

bool unsupported()
{
	return !supported;
}

void set_render_to_texture()
{
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), framebuffer_id);
	glViewport(0, 0, width(), height());
}

void set_render_to_screen()
{
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), video_framebuffer_id);
	glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());
}

render_scope::render_scope()
{
	set_render_to_texture();
}

render_scope::~render_scope()
{
	set_render_to_screen();
}

void set_as_current_texture()
{
	graphics::texture::set_current_texture(texture_id);
}

int current_texture_id()
{
	return texture_id;
}

void rebuild()
{
	if(!supported) {
		return;
	}
	EXT_CALL(glDeleteFramebuffers)(1, &framebuffer_id);
	glDeleteTextures(1, &texture_id);
	init(preferences::actual_screen_width(), preferences::actual_screen_height());
}

} // namespace texture_frame_buffer
