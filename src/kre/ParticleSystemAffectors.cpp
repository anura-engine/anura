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

#include "asserts.hpp"
#include "ParticleSystem.hpp"
#include "ParticleSystemAffectors.hpp"
#include "ParticleSystemEmitters.hpp"
#include "ParticleSystemParameters.hpp"
#include "variant_utils.hpp"

extern bool g_particle_ui_2d;

namespace KRE
{
	namespace Particles
	{
		const char* get_affector_name(AffectorType type)
		{
			switch(type) {
				case KRE::Particles::AffectorType::COLOR:				return "Time/Color";
				case KRE::Particles::AffectorType::JET:					return "jet";
				case KRE::Particles::AffectorType::VORTEX:				return "Vortex";
				case KRE::Particles::AffectorType::GRAVITY:				return "Gravity";
				case KRE::Particles::AffectorType::LINEAR_FORCE:		return "Linear Force";
				case KRE::Particles::AffectorType::SCALE:				return "Scale";
				case KRE::Particles::AffectorType::PARTICLE_FOLLOWER:	return "Particle Follower";
				case KRE::Particles::AffectorType::ALIGN:				return "Align";
				case KRE::Particles::AffectorType::FLOCK_CENTERING:		return "Flock Centering";
				case KRE::Particles::AffectorType::BLACK_HOLE:			return "Black Hole";
				case KRE::Particles::AffectorType::PATH_FOLLOWER:		return "Path Follower";
				case KRE::Particles::AffectorType::RANDOMISER:			return "Randomizer";
				case KRE::Particles::AffectorType::SINE_FORCE:			return "Sine Force";
				case KRE::Particles::AffectorType::TEXTURE_ROTATOR:		return "Texture Rotator";
				case KRE::Particles::AffectorType::ANIMATION:			return "Texture Animation";
				default:
					ASSERT_LOG(false, "No name for affector: " << static_cast<int>(type));
					break;
			}
			return nullptr;
		}

		// affectors to add: box_collider (width,height,depth, inner or outer collide, friction)
		// forcefield (delta, force, octaves, frequency, amplitude, persistence, size, worldsize(w,h,d), movement(x,y,z),movement_frequency)
		// geometry_rotator (use own rotation, speed(parameter), axis(x,y,z))
		// inter_particle_collider (sounds like a lot of calculations)
		// line
		// plane_collider
		// scale_velocity (parameter_ptr scale; bool since_system_start, bool stop_at_flip)
		// sphere_collider
		// texture_animator
		// texture_rotator
		// velocity matching

		Affector::Affector(std::weak_ptr<ParticleSystemContainer> parent, AffectorType type)
			: EmitObject(parent), 
			  type_(type),
			  mass_(1.0f),
			  position_(0.0f), 
			  scale_(1.0f),
			  node_()
		{
		}

		Affector::Affector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node, AffectorType type)
			: EmitObject(parent, node), 
			  type_(type),
			  mass_(float(node["mass_affector"].as_float(1.0f))),
			  position_(0.0f), 
			  scale_(1.0f),
			  node_(node)
		{
			if(node.has_key("position")) {
				position_ = variant_to_vec3(node["position"]);
			}
			if(node.has_key("scale")) {
				scale_ = variant_to_vec3(node["scale"]);
			}
		}
		
		Affector::~Affector()
		{
		}

		variant Affector::write() const
		{
			variant_builder res;
			if(mass_ != 1.0f) {
				res.add("mass_affector", 1.0f);
			}
			if(position_ != glm::vec3(0.0f)) {
				res.add("position", vec3_to_variant(position_));
			}
			if(scale_ != glm::vec3(1.0f)) {
				res.add("scale", vec3_to_variant(scale_));
			}
			switch (type_) {
				case AffectorType::COLOR:
					res.add("type", "color");
					break;
				case AffectorType::JET:
					res.add("type", "jet");
					break;
				case AffectorType::VORTEX:
					res.add("type", "vortex");
					break;
				case AffectorType::GRAVITY:
					res.add("type", "gravity");
					break;
				case AffectorType::LINEAR_FORCE:
					res.add("type", "linear_force");
					break;
				case AffectorType::SCALE:
					res.add("type", "scale");
					break;
				case AffectorType::PARTICLE_FOLLOWER:
					res.add("type", "particle_follower");
					break;
				case AffectorType::ALIGN:
					res.add("type", "align");
					break;
				case AffectorType::FLOCK_CENTERING:
					res.add("type", "flock_centering");
					break;
				case AffectorType::BLACK_HOLE:
					res.add("type", "black_hole");
					break;
				case AffectorType::PATH_FOLLOWER:
					res.add("type", "path_follower");
					break;
				case AffectorType::RANDOMISER:
					res.add("type", "randomizer");
					break;
				case AffectorType::SINE_FORCE:
					res.add("type", "sine_force");
					break;
				case AffectorType::TEXTURE_ROTATOR:
					res.add("type", "texture_rotator");
					break;
				case AffectorType::ANIMATION:
					res.add("type", "animation");
					break;
				default:
					ASSERT_LOG(false, "Bad affector type: " << static_cast<int>(type_));
					break;
			}
			handleWrite(&res);
			return res.build();
		}

		void Affector::handleEmitProcess(float t) 
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			internalApply(*psystem->getEmitter(),t);
			
