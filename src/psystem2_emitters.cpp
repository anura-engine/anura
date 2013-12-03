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
#include "level.hpp"
#include "psystem2.hpp"
#include "psystem2_affectors.hpp"
#include "psystem2_emitters.hpp"
#include "psystem2_parameters.hpp"

namespace graphics
{
	namespace 
	{
		GLsizei add_box_data_to_vbo(std::shared_ptr<GLuint> vbo_id)
		{
			std::vector<GLfloat> lines;
			lines.reserve(72);
			lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); 
			lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); 
			lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); 

			lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); 
			lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(0.5f); 
			lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); 

			lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); 
			lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); 

			lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); 
			lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); 

			lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); 
			lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(0.5f); lines.push_back(0.5f); lines.push_back(-0.5f); lines.push_back(-0.5f); 
			glBindBuffer(GL_ARRAY_BUFFER, *vbo_id);
			glBufferData(GL_ARRAY_BUFFER, lines.size()*sizeof(GLfloat), &lines[0], GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			return lines.size()/3;
		}

		std::shared_ptr<GLuint> get_box_outline_vbo(GLsizei& num_vertices)
		{
			static std::shared_ptr<GLuint> res;
			static GLsizei num_verts = 0;
			if(res == NULL) {
				// XXX This is probably broken since the context will be deleted before this destructor fires.
				res.reset(new GLuint, [](GLuint* id){glDeleteBuffers(1, id); delete id;});
				glGenBuffers(1, res.get());
				num_verts = add_box_data_to_vbo(res);
				GLenum ok = glGetError();
				ASSERT_EQ(ok, GL_NO_ERROR);
			}
			num_vertices = num_verts;
			return res;
		}
	}

	class BoxOutline
	{
	public:
		BoxOutline() : color_(0.25f, 1.0f, 0.25f, 1.0f), num_vertices_(0) {
			shader_ = gles2::shader_program::get_global("line_3d")->shader();
			ASSERT_LOG(shader_ != NULL, "FATAL: PSYSTEM2: test_draw_shader_ is null");
			u_mvp_matrix_ = shader_->get_fixed_uniform("mvp_matrix");
			ASSERT_LOG(u_mvp_matrix_ != -1, "FATAL: PSYSTEM2: Uniform 'mvp_matrix' unknown");
			u_color_ = shader_->get_fixed_uniform("color");
			ASSERT_LOG(u_color_ != -1, "FATAL: PSYSTEM2: Uniform 'color' unknown");
			a_position_ = shader_->get_fixed_attribute("vertex");
			ASSERT_LOG(a_position_ != -1, "FATAL: PSYSTEM2: Attribute 'vertex' unknown");

			// blah blah, allocate vbo then put data in that
			box_vbo_ = get_box_outline_vbo(num_vertices_);
		}
		~BoxOutline() {}
		const glm::vec4& get_color() const { return color_; }
		void set_color(const glm::vec4& c) {
			color_ = c;
		}
		void draw(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale) const {
			shader::manager m(shader_);
			glm::mat4 model = glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
			//glm::mat4 mvp = level::current().camera()->projection_mat() * level::current().camera()->view_mat() * model;
			glm::mat4 mvp = get_main_window()->camera()->projection_mat() * get_main_window()->camera()->view_mat() * model;
			
			glUniformMatrix4fv(u_mvp_matrix_, 1, GL_FALSE, glm::value_ptr(mvp));
			glUniform4fv(u_color_, 1, glm::value_ptr(color_));
#if defined(USE_SHADERS)
			glEnableVertexAttribArray(a_position_);
			glBindBuffer(GL_ARRAY_BUFFER, *box_vbo_);
			glVertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE, 0, 0);
			glDrawArrays(GL_LINES, 0, num_vertices_);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glDisableVertexAttribArray(a_position_);
#endif
		}
	private:
		GLsizei num_vertices_;
		std::shared_ptr<GLuint> vbo_id_;
		gles2::program_ptr shader_;
		std::shared_ptr<GLuint> box_vbo_;
		GLuint u_mvp_matrix_;
		GLuint u_color_;
		GLuint a_position_;
		glm::vec4 color_;
	};

	namespace particles
	{
		class circle_emitter : public emitter
		{
		public:
			circle_emitter(particle_system_container* parent, const variant& node) 			
				: emitter(parent, node), 
				circle_radius_(node["circle_radius"].as_decimal(decimal(0)).as_float()), 
				circle_step_(node["circle_step"].as_decimal(decimal(0.1)).as_float()), 
				circle_angle_(node["circle_angle"].as_decimal(decimal(0)).as_float()), 
				circle_random_(node["emit_random"].as_bool(true)) {
			}
			virtual ~circle_emitter() {}
		protected:
			void internal_create(particle& p, float t) {
				float angle = 0.0f;
				if(circle_random_) {
					angle = get_random_float(0.0f, 2.0f * M_PI);
				} else {
					angle = t * circle_step_;
				}
				p.initial.position.x += circle_radius_ * sin(angle + circle_angle_);
				p.initial.position.z += circle_radius_ * cos(angle + circle_angle_);
			}
			virtual emitter* clone() {
				return new circle_emitter(*this);
			}
		private:
			float circle_radius_;
			float circle_step_;
			float circle_angle_;
			bool circle_random_;

			circle_emitter();
		};

		class box_emitter : public emitter
		{
		public:
			box_emitter(particle_system_container* parent, const variant& node) 
				: emitter(parent, node), box_dimensions_(100.0f) {
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
			virtual ~box_emitter() {}
		protected:
			void internal_create(particle& p, float t) {
				p.initial.position.x += get_random_float(0.0f, box_dimensions_.x) - box_dimensions_.x/2;
				p.initial.position.y += get_random_float(0.0f, box_dimensions_.y) - box_dimensions_.y/2;
				p.initial.position.z += get_random_float(0.0f, box_dimensions_.z) - box_dimensions_.z/2;
			}
			virtual emitter* clone() {
				return new box_emitter(*this);
			}
		private:
			glm::vec3 box_dimensions_;
			box_emitter();
		};

		class line_emitter : public emitter
		{
		public:
			line_emitter(particle_system_container* parent, const variant& node) 
				: emitter(parent, node), line_end_(0.0f), 
				line_deviation_(0.0f),
				min_increment_(0.0f), 
				max_increment_(0.0f) {
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
			virtual ~line_emitter() {}
		protected:
			void internal_create(particle& p, float t) {
				// XXX todo
			}
			virtual emitter* clone() {
				return new line_emitter(*this);
			}
		private:
			glm::vec3 line_end_;
			float line_deviation_;
			float min_increment_;
			float max_increment_;

			line_emitter();
		};

		class point_emitter : public emitter
		{
		public:
			point_emitter(particle_system_container* parent, const variant& node) : emitter(parent, node) {}
			virtual ~point_emitter() {}
		protected:
			void internal_create(particle& p, float t) {
				// intentionally does nothing.
			}
			virtual emitter* clone() {
				return new point_emitter(*this);
			}
		private:
			point_emitter();
		};

		class sphere_surface_emitter : public emitter
		{
		public:
			sphere_surface_emitter(particle_system_container* parent, const variant& node) 
				: emitter(parent, node), 
				radius_(node["radius"].as_decimal(decimal(1.0)).as_float()) {
			}
			virtual ~sphere_surface_emitter() {}
		protected:
			void internal_create(particle& p, float t) {
				float theta = get_random_float(0, 2.0f*M_PI);
				float phi = acos(get_random_float(-1.0f, 1.0f));
				p.initial.position.x += radius_ * sin(phi) * cos(theta);
				p.initial.position.y += radius_ * sin(phi) * sin(theta);
				p.initial.position.z += radius_ * cos(phi);
			}
			virtual emitter* clone() {
				return new sphere_surface_emitter(*this);
			}
		private:
			float radius_;
			sphere_surface_emitter();
		};


		emitter::emitter(particle_system_container* parent, const variant& node)
			: emit_object(parent, node), 
			emission_fraction_(0.0f),
			force_emission_(node["force_emission"].as_bool(false)),
			force_emission_processed_(false), 
			can_be_deleted_(false),
			emits_type_(EMITS_VISUAL),
			color_(1.0f,1.0f,1.0f,1.0f)
		{
			init_physics_parameters(initial);
			init_physics_parameters(current);
			initial.time_to_live = current.time_to_live = 3;

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
				initial.direction = current.direction = variant_to_vec3(node["direction"]);
			}
			if(node.has_key("position")) {
				initial.position = current.position = variant_to_vec3(node["position"]);
			}
			if(node.has_key("orientation")) {
				initial.orientation = current.orientation = variant_to_quat(node["orientation"]);
			}
			if(node.has_key("orientation_start") && node.has_key("orientation_end")) {
				orientation_range_.reset(new std::pair<glm::quat, glm::quat>(variant_to_quat(node["orientation_start"]), variant_to_quat(node["orientation_end"])));
			}
			if(node.has_key("color")) {
				color_ = variant_to_vec4(node["color"]);
			} else if(node.has_key("colour")) {
				color_ = variant_to_vec4(node["colour"]);
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
			if(node.has_key("emits_type")) {
				ASSERT_LOG(node.has_key("emits_name"), 
					"FATAL: PSYSTEM2: Emitters that specify the 'emits_type' attribute must give have and 'emits_type' attribute");
				const std::string& etype = node["emits_type"].as_string();
				if(etype == "emitter_particle") {
					emits_type_ = EMITS_EMITTER;
				} else if(etype == "visual_particle") {
					emits_type_ = EMITS_VISUAL;
				} else if(etype == "technique_particle") {
					emits_type_ = EMITS_TECHNIQUE;
				} else if(etype == "affector_particle") {
					emits_type_ = EMITS_AFFECTOR;
				} else if(etype == "system_particle") {
					emits_type_ = EMITS_SYSTEM;
				} else {
					ASSERT_LOG(false, "FATAL: PSYSTEM2: Unrecognised 'emit_type' attribute value: " << etype);
				}
				emits_name_ = node["emits_name"].as_string();
			}
			if(node.has_key("debug_draw") && node["debug_draw"].as_bool()) {
				debug_draw_outline_.reset(new BoxOutline());
				if(node.has_key("debug_draw_color")) {
					debug_draw_outline_->set_color(variant_to_vec4(node["debug_draw_color"]));
				}
			}
			// Set a default duration for the emitter.
			ASSERT_LOG(duration_ != NULL, "FATAL: PSYSTEM2: duration_ is null");
			duration_remaining_ = duration_->get_value(0);
		}

		emitter::~emitter()
		{
		}

		emitter::emitter(const emitter& e)
			: emit_object(e),
			technique_(NULL),
			emission_rate_(e.emission_rate_),
			time_to_live_(e.time_to_live_),
			velocity_(e.velocity_),
			angle_(e.angle_),
			mass_(e.mass_),
			duration_(e.duration_),
			repeat_delay_(e.repeat_delay_),
			particle_width_(e.particle_width_),
			particle_height_(e.particle_height_),
			particle_depth_(e.particle_depth_),
			force_emission_(e.force_emission_),
			force_emission_processed_(false),
			can_be_deleted_(false),
			emits_type_(e.emits_type_),
			emits_name_(e.emits_name_),
			emission_fraction_(0),
			duration_remaining_(0),
			color_(e.color_)
		{
			if(e.orientation_range_) {
				orientation_range_.reset(new std::pair<glm::quat,glm::quat>(e.orientation_range_->first, e.orientation_range_->second));
			}
			if(e.color_range_) {
				color_range_.reset(new color_range(color_range_->first, color_range_->second));
			}
			if(e.debug_draw_outline_) {
				debug_draw_outline_.reset(new BoxOutline());
				debug_draw_outline_->set_color(e.debug_draw_outline_->get_color());
			}
			duration_remaining_ = duration_->get_value(0);
		}

		void emitter::handle_process(float t) 
		{
			ASSERT_LOG(technique_ != NULL, "FATAL: PSYSTEM2: technique is null");
			std::vector<particle>& particles = technique_->active_particles();

			float duration = duration_->get_value(t);
			if(duration == 0.0f || duration_remaining_ >= 0.0f) {
				if(emits_type_ == EMITS_VISUAL) {
					std::vector<particle>::iterator start;
					std::vector<particle>::iterator end;
					create_particles(particles, start, end, t);
					for(auto it = start; it != end; ++it) {
						internal_create(*it, t);
					}
					set_particle_starting_values(start, end);
				} else {
					if(emits_type_ == EMITS_EMITTER) {
						size_t cnt = calculate_particles_to_emit(t, technique_->emitter_quota(), technique_->active_emitters().size());
						//std::cerr << "XXX: Emitting " << cnt << " emitters" << std::endl;
						for(int n = 0; n != cnt; ++n) {
							emitter_ptr e = parent_container()->clone_emitter(emits_name_);
							e->emitted_by = this;
							init_particle(*e, t);
							internal_create(*e, t);
							memcpy(&e->current, &e->initial, sizeof(e->current));
							technique_->add_emitter(e);
						}
					} else if(emits_type_ == EMITS_AFFECTOR) {
						size_t cnt = calculate_particles_to_emit(t, technique_->affector_quota(), technique_->active_affectors().size());
						for(int n = 0; n != cnt; ++n) {
							affector_ptr a = parent_container()->clone_affector(emits_name_);
							a->emitted_by = this;
							init_particle(*a, t);
							internal_create(*a, t);
							memcpy(&a->current, &a->initial, sizeof(a->current));
							technique_->add_affector(a);
						}
					} else if(emits_type_ == EMITS_TECHNIQUE) {
						size_t cnt = calculate_particles_to_emit(t, technique_->technique_quota(), technique_->get_particle_system()->active_techniques().size());
						for(int n = 0; n != cnt; ++n) {
							technique_ptr tq = parent_container()->clone_technique(emits_name_);
							tq->emitted_by = this;
							init_particle(*tq, t);
							internal_create(*tq, t);
							memcpy(&tq->current, &tq->initial, sizeof(tq->current));
							technique_->get_particle_system()->add_technique(tq);
						}
					} else if(emits_type_ == EMITS_SYSTEM) {
						size_t cnt = calculate_particles_to_emit(t, technique_->system_quota(), parent_container()->active_particle_systems().size());
						for(int n = 0; n != cnt; ++n) {
							particle_system_ptr ps = parent_container()->clone_particle_system(emits_name_);
							ps->emitted_by = this;
							init_particle(*ps, t);
							internal_create(*ps, t);
							memcpy(&ps->current, &ps->initial, sizeof(ps->current));
							parent_container()->add_particle_system(ps.get());
						}
					} else {
						ASSERT_LOG(false, "FATAL: PSYSTEM2: unknown emits_type: " << emits_type_);
					}
				}

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

		size_t emitter::calculate_particles_to_emit(float t, size_t quota, size_t current_size)
		{
			size_t cnt = 0;
			if(force_emission_) {
				if(!force_emission_processed_) {
					// Single shot of all particles at once.
					cnt = emission_rate_->get_value(technique_->get_particle_system()->elapsed_time());
					force_emission_processed_ = true;
				}
			} else {
				cnt = get_emitted_particle_count_per_cycle(t);
			}
			if(current_size + cnt > quota) {
				if(current_size >= quota) {
					cnt = 0;
				} else {
					cnt = quota - current_size;
				}
			}
			return cnt;
		}

		void emitter::create_particles(std::vector<particle>& particles, 
			std::vector<particle>::iterator& start, 
			std::vector<particle>::iterator& end, 
			float t)
		{
			ASSERT_LOG(technique_ != NULL, "technique_ is null");
			size_t cnt = calculate_particles_to_emit(t, technique_->quota(), particles.size());

			// XXX: techincally this shouldn't be needed as we reserve the default quota upon initialising
			// the particle list. We could hit some pathological case where we allocate particles past
			// the quota (since it isn't enforced yet). This saves us from start from being invalidated
			// if push_back were to cause a reallocation.
			particles.reserve(particles.size() + cnt);
			start = particles.end();
			for(size_t n = 0; n != cnt; ++n) {
				particle p;
				init_particle(p, t);
				particles.push_back(p);
			}
			end = particles.end();
		}

		void emitter::set_particle_starting_values(const std::vector<particle>::iterator& start, const std::vector<particle>::iterator& end)
		{
			for(auto p = start; p != end; ++p) {
				memcpy(&p->current, &p->initial, sizeof(p->current));
			}
		}

		void emitter::init_particle(particle& p, float t)
		{
			init_physics_parameters(p.initial);
			init_physics_parameters(p.current);
			p.initial.position = current.position;
			p.initial.color = get_color();
			p.initial.time_to_live = time_to_live_->get_value(technique_->get_particle_system()->elapsed_time());
			p.initial.velocity = velocity_->get_value(technique_->get_particle_system()->elapsed_time());
			p.initial.mass = mass_->get_value(technique_->get_particle_system()->elapsed_time());
			p.initial.dimensions = technique_->default_dimensions();
			if(orientation_range_) {
				p.initial.orientation = glm::slerp(orientation_range_->first, orientation_range_->second, get_random_float(0.0f,1.0f));
			} else {
				p.initial.orientation = current.orientation;
			}
			p.initial.direction = get_initial_direction();
			p.emitted_by = this;
		}

		int emitter::get_emitted_particle_count_per_cycle(float t)
		{
			ASSERT_LOG(emission_rate_ != NULL, "FATAL: PSYSTEM2: emission_rate_ is NULL");
			// at each step we produce emission_rate()*process_step_time particles.
			float cnt = 0;
			emission_fraction_ = std::modf(emission_fraction_ + emission_rate_->get_value(t)*t, &cnt);
			return cnt;
		}

		float emitter::generate_angle() const
		{
			ASSERT_LOG(technique_ != NULL, "FATAL: PSYSTEM2: technique_ is null");
			ASSERT_LOG(technique_->get_particle_system() != NULL, "FATAL: PSYSTEM2: technique_->get_parent_system() is null");
			float angle = angle_->get_value(technique_->get_particle_system()->elapsed_time());
			if(angle_->type() == parameter::PARAMETER_FIXED) {
				return get_random_float() * angle;
			}
			return angle;
		}

		glm::vec3 emitter::get_initial_direction() const
		{
			float angle = generate_angle();
			//std::cerr << "angle:" << angle;
			if(angle != 0) {
				return create_deviating_vector(angle, current.direction);
			}
			return current.direction;
		}

		color_vector emitter::get_color() const
		{
			if(color_range_) {
				return glm::detail::tvec4<unsigned char>(
					get_random_float(color_range_->first.r,color_range_->second.r),
					get_random_float(color_range_->first.g,color_range_->second.g),
					get_random_float(color_range_->first.b,color_range_->second.b),
					get_random_float(color_range_->first.a,color_range_->second.a));
			}
			color_vector c;
			c.r = uint8_t(color_.r * 255.0f);
			c.g = uint8_t(color_.g * 255.0f);
			c.b = uint8_t(color_.b * 255.0f);
			c.a = uint8_t(color_.a * 255.0f);
			return c;
		}

		void emitter::handle_draw() const
		{
			if(debug_draw_outline_) {
				debug_draw_outline_->draw(current.position, current.orientation, glm::vec3(0.25f,0.25f,0.25f));
			}
		}

		emitter* emitter::factory(particle_system_container* parent, const variant& node)
		{
			ASSERT_LOG(node.has_key("type"), "FATAL: PSYSTEM2: emitter must have 'type' attribute");
			const std::string& ntype = node["type"].as_string();
			if(ntype == "circle") {
				return new circle_emitter(parent, node);
			} else if(ntype == "box") {
				return new box_emitter(parent, node);
			} else if(ntype == "line") {
				return new line_emitter(parent, node);
			} else if(ntype == "point") {
				return new point_emitter(parent, node);
			} else if(ntype == "sphere_surface") {
				return new sphere_surface_emitter(parent, node);
			}
			ASSERT_LOG(false, "FATAL: PSYSTEM2: Unrecognised emitter type: " << ntype);
			return NULL;
		}
	}
}
