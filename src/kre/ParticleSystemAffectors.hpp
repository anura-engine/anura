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

#pragma once

#include "ParticleSystemFwd.hpp"
#include "spline3d.hpp"

namespace KRE
{
	namespace Particles
	{
		enum class AffectorType
		{
			COLOR,
			JET,
			VORTEX,
			GRAVITY,
			LINEAR_FORCE,
			SCALE,
			PARTICLE_FOLLOWER,
			ALIGN,
			FLOCK_CENTERING,
			BLACK_HOLE,
			PATH_FOLLOWER,
			RANDOMISER,
			SINE_FORCE,
			TEXTURE_ROTATOR,
			ANIMATION,
		};

		class Affector : public EmitObject
		{
		public:
			explicit Affector(std::weak_ptr<ParticleSystemContainer> parent, AffectorType type);
			explicit Affector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node, AffectorType type);
			virtual ~Affector();
			virtual AffectorPtr clone() const = 0;

			AffectorType getType() const { return type_; }

			virtual bool showMassUI() const { return false; }
			virtual bool showPositionUI() const { return false; }
			virtual bool showScaleUI() const { return false; }

			float getMass() const { return mass_; }
			void setMass(float m) { mass_ = m; }
			const glm::vec3& getPosition() const { return position_; }
			void setPosition(const glm::vec3& pos) { position_ = pos; }
			const glm::vec3& getScale() const { return scale_; }
			void setScale(const glm::vec3& scale) { scale_ = scale; }

			const variant& node() const { return node_; }
			void setNode(const variant& new_node) { node_ = new_node; init(new_node); }

			variant write() const;

			static AffectorPtr factory(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			static AffectorPtr factory(std::weak_ptr<ParticleSystemContainer> parent, AffectorType type);
		protected:
			virtual void handleEmitProcess(float t) override;
		private:
			virtual void init(const variant& node) = 0;
			virtual void internalApply(Particle& p, float t) = 0;
			virtual void handleWrite(variant_builder* build) const override = 0;

			AffectorType type_;
			float mass_;
			glm::vec3 position_;
			glm::vec3 scale_;
			variant node_;

			Affector() = delete;
		};

		class TimeColorAffector : public Affector
		{
		public:
			enum class ColourOperation {
				COLOR_OP_SET,
				COLOR_OP_MULTIPLY,
			};
			typedef std::pair<float,glm::vec4> tc_pair;

			explicit TimeColorAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit TimeColorAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			ColourOperation getOperation() const { return operation_; }
			void setOperation(ColourOperation op) { operation_ = op; }

			const std::vector<tc_pair>& getTimeColorData() const { return tc_data_; }
			std::vector<tc_pair>& getTimeColorData() { return tc_data_; }
			void clearTimeColorData() { tc_data_.clear(); }
			void addTimecolorEntry(const tc_pair& tc) { tc_data_.emplace_back(tc); sort_tc_data(); }
			void setTimeColorData(const std::vector<tc_pair>& tc) { tc_data_ = tc; sort_tc_data(); }
			void removeTimeColorEntry(const tc_pair& f);

			bool isInterpolated() const { return interpolate_; }
			void setInterpolate(bool f) { interpolate_ = f; }
		private:
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<TimeColorAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			ColourOperation operation_;
			std::vector<tc_pair> tc_data_;
			bool interpolate_;	// interpolate or stepped.

			void sort_tc_data();
			std::vector<tc_pair>::iterator find_nearest_color(float dt);

			TimeColorAffector() = delete;
		};

		class AnimationAffector : public Affector
		{
		public:
			typedef std::pair<float,rectf> uv_pair;

			explicit AnimationAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit AnimationAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const std::vector<uv_pair>& getTimeCoordData() const { return uv_data_; }
			std::vector<uv_pair>& getTimeCoordData() { return uv_data_; }
			void clearTimeCoordData() { uv_data_.clear(); trf_uv_data_.clear(); }
			void addTimeCoordEntry(const uv_pair& tc) { uv_data_.emplace_back(tc); trf_uv_data_.clear(); }
			void setTimeCoordData(const std::vector<uv_pair>& tc) { uv_data_ = tc; trf_uv_data_.clear(); }
			void removeTimeCoordEntry(const uv_pair& f);

			bool isPixelCoords() const { return pixel_coords_; }
			void setUsePixelCoords(bool f) { pixel_coords_ = f; }

			bool useMassInsteadOfTime() const { return use_mass_instead_of_time_; }
			void setUseMassInsteadOfTime(bool f) { use_mass_instead_of_time_ = f; }
		private:
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<AnimationAffector>(*this);
			}
			void handleWrite(variant_builder* build) const override;
			void transformCoords();

