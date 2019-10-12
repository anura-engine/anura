/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <iostream>
#include <deque>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include "DisplayDevice.hpp"
#include "WindowManager.hpp"
#include "Texture.hpp"

#include "asserts.hpp"
#include "entity.hpp"
#include "formula.hpp"
#include "frame.hpp"
#include "particle_system.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "variant_utils.hpp"
#include "weather_particle_system.hpp"
#include "water_particle_system.hpp"

namespace 
{
	class ParticleAnimation 
	{
	public:
		explicit ParticleAnimation(variant node) :
		  id_(node["id"].as_string()),
		  texture_(KRE::Texture::createTexture(node["image"])),
		  duration_(node["duration"].as_int()),
		  reverse_frame_(node["reverse"].as_bool()),
		  loops_(node["loops"].as_bool(false))
		{
			rect base_area(node.has_key("rect") ? rect(node["rect"]) :
				   rect(node["x"].as_int(),
						node["y"].as_int(),
						node["w"].as_int(),
						node["h"].as_int()));
			width_  = base_area.w()*node["scale"].as_int(2);
			height_ = base_area.h()*node["scale"].as_int(2);
			int nframes = node["frames"].as_int(1);
			if(nframes < 1) {
				nframes = 1;
			}

			const int nframes_per_row = node["frames_per_row"].as_int(-1);
			const int pad = node["pad"].as_int();

			ffl::IntrusivePtr<Frame> frame_obj(new Frame(node));

			int row = 0, col = 0;
			for(int n = 0; n != nframes; ++n) {
				const Frame::FrameInfo& info = frame_obj->frameLayout()[n];
				const rect& area = info.area;

				frame_area a;
				rectf ra = texture_->getTextureCoords(0, area);
				a.u1 = ra.x();
				a.u2 = ra.x2();
				a.v1 = ra.y();
				a.v2 = ra.y2();

				a.x_adjust = info.x_adjust*2;
				a.y_adjust = info.y_adjust*2;
				a.x2_adjust = info.x2_adjust*2;
				a.y2_adjust = info.y2_adjust*2;

				frames_.push_back(a);

				++col;
				if(col == nframes_per_row) {
					col = 0;
					++row;
				}
			}
		}

		struct frame_area {
			float u1, v1, u2, v2;
			int x_adjust, y_adjust, x2_adjust, y2_adjust;
		};

		const frame_area& getFrame(int t) const {
			int index = t/duration_;
			if(index < 0) {
				index = 0;
			} else if(static_cast<unsigned>(index) >= frames_.size()) {
				if(loops_ && !reverse_frame_) {
					index = index % frames_.size();
				} else if (loops_ && reverse_frame_){
					index = static_cast<int>((runningInReverse(index) ? frames_.size()- 1 - index % frames_.size(): index % frames_.size()));
				} else {
					index = static_cast<int>(frames_.size() - 1);
				}
			}

			return frames_[index];
		}
		bool runningInReverse(int current_frame) const
		{
			return current_frame % (2* frames_.size()) >= frames_.size();
		}

		KRE::TexturePtr getTexture() const { return texture_; }
	
		int width() const { return width_; }
		int height() const { return height_; }
	private:
		std::string id_;
		KRE::TexturePtr texture_;

		std::vector<frame_area> frames_;
		int duration_;
		int reverse_frame_;
		int width_, height_;
		bool loops_;
	};

	struct SimpleParticleSystemInfo {
		SimpleParticleSystemInfo(variant node)
		  : spawn_rate_(node["spawn_rate"].as_int(1)),
			spawn_rate_random_(node["spawn_rate_random"].as_int()),
			system_time_to_live_(node["system_time_to_live"].as_int(-1)),
			time_to_live_(node["time_to_live"].as_int(50)),
			min_x_(node["min_x"].as_int(0)),
			max_x_(node["max_x"].as_int(0)),
			min_y_(node["min_y"].as_int(0)),
			max_y_(node["max_y"].as_int(0)),
			velocity_x_(node["velocity_x"].as_int(0)),
			velocity_y_(node["velocity_y"].as_int(0)),
			velocity_x_rand_(node["velocity_x_random"].as_int(0)),
			velocity_y_rand_(node["velocity_y_random"].as_int(0)),
			velocity_magnitude_(node["velocity_magnitude"].as_int(0)),
			velocity_magnitude_rand_(node["velocity_magnitude_random"].as_int(0)),
			velocity_rotate_(node["velocity_rotate"].as_int(0)),
			velocity_rotate_rand_(node["velocity_rotate_random"].as_int(0)),
			accel_x_(node["accel_x"].as_int(0)),
			accel_y_(node["accel_y"].as_int(0)),
			pre_pump_cycles_(node["pre_pump_cycles"].as_int(0)),
			delta_r_(node["delta_r"].as_int(0)),
			delta_g_(node["delta_g"].as_int(0)),
			delta_b_(node["delta_b"].as_int(0)),
			delta_a_(node["delta_a"].as_int(0)),
			random_schedule_(false)

