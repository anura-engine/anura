#pragma once

#include "geometry.hpp"
#include "Color.hpp"
#include "Shaders.hpp"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace graphics
{
	class AnuraShader : public game_logic::FormulaCallable
	{
	public:
		AnuraShader(const std::string& name);
		AnuraShader(const std::string& name, const variant& node);

		void setDiscard(bool discard) { discard_ = discard; }
		void setTexMap(int tex) { tex_map_ = tex; }
		void setMvpMatrix(const glm::mat4& mvp) { mvp_matrix_ = mvp; }
		void setSpriteArea(const rectf& area);
		void setDrawArea(const rect& draw_area);
		void setCycle(int cycle) { cycle_ = cycle; }
		void setColor(const KRE::Color& color) { color_ = color.as_u8vec4(); }
		void setPointSize(int ps) { point_size_ = static_cast<float>(ps); }

		KRE::ShaderProgramPtr getShader() const { return shader_; }
		void setUniformsForDraw();
	private:
		DECLARE_CALLABLE(AnuraShader);
		void init();
		KRE::ShaderProgramPtr shader_;

		int u_anura_discard_;
		int u_anura_tex_map_;
		int u_anura_mvp_matrix_;
		int u_anura_sprite_area_;
		int u_anura_draw_area_;
		int u_anura_cycle_;
		int u_anura_color_;
		int u_anura_point_size_;

		bool discard_;
		int tex_map_;
		glm::mat4 mvp_matrix_;
		glm::vec4 sprite_area_;
		glm::vec4 draw_area_;
		int cycle_;
		glm::u8vec4 color_;
		float point_size_;
	};

	typedef boost::intrusive_ptr<AnuraShader> AnuraShaderPtr;

	void set_alpha_test(bool alpha);
	bool get_alpha_test();
}
