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

#pragma once

#include <memory>

namespace KRE
{
	enum class DrawMode {
		POINTS,
		LINE_STRIP,
		LINE_LOOP,
		LINES,
		TRIANGLE_STRIP,
		TRIANGLE_FAN,
		TRIANGLES,
		QUAD_STRIP,
		QUADS,
		POLYGON,		
	};

	enum class IndexType {
		INDEX_NONE,
		INDEX_UCHAR,
		INDEX_USHORT,
		INDEX_ULONG,
	};

	enum class TextureType {
		TEXTURE_1D,
		TEXTURE_2D,
		TEXTURE_3D,
		TEXTURE_CUBIC,
	};

	enum class AttrType {
		UNKOWN,
		POSITION,
		COLOR, 
		TEXTURE,
		NORMAL,
	};
	enum class AttrFormat {
		BOOL,
		HALF_FLOAT,
		FLOAT,
		DOUBLE,
		FIXED,
		SHORT,
		UNSIGNED_SHORT,
		BYTE,
		UNSIGNED_BYTE,
		INT,
		UNSIGNED_INT,
		INT_2_10_10_10_REV,
		UNSIGNED_INT_2_10_10_10_REV,
		UNSIGNED_INT_10F_11F_11F_REV,
	};

	class DisplayDevice;
	typedef std::shared_ptr<DisplayDevice> DisplayDevicePtr;

	class Texture;
	typedef std::shared_ptr<Texture> TexturePtr;

	class Effect;
	typedef std::shared_ptr<Effect> EffectPtr;

	class BlendModeScope;
	typedef std::unique_ptr<BlendModeScope> BlendModeScopePtr;

	class BlendEquationImplBase;
	typedef std::shared_ptr<BlendEquationImplBase> BlendEquationImplBasePtr;

	class AttributeBase;
	typedef std::shared_ptr<AttributeBase> AttributeBasePtr;

	class GenericAttribute;
	typedef std::shared_ptr<GenericAttribute> GenericAttributePtr;

	class AttributeSet;
	typedef std::shared_ptr<AttributeSet> AttributeSetPtr;

	class HardwareAttribute;
	typedef std::shared_ptr<HardwareAttribute> HardwareAttributePtr;

	class Canvas;
	typedef std::shared_ptr<Canvas> CanvasPtr;

	class ClipScope;
	typedef std::unique_ptr<ClipScope> ClipScopePtr;

	class ClipShapeScope;
	typedef std::unique_ptr<ClipShapeScope> ClipShapeScopePtr;

	class StencilScope;
	typedef std::unique_ptr<StencilScope> StencilScopePtr;

	class Scissor;
	typedef std::shared_ptr<Scissor> ScissorPtr;

	class RenderTarget;
	typedef std::shared_ptr<RenderTarget> RenderTargetPtr;

	class Effect;
	typedef std::shared_ptr<Effect> EffectPtr;

	class ShaderProgram;
	typedef std::shared_ptr<ShaderProgram> ShaderProgramPtr;

	class UniformBufferBase;
}
