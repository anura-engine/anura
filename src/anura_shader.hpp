#pragma once

#include "geometry.hpp"
#include "Shaders.hpp"

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace graphics
{
	class AnuraShader : public game_logic::FormulaCallable
	{
	public:
		AnuraShader(const std::string& name);
		AnuraShader(const variant& node);

		void setDrawArea(const rect& draw_area);
		void setCycle(int cycle);

		void setSpriteArea(const rectf& area);
	private:
		DECLARE_CALLABLE(AnuraShader);
		void init();
		KRE::ShaderProgramPtr shader_;

		KRE::ActivesHandleBasePtr u_draw_area_;
		KRE::ActivesHandleBasePtr u_cycle_;
		KRE::ActivesHandleBasePtr u_discard_;
		KRE::ActivesHandleBasePtr u_sprite_area_;
	};

	typedef boost::intrusive_ptr<AnuraShader> AnuraShaderPtr;

	void set_alpha_test(bool alpha);
	bool get_alpha_test();
}
