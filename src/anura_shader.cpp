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
#include "ColorScope.hpp"
#include "ModelMatrixScope.hpp"

#include "anura_shader.hpp"
#include "array_callable.hpp"
#include "formula.hpp"
#include "level.hpp"
#include "module.hpp"
#include "TextureObject.hpp"

namespace graphics
{
	namespace 
	{
		bool g_alpha_test = false;

		KRE::DrawMode convert_mode(const std::string& smode)
		{
			if(smode == "points") {
				return KRE::DrawMode::POINTS;
			} else if(smode == "lines") {
				return KRE::DrawMode::LINES;
			} else if(smode == "line_strips") {
				return KRE::DrawMode::LINE_STRIP;
			} else if(smode == "line_loop") {
				return KRE::DrawMode::LINE_LOOP;
			} else if(smode == "triangles") {
				return KRE::DrawMode::TRIANGLES;
			} else if(smode == "triangle_strip") {
				return KRE::DrawMode::TRIANGLE_STRIP;
			} else if(smode == "triangle_fan") {
				return KRE::DrawMode::TRIANGLE_FAN;
			} else if(smode == "polygon") {
				return KRE::DrawMode::POLYGON;
			} else if(smode == "quads") {
				return KRE::DrawMode::QUADS;
			} else if(smode == "quad_strip") {
				return KRE::DrawMode::QUAD_STRIP;
			}
			ASSERT_LOG(false, "Unexpected mode type: " << smode);
			return KRE::DrawMode::POINTS;
		}

		class GetMvpMatrixFunction : public game_logic::FunctionExpression
		{
		public:
			explicit GetMvpMatrixFunction(const args_list& args)
			 : FunctionExpression("get_mvp_matrix", args, 0, 0)
			{}

			bool useSingletonVM() const override { return false; }
		private:
			variant execute(const game_logic::FormulaCallable& variables) const override {
				game_logic::Formula::failIfStaticContext();
				std::vector<variant> v;
				for(size_t n = 0; n < 16; n++) {
					v.push_back(variant((glm::value_ptr(KRE::get_global_model_matrix()))[n]));
				}
				return variant(&v);
			}
		};

		class BindTextureCommand : public game_logic::CommandCallable
		{
		public:
			explicit BindTextureCommand(TextureObject* tex, int binding_point)
				: texture_(tex), 
				  binding_point_(binding_point)
			{}
			virtual void execute(FormulaCallable& ob) const override
			{
				texture_->setBindingPoint(binding_point_);
			}
		private:
			ffl::IntrusivePtr<TextureObject> texture_;
			int binding_point_;
		};

		class BindTextureFunction : public game_logic::FunctionExpression 
		{
		public:
			explicit BindTextureFunction(const args_list& args)
			 : FunctionExpression("bind_texture", args, 1, 2)
			{}

			bool useSingletonVM() const override { return false; }
		private:
			variant execute(const game_logic::FormulaCallable& variables) const override
			{
				int binding_point = args().size() > 1 ? args()[1]->evaluate(variables).as_int() : 0;
				variant tex = args()[0]->evaluate(variables);
				TextureObject* tex_obj = tex.try_convert<TextureObject>();
				ASSERT_LOG(tex_obj != nullptr, "Unable to convert parameter to type TextureObject* in bind_texture.");
				return variant(new BindTextureCommand(tex_obj, binding_point));
			}
		};

		class LoadTextureFunction : public game_logic::FunctionExpression 
		{
		public:
			explicit LoadTextureFunction(const args_list& args)
			 : FunctionExpression("load_texture", args, 1, 2)
			{}

