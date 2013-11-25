/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
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
#include "psystem2.hpp"
#include "psystem2_emitters.hpp"
#include "psystem2_parameters.hpp"


namespace graphics
{
	namespace particles
	{
		namespace
		{
			// Compute any vector out of the infinite set perpendicular to v.
			glm::vec3 perpendicular(const glm::vec3& v) 
			{
				glm::vec3 perp = glm::cross(v, glm::vec3(1.0f,0.0f,0.0f));
				float len_sqr = perp.x*perp.x + perp.y*perp.y + perp.z*perp.z;
				if(len_sqr < 1e-12) {
					perp = glm::cross(v, glm::vec3(0.0f,1.0f,0.0f));
				}
				return glm::normalize(perp);
			}
		}

		class circle_emitter : public emitter
		{
		public:
			circle_emitter(const variant& node, technique* tech);
			virtual ~circle_emitter();
		protected:
			virtual void handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t);
		private:
			float circle_radius_;
			float circle_step_;
			float circle_angle_;
			bool circle_random_;

			circle_emitter();
			circle_emitter(const circle_emitter&);
		};

		class box_emitter : public emitter
		{
		public:
			box_emitter(const variant& node, technique* tech);
			virtual ~box_emitter();
		protected:
			virtual void handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t);
		private:
			glm::vec3 box_dimensions_;

