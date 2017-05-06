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
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <cmath>
#include <chrono>

#include "ModelMatrixScope.hpp"
#include "ParticleSystem.hpp"
#include "ParticleSystemAffectors.hpp"
#include "ParticleSystemParameters.hpp"
#include "ParticleSystemEmitters.hpp"
#include "SceneGraph.hpp"
#include "Shaders.hpp"
#include "spline.hpp"
#include "WindowManager.hpp"
#include "variant_utils.hpp"

namespace KRE
{
	namespace Particles
	{
		namespace 
		{
			SceneNodeRegistrar<ParticleSystemContainer> psc_register("particle_system_container");

			std::default_random_engine& get_rng_engine() 
			{
				static std::unique_ptr<std::default_random_engine> res;
				if(res == nullptr) {
					auto seed = std::chrono::system_clock::now().time_since_epoch().count();
					res.reset(new std::default_random_engine(std::default_random_engine::result_type(seed)));
				}
				return *res;
			}
		}

		void init_physics_parameters(PhysicsParameters& pp)
		{
			pp.position = glm::vec3(0.0f);
			pp.color = color_vector(255,255,255,255);
			pp.dimensions = glm::vec3(1.0f);
			pp.time_to_live = 10.0f;
			pp.mass = 1.0f;
			pp.velocity = 100.0f;
			pp.direction = glm::vec3(0.0f,1.0f,0.0f);
			pp.orientation = glm::quat(1.0f,0.0f,0.0f,0.0f);
			pp.area = rectf::fromCoordinates(0.0f, 0.0f, 1.0f, 1.0f);
		}

		float get_random_float(float min, float max)
		{
			if(min > max) {
				std::swap(min, max);
			}
			std::uniform_real_distribution<float> gen(min, max);
			return gen(get_rng_engine());
		}

		std::ostream& operator<<(std::ostream& os, const glm::vec3& v)
		{
			os << "[" << v.x << "," << v.y << "," << v.z << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const glm::vec4& v)
		{
			os << "[" << v.x << "," << v.y << "," << v.z << "," << v.w << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const glm::quat& v)
		{
			os << "[" << v.w << "," << v.x << "," << v.y << "," << v.z << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const color_vector& c)
		{
			os << "[" << int(c.r) << "," << int(c.g) << "," << int(c.b) << "," << int(c.a) << "]";
			return os;
		}

		std::ostream& operator<<(std::ostream& os, const Particle& p) 
		{
			os << "P"<< p.current.position 
				<< ", IP" << p.initial.position 
				<< ", DIM" << p.current.dimensions 
				<< ", IDIR" << p.initial.direction 
				<< ", DIR" << p.current.direction 
				<< ", TTL(" << p.current.time_to_live << ")" 
				<< ", ITTL(" <<  p.initial.time_to_live << ")"
				<< ", C" << p.current.color
				<< ", M(" << p.current.mass << ")"
				<< ", V(" << p.current.velocity << ")"
				<< std::endl
				<< "\tO(" << p.current.orientation << ")"
				<< "\tIO(" << p.initial.orientation << ")"
				;
			return os;
		}

		// Compute any vector out of the infinite set perpendicular to v.
		glm::vec3 perpendicular(const glm::vec3& v) 
		{
			glm::vec3 perp = glm::cross(v, glm::vec3(1.0f,0.0f,0.0f));
			float len_sqr = perp.x*perp.x + perp.y*perp.y + perp.z*perp.z;
			if(len_sqr < 1e-12) {
				perp = glm::cross(v, glm::vec3(0.0f,1.0f,0.0f));
			}
			float len = glm::length(perp);
			if(len > 1e-14f) {
				return perp / len;
			}
			return perp;
		}

		glm::vec3 create_deviating_vector(float angle, const glm::vec3& v, const glm::vec3& up)
		{
			glm::vec3 up_up = up;
			if(up == glm::vec3(0.0f)) {
				up_up = perpendicular(v);
			}

			glm::quat q = glm::angleAxis(glm::radians(angle), up_up);
			return q * v;
		}