			bool pixel_coords_;
			bool use_mass_instead_of_time_;
			std::vector<uv_pair> uv_data_;
			// transformed version of the uv data as required.
			std::vector<uv_pair> trf_uv_data_;

			void sort_uv_data();
			std::vector<uv_pair>::iterator find_nearest_coords(float dt);

			AnimationAffector() = delete;
		};


		class JetAffector : public Affector
		{
		public:
			explicit JetAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit JetAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const ParameterPtr& getAcceleration() const { return acceleration_; }
		private:
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<JetAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			ParameterPtr acceleration_;
			JetAffector() = delete;
		};

		class GravityAffector : public Affector
		{
		public:
			explicit GravityAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit GravityAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const ParameterPtr& getGravity() const { return gravity_; }

			virtual bool showMassUI() const override { return true; }
			virtual bool showPositionUI() const override { return true; }
		private:
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<GravityAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			ParameterPtr gravity_;
			GravityAffector() = delete;
		};

		class LinearForceAffector : public Affector
		{
		public:
			explicit LinearForceAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit LinearForceAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const ParameterPtr& getForce() const { return force_; }
			const glm::vec3& getDirection() const { return direction_; }
			void setDirection(const glm::vec3& d) { direction_ = d; }
		private:
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<LinearForceAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;

			ParameterPtr force_;
			glm::vec3 direction_;
			LinearForceAffector() = delete;
		};

		class ScaleAffector : public Affector
		{
		public:
			explicit ScaleAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit ScaleAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const ParameterPtr& getScaleX() const { return scale_x_; }
			const ParameterPtr& getScaleY() const { return scale_y_; }
			const ParameterPtr& getScaleZ() const { return scale_z_; }
			const ParameterPtr& getScaleXYZ() const { return scale_xyz_; }
			bool getSinceSystemStart() const { return since_system_start_; }
			void setSinceSystemStart(bool f) { since_system_start_ = f; }

			virtual bool showScaleUI() const override { return true; }
		private:
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<ScaleAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			ParameterPtr scale_x_;
			ParameterPtr scale_y_;
			ParameterPtr scale_z_;
			ParameterPtr scale_xyz_;
			bool since_system_start_;
			float calculateScale(ParameterPtr s, const Particle& p);
			ScaleAffector() = delete;
		};

		class VortexAffector : public Affector
		{
		public:
			explicit VortexAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit VortexAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const glm::vec3& getRotationAxis() const { return rotation_axis_; }
			void setRotationAxis(const glm::vec3& axis) { rotation_axis_ = axis; }
			const ParameterPtr& getRotationSpeed() const { return rotation_speed_; }
			virtual bool showPositionUI() const override { return true; }
		private:
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<VortexAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			glm::vec3 rotation_axis_;
			ParameterPtr rotation_speed_;
			VortexAffector() = delete;
		};

		class ParticleFollowerAffector : public Affector
		{
		public:
			explicit ParticleFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit ParticleFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			float getMinDistance() const { return min_distance_; }
			void setMinDistance(float min_dist) { 
				min_distance_ = min_dist; 
				if(min_distance_ > max_distance_ ) {
					min_distance_ = max_distance_;
				}
			}
			float getMaxDistance() const { return max_distance_; }
			void setMaxDistance(float max_dist) { 
				max_distance_ = max_dist; 
				if(max_distance_ < min_distance_) {
					max_distance_ = min_distance_;
				}
			}
		private:
			void handleEmitProcess(float t) override;
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<ParticleFollowerAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			float min_distance_;
			float max_distance_;
			// working variables
			std::vector<Particle>::iterator prev_particle_;
			ParticleFollowerAffector() = delete;
		};

		class AlignAffector : public Affector
		{
		public:
			explicit AlignAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit AlignAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			bool getResizeable() const { return resize_; }
			void setResizeable(bool r) { resize_ = r; }
		private:
			void internalApply(Particle& p, float t) override;
			void handleEmitProcess(float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<AlignAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			bool resize_;			
			std::vector<Particle>::iterator prev_particle_;
			AlignAffector() = delete;
		};

		class FlockCenteringAffector : public Affector
		{
		public:
			explicit FlockCenteringAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit FlockCenteringAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;
		private:
			void internalApply(Particle& p, float t) override;
			void handleEmitProcess(float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<FlockCenteringAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			glm::vec3 average_;
			std::vector<Particle>::iterator prev_particle_;
			FlockCenteringAffector() = delete;
		};