		{
			if(node.has_key("velocity_x_schedule")) {
				velocity_x_schedule_ = node["velocity_x_schedule"].as_list_int();
			}

			if(node.has_key("velocity_y_schedule")) {
				velocity_y_schedule_ = node["velocity_y_schedule"].as_list_int();
			}

			random_schedule_ = node["random_schedule"].as_bool(velocity_x_schedule_.empty() == false || velocity_y_schedule_.empty() == false);
		}
		int spawn_rate_, spawn_rate_random_;
		int system_time_to_live_;
		int time_to_live_;
		int min_x_, max_x_, min_y_, max_y_;
		int velocity_x_, velocity_y_;
		int velocity_x_rand_, velocity_y_rand_;
		int velocity_magnitude_, velocity_magnitude_rand_;
		int velocity_rotate_, velocity_rotate_rand_;
		int accel_x_, accel_y_;
	
		int pre_pump_cycles_;  //# of cycles to pre-emptively simulate so the particle system appears to have been running for a while, rather than visibly starting to emit particles just when the player walks onscreen

		int delta_r_, delta_g_, delta_b_, delta_a_;

		std::vector<int> velocity_x_schedule_, velocity_y_schedule_;

		bool random_schedule_;
	};

	class SimpleParticleSystemFactory : public ParticleSystemFactory {
	public:
		explicit SimpleParticleSystemFactory(variant node);
		~SimpleParticleSystemFactory() {}

		ParticleSystemPtr create(const Entity& e) const override;

		std::vector<ParticleAnimation> frames_;

		SimpleParticleSystemInfo info_;
	};

	SimpleParticleSystemFactory::SimpleParticleSystemFactory(variant node)
	  : info_(node)
	{
		for(variant frame_node : node["animation"].as_list()) {
			frames_.push_back(ParticleAnimation(frame_node));
		}
	}

	class SimpleParticleSystem : public ParticleSystem
	{
	public:
		SimpleParticleSystem(const Entity& e, const SimpleParticleSystemFactory& factory);
		~SimpleParticleSystem() {}

		bool isDestroyed() const override { return info_.system_time_to_live_ == 0 || (info_.spawn_rate_ < 0 && particles_.empty()); }
		bool shouldSave() const override { return info_.spawn_rate_ >= 0; }
		void process(const Entity& e) override;
		void draw(const KRE::WindowPtr& wm, const rect& area, const Entity& e) const override;
	private:
		DECLARE_CALLABLE(SimpleParticleSystem);

		void prepump(const Entity& e);

		const SimpleParticleSystemFactory& factory_;
		SimpleParticleSystemInfo info_;

		int cycle_;

		struct particle {
			float pos[2];
			float velocity[2];
			const ParticleAnimation* anim;
			int random;
		};

		struct generation {
			int members;
			int created_at;
		};

		std::deque<particle> particles_;
		std::deque<generation> generations_;

		int spawn_buildup_;

		std::shared_ptr<KRE::Attribute<KRE::vertex_texture_color>> attrib_;
	};

