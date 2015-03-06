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
		: shader_(KRE::ShaderProgram::getProgram(name)),
		  u_anura_discard_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_tex_map_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_mvp_matrix_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_sprite_area_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_draw_area_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_cycle_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_color_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_point_size_(KRE::ShaderProgram::INALID_UNIFORM),
		  discard_(false),
		  tex_map_(0),
		  mvp_matrix_(1.0f),
		  sprite_area_(0.0f),
		  draw_area_(0.0f),
		  cycle_(0),
		  color_(1.0f),
		  point_size_(1.0f)

	{
		 init();
	}

	AnuraShader::AnuraShader(const std::string& name, const variant& node)
		: shader_(),
		  u_anura_discard_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_tex_map_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_mvp_matrix_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_sprite_area_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_draw_area_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_cycle_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_color_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_point_size_(KRE::ShaderProgram::INALID_UNIFORM),
		  discard_(false),
		  tex_map_(0),
		  mvp_matrix_(1.0f),
		  sprite_area_(0.0f),
		  draw_area_(0.0f),
		  cycle_(0),
		  color_(1.0f),
		  point_size_(1.0f)
	{
		KRE::ShaderProgram::loadFromVariant(node);
		shader_ = KRE::ShaderProgram::getProgram(name);
		init();
	}

	void AnuraShader::init()
	{
		ASSERT_LOG(shader_ != nullptr, "No shader is set.");
		u_anura_discard_ = shader_->getUniform("u_anura_discard");
		u_anura_tex_map_ = shader_->getUniform("u_anura_tex_map");
		u_anura_mvp_matrix_ = shader_->getUniform("u_anura_mvp_matrix");
		u_anura_sprite_area_ = shader_->getUniform("u_anura_sprite_area");
		u_anura_draw_area_ = shader_->getUniform("u_anura_draw_area");
		u_anura_cycle_ = shader_->getUniform("u_anura_cycle");
		u_anura_color_ = shader_->getUniform("u_anura_color");
		u_anura_point_size_ = shader_->getUniform("u_anura_point_size");

		shader_->setUniformDrawFunction(std::bind(&AnuraShader::setUniformsForDraw, this));

		std::vector<std::pair<std::string, std::string>> attr_map;
		attr_map.emplace_back("a_anura_vertex", "position");
		attr_map.emplace_back("a_anura_texcoord", "texcoord");
		shader_->setAttributeMapping(attr_map);

		// XXX Set the draw commands here if required from shader_->getShaderVariant()
	}

	void AnuraShader::setDrawArea(const rect& draw_area)
	{
		draw_area_ = glm::vec4(static_cast<float>(draw_area.x()), 
			static_cast<float>(draw_area.y()), 
			static_cast<float>(draw_area.w()),
			static_cast<float>(draw_area.h()));
	}

	void AnuraShader::setSpriteArea(const rectf& area)
	{
		sprite_area_ = glm::vec4(area.x(), area.y(), area.w(), area.h());
	}

	void AnuraShader::setUniformsForDraw()
	{
		if(u_anura_discard_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_discard_, static_cast<int>(discard_));
		}
		if(u_anura_tex_map_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_tex_map_, tex_map_);
		}
		if(u_anura_mvp_matrix_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_mvp_matrix_, glm::value_ptr(mvp_matrix_));
		}
		if(u_anura_sprite_area_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_sprite_area_, glm::value_ptr(sprite_area_));
		}
		if(u_anura_draw_area_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_draw_area_, glm::value_ptr(draw_area_));
		}
		if(u_anura_cycle_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_cycle_, &cycle_);
		}
		if(u_anura_color_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_color_, glm::value_ptr(color_));
		}
		if(u_anura_point_size_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_point_size_, point_size_);
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