			box_emitter();
			box_emitter(const box_emitter&);
		};

		class line_emitter : public emitter
		{
		public:
			line_emitter(const variant& node, technique* tech);
			virtual ~line_emitter();
		protected:
			virtual void handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t);
		private:
			glm::vec3 line_end_;
			float line_deviation_;
			float min_increment_;
			float max_increment_;

			line_emitter();
			line_emitter(const line_emitter&);
		};

		class point_emitter : public emitter
		{
		public:
			point_emitter(const variant& node, technique* tech);
			virtual ~point_emitter();
		protected:
			virtual void handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t);
		private:
			point_emitter();
			point_emitter(const point_emitter&);
		};

		class sphere_surface_emitter : public emitter
		{
		public:
			sphere_surface_emitter(const variant& node, technique* tech);
			virtual ~sphere_surface_emitter();
		protected:
			virtual void handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t);
		private:
			float radius_;
			sphere_surface_emitter();
			sphere_surface_emitter(const point_emitter&);
		};


		emitter::emitter(const variant& node, technique* tech)
			: emission_fraction_(0.0f), technique_(tech)
		{
			ASSERT_LOG(technique_ != NULL, "technique_ is null");
			init_physics_parameters(initial_);
			init_physics_parameters(current_);
			initial_.time_to_live = current_.time_to_live = 3;

			if(node.has_key("emission_rate")) {
				emission_rate_ = parameter::factory(node["emission_rate"]);
			} else {
				emission_rate_.reset(new fixed_parameter(10));
			}
			if(node.has_key("time_to_live")) {
				time_to_live_ = parameter::factory(node["time_to_live"]);
			} else {
				time_to_live_.reset(new fixed_parameter(10.0f));
			}
			if(node.has_key("velocity")) {
				velocity_ = parameter::factory(node["velocity"]);
			} else {
				velocity_.reset(new fixed_parameter(100.0f));
			}
			if(node.has_key("angle")) {
				angle_ = parameter::factory(node["angle"]);
			} else {
				angle_.reset(new fixed_parameter(20.0f));
			}
			if(node.has_key("mass")) {
				mass_ = parameter::factory(node["mass"]);
			} else {
				mass_.reset(new fixed_parameter(1.0f));
			}
			if(node.has_key("duration")) {
				duration_ = parameter::factory(node["duration"]);
			} else {
				duration_.reset(new fixed_parameter(0.0f));
			}
			if(node.has_key("repeat_delay")) {
				repeat_delay_ = parameter::factory(node["repeat_delay"]);
			} else {
				repeat_delay_.reset(new fixed_parameter(0.0f));
			}
			if(node.has_key("direction")) {
				initial_.direction = current_.direction = variant_to_vec3(node["direction"]);
			}
			if(node.has_key("position")) {
				initial_.position = current_.position = variant_to_vec3(node["position"]);
			}
			if(node.has_key("orientation")) {
				initial_.orientation = current_.orientation = variant_to_quat(node["orientation"]);
			}
			if(node.has_key("orientation_start") && node.has_key("orientation_end")) {
				orientation_range_.reset(new std::pair<glm::quat, glm::quat>(variant_to_quat(node["orientation_start"]), variant_to_quat(node["orientation_end"])));
			}
			if(node.has_key("color")) {
				ASSERT_LOG(node["color"].is_list() && node["color"].num_elements() == 4,
					"FATAL: PSYSTEM2: 'color' should be a list of 4 elements.");
				initial_.color.r = current_.color.r = node["color"][0].as_int();
				initial_.color.g = current_.color.g = node["color"][1].as_int();
				initial_.color.b = current_.color.b = node["color"][2].as_int();
				initial_.color.a = current_.color.a = node["color"][3].as_int();
			}
			if(node.has_key("start_colour_range") && node.has_key("end_colour_range")) {
				glm::detail::tvec4<unsigned char> start;
				glm::detail::tvec4<unsigned char> end;
				ASSERT_LOG(node["start_colour_range"].is_list() && node["start_colour_range"].num_elements() == 4,
					"FATAL: PSYSTEM2: 'start_colour_range' should be a list of 4 elements.");
				start.r = node["start_colour_range"][0].as_int();
				start.g = node["start_colour_range"][1].as_int();
				start.b = node["start_colour_range"][2].as_int();
				start.a = node["start_colour_range"][3].as_int();
				ASSERT_LOG(node["end_colour_range"].is_list() && node["end_colour_range"].num_elements() == 4,
					"FATAL: PSYSTEM2: 'end_colour_range' should be a list of 4 elements.");
				end.r = node["end_colour_range"][0].as_int();
				end.g = node["end_colour_range"][1].as_int();
				end.b = node["end_colour_range"][2].as_int();
				end.a = node["end_colour_range"][3].as_int();
				color_range_.reset(new color_range(std::make_pair(start,end)));
			}
			if(node.has_key("particle_width")) {
				particle_width_ = parameter::factory(node["particle_width"]);
			}
			if(node.has_key("particle_height")) {
				particle_height_ = parameter::factory(node["particle_height"]);
			}
			if(node.has_key("particle_depth")) {
				particle_depth_ = parameter::factory(node["particle_depth"]);
			}
			if(node.has_key("name")) {
				name_ = node["name"].as_string();
			}
			// Set a default duration for the emitter.
			ASSERT_LOG(duration_ != NULL, "FATAL: PSYSTEM2: duration_ is null");
			duration_remaining_ = duration_->get_value(0);
		}

		emitter::~emitter()
		{
		}

		void emitter::process(std::vector<particle>& particles, float t, std::vector<particle>::iterator& start, std::vector<particle>::iterator& end)
		{
			// Create the new particles here, calling init_particle on them.
			// pass handle_process the start, end iterators to the newly created particles.
			// after handle_process complete set things like the time_to_live to initial_time_to_live.

			float duration = duration_->get_value(t);
			if(duration == 0.0f || duration_remaining_ >= 0.0f) {
				create_particles(particles, &start, &end, t);
				handle_process(start, end, t);
				set_particle_starting_values(start, end);

				duration_remaining_ -= t;
				if(duration_remaining_ < 0.0f) {
					ASSERT_LOG(repeat_delay_ != NULL, "FATAL: PSYSTEM2: repeat_delay_ is null");
					repeat_delay_remaining_ = repeat_delay_->get_value(t);
				}
			} else {
				repeat_delay_remaining_ -= t;
				if(repeat_delay_remaining_ < 0.0f) {
					ASSERT_LOG(duration_ != NULL, "FATAL: PSYSTEM2: duration_ is null");
					duration_remaining_ = duration_->get_value(t);
				}
			}
		}

		void emitter::create_particles(std::vector<particle>& particles, std::vector<particle>::iterator* start, std::vector<particle>::iterator* end, float t)
		{
			ASSERT_LOG(technique_ != NULL, "technique_ is null");
			int cnt = get_emitted_particle_count_per_cycle(t);
			if(particles.size() + cnt > technique_->quota()) {
				cnt = technique_->quota() - particles.size();
				if(cnt < 0) { 
					cnt = 0; 
				}
			}
			// XXX: techincally this shouldn't be needed as we reserve the default quota upon initialising
			// the particle list. We could hit some pathological case where we allocate particles past
			// the quota (since it isn't enforced yet). This saves us from start from being invalidated
			// if push_back were to cause a reallocation.
			particles.reserve(particles.size() + cnt);
			*start = particles.end();
			for(int n = 0; n != cnt; ++n) {
				particle p;
				init_particle(p, t);
				particles.push_back(p);
			}
			*end = particles.end();
		}

		void emitter::set_particle_starting_values(const std::vector<particle>::iterator start, const std::vector<particle>::iterator end)
		{
			for(auto p = start; p != end; ++p) {
				memcpy(&p->current, &p->initial, sizeof(p->current));
			}
		}

		void emitter::init_particle(particle& p, float t)
		{
			init_physics_parameters(p.initial);
			init_physics_parameters(p.current);
			p.initial.position = current_.position;
			p.initial.color = get_color();
			p.initial.time_to_live = time_to_live_->get_value(t);
			p.initial.velocity = velocity_->get_value(t);
			p.initial.mass = mass_->get_value(t);
			p.initial.dimensions = technique_->default_dimensions();
			if(orientation_range_) {
				p.initial.orientation = glm::lerp(orientation_range_->first, orientation_range_->second, get_random_float(0.0f,1.0f));
			} else {
				p.initial.orientation = current_.orientation;
			}
			p.initial.direction = glm::rotate(p.initial.orientation, get_initial_direction(glm::vec3(0,1,0)));
		}

		int emitter::get_emitted_particle_count_per_cycle(float t)
		{
			ASSERT_LOG(emission_rate_ != NULL, "FATAL: PSYSTEM2: emission_rate_ is NULL");
			// at each step we produce emission_rate()*process_step_time particles.
			float cnt = 0;
			emission_fraction_ = std::modf(emission_fraction_ + emission_rate_->get_value(t)*t, &cnt);
			//std::cerr << "XXX: Emitting " << cnt << " particles" << std::endl;
			return cnt;
		}

		float emitter::generate_angle() const
		{
			// fixme
			float angle = angle_->get_value(0);
			if(angle_->type() == parameter::PARAMETER_FIXED) {
				return get_random_float() * angle;
			}
			return angle;
		}

		glm::vec3 emitter::get_initial_direction(const glm::vec3& up) const
		{
			float angle = generate_angle();
			//std::cerr << "angle:" << angle;
			if(angle != 0) {
				glm::vec3 perp_up = perpendicular(up);

				glm::quat q = glm::angleAxis(get_random_float(0.0f,360.0f), current_.direction);
				perp_up = glm::rotate(q, perp_up);
				q = glm::angleAxis(angle, perp_up);
				auto w = glm::rotate(q, current_.direction);
				return w;
			}
			return current_.direction;
		}

		glm::detail::tvec4<unsigned char> emitter::get_color() const
		{
			if(color_range_) {
				return glm::detail::tvec4<unsigned char>(
					get_random_float(color_range_->first.r,color_range_->second.r),
					get_random_float(color_range_->first.g,color_range_->second.g),
					get_random_float(color_range_->first.b,color_range_->second.b),
					get_random_float(color_range_->first.a,color_range_->second.a));
			}
			return current_.color;
		}

		emitter_ptr emitter::factory(const variant& node, technique* tech)
		{
			ASSERT_LOG(node.has_key("type"), "FATAL: PSYSTEM2: emitter must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "circle") {
				return emitter_ptr(new circle_emitter(node, tech));
			} else if(ntype == "box") {
				return emitter_ptr(new box_emitter(node, tech));
			} else if(ntype == "line") {
				return emitter_ptr(new line_emitter(node, tech));
			} else if(ntype == "point") {
				return emitter_ptr(new point_emitter(node, tech));
			} else if(ntype == "sphere_surface") {
				return emitter_ptr(new sphere_surface_emitter(node, tech));
			} else {
				ASSERT_LOG(false, "FATAL: PSYSTEM2: Unrecognised emitter type: " << ntype);
			}
			return emitter_ptr();
		}

		circle_emitter::circle_emitter(const variant& node, technique* tech)
			: emitter(node, tech), 
			circle_radius_(node["circle_radius"].as_decimal(decimal(0)).as_float()), 
			circle_step_(node["circle_step"].as_decimal(decimal(0.1)).as_float()), 
			circle_angle_(node["circle_angle"].as_decimal(decimal(0)).as_float()), 
			circle_random_(node["emit_random"].as_bool(true))
		{
		}

		circle_emitter::~circle_emitter()
		{
		}

		void circle_emitter::handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t)
		{
			for(auto p = start; p != end; ++p) {
				float angle = 0.0f;
				if(circle_random_) {
					angle = get_random_float(0.0f, 2.0f * M_PI);
				} else {
					angle = t * circle_step_;
				}
				p->initial.position.x += circle_radius_ * sin(angle + circle_angle_);
				p->initial.position.z += circle_radius_ * cos(angle + circle_angle_);
			}
		}

		box_emitter::box_emitter(const variant& node, technique* tech)
			: emitter(node, tech), box_dimensions_(100.0f)
		{
			if(node.has_key("box_width")) {
				box_dimensions_.x = node["box_width"].as_decimal().as_float();
			}
			if(node.has_key("box_height")) {
				box_dimensions_.y = node["box_height"].as_decimal().as_float();
			}
			if(node.has_key("box_depth")) {
				box_dimensions_.z = node["box_depth"].as_decimal().as_float();
			}
		}

		box_emitter::~box_emitter()
		{
		}

		void box_emitter::handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t)
		{
			for(auto p = start; p != end; ++p) {
				p->initial.position.x += get_random_float(0.0f, box_dimensions_.x) - box_dimensions_.x/2;
				p->initial.position.y += get_random_float(0.0f, box_dimensions_.y) - box_dimensions_.y/2;
				p->initial.position.z += get_random_float(0.0f, box_dimensions_.z) - box_dimensions_.z/2;
			}
		}

		line_emitter::line_emitter(const variant& node, technique* tech)
			: emitter(node, tech), line_end_(0.0f), line_deviation_(0.0f),
			min_increment_(0.0f), max_increment_(0.0f)
		{
			if(node.has_key("max_deviation")) {
				line_deviation_ = node["max_deviation"].as_decimal().as_float();
			}
			if(node.has_key("min_increment")) {
				min_increment_ = node["min_increment"].as_decimal().as_float();
			}
			if(node.has_key("max_increment")) {
				max_increment_ = node["max_increment"].as_decimal().as_float();
			}
			// XXX line_end_ ?
		}

		line_emitter::~line_emitter()
		{
		}

		void line_emitter::handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t)
		{
			// XXX
		}

		point_emitter::point_emitter(const variant& node, technique* tech)
			: emitter(node, tech)
		{
		}

		point_emitter::~point_emitter()
		{
		}

		void point_emitter::handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t)
		{
			// nothing need be done
		}

		sphere_surface_emitter::sphere_surface_emitter(const variant& node, technique* tech)
			: emitter(node, tech), radius_(node["radius"].as_decimal(decimal(1.0)).as_float())
		{
		}

		sphere_surface_emitter::~sphere_surface_emitter()
		{
		}

		void sphere_surface_emitter::handle_process(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end, float t)
		{
			for(auto p = start; p != end; ++p) {
				float theta = get_random_float(0, 2.0f*M_PI);
				float phi = acos(get_random_float(-1.0f, 1.0f));
				p->initial.position.x += radius_ * sin(phi) * cos(theta);
				p->initial.position.y += radius_ * sin(phi) * sin(theta);
				p->initial.position.z += radius_ * cos(phi);
			}
		}
	}
}