	SimpleParticleSystem::SimpleParticleSystem(const Entity& e, const SimpleParticleSystemFactory& factory)
	  : factory_(factory), 
	    info_(factory.info_), 
	    cycle_(0), 
	    spawn_buildup_(0)
	{
		setShader(KRE::ShaderProgram::getProgram("vtc_shader"));

		auto as = KRE::DisplayDevice::createAttributeSet();
		attrib_.reset(new KRE::Attribute<KRE::vertex_texture_color>(KRE::AccessFreqHint::DYNAMIC, KRE::AccessTypeHint::DRAW));
		attrib_->addAttributeDesc(KRE::AttributeDesc(KRE::AttrType::POSITION, 2, KRE::AttrFormat::FLOAT, false, sizeof(KRE::vertex_texture_color), offsetof(KRE::vertex_texture_color, vertex)));
		attrib_->addAttributeDesc(KRE::AttributeDesc(KRE::AttrType::TEXTURE,  2, KRE::AttrFormat::FLOAT, false, sizeof(KRE::vertex_texture_color), offsetof(KRE::vertex_texture_color, texcoord)));
		attrib_->addAttributeDesc(KRE::AttributeDesc(KRE::AttrType::COLOR,    4, KRE::AttrFormat::UNSIGNED_BYTE, true, sizeof(KRE::vertex_texture_color), offsetof(KRE::vertex_texture_color, color)));
		as->addAttribute(KRE::AttributeBasePtr(attrib_));
		as->setDrawMode(KRE::DrawMode::TRIANGLES);
		
		addAttributeSet(as);
	}

	void SimpleParticleSystem::prepump(const Entity& e)
	{
		//cosmetic thing for very slow-moving particles:
		//it looks weird when you walk into a scene, with, say, a column of smoke that's presumably been rising for quite some time,
		//but it only begins rising the moment you arrive.  To overcome this, we can optionally have particle systems pre-simulate their particles
		//for the short period of time (often as low as 4 seconds) needed to eliminate that implementation artifact
		for( int i = 0; i < info_.pre_pump_cycles_; ++i)
		{
			process(e);
		}
	
	}

