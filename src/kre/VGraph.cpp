/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "asserts.hpp"
#include "geometry.hpp"
#include "VGraph.hpp"
#include "VGraphCairo.hpp"
#include "VGraphOGL.hpp"
#include "VGraphOGLFixed.hpp"

namespace KRE
{
	namespace Vector
	{
		Context::Context()
			: SceneObject("vector::context"),
              width_(0),
              height_(0)
		{
		}

		Context::Context(int width, int height)
			: SceneObject("vector::context"), 
			  width_(width), 
              height_(height)
			
		{
		}

		Context::~Context()
		{
		}

		ContextPtr Context::CreateInstance(const std::string& hint, int width, int height)
		{
			if(hint == "cairo") {
				return ContextPtr(new CairoContext(width, height));
			} else if(hint == "opengl") {
				// XXX
				// return ContextPtr(new OpenGLContext(width, height));
			} else if(hint == "opengl-fixed") {
				// XXX
				// return ContextPtr(new OpenGLFixedContext(width, height));
			} else {
				ASSERT_LOG(false, "Unrecognised hint to create vector graphics instance: " << hint);
			}
			return ContextPtr();
		}

		Path::Path()
		{
		}

		Path::~Path()
		{
		}

		Matrix::Matrix()
		{
		}

		MatrixPtr multiply(const MatrixPtr& a, const MatrixPtr& b)
		{
			auto result = a->clone();
			result->multiply(b);
			return result;
		}
	}
}
