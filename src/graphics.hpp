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
#ifndef GRAPHICS_HPP_INCLUDED
#define GRAPHICS_HPP_INCLUDED

#include "SDL.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#include "SDL_opengles.h"
#endif

#include "SDL_thread.h"

#if !TARGET_IPHONE_SIMULATOR && !TARGET_OS_HARMATTAN && !TARGET_OS_IPHONE
#include "SDL_ttf.h"
#include "SDL_mixer.h"
#endif

#ifdef USE_SHADERS

#if (defined(WIN32) || defined(__linux__) || defined(__APPLE__)) && !defined(__ANDROID__)
#include <GL/glew.h>
#else
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#endif

#include "gles2.hpp"

#else

#if !defined(SDL_VIDEO_OPENGL_ES) && !(TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE) && !defined(__ANDROID__) && !defined(__native_client__)
#include <GL/glew.h>
#endif

#if defined(__native_client__)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA) || defined(TARGET_BLACKBERRY)
#include <GLES/gl.h>
#ifdef TARGET_PANDORA
#include <GLES/glues.h>
#endif
#include <GLES/glext.h>
#else
#if defined(__ANDROID__)
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES/glplatform.h>
#else
#if defined( _WINDOWS )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#endif

#if defined(__APPLE__) && !(TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE)
#include <OpenGL/OpenGL.h>
#endif

#endif

#include "wm.hpp"
#include "lighting.hpp"

graphics::window_manager_ptr get_main_window();

#endif // GRAPHICS_HPP_INCLUDED