	void SimpleParticleSystem::process(const Entity& e)
	{
		--info_.system_time_to_live_;
		++cycle_;

		if(cycle_ == 1) {
			prepump(e);
		}

		while(!generations_.empty() && cycle_ - generations_.front().created_at == info_.time_to_live_) {
			particles_.erase(particles_.begin(), particles_.begin() + generations_.front().members);
			generations_.pop_front();
		}

		std::deque<particle>::iterator p = particles_.begin();
		for(generation& gen : generations_) {
			for(int n = 0; n != gen.members; ++n) {
				p->pos[0] += p->velocity[0];
				p->pos[1] += p->velocity[1];
				if(e.isFacingRight()) {
					p->velocity[0] += info_.accel_x_/1000.0f;
				} else {
					p->velocity[0] -= info_.accel_x_/1000.0f;
				}
				p->velocity[1] += info_.accel_y_/1000.0f;
				++p;
			}
		}

		if(info_.velocity_x_schedule_.empty() == false) {
			std::deque<particle>::iterator p = particles_.begin();
			for(generation& gen : generations_) {
				for(int n = 0; n != gen.members; ++n) {
					const int ncycle = p->random + cycle_ - gen.created_at - 1;
					p->velocity[0] += info_.velocity_x_schedule_[ncycle%info_.velocity_x_schedule_.size()];
					if(cycle_ - gen.created_at > 1) {
						p->velocity[0] -= info_.velocity_x_schedule_[(ncycle-1)%info_.velocity_x_schedule_.size()];
					}

					++p;
				}
			}
		}

		if(info_.velocity_y_schedule_.empty() == false) {
			std::deque<particle>::iterator p = particles_.begin();
			for(generation& gen : generations_) {
				for(int n = 0; n != gen.members; ++n) {
					const int ncycle = p->random + cycle_ - gen.created_at - 1;
					p->velocity[1] += info_.velocity_y_schedule_[ncycle%info_.velocity_y_schedule_.size()];
					if(cycle_ - gen.created_at > 1) {
						p->velocity[1] -= info_.velocity_y_schedule_[(ncycle-1)%info_.velocity_y_schedule_.size()];
					}

					++p;
				}
			}
		}

		int nspawn = info_.spawn_rate_;
		if(info_.spawn_rate_random_ > 0) {
			nspawn += rand()%info_.spawn_rate_random_;
		}

		if(nspawn > 0) {
			nspawn += spawn_buildup_;
		}

		spawn_buildup_ = nspawn%1000;
		nspawn /= 1000;

		generation new_gen;
		new_gen.members = nspawn;
		new_gen.created_at = cycle_;

		generations_.push_back(new_gen);

		while(nspawn-- > 0) {
			particle p;
			p.pos[0] = static_cast<float>(e.isFacingRight() ? (e.x() + info_.min_x_) : (e.x() + e.getCurrentFrame().width() - info_.max_x_));
			p.pos[1] = static_cast<float>(e.y() + info_.min_y_);
			p.velocity[0] = info_.velocity_x_/1000.0f;
			p.velocity[1] = info_.velocity_y_/1000.0f;

			if(info_.velocity_x_rand_ > 0) {
				p.velocity[0] += (rand()%info_.velocity_x_rand_)/1000.0f;
			}

			if(info_.velocity_y_rand_ > 0) {
				p.velocity[1] += (rand()%info_.velocity_y_rand_)/1000.0f;
			}

			int velocity_magnitude = info_.velocity_magnitude_;
			if(info_.velocity_magnitude_rand_ > 0) {
				velocity_magnitude += rand()%info_.velocity_magnitude_rand_;
			}

			if(velocity_magnitude) {
				int rotate_velocity = info_.velocity_rotate_;
				if(info_.velocity_rotate_rand_) {
					rotate_velocity += rand()%info_.velocity_rotate_rand_;
				}

				const float rotate_radians = (rotate_velocity/360.0f) * static_cast<float>(M_PI * 2.0);
				const float magnitude = velocity_magnitude/1000.0f;
				p.velocity[0] += sin(rotate_radians)*magnitude;
				p.velocity[1] += cos(rotate_radians)*magnitude;
			}

			ASSERT_GT(factory_.frames_.size(), 0);
			p.anim = &factory_.frames_[rand()%factory_.frames_.size()];

			const int diff_x = info_.max_x_ - info_.min_x_;
			float num_before = p.pos[0];
			if(diff_x > 0) {
				p.pos[0] += rand() % diff_x + (rand() % 1000)*0.001f;
			}

			const int diff_y = info_.max_y_ - info_.min_y_;
			if(diff_y > 0) {
				p.pos[1] += rand() % diff_y + (rand() % 1000)*0.001f;
			}

			if(!e.isFacingRight()) {
				p.velocity[0] = -p.velocity[0];
			}

			if(info_.random_schedule_) {
				p.random = rand();
			} else {
				p.random = 0;
			}

			particles_.push_back(p);
		}


		// XXXX This needs to be moved somewhere else. Maybe we should store Entity then do in preRender(...) ?
		if(particles_.empty()) {
			attrib_->clear();
			return;
		}
		auto pp = particles_.begin();
		setTexture(pp->anim->getTexture());
		std::vector<KRE::vertex_texture_color> vtc;
		const int facing = e.isFacingRight() ? 1 : -1;

		for(const generation& gen : generations_) {
			for(int n = 0; n != gen.members; ++n) {
				const ParticleAnimation* anim = pp->anim;
				const ParticleAnimation::frame_area& f = anim->getFrame(cycle_ - gen.created_at);
				glm::u8vec4 color(255,255,255,255);

				if(info_.delta_a_){
					color.a = std::max(256 - info_.delta_a_*(cycle_ - gen.created_at), 0);
				}

                const float x1 = pp->pos[0] + (f.x_adjust - anim->width()/2.0) * facing;
                const float x2 = pp->pos[0] + (anim->width()/2.0 - f.x2_adjust) * facing;
                const float y1 = pp->pos[1] + f.y_adjust - anim->height()/2.0;
                const float y2 = pp->pos[1] + anim->height()/2.0 - f.y2_adjust;

				vtc.emplace_back(glm::vec2(x1, y1), glm::vec2(f.u1, f.v1), color);
				vtc.emplace_back(glm::vec2(x2, y1), glm::vec2(f.u2, f.v1), color);
				vtc.emplace_back(glm::vec2(x1, y2), glm::vec2(f.u1, f.v2), color);

				vtc.emplace_back(glm::vec2(x1, y2), glm::vec2(f.u1, f.v2), color);
				vtc.emplace_back(glm::vec2(x2, y1), glm::vec2(f.u2, f.v1), color);
				vtc.emplace_back(glm::vec2(x2, y2), glm::vec2(f.u2, f.v2), color);
				++pp;
			}
		}
		attrib_->update(&vtc);
	}

	void SimpleParticleSystem::draw(const KRE::WindowPtr& wm, const rect& area, const Entity& e) const
	{
		if(getAttributeSet().back()->getCount() > 0) {
			wm->render(this);	
		}
	}