		ParticleSystem::ParticleSystem(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: EmitObject(parent, node), 
			  SceneObject(node),
			  default_particle_width_(node["default_particle_width"].as_float(1.0f)),
			  default_particle_height_(node["default_particle_height"].as_float(1.0f)),
			  default_particle_depth_(node["default_particle_depth"].as_float(1.0f)),
			  particle_quota_(node["particle_quota"].as_int32(100)),
			  elapsed_time_(0.0f), 
			  scale_velocity_(1.0f), 
			  scale_time_(1.0f),
			  scale_dimensions_(1.0f),
			  texture_node_(),
			  use_position_(node["use_position"].as_bool(true))
		{
			if(node.has_key("fast_forward")) {
				float ff_time = float(node["fast_forward"]["time"].as_float());
				float ff_interval = float(node["fast_forward"]["interval"].as_float());
				fast_forward_.reset(new std::pair<float,float>(ff_time, ff_interval));
			}

			if(node.has_key("scale_velocity")) {
				scale_velocity_ = float(node["scale_velocity"].as_float());
			}
			if(node.has_key("scale_time")) {
				scale_time_ = float(node["scale_time"].as_float());
			}
			if(node.has_key("scale")) {
				if(node["scale"].is_list()) {
					scale_dimensions_ = variant_to_vec3(node["scale"]);
				} else {
					float s = node["scale"].as_float();
					scale_dimensions_ = glm::vec3(s, s, s);
				}
			}

			if(node.has_key("emitter")) {
				if(node["emitter"].is_map()) {
					emitter_ = Emitter::factory(parent, node["emitter"]);					
				} else {
					ASSERT_LOG(false, "'emitter' attribute must be a map.");
				}
			}
			if(node.has_key("affector")) {
				if(node["affector"].is_map()) {
					auto aff = Affector::factory(parent, node["affector"]);
					affectors_.emplace_back(aff);
				} else if(node["affector"].is_list()) {
					for(int n = 0; n != node["affector"].num_elements(); ++n) {
						auto aff = Affector::factory(parent, node["affector"][n]);
						affectors_.emplace_back(aff);
					}
				} else {
					ASSERT_LOG(false, "'affector' attribute must be a list or map.");
				}
			}

			if(node.has_key("max_velocity")) {
				max_velocity_.reset(new float(node["max_velocity"].as_float()));
			}

			if(node.has_key("texture")) {
				setTextureNode(node["texture"]);
			}
			if(node.has_key("image")) {
				setTextureNode(node["image"]);
			}

			initAttributes();
		}

		void ParticleSystem::init()
		{
			active_emitter_ = emitter_->clone();
			active_emitter_->init();
			// In order to create as few re-allocations of particles, reserve space here
			active_particles_.reserve(particle_quota_);
		}

		void ParticleSystem::setTextureNode(const variant& node)
		{
			texture_node_ = node;
		}

		void ParticleSystem::fastForward()
		{
			if(fast_forward_) {
				for(float t = 0; t < fast_forward_->first; t += fast_forward_->second) {
					update(fast_forward_->second);
					elapsed_time_ += fast_forward_->second;
				}
			}
		}

		std::pair<float,float> ParticleSystem::getFastForward() const
		{
			if(fast_forward_) {
				return *fast_forward_;
			}

			return std::pair<float,float>(0.0f, 0.05f);
		}

		void ParticleSystem::setFastForward(const std::pair<float,float>& p)
		{
			fast_forward_.reset(new std::pair<float,float>(p));
		}

		ParticleSystem::ParticleSystem(const ParticleSystem& ps)
			: EmitObject(ps),
			  SceneObject(ps),
			  default_particle_width_(ps.default_particle_width_),
			  default_particle_height_(ps.default_particle_height_),
			  default_particle_depth_(ps.default_particle_depth_),
			  particle_quota_(ps.particle_quota_),
			  elapsed_time_(0),
			  scale_velocity_(ps.scale_velocity_),
			  scale_time_(ps.scale_time_),
			  scale_dimensions_(ps.scale_dimensions_),
			  texture_node_(),
			  use_position_(ps.use_position_)

		{
			if(ps.fast_forward_) {
				fast_forward_.reset(new std::pair<float,float>(ps.fast_forward_->first, ps.fast_forward_->second));
			}
			setShader(ShaderProgram::getProgram("particles_shader"));

			if(ps.max_velocity_) {
				max_velocity_.reset(new float(*ps.max_velocity_));
			}
			if(ps.texture_node_.is_map()) {
				setTextureNode(ps.texture_node_);
			}
			initAttributes();
		}

		void ParticleSystem::handleWrite(variant_builder* build) const
		{
			Renderable::writeData(build);

			if(use_position_ == false) {
				build->add("use_position", use_position_);
			}

			if(texture_node_.is_null() == false) {
				build->add("texture", texture_node_);
			}

			if(default_particle_width_ != 1.0f) {
				build->add("default_particle_width", default_particle_width_);
			}
			if(default_particle_height_ != 1.0f) {
				build->add("default_particle_height", default_particle_height_);
			}
			if(default_particle_depth_ != 1.0f) {
				build->add("default_particle_depth", default_particle_depth_);
			}
			if(particle_quota_ != 100) {
				build->add("particle_quota", particle_quota_);
			}
			if(scale_velocity_ != 1.0f) {
				build->add("scale_velocity", scale_velocity_);
			}
			if(scale_time_ != 1.0f) {
				build->add("scale_time", scale_time_);
			}
			if(scale_dimensions_ != glm::vec3(1.0f)) {
				if(scale_dimensions_.x == scale_dimensions_.y && scale_dimensions_.x == scale_dimensions_.z) {
					build->add("scale", scale_dimensions_.x);
				} else {
					build->add("scale", vec3_to_variant(scale_dimensions_));
				}
			}
			if(fast_forward_) {
				variant_builder res;
				res.add("time", fast_forward_->first);
				res.add("interval", fast_forward_->second);
				build->add("fast_forward", res.build());
			}
			if(max_velocity_) {
				build->add("max_velocity", *max_velocity_);
			}
			build->add("emitter", emitter_->write());
			for(const auto& aff : affectors_) {
				build->add("affector", aff->write());
			}
		}

		void ParticleSystem::update(float dt)
		{
			// run objects
			active_emitter_->emitProcess(dt);
			for(auto a : affectors_) {
				a->emitProcess(dt);
			}

			// Decrement the ttl on particles
			for(auto& p : active_particles_) {
				p.current.time_to_live -= dt;
			}

			active_emitter_->current.time_to_live -= dt;

			// Kill end-of-life particles
			active_particles_.erase(std::remove_if(active_particles_.begin(), active_particles_.end(),
				[](decltype(active_particles_[0]) p){return p.current.time_to_live <= 0.0f;}), 
				active_particles_.end());
			// Kill end-of-life emitters
			if(active_emitter_->current.time_to_live <= 0.0f) {
				active_emitter_.reset();
			}
			//active_affectors_.erase(std::remove_if(active_affectors_.begin(), active_affectors_.end(),
			//	[](decltype(active_affectors_[0]) e){return e->current.time_to_live < 0.0f;}), 
			//	active_affectors_.end());


			if(active_emitter_) {
				if(max_velocity_ && active_emitter_->current.velocity*glm::length(active_emitter_->current.direction) > *max_velocity_) {
					active_emitter_->current.direction *= *max_velocity_ / glm::length(active_emitter_->current.direction);
				}
				active_emitter_->current.position += active_emitter_->current.direction * active_emitter_->current.velocity * getScaleVelocity() * dt;
				//std::cerr << *active_emitter_ << std::endl;
			}

			/*for(auto a : active_affectors_) {
			if(max_velocity_ && a->current.velocity*glm::length(a->current.direction) > *max_velocity_) {
			a->current.direction *= *max_velocity_ / glm::length(a->current.direction);
			}
			a->current.position += a->current.direction * getParticleSystem()->getScaleVelocity() * static_cast<float>(t);
			//std::cerr << *a << std::endl;
			}*/

			// update particle positions
			for(auto& p : active_particles_) {
				if(max_velocity_ && p.current.velocity*glm::length(p.current.direction) > *max_velocity_) {
					p.current.direction *= *max_velocity_ / glm::length(p.current.direction);
				}

				p.current.position += p.current.direction * p.current.velocity * getScaleVelocity() * dt;

				//std::cerr << p << std::endl;
			}
			//if(active_particles_.size() > 0) {
			//	std::cerr << active_particles_[0] << std::endl;
			//}

			//std::cerr << "XXX: " << name() << " Active Particle Count: " << active_particles_.size() << std::endl;
			//std::cerr << "XXX: " << name() << " Active Emitter Count: " << active_emitters_.size() << std::endl;
		}

		void ParticleSystem::handleEmitProcess(float t)
		{
			t *= scale_time_;
			update(t);
			elapsed_time_ += t;
		}

		ParticleSystemPtr ParticleSystem::factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
		{
			auto ps = std::make_shared<ParticleSystem>(parent, node);
			return ps;
		}

		void ParticleSystem::initAttributes()
		{
			// XXX We need to render to a billboard style renderer ala 
			// http://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/billboards/
			/*auto& urv_ = std::make_shared<UniformRenderVariable<glm::vec4>>();
			urv_->AddVariableDescription(UniformRenderVariableDesc::COLOR, UniformRenderVariableDesc::FLOAT_VEC4);
			AddUniformRenderVariable(urv_);
			urv_->Update(glm::vec4(1.0f,1.0f,1.0f,1.0f));*/

			setShader(ShaderProgram::getProgram("particles_shader"));

			//auto as = DisplayDevice::createAttributeSet(true, false ,true);
			auto as = DisplayDevice::createAttributeSet(true, false, false);
			as->setDrawMode(DrawMode::TRIANGLES);

			arv_ = std::make_shared<Attribute<particle_s>>(AccessFreqHint::DYNAMIC);
			arv_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 3, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, vertex)));
			arv_->addAttributeDesc(AttributeDesc("a_center_position", 3, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, center)));
			arv_->addAttributeDesc(AttributeDesc("a_qrotation", 4, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, q)));
			arv_->addAttributeDesc(AttributeDesc("a_scale", 3, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, scale)));
			arv_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(particle_s), offsetof(particle_s, texcoord)));
			arv_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(particle_s), offsetof(particle_s, color)));

			as->addAttribute(arv_);
			addAttributeSet(as);
		}

		namespace {
			std::vector<glm::vec3> g_particle_system_translation;
		}

		ParticleSystem::TranslationScope::TranslationScope(const glm::vec3& v) {
			g_particle_system_translation.push_back(v);
		}

		ParticleSystem::TranslationScope::~TranslationScope() {
		}

		void ParticleSystem::preRender(const WindowPtr& wnd)
		{
			if(active_particles_.size() == 0) {
				arv_->clear();
				Renderable::disable();
				return;
			}
			Renderable::enable();
			//LOG_DEBUG("Technique::preRender, particle count: " << active_particles_.size());
			std::vector<particle_s> vtc;
			vtc.reserve(active_particles_.size() * 6);

			const auto tex = getTexture();
			//if(!tex) {
			//	return;
			//}
			for(auto it = active_particles_.begin(); it != active_particles_.end(); ++it) {
				auto& p = *it;

				const auto rf = p.current.area;//tex->getSourceRectNormalised();
				const glm::vec2 tl{ rf.x1(), rf.y2() };
				const glm::vec2 bl{ rf.x1(), rf.y1() };
				const glm::vec2 tr{ rf.x2(), rf.y2() };
				const glm::vec2 br{ rf.x2(), rf.y1() };

				if(!p.init_pos) {
					p.current.position += getPosition();
					if(!ignoreGlobalModelMatrix() && !useParticleSystemPosition()) {
						p.current.position += glm::vec3(get_global_model_matrix()[3]); // need global model translation.
					}

					p.init_pos = true;
				} else if(!useParticleSystemPosition() && g_particle_system_translation.empty() == false) {
					//This particle doesn't move relative to its object, so
					//just adjust it according to how much the screen translation
					//has changed since last frame.'
					p.current.position += g_particle_system_translation.back();
				}

				auto cp = p.current.position;

				for(int n = 0; n != 3; ++n) {
					cp[n] *= getScaleDimensions()[n];
				}

				if(!ignoreGlobalModelMatrix()) {
					if(useParticleSystemPosition()) {
						cp += glm::vec3(get_global_model_matrix()[3]); // need global model translation.
					}
				}

				const glm::vec3 p1 = cp - p.current.dimensions / 2.0f;
				const glm::vec3 p2 = cp + p.current.dimensions / 2.0f;
				const glm::vec4 q{ p.current.orientation.x, p.current.orientation.y, p.current.orientation.z, p.current.orientation.w };
				vtc.emplace_back(
					glm::vec3(p1.x, p1.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					tl,						// tex coord
					p.current.color);		// color
				vtc.emplace_back(
					glm::vec3(p2.x, p1.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),	// scale
					tr,						// tex coord
					p.current.color);		// color
				vtc.emplace_back(
					glm::vec3(p1.x, p2.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					bl,						// tex coord
					p.current.color);		// color

				vtc.emplace_back(
					glm::vec3(p1.x, p2.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					bl,						// tex coord
					p.current.color);		// color
				vtc.emplace_back(
					glm::vec3(p2.x, p2.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					br,						// tex coord
					p.current.color);		// color
				vtc.emplace_back(
					glm::vec3(p2.x, p1.y, p1.z),
					cp,		// center position
					q,
					getScaleDimensions(),		// scale
					tr,						// tex coord
					p.current.color);		// color
			}
			arv_->update(&vtc);
		}

		void ParticleSystem::postRender(const WindowPtr& wnd)
		{
			if(active_emitter_) {
				if(active_emitter_->doDebugDraw()) {
					active_emitter_->draw(wnd);
				}
			}
			for(auto& aff : affectors_) {
				if(aff->doDebugDraw()) {
					aff->draw(wnd);
				}
			}
		}

		ParticleSystemContainer::ParticleSystemContainer(std::weak_ptr<SceneGraph> sg, const variant& node) 
			: SceneNode(sg, node),
			  particle_system_()
		{
		}

		void ParticleSystemContainer::notifyNodeAttached(std::weak_ptr<SceneNode> parent)
		{
			attachObject(particle_system_);
		}

		ParticleSystemContainerPtr ParticleSystemContainer::get_this_ptr()
		{
			return std::static_pointer_cast<ParticleSystemContainer>(shared_from_this());
		}

		variant ParticleSystemContainer::write() const 
		{
			if(particle_system_) {
				return particle_system_->write();
			}
			return variant();
		}
		
		void ParticleSystemContainer::init(const variant& node)
		{
			particle_system_ = ParticleSystem::factory(get_this_ptr(), node);
			particle_system_->init();
		}

		ParticleSystemContainerPtr ParticleSystemContainer::create(std::weak_ptr<SceneGraph> sg, const variant& node)
		{
			auto ps = std::make_shared<ParticleSystemContainer>(sg, node);
			ps->init(node);
			return ps;
		}

		void ParticleSystemContainer::process(float delta_time)
		{
			//LOG_DEBUG("ParticleSystemContainer::Process: " << delta_time);
			particle_system_->emitProcess(delta_time);
		}

		EmitObject::EmitObject(std::weak_ptr<ParticleSystemContainer> parent)
			: name_(),
			  enabled_(true),
			  do_debug_draw_(false),
			  parent_container_(parent) 
		{
			ASSERT_LOG(parent.lock() != nullptr, "parent is null");
			std::stringstream ss;
			ss << "emit_object_" << static_cast<int>(get_random_float()*100);
			name_ = ss.str();
		}

		EmitObject::EmitObject(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: name_(),
			  enabled_(node["enabled"].as_bool(true)),
			  do_debug_draw_(node["debug_draw"].as_bool(false)),
			  parent_container_(parent) 
		{
			ASSERT_LOG(parent.lock() != nullptr, "parent is null");
			if(node.has_key("name")) {
				name_ = node["name"].as_string();
			} else if(node.has_key("id")) {
				name_ = node["id"].as_string();
			} else {
				std::stringstream ss;
				ss << "emit_object_" << static_cast<int>(get_random_float()*100);
				name_ = ss.str();
			}
		}

		variant EmitObject::write() const
		{
			variant_builder res;
			res.add("name", name_);
			if(!enabled_) {
				res.add("enabled", enabled_);
			}
			if(do_debug_draw_) {
				res.add("debug_draw", do_debug_draw_);
			}
			handleWrite(&res);
			return res.build();
		}

		ParticleSystemContainerPtr EmitObject::getParentContainer() const 
		{ 
			auto parent = parent_container_.lock();
			ASSERT_LOG(parent != nullptr, "parent container is nullptr");
			return parent; 
		}

		DebugDrawHelper::DebugDrawHelper() 
				: SceneObject("DebugDrawHelper")
		{
			setShader(ShaderProgram::getProgram("attr_color_shader"));

			auto as = DisplayDevice::createAttributeSet(true, false, false);
			as->setDrawMode(DrawMode::LINES);

			attrs_ = std::make_shared<Attribute<vertex_color3>>(AccessFreqHint::DYNAMIC);
			attrs_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 3, AttrFormat::FLOAT, false, sizeof(vertex_color3), offsetof(vertex_color3, vertex)));
			attrs_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_color3), offsetof(vertex_color3, color)));

			as->addAttribute(attrs_);
			addAttributeSet(as);
		}

		void DebugDrawHelper::update(const glm::vec3& p1, const glm::vec3& p2, const Color& col) 
		{
			std::vector<vertex_color3> res;
			const glm::u8vec4& color = col.as_u8vec4();
			res.emplace_back(p1, color);
			res.emplace_back(glm::vec3(p1.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p2.x, p1.y, p1.z), color);

			res.emplace_back(glm::vec3(p1.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p1.x, p2.y, p1.z), color);

			res.emplace_back(glm::vec3(p1.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p1.x, p1.y, p2.z), color);

			res.emplace_back(glm::vec3(p2.x, p2.y, p2.z), color);
			res.emplace_back(glm::vec3(p1.x, p2.y, p2.z), color);
					
			res.emplace_back(glm::vec3(p2.x, p2.y, p2.z), color);
			res.emplace_back(glm::vec3(p2.x, p1.y, p2.z), color);
					
			res.emplace_back(glm::vec3(p2.x, p2.y, p2.z), color);
			res.emplace_back(glm::vec3(p2.x, p2.y, p1.z), color);

			res.emplace_back(glm::vec3(p1.x, p2.y, p1.z), color);
			res.emplace_back(glm::vec3(p1.x, p2.y, p2.z), color);

			res.emplace_back(glm::vec3(p1.x, p2.y, p1.z), color);
			res.emplace_back(glm::vec3(p2.x, p2.y, p1.z), color);

			res.emplace_back(glm::vec3(p2.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p2.x, p2.y, p1.z), color);

			res.emplace_back(glm::vec3(p2.x, p1.y, p1.z), color);
			res.emplace_back(glm::vec3(p2.x, p1.y, p2.z), color);

			attrs_->update(&res);
		}

		void convert_quat_to_axis_angle(const glm::quat& q, float* angle, glm::vec3* axis)
		{
			glm::quat newq = q;
			if(q.w > 1.0f) {
				newq = glm::normalize(q);
			}
			if(angle) {
				*angle = 2.0f * std::acos(newq.w);
			}
			float s = std::sqrt(1.0f - newq.w * newq.w);
			if(s < 0.001f) {
				if(axis) {
					axis->x = newq.x;
					axis->y = newq.y;
					axis->z = newq.z;
				}
			} else {
				if(axis) {
					axis->x = newq.x / s;
					axis->y = newq.y / s;
					axis->z = newq.z / s;
				}
			}
		}
	}
}
