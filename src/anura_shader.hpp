#pragma once

#include "geometry.hpp"
#include "Color.hpp"
#include "SceneObject.hpp"
#include "Shaders.hpp"
#include "WindowManagerFwd.hpp"

#include "entity_fwd.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"

namespace graphics
{
	class AnuraShader;
	typedef boost::intrusive_ptr<AnuraShader> AnuraShaderPtr;

	class ShaderRenderable : public KRE::SceneObject
	{
	public:
		ShaderRenderable();
		void addAttribute(const variant& node);
		void clearAttributes();
		void setDrawMode(KRE::DrawMode dmode);
		KRE::GenericAttributePtr getAttributeOrDie(int attr);
	private:
		ShaderRenderable(const ShaderRenderable&);
		void operator=(const ShaderRenderable&);
		std::map<int, KRE::GenericAttributePtr> attrs_;
	};

	class AnuraShader : public game_logic::FormulaCallable
	{
	public:
		AnuraShader(const std::string& name);
		AnuraShader(const std::string& name, const variant& node);
		AnuraShader(const AnuraShader& o);

		void setDiscard(bool discard) { discard_ = discard; }
		void setTexMap(int tex) { tex_map_ = tex; }
		void setMvpMatrix(const glm::mat4& mvp) { mvp_matrix_ = mvp; }
		void setSpriteArea(const rectf& area);
		void setDrawArea(const rect& draw_area);
		void setCycle(int cycle) { cycle_ = cycle; }
		void setColor(const KRE::Color& color) { color_ = color.as_u8vec4(); }
		void setPointSize(int ps) { point_size_ = static_cast<float>(ps); }

		game_logic::FormulaPtr createFormula(const variant& v) override;
		bool executeCommand(const variant& var) override;
		void clear();

		bool isEnabled() const { return enabled_; }

		KRE::ShaderProgramPtr getShader() const { return shader_; }
		void setUniformsForDraw();

		void process();

		void draw(KRE::WindowManagerPtr wnd) const;

		void setParent(Entity* parent);
		Entity* getParent() const { return parent_; }

		const std::string& getName() const { return name_; }

		int zorder() const { return zorder_; }
	private:
		DECLARE_CALLABLE(AnuraShader);
		void init();

		struct DrawCommand {
			DrawCommand();
			std::string name;
			int target;
			KRE::GenericAttributePtr attr_target;
			bool increment;
			variant value;
		};

		class UniformCommandsCallable : public game_logic::FormulaCallable
		{
		public:
			void setShader(AnuraShaderPtr program) { program_ = program; }
			void executeOnDraw();
		private:
			virtual variant getValue(const std::string& key) const override;
			virtual void setValue(const std::string& key, const variant& value) override;

			AnuraShaderPtr program_;
			std::vector<DrawCommand> uniform_commands_;
		};

		class AttributeCommandsCallable : public game_logic::FormulaCallable
		{
		public:
			void setShader(AnuraShaderPtr program) { program_ = program; }
			void executeOnDraw();
		private:
			virtual variant getValue(const std::string& key) const override;
			virtual void setValue(const std::string& key, const variant& value) override;

			AnuraShaderPtr program_;
			std::vector<DrawCommand> attribute_commands_;
		};

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

		std::vector<std::string> draw_commands_;
		std::vector<std::string> create_commands_;
		std::vector<game_logic::FormulaPtr> draw_formulas_;
		std::vector<game_logic::FormulaPtr> create_formulas_;

		boost::intrusive_ptr<UniformCommandsCallable> uniform_commands_;
		boost::intrusive_ptr<AttributeCommandsCallable> attribute_commands_;

		Entity* parent_;

		bool enabled_;

		int zorder_;

		std::map<int, variant> uniforms_to_set_;

		std::string name_;
		
		ShaderRenderable renderable_;
	};

	void set_alpha_test(bool alpha);
	bool get_alpha_test();
}