	BEGIN_DEFINE_CALLABLE(SimpleParticleSystem, ParticleSystem)
		DEFINE_FIELD(spawn_rate, "int")
			return variant(obj.info_.spawn_rate_);
		DEFINE_SET_FIELD
			obj.info_.spawn_rate_ = value.as_int();

		DEFINE_FIELD(spawn_rate_random, "int")
			return variant(obj.info_.spawn_rate_random_);
		DEFINE_SET_FIELD
			obj.info_.spawn_rate_random_ = value.as_int();

		DEFINE_FIELD(system_time_to_live, "int")
			return variant(obj.info_.system_time_to_live_);
		DEFINE_SET_FIELD
			obj.info_.system_time_to_live_ = value.as_int();

		DEFINE_FIELD(time_to_live, "int")
			return variant(obj.info_.time_to_live_);
		DEFINE_SET_FIELD
			obj.info_.time_to_live_ = value.as_int();

		DEFINE_FIELD(min_x, "int")
			return variant(obj.info_.min_x_);
		DEFINE_SET_FIELD
			obj.info_.min_x_ = value.as_int();

		DEFINE_FIELD(max_x, "int")
			return variant(obj.info_.max_x_);
		DEFINE_SET_FIELD
			obj.info_.max_x_ = value.as_int();

		DEFINE_FIELD(min_y, "int")
			return variant(obj.info_.min_y_);
		DEFINE_SET_FIELD
			obj.info_.min_y_ = value.as_int();

		DEFINE_FIELD(max_y, "int")
			return variant(obj.info_.max_y_);
		DEFINE_SET_FIELD
			obj.info_.max_y_ = value.as_int();

		DEFINE_FIELD(velocity_x, "int")
			return variant(obj.info_.velocity_x_);
		DEFINE_SET_FIELD
			obj.info_.velocity_x_ = value.as_int();

		DEFINE_FIELD(velocity_y, "int")
			return variant(obj.info_.velocity_y_);
		DEFINE_SET_FIELD
			obj.info_.velocity_y_ = value.as_int();

		DEFINE_FIELD(velocity_x_random, "int")
			return variant(obj.info_.velocity_x_rand_);
		DEFINE_SET_FIELD
			obj.info_.velocity_x_rand_ = value.as_int();

		DEFINE_FIELD(velocity_y_random, "int")
			return variant(obj.info_.velocity_y_rand_);
		DEFINE_SET_FIELD
			obj.info_.velocity_y_rand_ = value.as_int();

		DEFINE_FIELD(velocity_magnitude, "int")
			return variant(obj.info_.velocity_magnitude_);
		DEFINE_SET_FIELD
			obj.info_.velocity_magnitude_ = value.as_int();

		DEFINE_FIELD(velocity_magnitude_random, "int")
			return variant(obj.info_.velocity_magnitude_rand_);
		DEFINE_SET_FIELD
			obj.info_.velocity_magnitude_rand_ = value.as_int();

		DEFINE_FIELD(velocity_rotate, "int")
			return variant(obj.info_.velocity_rotate_);
		DEFINE_SET_FIELD
			obj.info_.velocity_rotate_ = value.as_int();

		DEFINE_FIELD(velocity_rotate_random, "int")
			return variant(obj.info_.velocity_rotate_rand_);
		DEFINE_SET_FIELD
			obj.info_.velocity_rotate_rand_ = value.as_int();

		DEFINE_FIELD(accel_x, "int")
			return variant(obj.info_.accel_x_);
		DEFINE_SET_FIELD
			obj.info_.accel_x_ = value.as_int();

		DEFINE_FIELD(accel_y, "int")
			return variant(obj.info_.accel_y_);
		DEFINE_SET_FIELD
			obj.info_.accel_y_ = value.as_int();

		DEFINE_FIELD(pre_pump_cycles, "int")
			return variant(obj.info_.pre_pump_cycles_);
		DEFINE_SET_FIELD
			obj.info_.pre_pump_cycles_ = value.as_int();

		DEFINE_FIELD(delta_r, "int")
			return variant(obj.info_.delta_r_);
		DEFINE_SET_FIELD
			obj.info_.delta_r_ = value.as_int();

		DEFINE_FIELD(delta_g, "int")
			return variant(obj.info_.delta_g_);
		DEFINE_SET_FIELD
			obj.info_.delta_g_ = value.as_int();

