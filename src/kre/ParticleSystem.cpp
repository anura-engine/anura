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

#include "ParticleSystem.hpp"
#include "ParticleSystemAffectors.hpp"
#include "ParticleSystemParameters.hpp"
#include "ParticleSystemEmitters.hpp"
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

		}

		float get_random_float(float min, float max)
		{
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
			glm::quat q = glm::angleAxis(get_random_float(0.0f,360.0f), v);
			up_up = q * up_up;

			q = glm::angleAxis(angle, up_up);
			return q * v;
		}

		ParticleSystem::ParticleSystem(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: EmitObject(parent, node), 
			  SceneNode(getParentContainer()->getParentGraph(), node),
			  elapsed_time_(0.0f), 
			  scale_velocity_(1.0f), 
			  scale_time_(1.0f),
			  scale_dimensions_(1.0f)
		{
			ASSERT_LOG(node.has_key("technique"), "Must have a list of techniques to create particles.");
			ASSERT_LOG(node["technique"].is_map() || node["technique"].is_list(), "'technique' attribute must be map or list." << node["technique"].to_debug_string());
			if(node["technique"].is_map()) {
				getParentContainer()->addTechnique(Technique::create(parent, node["technique"]));
			} else {
				for(size_t n = 0; n != node["technique"].num_elements(); ++n) {
					getParentContainer()->addTechnique(Technique::create(parent, node["technique"][n]));
				}
			}
			if(node.has_key("fast_forward")) {
				float ff_time = float(node["fast_forward"]["time"].as_float());
				float ff_interval = float(node["fast_forward"]["time"].as_float());
				fast_forward_.reset(new std::pair<float,float>(ff_time, ff_interval));
			}

			if(node.has_key("scale_velocity")) {
				scale_velocity_ = float(node["scale_velocity"].as_float());
			}
			if(node.has_key("scale_time")) {
				scale_time_ = float(node["scale_time"].as_float());
			}
			if(node.has_key("scale")) {
				scale_dimensions_ = variant_to_vec3(node["scale"]);
			}
		}

		void ParticleSystem::init(const variant& node)
		{
			// process "active_techniques" here
			if(node.has_key("active_techniques")) {
				if(node["active_techniques"].is_list()) {
					for(size_t n = 0; n != node["active_techniques"].num_elements(); ++n) {
						active_techniques_.emplace_back(getParentContainer()->cloneTechnique(node["active_techniques"][n].as_string()));
						active_techniques_.back()->setParent(get_this_ptr());
					}
				} else if(node["active_techniques"].is_string()) {
					active_techniques_.emplace_back(getParentContainer()->cloneTechnique(node["active_techniques"].as_string()));
					active_techniques_.back()->setParent(get_this_ptr());					
				} else {
					ASSERT_LOG(false, "'active_techniques' attribute must be list of strings or single string.");
				}
			} else {
				active_techniques_ = getParentContainer()->cloneTechniques();
				for(auto tq : active_techniques_) {
					tq->setParent(get_this_ptr());
				}
			}

			if(fast_forward_) {
				for(float t = 0; t < fast_forward_->first; t += fast_forward_->second) {
					update(fast_forward_->second);
					elapsed_time_ += fast_forward_->second;
				}
			}
		}

		ParticleSystemPtr ParticleSystem::clone() const
		{
			//auto ps = std::make_shared<ParticleSystem>(*this);
			auto psc = new ParticleSystem(*this);
			auto ps = std::shared_ptr<ParticleSystem>(psc);
			for(auto tq : active_techniques_) {
				ps->active_techniques_.emplace_back(tq->clone());
			}
			return ps;
		}

		ParticleSystemPtr ParticleSystem::get_this_ptr()
		{
			return std::static_pointer_cast<ParticleSystem>(shared_from_this());
		}

		ParticleSystem::ParticleSystem(const ParticleSystem& ps)
			: EmitObject(ps),
			  SceneNode(ps),
			  elapsed_time_(0),
			  scale_velocity_(ps.scale_velocity_),
			  scale_time_(ps.scale_time_),
			  scale_dimensions_(ps.scale_dimensions_)
		{
			if(ps.fast_forward_) {
				fast_forward_.reset(new std::pair<float,float>(ps.fast_forward_->first, ps.fast_forward_->second));
			}
		}

		void ParticleSystem::notifyNodeAttached(std::weak_ptr<SceneNode> parent)
		{
			for(auto t : active_techniques_) {
				attachObject(t);
			}
		}

		void ParticleSystem::update(float dt)
		{
			for(auto t : active_techniques_) {
				t->emitProcess(dt);
			}
		}

		void ParticleSystem::handleEmitProcess(float t)
		{
			update(static_cast<float>(t));
			elapsed_time_ += static_cast<float>(t);
		}

		void ParticleSystem::addTechnique(TechniquePtr tq)
		{
			active_techniques_.emplace_back(tq);
			tq->setParent(get_this_ptr());
		}

		ParticleSystemPtr ParticleSystem::factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
		{
			auto ps = std::make_shared<ParticleSystem>(parent, node);
			ps->init(node);
			return ps;
		}


		TechniquePtr Technique::create(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
		{
			auto tq = std::make_shared<Technique>(parent, node);
			tq->init(node);
			return tq;
		}

		Technique::Technique(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: SceneObject(node),
			  EmitObject(parent, node), 
			  default_particle_width_(node["default_particle_width"].as_float(1.0f)),
			  default_particle_height_(node["default_particle_height"].as_float(1.0f)),
			  default_particle_depth_(node["default_particle_depth"].as_float(1.0f)),
			  lod_index_(node["lod_index"].as_int32(0)), velocity_(1.0f),
			  emitter_quota_(node["emitted_emitter_quota"].as_int32(50)),
			  affector_quota_(node["emitted_affector_quota"].as_int32(10)),
			  technique_quota_(node["emitted_technique_quota"].as_int32(10)),			
			  system_quota_(node["emitted_system_quota"].as_int32(10))
		{
			ASSERT_LOG(node.has_key("visual_particle_quota"), "'Technique' must have 'visual_particle_quota' attribute.");
			particle_quota_ = node["visual_particle_quota"].as_int32();
			ASSERT_LOG(node.has_key("texture"), "'Technique' must have 'material' attribute.");
			//ASSERT_LOG(node.has_key("renderer"), "'Technique' must have 'renderer' attribute.");
			//renderer_.reset(new renderer(node["renderer"]));
			if(node.has_key("emitter")) {
				if(node["emitter"].is_map()) {
					getParentContainer()->addEmitter(Emitter::factory(parent, node["emitter"]));
				} else if(node["emitter"].is_list()) {
					for(size_t n = 0; n != node["emitter"].num_elements(); ++n) {
						getParentContainer()->addEmitter(Emitter::factory(parent, node["emitter"][n]));
					}
				} else {
					ASSERT_LOG(false, "'emitter' attribute must be a list or map.");
				}
			}
			if(node.has_key("affector")) {
				if(node["affector"].is_map()) {
					getParentContainer()->addAffector(Affector::factory(parent, node["affector"]));
				} else if(node["affector"].is_list()) {
					for(size_t n = 0; n != node["affector"].num_elements(); ++n) {
						getParentContainer()->addAffector(Affector::factory(parent, node["affector"][n]));
					}
				} else {
					ASSERT_LOG(false, "'affector' attribute must be a list or map.");
				}
			}
			if(node.has_key("max_velocity")) {
				max_velocity_.reset(new float(node["max_velocity"].as_float()));
			}

		}

		void Technique::init(const variant& node)
		{
			// conditional addition of emitters/affectors
			if(node.has_key("active_emitters")) {
				std::vector<std::string> active_emitters = node["active_emitters"].as_list_string();
				for(auto e : active_emitters) {
					auto em = getParentContainer()->cloneEmitter(e);
					active_emitters_.emplace_back(em);
					em->setParentTechnique(shared_from_this());
				}
			} else {
				for(auto es : getParentContainer()->cloneEmitters()) {
					active_emitters_.emplace_back(es);
					es->setParentTechnique(shared_from_this());
				}
			}
			if(node.has_key("active_affectors")) {
				std::vector<std::string> active_affectors = node["active_affectors"].as_list_string();
				for(auto a : active_affectors) {
					auto aff = getParentContainer()->cloneAffector(a);
					active_affectors_.emplace_back(aff);
					aff->setParentTechnique(shared_from_this());
				}
			} else {
				for(auto as : getParentContainer()->cloneAffectors()) {
					active_affectors_.emplace_back(as);
					as->setParentTechnique(shared_from_this());
				}
			}

			// In order to create as few re-allocations of particles, reserve space here
			active_particles_.reserve(particle_quota_);

			initAttributes();
		}

		ParticleSystemPtr Technique::getParticleSystem() const
		{ 
			auto ps = particle_system_.lock();
			ASSERT_LOG(ps != nullptr, "Parent particle system was null.");
			return ps;
		}

		Technique::Technique(const Technique& tq) 
			: SceneObject(tq),
			  EmitObject(tq),
			  default_particle_width_(tq.default_particle_width_),
			  default_particle_height_(tq.default_particle_height_),
			  default_particle_depth_(tq.default_particle_depth_),
			  particle_quota_(tq.particle_quota_),
			  emitter_quota_(tq.emitter_quota_),
			  affector_quota_(tq.affector_quota_),
			  technique_quota_(tq.technique_quota_),
			  system_quota_(tq.system_quota_),
			  lod_index_(tq.lod_index_),
			  velocity_(tq.velocity_),
			  particle_system_(tq.particle_system_)
		{
			setShader(ShaderProgram::getProgram("vtc_shader"));

			if(tq.getTexture()) {
				setTexture(tq.getTexture()->clone());
			}
			if(tq.max_velocity_) {
				max_velocity_.reset(new float(*tq.max_velocity_));
			}

			initAttributes();
		}

		void Technique::setParent(std::weak_ptr<ParticleSystem> parent)
		{
			ASSERT_LOG(parent.lock() != nullptr, "parent is null");
			particle_system_ = parent;
		}

		void Technique::addEmitter(EmitterPtr e) 
		{
			e->setParentTechnique(shared_from_this());
			//active_emitters_.emplace_back(e);
			instanced_emitters_.emplace_back(e);
		}

		void Technique::addAffector(AffectorPtr a) 
		{
			a->setParentTechnique(shared_from_this());
			//active_affectors_.emplace_back(a);
			instanced_affectors_.emplace_back(a);
		}

		void Technique::handleEmitProcess(float t)
		{
			// run objects
			for(auto e : active_emitters_) {
				e->emitProcess(t);
			}
			for(auto e : instanced_emitters_) {
				e->emitProcess(t);
			}
			for(auto a : active_affectors_) {
				a->emitProcess(t);
			}
			for(auto a : instanced_affectors_) {
				a->emitProcess(t);
			}

			// Decrement the ttl on particles
			for(auto& p : active_particles_) {
				p.current.time_to_live -= t;
			}
			// Decrement the ttl on instanced emitters
			for(auto e : instanced_emitters_) {
				e->current.time_to_live -= t;
			}

			// Kill end-of-life particles
			active_particles_.erase(std::remove_if(active_particles_.begin(), active_particles_.end(),
				[](decltype(active_particles_[0]) p){return p.current.time_to_live < 0.0f;}), 
				active_particles_.end());
			// Kill end-of-life emitters
			instanced_emitters_.erase(std::remove_if(instanced_emitters_.begin(), instanced_emitters_.end(),
				[](decltype(instanced_emitters_[0]) e){return e->current.time_to_live < 0.0f;}), 
				instanced_emitters_.end());


			for(auto e : instanced_emitters_) {
				if(max_velocity_ && e->current.velocity*glm::length(e->current.direction) > *max_velocity_) {
					e->current.direction *= *max_velocity_ / glm::length(e->current.direction);
				}
				e->current.position += e->current.direction * getParticleSystem()->getScaleVelocity() * static_cast<float>(t);
				//std::cerr << *e << std::endl;
			}

			// update particle positions
			for(auto& p : active_particles_) {
				if(max_velocity_ && p.current.velocity*glm::length(p.current.direction) > *max_velocity_) {
					p.current.direction *= *max_velocity_ / glm::length(p.current.direction);
				}

				p.current.position += p.current.direction * getParticleSystem()->getScaleVelocity() * static_cast<float>(t);

				//std::cerr << p << std::endl;
			}

			//std::cerr << "XXX: Active Particle Count: " << active_particles_.size() << std::endl;
			//std::cerr << "XXX: Active Emitter Count: " << active_emitters_.size() << std::endl;
		}

		void Technique::initAttributes()
		{
			// XXX We need to render to a billboard style renderer ala 
			// http://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/billboards/
			/*auto& urv_ = std::make_shared<UniformRenderVariable<glm::vec4>>();
			urv_->AddVariableDescription(UniformRenderVariableDesc::COLOR, UniformRenderVariableDesc::FLOAT_VEC4);
			AddUniformRenderVariable(urv_);
			urv_->Update(glm::vec4(1.0f,1.0f,1.0f,1.0f));*/

			setShader(ShaderProgram::getProgram("vtc_shader"));

			//auto as = DisplayDevice::createAttributeSet(true, false ,true);
			auto as = DisplayDevice::createAttributeSet(true, false, false);
			as->setDrawMode(DrawMode::TRIANGLES);

			arv_ = std::make_shared<Attribute<vertex_texture_color3>>(AccessFreqHint::DYNAMIC);
			arv_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 3, AttrFormat::FLOAT, false, sizeof(vertex_texture_color3), offsetof(vertex_texture_color3, vertex)));
			arv_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texture_color3), offsetof(vertex_texture_color3, texcoord)));
			arv_->addAttributeDesc(AttributeDesc(AttrType::COLOR, 4, AttrFormat::UNSIGNED_BYTE, true, sizeof(vertex_texture_color3), offsetof(vertex_texture_color3, color)));

			as->addAttribute(arv_);
			addAttributeSet(as);

			setOrder(1);
		}

		void Technique::preRender(const WindowPtr& wnd)
		{
			//LOG_DEBUG("Technique::preRender, particle count: " << active_particles_.size());
			std::vector<vertex_texture_color3> vtc;
			vtc.reserve(active_particles_.size() * 6);
			for(auto& p : active_particles_) {
				vtc.emplace_back(glm::vec3(p.current.position.x,p.current.position.y,p.current.position.z), glm::vec2(0.0f,0.0f), p.current.color);
				vtc.emplace_back(glm::vec3(p.current.position.x,p.current.position.y+p.current.dimensions.y,p.current.position.z), glm::vec2(0.0f,1.0f), p.current.color);
				vtc.emplace_back(glm::vec3(p.current.position.x+p.current.dimensions.x,p.current.position.y,p.current.position.z), glm::vec2(1.0f,0.0f), p.current.color);

				vtc.emplace_back(glm::vec3(p.current.position.x+p.current.dimensions.x,p.current.position.y,p.current.position.z), glm::vec2(1.0f,0.0f), p.current.color);
				vtc.emplace_back(glm::vec3(p.current.position.x,p.current.position.y+p.current.dimensions.y,p.current.position.z), glm::vec2(0.0f,1.0f), p.current.color);
				vtc.emplace_back(glm::vec3(p.current.position.x+p.current.dimensions.x,p.current.position.y+p.current.dimensions.y,p.current.position.z), glm::vec2(1.0f,1.0f), p.current.color);
			}
			arv_->update(&vtc);

			//getAttributeSet().back()->setCount(active_particles_.size());
		}

		ParticleSystemContainer::ParticleSystemContainer(std::weak_ptr<SceneGraph> sg, const variant& node) 
			: SceneNode(sg, node)
		{
		}

		void ParticleSystemContainer::notifyNodeAttached(std::weak_ptr<SceneNode> parent)
		{
			for(auto& a : active_particle_systems_) {
				attachNode(a);
				a->setNodeName("ps_node_" + a->name());
			}
		}

		ParticleSystemContainerPtr ParticleSystemContainer::get_this_ptr()
		{
			return std::static_pointer_cast<ParticleSystemContainer>(shared_from_this());
		}
		
		void ParticleSystemContainer::init(const variant& node)
		{
			if(node.has_key("systems")) {
				if(node["systems"].is_list()) {
					for(size_t n = 0; n != node["systems"].num_elements(); ++n) {
						addParticleSystem(ParticleSystem::factory(get_this_ptr(), node["systems"][n]));
					}
				} else if(node["systems"].is_map()) {
					addParticleSystem(ParticleSystem::factory(get_this_ptr(), node["systems"]));
				} else {
					ASSERT_LOG(false, "unrecognised type for 'systems' attribute must be list or map");
				}
			} else {
				addParticleSystem(ParticleSystem::factory(get_this_ptr(), node));
			}

			if(node.has_key("active_systems")) {
				if(node["active_systems"].is_list()) {
					for(size_t n = 0; n != node["active_systems"].num_elements(); ++n) {
						active_particle_systems_.emplace_back(cloneParticleSystem(node["active_systems"][n].as_string()));
					}
				} else if(node["active_systems"].is_string()) {
					active_particle_systems_.emplace_back(cloneParticleSystem(node["active_systems"].as_string()));
				} else {
					ASSERT_LOG(false, "'active_systems' attribute must be a string or list of strings.");
				}
			} else {
				active_particle_systems_ = cloneParticleSystems();
			}
		}

		ParticleSystemContainerPtr ParticleSystemContainer::create(std::weak_ptr<SceneGraph> sg, const variant& node)
		{
			auto ps = std::make_shared<ParticleSystemContainer>(sg, node);
			ps->init(node);
			return ps;
		}

		void ParticleSystemContainer::process(float delta_time)
		{
			//LOG_DEBUG("ParticleSystemContainer::Process: " << current_time);
			for(auto ps : active_particle_systems_) {
				ps->emitProcess(delta_time);
			}
		}

		void ParticleSystemContainer::addParticleSystem(ParticleSystemPtr obj)
		{
			particle_systems_.emplace_back(obj);
		}

		void ParticleSystemContainer::addTechnique(TechniquePtr obj)
		{
			techniques_.emplace_back(obj);
		}

		void ParticleSystemContainer::addEmitter(EmitterPtr obj)
		{
			emitters_.emplace_back(obj);
		}

		void ParticleSystemContainer::addAffector(AffectorPtr obj) 
		{
			affectors_.emplace_back(obj);
		}

		void ParticleSystemContainer::getActivateParticleSystem(const std::string& name)
		{
			active_particle_systems_.emplace_back(cloneParticleSystem(name));
		}

		ParticleSystemPtr ParticleSystemContainer::cloneParticleSystem(const std::string& name)
		{
			for(auto ps : particle_systems_) {
				if(ps->name() == name) {
					return std::make_shared<ParticleSystem>(*ps);
				}
			}
			ASSERT_LOG(false, "ParticleSystem not found: " << name);
			return ParticleSystemPtr();
		}

		TechniquePtr ParticleSystemContainer::cloneTechnique(const std::string& name)
		{
			for(auto tq : techniques_) {
				if(tq->name() == name) {
					return tq->clone();
				}
			}
			ASSERT_LOG(false, "Technique not found: " << name);
			return TechniquePtr();
		}

		EmitterPtr ParticleSystemContainer::cloneEmitter(const std::string& name)
		{
			for(auto e : emitters_) {
				if(e->name() == name) {
					return e->clone();
				}
			}
			ASSERT_LOG(false, "emitter not found: " << name);
			return EmitterPtr();
		}

		AffectorPtr ParticleSystemContainer::cloneAffector(const std::string& name)
		{
			for(auto a : affectors_) {
				if(a->name() == name) {
					return a->clone();
				}
			}
			ASSERT_LOG(false, "affector not found: " << name);
			return AffectorPtr();
		}

		std::vector<ParticleSystemPtr> ParticleSystemContainer::cloneParticleSystems()
		{
			std::vector<ParticleSystemPtr> res;
			for(auto ps : particle_systems_) {
				res.emplace_back(ps->clone());
			}
			return res;
		}

		TechniquePtr Technique::clone() const
		{
			auto tq = std::make_shared<Technique>(*this);
			// XXX I'm not sure this should clone all the currently active 
			// emitters/affectors, or whether we should maintain a list of 
			// emitters/affectors that were initially specified.
			for(auto e : active_emitters_) {
				tq->active_emitters_.emplace_back(EmitterPtr(e->clone()));
				tq->active_emitters_.back()->setParentTechnique(tq);
			}
			for(auto a : active_affectors_) {
				tq->active_affectors_.emplace_back(AffectorPtr(a->clone()));
				tq->active_affectors_.back()->setParentTechnique(tq);
			}
			tq->active_particles_.reserve(particle_quota_);
			return tq;
		}
		
		std::vector<TechniquePtr> ParticleSystemContainer::cloneTechniques()
		{
			std::vector<TechniquePtr> res;
			for(auto tq : techniques_) {
				res.emplace_back(tq->clone());
			}
			return res;
		}

		std::vector<EmitterPtr> ParticleSystemContainer::cloneEmitters()
		{
			std::vector<EmitterPtr> res;
			for(auto e : emitters_) {
				res.emplace_back(e->clone());
			}
			return res;
		}

		std::vector<AffectorPtr> ParticleSystemContainer::cloneAffectors()
		{
			std::vector<AffectorPtr> res;
			for(auto a : affectors_) {
				res.emplace_back(a->clone());
			}
			return res;
		}

		EmitObject::EmitObject(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: parent_container_(parent) 
		{
			ASSERT_LOG(parent.lock() != nullptr, "parent is null");
			if(node.has_key("name")) {
				name_ = node["name"].as_string();
			} else {
				std::stringstream ss;
				ss << "emit_object_" << int(get_random_float());
				name_ = ss.str();
			}
		}

		ParticleSystemContainerPtr EmitObject::getParentContainer() const 
		{ 
			auto parent = parent_container_.lock();
			ASSERT_LOG(parent != nullptr, "parent container is nullptr");
			return parent; 
		}

		const glm::vec3& EmitObject::getPosition() const 
		{ 
			static glm::vec3 res; 
			return res; 
		}
	}
}