		class BlackHoleAffector : public Affector
		{
		public:
			explicit BlackHoleAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit BlackHoleAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const ParameterPtr& getVelocity() const { return velocity_; };
			const ParameterPtr& getAcceleration() const { return acceleration_; }
			virtual bool showPositionUI() const override { return true; }
		private:
			void handleEmitProcess(float t) override;
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<BlackHoleAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;

			ParameterPtr velocity_;
			ParameterPtr acceleration_;
			// working
			float wvelocity_;
			BlackHoleAffector() = delete;
		};

		class PathFollowerAffector : public Affector
		{
		public:
			explicit PathFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit PathFollowerAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const std::vector<glm::vec3>& getPoints() const { return points_; }
			void clearPoints();
			void addPoint(const glm::vec3& p);
			void setPoints(const std::vector<glm::vec3>& points);
			void setPoints(const variant& p);
		private:
			void internalApply(Particle& p, float t) override;
			void handleEmitProcess(float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<PathFollowerAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			std::vector<glm::vec3> points_;
			// working variables.
			std::shared_ptr<geometry::spline3d<float>> spl_;
			std::vector<Particle>::iterator prev_particle_;
			PathFollowerAffector() = delete;
		};

		class RandomiserAffector : public Affector
		{
		public:
			explicit RandomiserAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit RandomiserAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const glm::vec3& getDeviation() const { return max_deviation_; }
			void setDeviation(const glm::vec3& d) { max_deviation_ = d; }
			void setDeviation(float x, float y, float z) { max_deviation_ = glm::vec3(x, y, z); }
			bool isRandomDirection() const { return random_direction_; }
			void setRandomDirection(bool f) { random_direction_ = f; }
			float getTimeStep() const { return time_step_; }
			void setTimeStep(float step) { time_step_ = step; }

			bool showScaleUI() const override { return true; }
		private:
			void internalApply(Particle& p, float t) override;
			void handle_apply(std::vector<Particle>& particles, float t);
			void handle_apply(const EmitterPtr& objs, float t);
			virtual void handleProcess(float t);
			AffectorPtr clone() const override {
				return std::make_shared<RandomiserAffector>(*this);
			}
			virtual void handleWrite(variant_builder* build) const override;
		
			// randomiser (bool random_direction_, float time_step_ glm::vec3 max_deviation_)
			bool random_direction_;
			float time_step_;
			glm::vec3 max_deviation_;
			float last_update_time_[2];
			RandomiserAffector() = delete;
		};

		class SineForceAffector : public Affector
		{
		public:
			enum class ForceApplication {
				FA_ADD,
				FA_AVERAGE,
			};

			explicit SineForceAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit SineForceAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			ForceApplication getForceApplication() const { return fa_; }
			void setForceApplication(ForceApplication fa) { fa_ = fa; }
			const glm::vec3& getForceVector() const { return force_vector_; }
			void setForceVector(const glm::vec3& fv) { force_vector_ = fv; }
			void setForceVector(float x, float y, float z) { force_vector_ = glm::vec3(x, y, z); }
			const glm::vec3& getScaleVector() const { return scale_vector_; }
			void setScaleVector(const glm::vec3& sv) { scale_vector_ = sv; }
			float getMinFrequency() const { return min_frequency_; }
			void setMinFrequency(float min_freq) { min_frequency_ = min_freq; }
			float getMaxFrequency() const { return max_frequency_; }
			void setMaxFrequency(float max_freq) { max_frequency_ = max_freq; }
			float getAngle() const { return angle_; }
			void setAngle(float a) { angle_ = a; }

		private:
			void handleEmitProcess(float t) override;
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<SineForceAffector>(*this);
			}
			void handleWrite(variant_builder* build) const override;
		
			glm::vec3 force_vector_;
			glm::vec3 scale_vector_;
			float min_frequency_;
			float max_frequency_;
			ForceApplication fa_;
			// working variable
			float frequency_;
			float angle_;
			SineForceAffector() = delete;
		};

		class TextureRotatorAffector : public Affector
		{
		public:
			explicit TextureRotatorAffector(std::weak_ptr<ParticleSystemContainer> parent);
			explicit TextureRotatorAffector(std::weak_ptr<ParticleSystemContainer> parent, const variant& node);
			void init(const variant& node) override;

			const ParameterPtr& getAngle() const { return angle_; }
			const ParameterPtr& getSpeed() const { return speed_; }
		private:
			void internalApply(Particle& p, float t) override;
			AffectorPtr clone() const override {
				return std::make_shared<TextureRotatorAffector>(*this);
			}
			void handleWrite(variant_builder* build) const override;

			ParameterPtr angle_;
			ParameterPtr speed_;

			TextureRotatorAffector() = delete;
		};

		const char* get_affector_name(AffectorType type);
	}
}