		DEFINE_FIELD(delta_b, "int")
			return variant(obj.info_.delta_b_);
		DEFINE_SET_FIELD
			obj.info_.delta_b_ = value.as_int();

		DEFINE_FIELD(delta_a, "int")
			return variant(obj.info_.delta_a_);
		DEFINE_SET_FIELD
			obj.info_.delta_a_ = value.as_int();
	END_DEFINE_CALLABLE(SimpleParticleSystem)


	ParticleSystemPtr SimpleParticleSystemFactory::create(const Entity& e) const
	{
		return ParticleSystemPtr(new SimpleParticleSystem(e, *this));
	}

	struct PointParticleInfo
	{
		explicit PointParticleInfo(variant node)
		  : generation_rate_millis(node["generation_rate_millis"].as_int()),
			pos_x(node["pos_x"].as_int()*1024),
			pos_y(node["pos_y"].as_int()*1024),
			pos_x_rand(node["pos_x_rand"].as_int()*1024),
			pos_y_rand(node["pos_y_rand"].as_int()*1024),
			velocity_x(node["velocity_x"].as_int()),
			velocity_y(node["velocity_y"].as_int()),
			accel_x(node["accel_x"].as_int()),
			accel_y(node["accel_y"].as_int()),
			velocity_x_rand(node["velocity_x_rand"].as_int()),
			velocity_y_rand(node["velocity_y_rand"].as_int()),
			dot_size(node["dot_size"].as_int(1)),
			dot_rounded(node["dot_rounded"].as_bool(false)),
			time_to_live(node["time_to_live"].as_int()),
			time_to_live_max(node["time_to_live_rand"].as_int() + time_to_live) {

			if(node.has_key("colors")) {
				const std::vector<variant> colors_vec = node["colors"].as_list();
				for(const variant& col : colors_vec) {
					colors.push_back(KRE::Color(col));
				}
			}

			if(node.has_key("colors_expression")) {
				const variant v = game_logic::Formula(node["colors_expression"]).execute();
				for(int n = 0; n != v.num_elements(); ++n) {
					const variant u = v[n];
					colors.emplace_back(u);
				}
			}

			std::reverse(colors.begin(), colors.end());

			ttl_divisor = time_to_live_max/static_cast<int>(colors.size() - 1);

			rgba[0] = node["red"].as_int();
			rgba[1] = node["green"].as_int();
			rgba[2] = node["blue"].as_int();
			rgba[3] = node["alpha"].as_int(255);
			rgba_rand[0] = node["red_rand"].as_int();
			rgba_rand[1] = node["green_rand"].as_int();
			rgba_rand[2] = node["blue_rand"].as_int();
			rgba_rand[3] = node["alpha_rand"].as_int();
			rgba_delta[0] = node["red_delta"].as_int();
			rgba_delta[1] = node["green_delta"].as_int();
			rgba_delta[2] = node["blue_delta"].as_int();
			rgba_delta[3] = node["alpha_delta"].as_int();
		}

		int generation_rate_millis;
		int pos_x, pos_y, pos_x_rand, pos_y_rand;
		int velocity_x, velocity_y, velocity_x_rand, velocity_y_rand;
		int accel_x, accel_y;
		int time_to_live, time_to_live_max;
		unsigned char rgba[4];
		unsigned char rgba_rand[4];
		char rgba_delta[4];
		int dot_size;
		bool dot_rounded;

		std::vector<KRE::Color> colors;
		int ttl_divisor;
	};