			for(auto& p : psystem->getActiveParticles()) {
				internalApply(p,t);
			}
		}

		AffectorPtr Affector::factory(std::weak_ptr<ParticleSystemContainer> parent, AffectorType type)
		{
			switch (type)
			{
			case AffectorType::COLOR:
				return std::make_shared<TimeColorAffector>(parent);
			case AffectorType::JET:
				return std::make_shared<JetAffector>(parent);
			case AffectorType::VORTEX:
				return std::make_shared<VortexAffector>(parent);
			case AffectorType::GRAVITY:
				return std::make_shared<GravityAffector>(parent);
			case AffectorType::LINEAR_FORCE:
				return std::make_shared<LinearForceAffector>(parent);
			case AffectorType::SCALE:
				return std::make_shared<ScaleAffector>(parent);
			case AffectorType::PARTICLE_FOLLOWER:
				return std::make_shared<ParticleFollowerAffector>(parent);
			case AffectorType::ALIGN:
				return std::make_shared<AlignAffector>(parent);
			case AffectorType::FLOCK_CENTERING:
				return std::make_shared<FlockCenteringAffector>(parent);
			case AffectorType::BLACK_HOLE:
				return std::make_shared<BlackHoleAffector>(parent);
			case AffectorType::PATH_FOLLOWER:
				return std::make_shared<PathFollowerAffector>(parent);
			case AffectorType::RANDOMISER:
				return std::make_shared<RandomiserAffector>(parent);
			case AffectorType::SINE_FORCE:
				return std::make_shared<SineForceAffector>(parent);
			case AffectorType::TEXTURE_ROTATOR:
				return std::make_shared<TextureRotatorAffector>(parent);
			case AffectorType::ANIMATION:
				return std::make_shared<AnimationAffector>(parent);
			default:
				ASSERT_LOG(false, "Unrecognised afftor type: " << static_cast<int>(type));
				break;
			}
			return nullptr;
		}

		AffectorPtr Affector::factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
		{
			ASSERT_LOG(node.has_key("type"), "affector must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "color" || ntype == "colour") {
				return std::make_shared<TimeColorAffector>(parent, node);
			} else if(ntype == "jet") {
				return std::make_shared<JetAffector>(parent, node);
			} else if(ntype == "vortex") {
				return std::make_shared<VortexAffector>(parent, node);
			} else if(ntype == "gravity") {
				return std::make_shared<GravityAffector>(parent, node);
			} else if(ntype == "linear_force") {
				return std::make_shared<LinearForceAffector>(parent, node);
			} else if(ntype == "scale") {
				return std::make_shared<ScaleAffector>(parent, node);
			} else if(ntype == "particle_follower") {
				return std::make_shared<ParticleFollowerAffector>(parent, node);
			} else if(ntype == "align") {
				return std::make_shared<AlignAffector>(parent, node);
			} else if(ntype == "randomiser" || ntype == "randomizer") {
				return std::make_shared<RandomiserAffector>(parent, node);
			} else if(ntype == "sine_force" || ntype == "sin_force") {
				return std::make_shared<SineForceAffector>(parent, node);
			} else if(ntype == "path_follower") {
				return std::make_shared<PathFollowerAffector>(parent, node);
			} else if(ntype == "black_hole") {
				return std::make_shared<BlackHoleAffector>(parent, node);
			} else if(ntype == "flock_centering") {
				return std::make_shared<FlockCenteringAffector>(parent, node);
			} else if(ntype == "texture_rotator") {
				return std::make_shared<TextureRotatorAffector>(parent, node);
			} else if(ntype == "animation") {
				return std::make_shared<AnimationAffector>(parent, node);
			} else {
				ASSERT_LOG(false, "Unrecognised affector type: " << ntype);
			}
			return nullptr;
		}


		TimeColorAffector::TimeColorAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::COLOR), 
			  operation_(ColourOperation::COLOR_OP_SET),
			  tc_data_(),
			  interpolate_(true)
		{			
		}

		TimeColorAffector::TimeColorAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node, AffectorType::COLOR), 
			  operation_(ColourOperation::COLOR_OP_SET),
			  tc_data_(),
			  interpolate_(true)
		{
			init(node);
		}

		void TimeColorAffector::init(const variant& node)
		{
			std::string op;
			if(node.has_key("color_operation")) {
				op = node["color_operation"].as_string();
			} else if(node.has_key("colour_operation")) {
				op = node["colour_operation"].as_string();
			}
			if(!op.empty()) {
				if(op == "multiply") {
					operation_ = ColourOperation::COLOR_OP_MULTIPLY;
				} else if(op == "set") {
					operation_ = ColourOperation::COLOR_OP_SET;
				} else {
					ASSERT_LOG(false, "unrecognised time_color affector operation: " << op);
				}
			}
			if(node.has_key("interpolate")) {
				interpolate_ = node["interpolate"].as_bool();
			}
			ASSERT_LOG(node.has_key("time_colour") || node.has_key("time_color"), "Must be a 'time_colour' attribute");
			const variant& tc_node = node.has_key("time_colour") ? node["time_colour"] : node["time_color"];
			if(tc_node.is_map()) {
				float t = tc_node["time"].as_float();
				glm::vec4 result;
				if(tc_node.has_key("color")) {
					ASSERT_LOG(tc_node["color"].is_list() && tc_node["color"].num_elements() == 4, "Expected vec4 variant but found " << tc_node["color"].write_json());
					result.r = tc_node["color"][0].as_float();
					result.g = tc_node["color"][1].as_float();
					result.b = tc_node["color"][2].as_float();
					result.a = tc_node["color"][3].as_float();
				} else if(tc_node.has_key("colour")) {
					ASSERT_LOG(tc_node["colour"].is_list() && tc_node["colour"].num_elements() == 4, "Expected vec4 variant but found " << tc_node["colour"].write_json());
					result.r = tc_node["colour"][0].as_float();
					result.g = tc_node["colour"][1].as_float();
					result.b = tc_node["colour"][2].as_float();
					result.a = tc_node["colour"][3].as_float();
				} else {
					ASSERT_LOG(false, "PSYSTEM2, time_colour nodes must have a 'color' or 'colour' attribute");
				}
				tc_data_.push_back(std::make_pair(t, result));
			} else if(tc_node.is_list()) {
				for(int n = 0; n != tc_node.num_elements(); ++n) {
					float t = tc_node[n]["time"].as_float();
					glm::vec4 result;
					if(tc_node[n].has_key("color")) {
						ASSERT_LOG(tc_node[n]["color"].is_list() && tc_node[n]["color"].num_elements() == 4, "Expected vec4 variant but found " << tc_node[n]["color"].write_json());
						result.r = tc_node[n]["color"][0].as_float();
						result.g = tc_node[n]["color"][1].as_float();
						result.b = tc_node[n]["color"][2].as_float();
						result.a = tc_node[n]["color"][3].as_float();
					} else if(tc_node[n].has_key("colour")) {
						ASSERT_LOG(tc_node[n]["colour"].is_list() && tc_node[n]["colour"].num_elements() == 4, "Expected vec4 variant but found " << tc_node[n]["colour"].write_json());
						result.r = tc_node[n]["colour"][0].as_float();
						result.g = tc_node[n]["colour"][1].as_float();
						result.b = tc_node[n]["colour"][2].as_float();
						result.a = tc_node[n]["colour"][3].as_float();
					} else {
						ASSERT_LOG(false, "PSYSTEM2, time_colour nodes must have a 'color' or 'colour' attribute");
					}
					tc_data_.push_back(std::make_pair(t, result));
				}
			}
			sort_tc_data();
		}

		void TimeColorAffector::sort_tc_data()
		{
			std::sort(tc_data_.begin(), tc_data_.end(), [](const tc_pair& lhs, const tc_pair& rhs){
				return lhs.first < rhs.first;
			});
		}

		void TimeColorAffector::removeTimeColorEntry(const tc_pair& f)
		{
			auto it = std::find(tc_data_.begin(), tc_data_.end(), f);
			if(it != tc_data_.end()) {
				tc_data_.erase(it);
			}
		}

		void TimeColorAffector::internalApply(Particle& p, float t)
		{
			if(tc_data_.empty()) {
				return;
			}
			glm::vec4 c;
			float ttl_percentage = 1.0f - p.current.time_to_live / p.initial.time_to_live;
			auto it1 = find_nearest_color(ttl_percentage);
			auto it2 = it1 + 1;
			if(it2 != tc_data_.end()) {
				if(interpolate_) {
					c = it1->second + ((it2->second - it1->second) * ((ttl_percentage - it1->first)/(it2->first - it1->first)));
				} else {
					c = it1->second;
				}
			} else {
				c = it1->second;
			}
			if(operation_ == ColourOperation::COLOR_OP_SET) {
				p.current.color = color_vector(color_vector::value_type(c.r*255.0f), 
					color_vector::value_type(c.g*255.0f), 
					color_vector::value_type(c.b*255.0f), 
					color_vector::value_type(c.a*255.0f));
			} else {
				p.current.color = color_vector(color_vector::value_type(c.r*p.initial.color.r), 
					color_vector::value_type(c.g*p.initial.color.g), 
					color_vector::value_type(c.b*p.initial.color.b), 
					color_vector::value_type(c.a*p.initial.color.a));
			}
		}


		void TimeColorAffector::handleWrite(variant_builder* build) const 
		{
			build->add("color_operation", operation_ == ColourOperation::COLOR_OP_SET ? "set" : "multiply");
			build->add("interpolate", interpolate_);
			for(const auto& tc : tc_data_) {
				variant_builder res;
				res.add("time", tc.first);
				res.add("color", vec4_to_variant(tc.second));
				build->add("time_color", res.build());
			}
		}

		// Find nearest iterator to the time fraction "dt"
		std::vector<TimeColorAffector::tc_pair>::iterator TimeColorAffector::find_nearest_color(float dt)
		{
			auto it = tc_data_.begin();
			for(; it != tc_data_.end(); ++it) {
				if(dt < it->first) {
					if(it == tc_data_.begin()) {
						return it;
					} else {
						return --it;
					}
				} 
			}
			return --it;
		}

		JetAffector::JetAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::JET),
			  acceleration_(new Parameter(1.0f))
		{
		}

		JetAffector::JetAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node, AffectorType::JET),
			  acceleration_(nullptr)
		{
			init(node);
		}

		void JetAffector::init(const variant& node)
		{
			if(node.has_key("acceleration")) {
				acceleration_ = Parameter::factory(node["acceleration"]);
			} else {
				acceleration_.reset(new Parameter(1.0f));
			}
		}

		void JetAffector::internalApply(Particle& p, float t)
		{
			float scale = t * acceleration_->getValue(1.0f - p.current.time_to_live/p.initial.time_to_live);
			if(p.current.direction.x == 0 && p.current.direction.y == 0 && p.current.direction.z == 0) {
				p.current.direction += p.initial.direction * scale;
			} else {
				p.current.direction += p.current.direction * scale;
			}
		}

		void JetAffector::handleWrite(variant_builder* build) const 
		{
			if(acceleration_) {
				build->add("acceleration", acceleration_->write());
			}
		}

		VortexAffector::VortexAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::VORTEX), 
			  rotation_axis_(0.0f, 1.0f, 0.0f),
			  rotation_speed_(new Parameter(1.0f))
		{
		}

		VortexAffector::VortexAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node, AffectorType::VORTEX), 
			  rotation_axis_(0.0f, 1.0f, 0.0f),
			  rotation_speed_(new Parameter(1.0f))
		{
			init(node);
		}

		void VortexAffector::init(const variant& node)
		{
			if(node.has_key("rotation_speed")) {
				rotation_speed_ = Parameter::factory(node["rotation_speed"]);
			} else {
				rotation_speed_.reset(new Parameter(1.0f));
			}
			if(node.has_key("rotation_axis")) {
				rotation_axis_ = variant_to_vec3(node["rotation_axis"]);
			}
		}

		void VortexAffector::internalApply(Particle& p, float t)
		{
			glm::vec3 local = p.current.position - getPosition();
			auto& psystem = getParentContainer()->getParticleSystem();
			float spd = rotation_speed_->getValue(psystem->getElapsedTime());
			glm::quat rotation = glm::angleAxis(glm::radians(spd), rotation_axis_);
			p.current.position = getPosition() + rotation * local;
			p.current.direction = rotation * p.current.direction;
		}

		void VortexAffector::handleWrite(variant_builder* build) const 
		{
			if(rotation_speed_ && rotation_speed_->getType() != ParameterType::FIXED && rotation_speed_->getValue() != 1.0f) {
				build->add("rotation_speed", rotation_speed_->write());
			}
			if(rotation_axis_ != glm::vec3(0.0f, 1.0f, 0.0f)) {
				build->add("rotation_axis", vec3_to_variant(rotation_axis_));
			}
		}

		GravityAffector::GravityAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::GRAVITY), 
			  gravity_(new Parameter(1.0f))
		{
		}

		GravityAffector::GravityAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node, AffectorType::GRAVITY), 
			  gravity_()
		{
			init(node);
		}

		void GravityAffector::init(const variant& node)
		{
			if(node.has_key("gravity")) {
				gravity_ = Parameter::factory(node["gravity"]);
			} else {
				gravity_ .reset(new Parameter(1.0f));
			}
		}

		void GravityAffector::internalApply(Particle& p, float t)
		{
			glm::vec3 d = getPosition() - p.current.position;
			float len_sqr = sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
			if(len_sqr > 0) {
				float force = (gravity_->getValue(t) * p.current.mass * getMass()) / len_sqr;
				p.current.direction += (force * t) * d;
			}
		}

		void GravityAffector::handleWrite(variant_builder* build) const 
		{
			if(gravity_ && gravity_->getType() != ParameterType::FIXED && gravity_->getValue() != 1.0f) {
				build->add("gravity", gravity_->write());
			}
		}

		ScaleAffector::ScaleAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::SCALE), 
			  scale_x_(nullptr),
			  scale_y_(nullptr),
			  scale_z_(nullptr),
			  scale_xyz_(new Parameter(1.0f)),
			  since_system_start_(false)
		{
		}

		ScaleAffector::ScaleAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node, AffectorType::SCALE), 
			  scale_x_(nullptr),
			  scale_y_(nullptr),
			  scale_z_(nullptr),
			  scale_xyz_(nullptr),
			  since_system_start_(false)
		{
			init(node);
		}

		void ScaleAffector::init(const variant& node)
		{
			since_system_start_ = (node["since_system_start"].as_bool(false));
			if(node.has_key("scale_x")) {
				scale_x_ = Parameter::factory(node["scale_x"]);
			}
			if(node.has_key("scale_y")) {
				scale_y_ = Parameter::factory(node["scale_y"]);
			}
			if(node.has_key("scale_z")) {
				scale_z_ = Parameter::factory(node["scale_z"]);
			}
			if(node.has_key("scale_xyz")) {
				scale_xyz_ = Parameter::factory(node["scale_xyz"]);
			}
		}

		float ScaleAffector::calculateScale(ParameterPtr s, const Particle& p)
		{
			float scale;
			if(since_system_start_) {
				auto& psystem = getParentContainer()->getParticleSystem();
				scale = s->getValue(psystem->getElapsedTime());
			} else {
				scale = s->getValue(1.0f - p.current.time_to_live / p.initial.time_to_live);
			}
			return scale;
		}

		void ScaleAffector::internalApply(Particle& p, float t)
		{
			if(scale_xyz_) {
				float calc_scale = calculateScale(scale_xyz_, p);
				float value = p.initial.dimensions.x * calc_scale * getScale().x;
				if(value > 0) {
					p.current.dimensions.x = value;
				}
				value = p.initial.dimensions.y * calc_scale * getScale().y;
				if(value > 0) {
					p.current.dimensions.y = value;
				}
				if(g_particle_ui_2d == false) {
					value = p.initial.dimensions.z * calc_scale * getScale().z;
					if(value > 0) {
						p.current.dimensions.z = value;
					}
				}
			} else {
				if(scale_x_) {
					float calc_scale = calculateScale(scale_x_, p);
					float value = p.initial.dimensions.x * calc_scale * getScale().x;
					if(value > 0) {
						p.current.dimensions.x = value;
					}
				}
				if(scale_y_) {
					float calc_scale = calculateScale(scale_y_, p);
					float value = p.initial.dimensions.x * calc_scale * getScale().y;
					if(value > 0) {
						p.current.dimensions.y = value;
					}
				}
				if(scale_z_) {
					float calc_scale = calculateScale(scale_z_, p);
					float value = p.initial.dimensions.z * calc_scale * getScale().z;
					if(value > 0) {
						p.current.dimensions.z = value;
					}
				}
			}
		}

		void ScaleAffector::handleWrite(variant_builder* build) const 
		{
			if(since_system_start_) {
				build->add("since_system_start", since_system_start_);
			}
			if(scale_xyz_) {
				build->add("scale_xyz", scale_xyz_->write());
			} else {
				if(scale_x_) {
					build->add("scale_x", scale_x_->write());
				}
				if(scale_y_) {
					build->add("scale_y", scale_y_->write());
				}
				if(scale_z_) {
					build->add("scale_z", scale_z_->write());
				}
			}
		}

		LinearForceAffector::LinearForceAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::LINEAR_FORCE),
			  force_(new Parameter(1.0f)),
			  direction_(0.0f, 0.0f, 1.0f)
		{
		}

		LinearForceAffector::LinearForceAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node, AffectorType::LINEAR_FORCE),
			  force_(nullptr),
			  direction_(0.0f, 0.0f, 1.0f)
		{
			init(node);
		}

		void LinearForceAffector::init(const variant& node)
		{
			if(node.has_key("force")) {
				force_ = Parameter::factory(node["force"]);
			} else {
				force_.reset(new Parameter(1.0f));
			}

			direction_ = variant_to_vec3(node["direction"]);
		}

		void LinearForceAffector::internalApply(Particle& p, float t) 
		{
			float scale = t * force_->getValue(1.0f - p.current.time_to_live/p.initial.time_to_live);
			p.current.direction += direction_*scale;
		}

		void LinearForceAffector::handleWrite(variant_builder* build) const 
		{
			if(force_) {
				build->add("force", force_->write());
			}
			if(direction_ != glm::vec3(0.0f, 0.0f, 1.0f)) {
				build->add("direction", vec3_to_variant(direction_));
			}
		}

		ParticleFollowerAffector::ParticleFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::PARTICLE_FOLLOWER),
			  min_distance_(0.0f),
			  max_distance_(std::numeric_limits<float>::max()),
			  prev_particle_()
		{
		}

		ParticleFollowerAffector::ParticleFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, node, AffectorType::PARTICLE_FOLLOWER),
			  min_distance_(node["min_distance"].as_float(1.0f)),
			  max_distance_(node["max_distance"].as_float(std::numeric_limits<float>::max())),
			  prev_particle_()
		{
			init(node);
		}

		void ParticleFollowerAffector::init(const variant& node) 
		{
		}

		void ParticleFollowerAffector::handleEmitProcess(float t) 
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			std::vector<Particle>& particles = psystem->getActiveParticles();
			// keeps particles following wihin [min_distance, max_distance]
			if(particles.size() < 1) {
				return;
			}
			prev_particle_ = particles.begin();
			for(auto p = particles.begin(); p != particles.end(); ++p) {
				internalApply(*p, t);
				prev_particle_ = p;
			}
		}

		void ParticleFollowerAffector::internalApply(Particle& p, float t) 
		{
			auto distance = glm::length(p.current.position - prev_particle_->current.position);
			if(distance > min_distance_ && distance < max_distance_) {
				p.current.position = prev_particle_->current.position + (min_distance_/distance)*(p.current.position-prev_particle_->current.position);
			}
		}

		void ParticleFollowerAffector::handleWrite(variant_builder* build) const 
		{
			if(min_distance_ != 1.0f) {
				build->add("min_distance", min_distance_);
			}
			if(max_distance_ != std::numeric_limits<float>::max()) {
				build->add("max_distance", max_distance_);
			}
		}

		AlignAffector::AlignAffector(std::weak_ptr<ParticleSystemContainer> parent) 
			: Affector(parent, AffectorType::ALIGN), 
			  resize_(false),
			  prev_particle_()
		{
		}

		AlignAffector::AlignAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Affector(parent, node, AffectorType::ALIGN), 
			  resize_(false),
			  prev_particle_() 
		{
			init(node);
		}

		void AlignAffector::init(const variant& node) 
		{
			resize_ = (node["resize"].as_bool(false));
		}

		void AlignAffector::internalApply(Particle& p, float t) 
		{
			glm::vec3 distance = prev_particle_->current.position - p.current.position;
			if(resize_) {
				p.current.dimensions.y = glm::length(distance);
			}
			if(std::abs(glm::length(distance)) > 1e-12) {
				distance = glm::normalize(distance);
			}
			p.current.orientation.x = distance.x;
			p.current.orientation.y = distance.y;
			p.current.orientation.z = distance.z;
		}

		void AlignAffector::handleEmitProcess(float t) 
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			std::vector<Particle>& particles = psystem->getActiveParticles();
			if(particles.size() < 1) {
				return;
			}
			prev_particle_ = particles.begin();				
			for(auto p = particles.begin(); p != particles.end(); ++p) {
				internalApply(*p, t);
				prev_particle_ = p;
			}
		}

		void AlignAffector::handleWrite(variant_builder* build) const 
		{
			if(resize_) {
				build->add("resize", resize_);
			}
		}

		FlockCenteringAffector::FlockCenteringAffector(std::weak_ptr<ParticleSystemContainer> parent) 
			: Affector(parent, AffectorType::FLOCK_CENTERING), 
			  average_(0.0f),
			  prev_particle_()
		{
		}

		FlockCenteringAffector::FlockCenteringAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Affector(parent, node, AffectorType::FLOCK_CENTERING), 
		 	  average_(0.0f),
			  prev_particle_()
		{
			init(node);
		}

		void FlockCenteringAffector::init(const variant& node) 
		{
		}

		void FlockCenteringAffector::internalApply(Particle& p, float t) 
		{
			p.current.direction = (average_ - p.current.position) * t;
		}

		void FlockCenteringAffector::handleEmitProcess(float t) 
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			std::vector<Particle>& particles = psystem->getActiveParticles();
			if(particles.size() < 1) {
				return;
			}
			auto count = particles.size();
			glm::vec3 sum(0.0f);
			for(const auto& p : particles) {
				sum += p.current.position;
			}
			average_ /= static_cast<float>(count);

			prev_particle_ = particles.begin();				
			for(auto p = particles.begin(); p != particles.end(); ++p) {
				internalApply(*p, t);
				prev_particle_ = p;
			}
		}

		void FlockCenteringAffector::handleWrite(variant_builder* build) const 
		{
			// nothing needed
		}

		BlackHoleAffector::BlackHoleAffector(std::weak_ptr<ParticleSystemContainer> parent) 
			: Affector(parent, AffectorType::BLACK_HOLE), 
			  velocity_(new Parameter(1.0f)), 
			  acceleration_(new Parameter(1.0f)),
			  wvelocity_(0.0f)
		{
		}

		BlackHoleAffector::BlackHoleAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Affector(parent, node, AffectorType::BLACK_HOLE), 
			  velocity_(nullptr), 
			  acceleration_(nullptr),
			  wvelocity_(0.0f)
		{
			init(node);
		}

		void BlackHoleAffector::init(const variant& node) 
		{
			if(node.has_key("velocity")) {
				velocity_ = Parameter::factory(node["velocity"]);
			} else {
				velocity_.reset(new Parameter(1.0f));
			}
			wvelocity_ = velocity_->getValue(0.0f);
			if(node.has_key("acceleration")) {
				acceleration_ = Parameter::factory(node["acceleration"]);
			} else {
				acceleration_.reset(new Parameter(0.0f));
			}
		}

		void BlackHoleAffector::handleEmitProcess(float t) 
		{
			wvelocity_ += acceleration_->getValue(t);
			Affector::handleEmitProcess(t);
		}

		void BlackHoleAffector::internalApply(Particle& p, float t) 
		{
			glm::vec3 diff = getPosition() - p.current.position;
			float len = glm::length(diff);
			if(len > wvelocity_) {
				diff *= wvelocity_/len;
			} else {
				p.current.time_to_live = 0;
			}

			p.current.position += diff;
		}

		void BlackHoleAffector::handleWrite(variant_builder* build) const 
		{
			if(velocity_ && velocity_->getType() != ParameterType::FIXED && velocity_->getValue() != 1.0f) {
				build->add("velocity", velocity_->write());
			}
			if(acceleration_ && acceleration_->getType() != ParameterType::FIXED && acceleration_->getValue() != 0.0f) {
				build->add("acceleration", acceleration_->write());
			}
		}

		PathFollowerAffector::PathFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent) 
			: Affector(parent, AffectorType::PATH_FOLLOWER),
			  points_(),
			  spl_()
		{
		}

		PathFollowerAffector::PathFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Affector(parent, node, AffectorType::PATH_FOLLOWER),
			  points_(),
			  spl_()
		{
			init(node);
		}

		void PathFollowerAffector::init(const variant& node) 
		{
			ASSERT_LOG(node.has_key("path") && node["path"].is_list(),
				"path_follower must have a 'path' attribute.");
			
			setPoints(node["path"]);
		}

		void PathFollowerAffector::clearPoints()
		{
			points_.clear();
			spl_.reset();
		}

		void PathFollowerAffector::addPoint(const glm::vec3& p)
		{
			points_.emplace_back(p);
			spl_.reset(new geometry::spline3d<float>(points_));
		}

		void PathFollowerAffector::setPoints(const std::vector<glm::vec3>& points)
		{
			points_ = points;
			spl_.reset(new geometry::spline3d<float>(points_));
		}

		void PathFollowerAffector::setPoints(const variant& path_list)
		{
			points_.clear();

			for(unsigned n = 0; n != path_list.num_elements(); ++n) {
				const auto& pt = path_list[n];
				ASSERT_LOG(pt.is_list() && pt.num_elements() > 0, "points in path must be lists of more than one element.");
				const float x = pt[0].as_float();
				const float y = pt.num_elements() > 1 ? pt[1].as_float() : 0.0f;
				const float z = pt.num_elements() > 2 ? pt[2].as_float() : 0.0f;
				points_.emplace_back(x,y,z);
			}
			spl_ = std::make_shared<geometry::spline3d<float>>(points_);
		}

		void PathFollowerAffector::internalApply(Particle& p, float t) 
		{
			const float time_fraction = (p.initial.time_to_live - p.current.time_to_live) / p.initial.time_to_live;
			const float time_fraction_next = std::min<float>(1.0f, (p.initial.time_to_live - (p.current.time_to_live - t)) / p.initial.time_to_live);
			auto pt = spl_->interpolate(time_fraction);
			auto pn = spl_->interpolate(time_fraction_next);
			p.current.position += spl_->interpolate(time_fraction_next) - spl_->interpolate(time_fraction);
		}

		void PathFollowerAffector::handleEmitProcess(float t) 
		{
			if(spl_ == nullptr) {
				return;
			}
			auto& psystem = getParentContainer()->getParticleSystem();
			std::vector<Particle>& particles = psystem->getActiveParticles();
			if(particles.size() < 1) {
				return;
			}

			prev_particle_ = particles.begin();				
			for(auto p = particles.begin(); p != particles.end(); ++p) {
				internalApply(*p, t);
				prev_particle_ = p;
			}
		}

		void PathFollowerAffector::handleWrite(variant_builder* build) const 
		{
			for(const auto& pt : points_) {
				build->add("path", vec3_to_variant(pt));
			}
		}

		RandomiserAffector::RandomiserAffector(std::weak_ptr<ParticleSystemContainer> parent) 
			: Affector(parent, AffectorType::RANDOMISER), 
			  random_direction_(true),
			  time_step_(0),
			  max_deviation_(0.0f), 
			  last_update_time_()
		{
		}

		RandomiserAffector::RandomiserAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Affector(parent, node, AffectorType::RANDOMISER), 
			  random_direction_(true),
			  time_step_(0),
			  max_deviation_(0.0f), 
			  last_update_time_()
		{
			init(node);
		}

		void RandomiserAffector::init(const variant& node)
		{
			time_step_ = static_cast<float>(node["time_step"].as_float(0));
			random_direction_ = node["use_direction"].as_bool(true);

			if(node.has_key("max_deviation_x")) {
				max_deviation_.x = static_cast<float>(node["max_deviation_x"].as_float());
			}
			if(node.has_key("max_deviation_y")) {
				max_deviation_.y = static_cast<float>(node["max_deviation_y"].as_float());
			}
			if(node.has_key("max_deviation_z")) {
				max_deviation_.z = static_cast<float>(node["max_deviation_z"].as_float());
			}
			last_update_time_[0] = last_update_time_[1] = 0.0f;
		}

		void RandomiserAffector::internalApply(Particle& p, float t)
		{
			if(random_direction_) {
				// change direction per update
				p.current.direction += glm::vec3(get_random_float(-max_deviation_.x, max_deviation_.x),
					get_random_float(-max_deviation_.y, max_deviation_.y),
					get_random_float(-max_deviation_.z, max_deviation_.z));
			} else {
				// change position per update.
				p.current.position += getScale() * glm::vec3(get_random_float(-max_deviation_.x, max_deviation_.x),
					get_random_float(-max_deviation_.y, max_deviation_.y),
					get_random_float(-max_deviation_.z, max_deviation_.z));
			}
		}

		void RandomiserAffector::handle_apply(std::vector<Particle>& particles, float t)
		{
			last_update_time_[0] += t;
			if(last_update_time_[0] > time_step_) {
				last_update_time_[0] -= time_step_;
				for(auto& p : particles) {
					internalApply(p, t);
				}
			}
		}

		void RandomiserAffector::handle_apply(const EmitterPtr& objs, float t) 
		{
			last_update_time_[1] += t;
			if(last_update_time_[1] > time_step_) {
				last_update_time_[1] -= time_step_;
				internalApply(*objs, t);
			}
		}
		
		void RandomiserAffector::handleProcess(float t) 
		{
			auto& psystem = getParentContainer()->getParticleSystem();
			handle_apply(psystem->getActiveParticles(), t);
			handle_apply(psystem->getEmitter(), t);
		}

		void RandomiserAffector::handleWrite(variant_builder* build) const 
		{
			if(time_step_ != 0.0f) {
				build->add("time_step", time_step_);
			}
			if(!random_direction_) {
				build->add("use_direction", random_direction_);
			}
			if(max_deviation_.x != 0.0f) {
				build->add("max_deviation_x", max_deviation_.x);
			}
			if(max_deviation_.y != 0.0f) {
				build->add("max_deviation_y", max_deviation_.y);
			}
			if(max_deviation_.z != 0.0f) {
				build->add("max_deviation_z", max_deviation_.z);
			}
		}

		SineForceAffector::SineForceAffector(std::weak_ptr<ParticleSystemContainer> parent) 
			: Affector(parent, AffectorType::SINE_FORCE),
			  force_vector_(0.0f),
			  scale_vector_(0.0f),
			  min_frequency_(1.0f),
			  max_frequency_(1.0f),
			  fa_(ForceApplication::FA_ADD),
			  frequency_(1.0f),
			  angle_(0.0f)
		{
		}

		SineForceAffector::SineForceAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node) 
			: Affector(parent, node, AffectorType::SINE_FORCE),
			  force_vector_(0.0f),
			  scale_vector_(0.0f),
			  min_frequency_(1.0f),
			  max_frequency_(1.0f),
			  fa_(ForceApplication::FA_ADD),
			  frequency_(1.0f),
			  angle_(0.0f)
		{
			init(node);
		}

		void SineForceAffector::init(const variant& node)
		{
			if(node.has_key("max_frequency")) {
				max_frequency_ = static_cast<float>(node["max_frequency"].as_float());
				frequency_ = max_frequency_;
			}
			if(node.has_key("min_frequency")) {
				min_frequency_ = static_cast<float>(node["min_frequency"].as_float());					
				if(min_frequency_ > max_frequency_) {
					frequency_ = min_frequency_;
				}
			}
			if(node.has_key("force_vector")) {
				force_vector_ = variant_to_vec3(node["force_vector"]);
			}
			if(node.has_key("force_application")) {
				const std::string& fa = node["force_application"].as_string();
				if(fa == "average") {
					fa_ = ForceApplication::FA_AVERAGE;
				} else if(fa == "add") {
					fa_ = ForceApplication::FA_ADD;
				} else {
					ASSERT_LOG(false, "'force_application' attribute should have value average or add");
				}
			}
		}

		void SineForceAffector::handleEmitProcess(float t)
		{
			angle_ += /*2.0f * M_PI **/ frequency_ * t;
			float sine_value = sin(angle_);
			scale_vector_ = force_vector_ * t * sine_value;
			//std::cerr << "XXX: angle: " << angle_ << " scale_vec: " << scale_vector_ << std::endl;
			if(angle_ > static_cast<float>(M_PI * 2.0f)) {
				angle_ -= static_cast<float>(M_PI * 2.0f);
				if(min_frequency_ != max_frequency_) {
					frequency_ = get_random_float(min_frequency_, max_frequency_);
				}
			}
			Affector::handleEmitProcess(t);
		}

		void SineForceAffector::internalApply(Particle& p, float t)
		{
			if(fa_ == ForceApplication::FA_ADD) {
				p.current.direction += scale_vector_;
			} else {
				p.current.direction = (p.current.direction + force_vector_) / 2.0f;
			}
		}

		void SineForceAffector::handleWrite(variant_builder* build) const 
		{
			build->add("force_application", fa_ == ForceApplication::FA_AVERAGE ? "average" : "add");
			if(min_frequency_ != 0.0f) {
				build->add("min_frequency", min_frequency_);
			}
			if(max_frequency_ != 0.0f) {
				build->add("max_frequency", max_frequency_);
			}
			if(force_vector_ != glm::vec3(0.0f)) {
				build->add("force_vector", vec3_to_variant(force_vector_));
			}
		}

		TextureRotatorAffector::TextureRotatorAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::TEXTURE_ROTATOR),
			  angle_(new Parameter(1.0f)),
			  speed_(new Parameter(1.0f))
		{
		}

		TextureRotatorAffector::TextureRotatorAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, AffectorType::TEXTURE_ROTATOR),
			  angle_(nullptr),
			  speed_(nullptr)
		{
			init(node);
		}

		void TextureRotatorAffector::init(const variant& node) 
		{
			if(node.has_key("angle")) {
				angle_ = Parameter::factory(node["angle"]);
			} else {
				angle_.reset(new Parameter(0.0f));
			}
			if(node.has_key("speed")) {
				speed_ = Parameter::factory(node["speed"]);
			} else {
				speed_.reset(new Parameter(1.0f));
			}
		}

		void TextureRotatorAffector::internalApply(Particle& p, float t)
		{
			const float angle = angle_->getValue(t);
			const float speed = speed_->getValue(t);
			const auto qaxis = glm::angleAxis(angle / 180.0f * static_cast<float>(M_PI), glm::vec3(0.0f, 0.0f, 1.0f));
			/// XXX properly work out speed/angle handling and what we want it to mean
			p.current.orientation = qaxis * p.current.orientation;
		}

		void TextureRotatorAffector::handleWrite(variant_builder* build) const 
		{
			if(angle_) {
				build->add("angle", angle_->write());
			}
			if(speed_) {
				build->add("speed", speed_->write());
			}
		}

		AnimationAffector::AnimationAffector(std::weak_ptr<ParticleSystemContainer> parent)
			: Affector(parent, AffectorType::ANIMATION), 
			  pixel_coords_(false),
			  use_mass_instead_of_time_(false),
			  uv_data_(),
			  trf_uv_data_()
		{
		}

		AnimationAffector::AnimationAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node)
			: Affector(parent, AffectorType::ANIMATION), 
			  pixel_coords_(false),
			  uv_data_(),
			  trf_uv_data_()
		{
			init(node);
		}

		void AnimationAffector::init(const variant& node) 
		{
			uv_data_.clear();
			if(node.has_key("pixel_coords")) {
				pixel_coords_ = node["pixel_coords"].as_bool();
			}

			if(node.has_key("use_mass_instead_of_time")) {
				use_mass_instead_of_time_ = node["use_mass_instead_of_time"].as_bool();
			}
			ASSERT_LOG(node.has_key("time_uv") || node.has_key("time_uv"), "Must be a 'time_uv' attribute");
			const variant& uv_node = node.has_key("time_uv") ? node["time_uv"] : node["time_uv"];
			auto psystem = getParentContainer()->getParticleSystem();
			if(uv_node.is_map()) {
				float t = uv_node["time"].as_float();
				uv_data_.emplace_back(std::make_pair(t, rectf(uv_node["area"])));
			} else if(uv_node.is_list()) {
				for(int n = 0; n != uv_node.num_elements(); ++n) {
					float t = uv_node[n]["time"].as_float();
					uv_data_.emplace_back(std::make_pair(t, rectf(uv_node[n]["area"])));
				}
			}
		}

		void AnimationAffector::removeTimeCoordEntry(const uv_pair& f)
		{
			auto it = std::find(uv_data_.begin(), uv_data_.end(), f);
			if(it != uv_data_.end()) {
				uv_data_.erase(it);
			}
		}

		void AnimationAffector::transformCoords()
		{
			sort_uv_data();
			trf_uv_data_.clear();
			if(!pixel_coords_) {
				std::copy(uv_data_.begin(), uv_data_.end(), std::back_inserter(trf_uv_data_));
				return;
			}

			auto psystem = getParentContainer()->getParticleSystem();
			auto tex = psystem->getTexture();
			ASSERT_LOG(tex != nullptr, "No texture is defined.");

			for(const auto& uvp : uv_data_) {
				trf_uv_data_.emplace_back(uvp.first, tex->getTextureCoords(0, uvp.second));
			}
		}

		void AnimationAffector::internalApply(Particle& p, float t) 
		{
			if(uv_data_.empty()) {
				return;
			}
			if(trf_uv_data_.empty()) {
				transformCoords();
			}
			float ttl_percentage = use_mass_instead_of_time_ ? p.current.mass : 1.0f - p.current.time_to_live / p.initial.time_to_live;
			auto it1 = find_nearest_coords(ttl_percentage);
			p.current.area = it1->second;
		}

		void AnimationAffector::handleWrite(variant_builder* build) const 
		{
			build->add("pixel_coords", pixel_coords_);
			build->add("use_mass_instead_of_time", use_mass_instead_of_time_);
			if(uv_data_.empty()) {
				std::vector<variant> time_uv;
				build->add("time_uv", variant(&time_uv));
			}
			for(const auto& uv : uv_data_) {
				variant_builder res;
				res.add("time", uv.first);
				res.add("area", uv.second.write());
				build->add("time_uv", res.build());
			}
		}

		void AnimationAffector::sort_uv_data()
		{
			std::sort(uv_data_.begin(), uv_data_.end(), [](const uv_pair& lhs, const uv_pair& rhs){
				return lhs.first < rhs.first;
			});
		}

		std::vector<AnimationAffector::uv_pair>::iterator AnimationAffector::find_nearest_coords(float dt)
		{
			auto it = trf_uv_data_.begin();
			for(; it != trf_uv_data_.end(); ++it) {
				if(dt < it->first) {
					if(it == trf_uv_data_.begin()) {
						return it;
					} else {
						return --it;
					}
				} 
			}
			return --it;
		}

	}
}
