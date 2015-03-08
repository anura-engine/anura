/*
	Copyright (C) 2013-2014 by David White <davewx7@gmail.com> and 
	Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Blend.hpp"
#include "ModelMatrixScope.hpp"

#include "anura_shader.hpp"
#include "formula.hpp"
#include "level.hpp"
#include "module.hpp"
#include "TextureObject.hpp"

namespace graphics
{
	namespace 
	{
		// XXX Load shaders from file here.
		bool g_alpha_test = false;

		class GetMvpMatrixFunction : public game_logic::FunctionExpression
		{
		public:
			explicit GetMvpMatrixFunction(const args_list& args)
			 : FunctionExpression("get_mvp_matrix", args, 0, 0)
			{}
		private:
			variant execute(const game_logic::FormulaCallable& variables) const {
				game_logic::Formula::failIfStaticContext();
				std::vector<variant> v;
				for(size_t n = 0; n < 16; n++) {
					v.push_back(variant((glm::value_ptr(KRE::get_global_model_matrix()))[n]));
				}
				return variant(&v);
			}
		};

		/*class BindTextureFunction : public game_logic::CommandCallable
		{
		public:
			explicit BindTextureFunction(unsigned int tex_id, unsigned int active = 0)
				: tex_id_(tex_id), active_(active)
			{}
			virtual void execute(FormulaCallable& ob) const
			{
				glActiveTexture(GL_TEXTURE0 + active_);
				GLenum err = glGetError();
				ASSERT_LOG(err == GL_NO_ERROR, "glActiveTexture failed: " << active_ << ", " << (active_ + GL_TEXTURE0) << ", " << err);
				glBindTexture(GL_TEXTURE_2D, tex_id_);
				err = glGetError();
				ASSERT_LOG(err == GL_NO_ERROR, "glBindTexture failed: " << tex_id_ << ", " << err);
			}
		private:
			GLuint tex_id_;
			GLuint active_;
		};

		class bind_texture_function : public game_logic::function_expression 
		{
		public:
			explicit bind_texture_function(const args_list& args)
			 : function_expression("bind_texture", args, 1, 2)
			{}
		private:
			variant execute(const game_logic::formula_callable& variables) const 
			{
				GLuint active_tex = args().size() > 1 ? args()[1]->evaluate(variables).as_int() : 0;
				return variant(new bind_texture_command(GLuint(args()[0]->evaluate(variables).as_int()), active_tex));
			}
		};*/

		class LoadTextureFunction : public game_logic::FunctionExpression 
		{
		public:
			explicit LoadTextureFunction(const args_list& args)
			 : FunctionExpression("load_texture", args, 1, 1)
			{}
		private:
			variant execute(const game_logic::FormulaCallable& variables) const 
			{
				game_logic::Formula::failIfStaticContext();
				const std::string filename = module::map_file(args()[0]->evaluate(variables).as_string());
				auto tex = KRE::Texture::createTexture(filename);
				return variant(new TextureObject(tex));
			}
		};

		class BlendModeCommand : public game_logic::CommandCallable
		{
		public:
			explicit BlendModeCommand(const KRE::BlendMode& bm)
				: bm_(bm)
			{}
			virtual void execute(FormulaCallable& ob) const override
			{
				ASSERT_LOG(false, "fixme");
			}
		private:
			KRE::BlendMode bm_;
		};

		class BlendModeFunction : public game_logic::FunctionExpression 
		{
		public:
			explicit BlendModeFunction(const args_list& args)
			 : FunctionExpression("blend_mode", args, 2, 2)
			{}
		private:
			variant execute(const game_logic::FormulaCallable& variables) const 
			{
				KRE::BlendMode bm(args()[0]->evaluate(variables));
				return variant(new BlendModeCommand(bm));
			}
		};


		class ShaderSymbolTable : public game_logic::FunctionSymbolTable
		{
		public:
			ShaderSymbolTable()
			{}

			game_logic::ExpressionPtr createFunction(const std::string& fn,
				const std::vector<game_logic::ExpressionPtr>& args,
				game_logic::ConstFormulaCallableDefinitionPtr callable_def) const override
			{
				if(fn == "get_mvp_matrix") {
					return game_logic::ExpressionPtr(new GetMvpMatrixFunction(args));
				//} else if(fn == "bind_texture") {
				//	return game_logic::ExpressionPtr(new BindTextureFunction(args));
				} else if(fn == "load_texture") {
					return game_logic::ExpressionPtr(new LoadTextureFunction(args));
				} else if(fn == "blend_mode") {
					return game_logic::ExpressionPtr(new BlendModeFunction(args));
				}
				return FunctionSymbolTable::createFunction(fn, args, callable_def);
			}
		};


		game_logic::FunctionSymbolTable& get_shader_symbol_table()
		{
			static ShaderSymbolTable table;
			return table;
		}
	}

	void set_alpha_test(bool alpha)
	{
		g_alpha_test = alpha;
	}

	bool get_alpha_test()
	{
		return g_alpha_test;
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
		  point_size_(1.0f),
		  parent_(nullptr),
		  enabled_(true),
		  name_(name)

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
		  point_size_(1.0f),
		  enabled_(true),
		  name_(name)
	{
		KRE::ShaderProgram::loadFromVariant(node);
		shader_ = KRE::ShaderProgram::getProgram(name);
		init();
	}

	AnuraShader::AnuraShader(const AnuraShader& o) 
		: shader_(o.shader_),
		  u_anura_discard_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_tex_map_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_mvp_matrix_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_sprite_area_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_draw_area_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_cycle_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_color_(KRE::ShaderProgram::INALID_UNIFORM),
		  u_anura_point_size_(KRE::ShaderProgram::INALID_UNIFORM),
		  discard_(o.discard_),
		  tex_map_(o.tex_map_),
		  mvp_matrix_(o.mvp_matrix_),
		  sprite_area_(o.sprite_area_),
		  draw_area_(o.draw_area_),
		  cycle_(o.cycle_),
		  color_(o.color_),
		  point_size_(o.point_size_),
		  enabled_(o.enabled_),
		  name_(o.name_)
	{
		shader_ = o.shader_->clone();
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

		std::vector<std::pair<std::string, std::string>> uniform_map;
		uniform_map.emplace_back("u_anura_mvp_matrix", "mvp_matrix");
		uniform_map.emplace_back("u_anura_color", "color");
		shader_->setUniformMapping(uniform_map);

		uniform_commands_.reset(new UniformCommandsCallable);
		uniform_commands_->setShader(AnuraShaderPtr(this));

		// Set the draw commands here if required from shader_->getShaderVariant()
		const variant& node = shader_->getShaderVariant();
		game_logic::FormulaCallable* e = this;
		if(node.has_key("draw")) {
			const variant& d = node["draw"];
			if(d.is_list()) {
				for(int n = 0; n < d.num_elements(); ++n) {
					std::string cmd = d[n].as_string();
					draw_commands_.push_back(cmd);
					ASSERT_LOG(node.has_key(cmd) == true, "No attribute found with name: " << cmd);
					draw_formulas_.push_back(e->createFormula(node[cmd]));
				}
			} else if(d.is_string()) {
				draw_formulas_.push_back(e->createFormula(d));
			} else {
				ASSERT_LOG(false, "draw must be string or list, found: " << d.to_debug_string());
			}
		}

		if(node.has_key("create")) {
			const variant& c = node["create"];
			if(c.is_list()) {
				for(int n = 0; n < c.num_elements(); ++n) {
					std::string cmd = c[n].as_string();
					create_commands_.push_back(cmd);
					ASSERT_LOG(node.has_key(cmd) == true, "No attribute found with name: " << cmd);
					create_formulas_.push_back(e->createFormula(node[cmd]));
				}
			} else if(c.is_string()) {
				create_formulas_.push_back(e->createFormula(c));
			} else {
				ASSERT_LOG(false, "draw must be string or list, found: " << c.to_debug_string());
			}
		}
	}

	void AnuraShader::setDrawArea(const rect& draw_area)
	{
		draw_area_ = glm::vec4(static_cast<float>(draw_area.x1()), 
			static_cast<float>(draw_area.y1()), 
			static_cast<float>(draw_area.x2()),
			static_cast<float>(draw_area.y2()));
	}

	void AnuraShader::setSpriteArea(const rectf& area)
	{
		sprite_area_ = glm::vec4(area.x1(), area.y1(), area.x2(), area.y2());
	}

	void AnuraShader::setUniformsForDraw()
	{
		if(u_anura_discard_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_discard_, static_cast<int>(discard_));
		}
		if(u_anura_tex_map_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_tex_map_, tex_map_);
		}
		//if(u_anura_mvp_matrix_ != KRE::ShaderProgram::INALID_UNIFORM) {
		//	shader_->setUniformValue(u_anura_mvp_matrix_, glm::value_ptr(mvp_matrix_));
		//}
		if(u_anura_sprite_area_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_sprite_area_, glm::value_ptr(sprite_area_));
		}
		if(u_anura_draw_area_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_draw_area_, glm::value_ptr(draw_area_));
		}
		if(u_anura_cycle_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_cycle_, cycle_);
		}
		if(u_anura_color_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_color_, glm::value_ptr(color_));
		}
		if(u_anura_point_size_ != KRE::ShaderProgram::INALID_UNIFORM) {
			shader_->setUniformValue(u_anura_point_size_, point_size_);
		}
		
		for(auto& u : uniforms_to_set_) {
			if(u.first != KRE::ShaderProgram::INALID_UNIFORM) {
				shader_->setUniformFromVariant(u.first, u.second);
			}
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(AnuraShader)
		DEFINE_FIELD(uniform_commands, "object")
			return variant(obj.uniform_commands_.get());
		DEFINE_FIELD(enabled, "bool")
			return variant(obj.enabled_);
		DEFINE_SET_FIELD
			obj.enabled_ = value.as_bool();
		DEFINE_FIELD(level, "object")
			return variant(Level::getCurrentPtr());
		DEFINE_FIELD(parent, "object")
			ASSERT_LOG(obj.parent_ != nullptr, "Tried to request parent, when value is null: " << obj.shader_->getShaderVariant()["name"]);
			return variant(obj.parent_);
		DEFINE_FIELD(object, "object")
			ASSERT_LOG(obj.parent_ != nullptr, "Tried to request parent, when value is null: " << obj.shader_->getShaderVariant()["name"]);
			return variant(obj.parent_);
	END_DEFINE_CALLABLE(AnuraShader)

	void AnuraShader::clear()
	{
		shader_.reset();
		draw_commands_.clear();
		draw_formulas_.clear();
		create_commands_.clear();
		create_formulas_.clear();
	}

	bool AnuraShader::executeCommand(const variant& var)
	{
		bool result = true;
		if(var.is_null()) {
			return result;
		}

		if(var.is_list()) {
			const int num_elements = var.num_elements();
			for(int n = 0; n != num_elements; ++n) {
				if(var[n].is_null() == false) {
					result = executeCommand(var[n]) && result;
				}
			}
		} else {
			game_logic::CommandCallable* cmd = var.try_convert<game_logic::CommandCallable>();
			if(cmd != NULL) {
				cmd->runCommand(*this);
			}
		}
		return result;
	}

	game_logic::FormulaPtr AnuraShader::createFormula(const variant& v)
	{
		return game_logic::FormulaPtr(new game_logic::Formula(v, &get_shader_symbol_table()));
	}

	void AnuraShader::process()
	{
		game_logic::FormulaCallable* e = this;
		for(auto& f : draw_formulas_) {
			e->executeCommand(f->execute(*e));
		}
		uniform_commands_->executeOnDraw();
	}

	AnuraShader::DrawCommand::DrawCommand() 
		: name(),
		  target(KRE::ShaderProgram::INALID_UNIFORM), 
		  increment(false),
		  value()
	{
	}

	void AnuraShader::UniformCommandsCallable::executeOnDraw()
	{
		for(auto& cmd : uniform_commands_) {
			if(cmd.increment) {
				cmd.value = cmd.value + variant(1);
			}

			program_->uniforms_to_set_[cmd.target] = cmd.value;
		}
	}

	variant AnuraShader::UniformCommandsCallable::getValue(const std::string& key) const
	{
		return variant();
	}

	void AnuraShader::UniformCommandsCallable::setValue(const std::string& key, const variant& value)
	{
		ASSERT_LOG(program_ != nullptr, "NO PROGRAM SET FOR UNIFORM CALLABLE");

		DrawCommand* target = nullptr;
		for(DrawCommand& cmd : uniform_commands_) {
			if(cmd.name == key) {
				target = &cmd;
				break;
			}
		}

		if(target == nullptr) {
			uniform_commands_.push_back(DrawCommand());
			target = &uniform_commands_.back();
			target->target = program_->shader_->getUniform(key);
		}

		if(value.is_map()) {
			target->increment = value["increment"].as_bool(false);
			target->value = value["value"];
		} else {
			target->value = value;
			target->increment = false;
		}
	}

	void AnuraShader::setParent(Entity* parent) 
	{ 
		parent_ = parent; 
		game_logic::FormulaCallablePtr e(this);
		for(auto & cf : create_formulas_) {
			e->executeCommand(cf->execute(*e));
		}
	}
}