	class PointParticleSystem : public ParticleSystem, public std::enable_shared_from_this<PointParticleSystem>
	{
		int u_point_size_;
		int u_is_circular_;
	public:
		PointParticleSystem(const Entity& obj, const PointParticleInfo& info) 
			: obj_(obj), 
			  info_(info), 
			  particle_generation_(0), 
			  generation_rate_millis_(info.generation_rate_millis), 
			  pos_x_(info.pos_x), 
			  pos_x_rand_(info.pos_x_rand), 
			  pos_y_(info.pos_y), 
			  pos_y_rand_(info.pos_y_rand) 
		{
			setShader(KRE::ShaderProgram::getProgram("point_shader")->clone());
			getShader()->setUniformDrawFunction(std::bind(&PointParticleSystem::executeOnDraw, this));
			u_point_size_  = getShader()->getUniform("u_point_size");
			u_is_circular_ = getShader()->getUniform("u_is_circular");

			// turn on hardware-backed, not indexed and instanced draw if available.
			auto as = KRE::DisplayDevice::createAttributeSet(true, false, false);
			as->setDrawMode(KRE::DrawMode::POINTS);
			attribs_ = std::make_shared<KRE::Attribute<Coord>>(KRE::AccessFreqHint::DYNAMIC);
			attribs_->addAttributeDesc(KRE::AttributeDesc(KRE::AttrType::POSITION, 2, KRE::AttrFormat::FLOAT, false, sizeof(Coord), offsetof(Coord, vertex)));
			attribs_->addAttributeDesc(KRE::AttributeDesc(KRE::AttrType::COLOR, 4, KRE::AttrFormat::UNSIGNED_BYTE, true, sizeof(Coord), offsetof(Coord, color)));
			as->addAttribute(attribs_);
			addAttributeSet(as);
		}

		void executeOnDraw() {
			getShader()->setUniformValue(u_point_size_, info_.dot_size);
			getShader()->setUniformValue(u_is_circular_, true);
		}
		
		void process(const Entity& e) override {
			particle_generation_ += generation_rate_millis_;
			
			particles_.erase(std::remove_if(particles_.begin(), particles_.end(), particle_destroyed), particles_.end());

			for(auto& p : particles_) {
				p.pos_x += p.velocity_x;
				p.pos_y += p.velocity_y;
				if(e.isFacingRight()) {
					p.velocity_x += static_cast<short>(info_.accel_x/1000.0);
				} else {
					p.velocity_x -= static_cast<short>(info_.accel_x/1000.0);
				}
				p.velocity_y += static_cast<short>(info_.accel_y/1000.0);
				p.color.setRed(p.color.r_int() + info_.rgba_delta[0]);
				p.color.setGreen(p.color.g_int() + info_.rgba_delta[1]);
				p.color.setBlue(p.color.b_int() + info_.rgba_delta[2]);
				p.color.setAlpha(p.color.a_int() + info_.rgba_delta[3]);
				p.ttl--;
			}

			while(particle_generation_ >= 1000) {
				particles_.push_back(particle());
				particle& p = particles_.back();
				p.ttl = info_.time_to_live;
				if(info_.time_to_live_max != info_.time_to_live) {
					p.ttl += rand()%(info_.time_to_live_max - info_.time_to_live);
				}

				p.velocity_x = info_.velocity_x;
				p.velocity_y = info_.velocity_y;

				if(info_.velocity_x_rand) {
					p.velocity_x += rand()%info_.velocity_x_rand;
				}

				if(info_.velocity_y_rand) {
					p.velocity_y += rand()%info_.velocity_y_rand;
				}

				p.pos_x = e.x()*1024 + pos_x_;
				p.pos_y = e.y()*1024 + pos_y_;

				if(pos_x_rand_) {
					p.pos_x += rand()%pos_x_rand_;
				}
			
				if(pos_y_rand_) {
					p.pos_y += rand()%pos_y_rand_;
				}

				p.color = KRE::Color(info_.rgba[0], info_.rgba[1], info_.rgba[2], info_.rgba[3]);

				if(info_.rgba_rand[0]) {
					p.color.setRed(p.color.r_int() + rand()%info_.rgba_rand[0]);
				}

				if(info_.rgba_rand[1]) {
					p.color.setGreen(p.color.g_int() + rand()%info_.rgba_rand[1]);
				}

				if(info_.rgba_rand[2]) {
					p.color.setBlue(p.color.b_int() + rand()%info_.rgba_rand[2]);
				}

				if(info_.rgba_rand[3]) {
					p.color.setAlpha(p.color.a_int() + rand()%info_.rgba_rand[3]);
				}

				particle_generation_ -= 1000;
			}

			std::vector<Coord> coords;
			coords.reserve(particles_.size());
			for(const auto& p : particles_) {
				glm::u8vec4 col(p.color.as_u8vec4());
				if(info_.colors.size() >= 2) {
					col = info_.colors[p.ttl/info_.ttl_divisor].as_u8vec4();
				}
				coords.emplace_back(glm::vec2(p.pos_x/1024, p.pos_y/1024), col);
			}
			getAttributeSet().back()->setCount(coords.size());
			attribs_->update(&coords);
			// XXX we need to set a uniform to indicate we draw a point as a sphere.
			// uniform bool is_circlular;
			// uniform float radius;
			// int main() {
			//   vec2 pos = abs(gl_FragCoord.xy, a_position);
			//   float dist = dot(pos, pos);
			//	 if(dist < radius) {
			//	    glFragColor = xx;
			//   }
			// XXX we need to set a uniform with the point size
		}