			bool useSingletonVM() const override { return false; }
		private:
			variant execute(const game_logic::FormulaCallable& variables) const override
			{
				game_logic::Formula::failIfStaticContext();
				const std::string filename = module::map_file(args()[0]->evaluate(variables).as_string());
				auto tex = KRE::Texture::createTexture(filename, args().size() > 1 ? args()[1]->evaluate(variables) : variant());
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

			bool useSingletonVM() const override { return false; }
		private:
			variant execute(const game_logic::FormulaCallable& variables) const override
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
				} else if(fn == "bind_texture") {
					return game_logic::ExpressionPtr(new BindTextureFunction(args));
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
		  u_anura_discard_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_tex_map_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_mvp_matrix_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_sprite_area_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_draw_area_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_cycle_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_color_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_point_size_(KRE::ShaderProgram::INVALID_UNIFORM),
		  discard_(false),
		  tex_map_(0),
		  mvp_matrix_(1.0f),
		  sprite_area_(0.0f),
		  draw_area_(0.0f),
		  cycle_(0),
		  color_(255, 255, 255, 255),
		  point_size_(1.0f),
		  parent_(nullptr),
		  enabled_(true),
		  zorder_(-1),
		  name_(name)

	{
		 init();
	}

	AnuraShader::AnuraShader(const std::string& name, const variant& node)
		: shader_(),
		  u_anura_discard_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_tex_map_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_mvp_matrix_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_sprite_area_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_draw_area_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_cycle_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_color_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_point_size_(KRE::ShaderProgram::INVALID_UNIFORM),
		  discard_(false),
		  tex_map_(0),
		  mvp_matrix_(1.0f),
		  sprite_area_(0.0f),
		  draw_area_(0.0f),
		  cycle_(0),
		  color_(255, 255, 255, 255),
		  point_size_(1.0f),
		  parent_(nullptr),
		  enabled_(true),
		  zorder_(node["zorder"].as_int(-1)),
		  name_(name),
		  initialised_(false)
	{
		KRE::ShaderProgram::loadFromVariant(node);
		shader_ = KRE::ShaderProgram::getProgram(name);
		init();
	}

	AnuraShader::AnuraShader(const AnuraShader& o) 
		: shader_(o.shader_),
		  u_anura_discard_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_tex_map_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_mvp_matrix_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_sprite_area_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_draw_area_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_cycle_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_color_(KRE::ShaderProgram::INVALID_UNIFORM),
		  u_anura_point_size_(KRE::ShaderProgram::INVALID_UNIFORM),
		  discard_(o.discard_),
		  tex_map_(o.tex_map_),
		  mvp_matrix_(o.mvp_matrix_),
		  sprite_area_(o.sprite_area_),
		  draw_area_(o.draw_area_),
		  cycle_(o.cycle_),
		  color_(o.color_),
		  point_size_(o.point_size_),
		  draw_commands_(o.draw_commands_),
		  create_commands_(o.create_commands_),
		  parent_(o.parent_),
		  enabled_(o.enabled_),
		  zorder_(o.zorder_),
		  name_(o.name_),
		  initialised_(false)
	{
		ASSERT_LOG(o.shader_ != nullptr, "No shader to copy.");
		shader_ = o.shader_->clone();
		renderable_.clearAttributes();
		draw_formulas_ = o.draw_formulas_;
		create_formulas_ = o.create_formulas_;
		draw_commands_.clear();
		create_formulas_.clear();
		create_commands_.clear();
		init();
	}

	void AnuraShader::init()
	{
		ASSERT_LOG(shader_ != nullptr, "No shader is set.");
		ASSERT_LOG(!name_.empty(), "Name was empty");
		u_anura_discard_ = shader_->getUniform("u_anura_discard");
		u_anura_tex_map_ = shader_->getUniform("u_anura_tex_map");
		u_anura_mvp_matrix_ = shader_->getUniform("u_anura_mvp_matrix");
		u_anura_sprite_area_ = shader_->getUniform("u_anura_sprite_area");
		u_anura_draw_area_ = shader_->getUniform("u_anura_draw_area");
		u_anura_cycle_ = shader_->getUniform("u_anura_cycle");
		u_anura_color_ = shader_->getUniform("u_anura_color");
		u_anura_point_size_ = shader_->getUniform("u_anura_point_size");

		shader_->setUniformDrawFunction(std::bind(&AnuraShader::setUniformsForDraw, this));

		const variant& shader_node = shader_->getShaderVariant();
		if(!shader_node.has_key("attributes")) {
			std::vector<std::pair<std::string, std::string>> attr_map;
			attr_map.emplace_back("a_anura_vertex", "position");
			attr_map.emplace_back("a_anura_texcoord", "texcoord");
			shader_->setAttributeMapping(attr_map);
		}

		if(!shader_node.has_key("uniforms")) {
			std::vector<std::pair<std::string, std::string>> uniform_map;
			uniform_map.emplace_back("u_anura_mvp_matrix", "mvp_matrix");
			uniform_map.emplace_back("u_anura_color", "color");
			shader_->setUniformMapping(uniform_map);
		}

		uniform_commands_.reset(new UniformCommandsCallable);
		uniform_commands_->setShader(AnuraShaderPtr(this));

		attribute_commands_.reset(new AttributeCommandsCallable);
		attribute_commands_->setShader(AnuraShaderPtr(this));

		renderable_.setShader(shader_);

		// Set the draw commands here if required from shader_->getShaderVariant()
		game_logic::FormulaCallable* e = this;
		if(draw_formulas_.empty() && shader_node.has_key("draw")) {
			const variant& d = shader_node["draw"];
			if(d.is_list()) {
				for(int n = 0; n < d.num_elements(); ++n) {
					std::string cmd = d[n].as_string();
					draw_commands_.push_back(cmd);
					ASSERT_LOG(shader_node.has_key(cmd) == true, "No attribute found with name: " << cmd);
					draw_formulas_.push_back(e->createFormula(shader_node[cmd]));
				}
			} else if(d.is_string()) {
				draw_formulas_.push_back(e->createFormula(d));
			} else {
				ASSERT_LOG(false, "draw must be string or list, found: " << d.to_debug_string());
			}
		}
	
		if(create_formulas_.empty() && shader_node.has_key("create")) {
			const variant& c = shader_node["create"];
			if(c.is_list()) {
				for(int n = 0; n < c.num_elements(); ++n) {
					std::string cmd = c[n].as_string();
					create_commands_.push_back(cmd);
					ASSERT_LOG(shader_node.has_key(cmd) == true, "No attribute found with name: " << cmd);
					create_formulas_.push_back(e->createFormula(shader_node[cmd]));
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
		if(u_anura_discard_ != KRE::ShaderProgram::INVALID_UNIFORM) {
			shader_->setUniformValue(u_anura_discard_, static_cast<int>(discard_));
		}
		if(u_anura_tex_map_ != KRE::ShaderProgram::INVALID_UNIFORM) {
			shader_->setUniformValue(u_anura_tex_map_, tex_map_);
		}
		//if(u_anura_mvp_matrix_ != KRE::ShaderProgram::INVALID_UNIFORM) {
		//	shader_->setUniformValue(u_anura_mvp_matrix_, glm::value_ptr(mvp_matrix_));
		//}
		if(u_anura_sprite_area_ != KRE::ShaderProgram::INVALID_UNIFORM) {
			//LOG_DEBUG("'" << getName() << "' set sprite area: " << sprite_area_[0] << "," << sprite_area_[1] << "," << sprite_area_[2] << "," << sprite_area_[3]);
			shader_->setUniformValue(u_anura_sprite_area_, glm::value_ptr(sprite_area_));
		}
		if(u_anura_draw_area_ != KRE::ShaderProgram::INVALID_UNIFORM) {
			shader_->setUniformValue(u_anura_draw_area_, glm::value_ptr(draw_area_));
		}
		if(u_anura_cycle_ != KRE::ShaderProgram::INVALID_UNIFORM) {
			shader_->setUniformValue(u_anura_cycle_, cycle_);
		}
		if(u_anura_color_ != KRE::ShaderProgram::INVALID_UNIFORM) {
			shader_->setUniformValue(u_anura_color_, glm::value_ptr(color_));
		}
		if(u_anura_point_size_ != KRE::ShaderProgram::INVALID_UNIFORM) {
			shader_->setUniformValue(u_anura_point_size_, point_size_);
		}

		int binding_point = 2;

		for(auto& tex : textures_) {
			tex->texture()->bind(tex->getBindingPoint());
			if(tex->getBindingPoint() >= binding_point) {
				binding_point = tex->getBindingPoint() + 1;
			}
		}

		std::vector<KRE::Texture*> seen_textures;

		for(const ObjectPropertyUniform& u : object_uniforms_) {
			variant v = parent_->queryValueBySlot(u.slot);

			if(v.is_callable()) {
				auto p = v.try_convert<TextureObject>();
				if(p) {
					int point = 0;
					for(auto t : textures_) {
						if(p->texture() == t->texture()) {
							point = t->getBindingPoint();
							break;
						}
					}

					for(int n = 0; n < static_cast<int>(seen_textures.size()); ++n) {
						if(seen_textures[n] == p->texture().get()) {
							point = binding_point - seen_textures.size() + n;
							break;
						}
					}

					if(point == 0) {
						point = binding_point++;
						seen_textures.push_back(p->texture().get());
						p->texture()->bind(point);
					}

					shader_->setUniformValue(u.uniform, point);

					continue;
				}
			} else if(v.is_null() == false) {
				shader_->setUniformFromVariant(u.uniform, v);
			}
		}
		
		for(auto& u : uniforms_to_set_) {
			if(u.first != KRE::ShaderProgram::INVALID_UNIFORM) {
				shader_->setUniformFromVariant(u.first, u.second);
				//LOG_DEBUG("'" << getName() << "' set " << u.first << " : " << u.second.to_debug_string());
			}
		}
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(AnuraShader)
		DEFINE_FIELD(uniform_commands, "object")
			return variant(obj.uniform_commands_.get());
		DEFINE_FIELD(attribute_commands, "object")
			return variant(obj.attribute_commands_.get());
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
		DEFINE_FIELD(draw_mode, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("string")
			obj.renderable_.setDrawMode(convert_mode(value.as_string()));
		DEFINE_FIELD(attributes, "null")
			return variant();
		DEFINE_SET_FIELD_TYPE("[map]")
	        obj.attribute_commands_->clearCommands();

			obj.renderable_.clearAttributes();
			for(int n = 0; n != value.num_elements(); ++n) {
				obj.renderable_.addAttribute(value[n]);
			}
		DEFINE_FIELD(color, "[decimal]")
			if(obj.renderable_.isColorSet()) {
				return obj.renderable_.getColor().write();
			} 
			return KRE::ColorScope::getCurrentColor().write();
		DEFINE_FIELD(textures, "[object]")
			std::vector<variant> res;
			for(const auto& tex : obj.textures_) {
				res.emplace_back(tex.get());
			}
			auto vvv = variant(&res);
			//LOG_DEBUG("textures: " << vvv.to_debug_string() << " : " << obj.getName());
			return vvv;
		DEFINE_SET_FIELD
			obj.textures_.clear();
			//LOG_DEBUG("set textures: " << value.to_debug_string());
			for(int n = 0; n != value.num_elements(); ++n) {
				TextureObject* tex_obj = value[n].try_convert<TextureObject>();
				ASSERT_LOG(tex_obj != nullptr, "Couldn't convert to TextureObject: " << value[n].to_debug_string());
				obj.textures_.emplace_back(tex_obj);
				//LOG_DEBUG("Added texture: " << tex_obj->texture()->getFrontSurface()->getName() << " : " << obj.getName());
			}
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
			if(cmd != nullptr) {
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
		if(!initialised_) {
			setParent(nullptr);
		}

		game_logic::FormulaCallable* e = this;
		for(auto& f : draw_formulas_) {
			e->executeCommand(f->execute(*e));
		}
		attribute_commands_->executeOnDraw();
		uniform_commands_->executeOnDraw();
	}

	AnuraShader::DrawCommand::DrawCommand() 
		: name(),
		  target(KRE::ShaderProgram::INVALID_UNIFORM), 
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

	void AnuraShader::surrenderReferences(GarbageCollector* collector)
	{
		collector->surrenderPtr(&uniform_commands_);
		collector->surrenderPtr(&attribute_commands_);
		for(std::pair<const int, variant>& p : uniforms_to_set_) {
			collector->surrenderVariant(&p.second);
		}

		for(ffl::IntrusivePtr<TextureObject>& t : textures_) {
			collector->surrenderPtr(&t);
		}
	}

	void AnuraShader::UniformCommandsCallable::surrenderReferences(GarbageCollector* collector)
	{
		for(const DrawCommand& cmd : uniform_commands_) {
			collector->surrenderVariant(&cmd.value);
		}

		collector->surrenderPtr(&program_);
	}

	void AnuraShader::AttributeCommandsCallable::surrenderReferences(GarbageCollector* collector)
	{
		for(const DrawCommand& cmd : attribute_commands_) {
			collector->surrenderVariant(&cmd.value);
		}

		collector->surrenderPtr(&program_);
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
			target->name = key;
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

	void AnuraShader::AttributeCommandsCallable::executeOnDraw()
	{
		for(auto& cmd : attribute_commands_) {
			if(cmd.increment) {
				cmd.value = cmd.value + variant(1);
			}

			if(cmd.value.is_callable()) {
				game_logic::FloatArrayCallable* f = cmd.value.try_convert<game_logic::FloatArrayCallable>();
				if(f != nullptr) {
					int count = f->num_elements();
					int divisor = 0;
					for(auto& desc : cmd.attr_target->getAttrDesc()) {
						divisor += desc.getNumElements();
					}
					count /= divisor;
					cmd.attr_target->update(&f->floats()[0], f->num_elements() * sizeof(float), count);
					continue;
				}
				game_logic::ShortArrayCallable* s = cmd.value.try_convert<game_logic::ShortArrayCallable>();
				if(s != nullptr) {
					int count = s->num_elements();
					int divisor = 0;
					for(auto& desc : cmd.attr_target->getAttrDesc()) {
						divisor += desc.getNumElements();
					}
					count /= divisor;
					cmd.attr_target->update(&s->shorts()[0], s->num_elements() * sizeof(short), count);
					continue;
				}
			} else {
				ASSERT_LOG(false, "XXX no support for normal variants-- yet");
			}
		}
	}

	variant AnuraShader::AttributeCommandsCallable::getValue(const std::string& key) const
	{
		return variant();
	}

	void AnuraShader::AttributeCommandsCallable::setValue(const std::string& key, const variant& value)
	{
		ASSERT_LOG(program_ != nullptr, "NO PROGRAM SET FOR UNIFORM CALLABLE");

		DrawCommand* target = nullptr;
		for(DrawCommand& cmd : attribute_commands_) {
			if(cmd.name == key) {
				target = &cmd;
				break;
			}
		}

		if(target == nullptr) {
			attribute_commands_.push_back(DrawCommand());
			target = &attribute_commands_.back();
			target->name = key;
			target->target = program_->shader_->getAttribute(key);
			target->attr_target = program_->renderable_.getAttributeOrDie(target->target);
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
		//LOG_DEBUG("Set parent for '" << getName() << "' to " << parent);
		parent_ = parent; 
		game_logic::FormulaCallablePtr e(this);
		for(auto & cf : create_formulas_) {
			e->executeCommand(cf->execute(*e));
		}

		object_attributes_.clear();
		object_uniforms_.clear();

		if(parent != nullptr) {
			static const std::string UniformPrefix = "u_property_";
			auto v = shader_->getAllUniforms();
			for(const std::string& s : v) {
				if(s.size() > UniformPrefix.size() && std::equal(UniformPrefix.begin(), UniformPrefix.end(), s.begin())) {
					std::string prop_name(s.begin() + UniformPrefix.size(), s.end());
					const int slot = parent->getValueSlot(prop_name);
					ASSERT_LOG(slot >= 0, "Unknown shader property: " << s << " for object " << parent->getDebugDescription());

					ObjectPropertyUniform u;
					u.name = prop_name;
					u.slot = slot;
					u.uniform = shader_->getUniformOrDie(s);
					object_uniforms_.push_back(u);
				}
			}

			static const std::string AttributePrefix = "a_property_";
			v = shader_->getAllAttributes();
			for(const std::string& s : v) {
				if(s.size() > AttributePrefix.size() && std::equal(AttributePrefix.begin(), AttributePrefix.end(), s.begin())) {
					using namespace KRE;

					std::string prop_name(s.begin() + AttributePrefix.size(), s.end());
					const int slot = parent->getValueSlot(prop_name);
					ASSERT_LOG(slot >= 0, "Unknown shader property: " << s << " for object " << parent->getDebugDescription());

					ObjectPropertyAttribute u;
					u.name = prop_name;
					u.slot = slot;
					u.attr = shader_->getAttributeOrDie(s);

					int num_components = 1;
					AttrFormat fmt = KRE::AttrFormat::FLOAT;
					bool normalise = false;
					ptrdiff_t stride = 0;
					ptrdiff_t offset = 0;
					int divisor = 1;

					KRE::AttributeDesc desc(s, num_components, fmt, normalise, stride, offset, divisor);

					auto attr = std::make_shared<GenericAttribute>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
					attr->addAttributeDesc(desc);
					getShader()->configureAttribute(attr);

					auto loc = attr->getAttrDesc().back().getLocation();
					ASSERT_LOG(loc != ShaderProgram::INVALID_ATTRIBUTE, "No attribute with name '" << s << "' in shader.");
					ASSERT_LOG(loc == u.attr, "Attribute mismatch: " << loc << " vs " << u.attr);

					renderable_.getAttributeSet().front()->addAttribute(attr);
					ASSERT_LOG(attr->getParent() != nullptr, "attribute parent was null after adding to attribute set.");
					u.attr_target = attr;

					object_attributes_.push_back(u);
				}
			}
		}

		initialised_ = true;
	}

	KRE::GenericAttributePtr AnuraShader::getAttributeOrDie(int attr) const
	{
		return const_cast<ShaderRenderable&>(renderable_).getAttributeOrDie(attr);
	}

	void AnuraShader::draw(KRE::WindowPtr wnd) const
	{
		wnd->render(&renderable_);
	}

	ShaderRenderable::ShaderRenderable()
		: SceneObject("ShaderRenderable")
	{
		addAttributeSet(KRE::DisplayDevice::createAttributeSet(false, false, false));		
	}

	void ShaderRenderable::addAttribute(const variant& node)
	{
		ASSERT_LOG(getShader() != nullptr, "shader was null.");
		using namespace KRE;

		int num_components = 2;
		AttrFormat fmt = KRE::AttrFormat::FLOAT;
		bool normalise = false;
		ptrdiff_t stride = 0;
		ptrdiff_t offset = 0;
		int divisor = 1;
		ASSERT_LOG(node.has_key("name"), "ShaderRenderable::addAttribute must have 'name' attribute at a minimum.");
		if(node.has_key("type")) {
			const std::string type = node["type"].as_string();
			if(type == "float") {
				fmt = AttrFormat::FLOAT;
			} else if(type == "short" || type == "unsigned short" || type == "unsigned_short" || type == "ushort") {
				fmt = AttrFormat::UNSIGNED_SHORT;
			} else {
				ASSERT_LOG(false, "Did not recognise type of attribute: " << type);
			}
		}
		if(node.has_key("components")) {
			num_components = node["components"].as_int32();
		}
		if(node.has_key("stride")) {
			stride = static_cast<ptrdiff_t>(node["stride"].as_int32());
		}
		if(node.has_key("offset")) {
			offset = static_cast<ptrdiff_t>(node["offset"].as_int32());
		}
		if(node.has_key("divisor")) {
			divisor = node["divisor"].as_int32();
		}
		std::string name = node["name"].as_string();
		KRE::AttributeDesc desc(name, num_components, fmt, normalise, stride, offset, divisor);
		// XXX we should add in decodes for AccessFreqHint and AccessTypeHint

		auto attr = std::make_shared<GenericAttribute>(AccessFreqHint::DYNAMIC, AccessTypeHint::DRAW);
		attr->addAttributeDesc(desc);
		getShader()->configureAttribute(attr);
		auto loc = attr->getAttrDesc().back().getLocation();
		ASSERT_LOG(loc != ShaderProgram::INVALID_ATTRIBUTE, "No attribute with name '" << name << "' in shader.");
		getAttributeSet().front()->addAttribute(attr);
		ASSERT_LOG(attr->getParent() != nullptr, "attribute parent was null after adding to attribute set.");
		attrs_[loc] = attr;
		LOG_DEBUG("Added attribute at " << loc << " : " << name << "  ; " << this);
	}

	void ShaderRenderable::clearAttributes()
	{
		attrs_.clear();
		clearAttributeSets();
		addAttributeSet(KRE::DisplayDevice::createAttributeSet(false, false, false));
	}

	void ShaderRenderable::setDrawMode(KRE::DrawMode dmode)
	{
		getAttributeSet().back()->setDrawMode(dmode);
	}

	KRE::GenericAttributePtr ShaderRenderable::getAttributeOrDie(int attr)
	{
		auto it = attrs_.find(attr);
		if(it == attrs_.end()) {
			std::ostringstream ss;
			for(auto& a : attrs_) {
				ss << "{ " << a.first << ":" << a.second << "},";
			}
			ASSERT_LOG(false, "Unable to find attribute: " << attr << " : " << ss.str() << "  ; " << this);
		}
		return it->second;
	}

}

