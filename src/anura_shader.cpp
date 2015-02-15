#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "anura_shader.hpp"

namespace graphics
{
	namespace 
	{
		// XXX Load shaders from file here.
		bool g_alpha_test = false;
	}

	AnuraShader::AnuraShader(const std::string& name)
		: shader_(KRE::ShaderProgram::getProgram(name))
	{
		 init();
	}

	AnuraShader::AnuraShader(const variant& node)
		: shader_(KRE::ShaderProgram::getProgram(node))
	{
		init();
	}

	void AnuraShader::init()
	{
		u_draw_area_ = shader_->getHandle("u_anura_draw_area");
		u_cycle_ = shader_->getHandle("u_anura_cycle");
		u_discard_ = shader_->getHandle("u_anura_discard");
		u_sprite_area_ = shader_->getHandle("u_anura_sprite_area");
	}

	void AnuraShader::setDrawArea(const rect& draw_area)
	{
		if(u_draw_area_) {
			glm::vec4 da(static_cast<float>(draw_area.x()), 
				static_cast<float>(draw_area.y()), 
				static_cast<float>(draw_area.w()),
				static_cast<float>(draw_area.h()));
			shader_->setUniform(u_draw_area_, glm::value_ptr(da));
		}
	}

	void AnuraShader::setCycle(int cycle)
	{
		if(u_draw_area_) {
			shader_->setUniform(u_draw_area_, reinterpret_cast<void*>(cycle));
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(AnuraShader)
		DEFINE_FIELD(dummy, "int")
			return variant(0);
	END_DEFINE_CALLABLE(AnuraShader)

	void set_alpha_test(bool alpha)
	{
		g_alpha_test = alpha;
	}

	bool get_alpha_test()
	{
		return g_alpha_test;
	}
}