		void draw(const KRE::WindowPtr& wm, const rect& area, const Entity& e) const override {
			if(particles_.empty()) {
				return;
			}
			wm->render(this);
		}
	private:
		DECLARE_CALLABLE(PointParticleSystem);
		struct Coord {
			Coord(const glm::vec2& v, const glm::u8vec4& c) : vertex(v), color(c) {}
			glm::vec2 vertex;
			glm::u8vec4 color;
		};
		std::shared_ptr<KRE::Attribute<Coord>> attribs_;

		const Entity& obj_;
		const PointParticleInfo& info_;

		struct particle {
			short velocity_x, velocity_y;
			int pos_x, pos_y;
			KRE::Color color;
			int ttl;
		};

		static bool particle_destroyed(const particle& p) { return p.ttl <= 0; }

		int particle_generation_;
		int generation_rate_millis_;
		int pos_x_, pos_x_rand_, pos_y_, pos_y_rand_;
		std::vector<particle> particles_;
	};

	class PointParticleSystemFactory : public ParticleSystemFactory
	{
	public:
		explicit PointParticleSystemFactory(variant node)
		  : info_(node)
		{}

		ParticleSystemPtr create(const Entity& e) const override {
			return ParticleSystemPtr(new PointParticleSystem(e, info_));
		}

	private:
		PointParticleInfo info_;
	};

	BEGIN_DEFINE_CALLABLE(PointParticleSystem, ParticleSystem)
		DEFINE_FIELD(generation_rate, "int")
			return variant(obj.generation_rate_millis_);
		DEFINE_SET_FIELD
			obj.generation_rate_millis_ = value.as_int();

		DEFINE_FIELD(generation_rate_millis, "int")
			return variant(obj.generation_rate_millis_);
		DEFINE_SET_FIELD
			obj.generation_rate_millis_ = value.as_int();

		DEFINE_FIELD(pos_x, "int")
			return variant(obj.pos_x_ / 1024);
		DEFINE_SET_FIELD
			obj.pos_x_ = value.as_int()*1024;

		DEFINE_FIELD(pos_y, "int")
			return variant(obj.pos_y_ / 1024);
		DEFINE_SET_FIELD
			obj.pos_y_ = value.as_int()*1024;

		DEFINE_FIELD(pos_x_rand, "int")
			return variant(obj.pos_x_rand_ / 1024);
		DEFINE_SET_FIELD
			obj.pos_x_rand_ = value.as_int()*1024;

		DEFINE_FIELD(pos_y_rand, "int")
			return variant(obj.pos_y_rand_ / 1024);
		DEFINE_SET_FIELD
			obj.pos_y_rand_ = value.as_int()*1024;
	END_DEFINE_CALLABLE(PointParticleSystem)

}

ConstParticleSystemFactoryPtr ParticleSystemFactory::create_factory(variant node)
{
	const std::string& type = node["type"].as_string();
	if(type == "simple") {
		return ConstParticleSystemFactoryPtr(new SimpleParticleSystemFactory(node));
	} else if (type == "weather") {
		return ConstParticleSystemFactoryPtr(new WeatherParticleSystemFactory(node));
	} else if (type == "water") {
		return ConstParticleSystemFactoryPtr(new WaterParticleSystemFactory(node));
	} else if(type == "point") {
		return ConstParticleSystemFactoryPtr(new PointParticleSystemFactory(node));
	}

	ASSERT_LOG(false, "Unrecognized particle system type: " << node["type"].as_string());
}

ParticleSystemFactory::~ParticleSystemFactory()
{
}

ParticleSystem::~ParticleSystem()
{
}

BEGIN_DEFINE_CALLABLE_NOBASE(ParticleSystem)
	DEFINE_FIELD(type, "string")
		return variant(obj.type());
END_DEFINE_CALLABLE(ParticleSystem)
