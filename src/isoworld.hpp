#pragma once

#if defined(USE_ISOMAP)
#if !defined(USE_GLES2)
#error in order to build with Iso tiles you need to be building with shaders (USE_GLES2)
#endif

#include <boost/intrusive_ptr.hpp>
#include <boost/unordered_map.hpp>

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "geometry.hpp"
#include "graphics.hpp"
#include "isochunk.hpp"
#include "raster.hpp"
#include "shaders.hpp"
#include "variant.hpp"

namespace isometric
{
	class world : public game_logic::formula_callable
	{
	public:
		explicit world(const variant& node);
		virtual ~world();
		
		gles2::program_ptr shader() { return shader_; }

		float gamma() const { return gamma_; }
		void set_gamma(float g);

		float light_power() const { return light_power_; }
		void set_light_power(float lp);

		const glm::vec3& light_position() const { return light_position_; }
		void set_light_position(const glm::vec3& lp);
		void set_light_position(const variant& lp);

		bool lighting_enabled() const { return lighting_enabled_; }

		void build();
		void draw() const;
		variant write();
		void process();
	protected:
	private:
		DECLARE_CALLABLE(world);
		gles2::program_ptr shader_;
		GLuint u_lightposition_;
		GLuint u_lightpower_;
		GLuint u_gamma_;

		glm::vec3 light_position_;
		float light_power_;
		float gamma_;

		bool lighting_enabled_;

		int view_distance_;

		uint32_t seed_;

		std::vector<chunk_ptr> active_chunks_;
		boost::unordered_map<position, chunk_ptr> chunks_;
		
		void get_active_chunks();

		world();
		world(const world&);
	};

	typedef boost::intrusive_ptr<world> world_ptr;
}

#endif
