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

#include "geometry.hpp"
#include "graphics.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "pathfinding.hpp"
#include "raster.hpp"
#include "shaders.hpp"
#include "variant.hpp"

namespace isometric
{
	struct position
	{
		position(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}
		int x, y, z;
	};
	bool operator==(position const& p1, position const& p2);
	std::size_t hash_value(position const& p);

	struct tile_data
	{
		tile_data() {color.r = 0; color.g = 0; color.b = 0; color.a = 255;}
		explicit tile_data(const std::string& s) : name(s) {}
		explicit tile_data(const SDL_Color& c) {color.r = c.r; color.g = c.g; color.b = c.b; color.a = c.a;}
		std::string name;
		SDL_Color color;
	};

	typedef boost::unordered_map<position, tile_data> tile_type;

	struct tile_editor_info
	{
		std::string name;
		std::string group;
		std::string id;
		graphics::texture tex;
		rect area;
	};

	class isomap : public game_logic::formula_callable
	{
	public:
		enum {
			FRONT	= 1,
			RIGHT	= 2,
			TOP		= 4,
			BACK	= 8,
			LEFT	= 16,
			BOTTOM	= 32,
		};

		isomap();
		explicit isomap(variant node);
		virtual ~isomap();
		
		void init();
		void build();
		void rebuild();
		virtual void draw() const;
		variant write();

		bool is_solid(int x, int y, int z) const;
		bool is_xedge(int x, int size_x) const;
		bool is_yedge(int y, int size_y) const;
		bool is_zedge(int z, int size_z) const;
		std::string get_tile_type(int x, int y, int z) const;
		static variant get_tile_info(const std::string& type);
		pathfinding::directed_graph_ptr create_directed_graph(bool allow_diagonals=false);

		void set_tile(int x, int y, int z, const std::string& type);
		void del_tile(int x, int y, int z);

		static const std::vector<tile_editor_info>& get_editor_tiles();
	protected:
		const GLfloat* model() const { return glm::value_ptr(model_); }
	private:
		DECLARE_CALLABLE(isomap);

		enum {
			FRONT_FACE,
			RIGHT_FACE,
			TOP_FACE,
			BACK_FACE,
			LEFT_FACE,
			BOTTOM_FACE,
			MAX_FACES,
		};

		struct shader_data
		{
			bool textured_;
			graphics::vbo_array vbos_;
			std::vector<std::vector<GLfloat> > varray_;
			std::vector<std::vector<GLfloat> > tarray_;
			std::vector<std::vector<uint8_t> > carray_;
			std::vector<size_t> vattrib_offsets_;
			std::vector<size_t> tcattrib_offsets_;
			std::vector<size_t> num_vertices_;

			int size_x_;
			int size_y_;
			int size_z_;
			tile_type tiles_;
		};

		void build_colored(shader_data&);
		void build_textured(shader_data&);

		void draw_textured(const shader_data&) const;
		void draw_colored(const shader_data&) const;

		void add_carray_data(const SDL_Color& col, std::vector<uint8_t>& carray);
		void add_tarray_data(int face, const rectf& area, std::vector<GLfloat>& tarray);
		void add_vertex_data(int face, GLfloat x, GLfloat y, GLfloat z, GLfloat size, std::vector<GLfloat>& varray);

		void add_colored_face_left(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_colored_face_right(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_colored_face_front(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_colored_face_back(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_colored_face_top(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_colored_face_bottom(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);

		void add_face_left(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_right(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_front(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_back(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_top(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_bottom(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);

		std::vector<shader_data> shader_data_;

		gles2::program_ptr shader_;
		GLuint u_mvp_matrix_;
		GLuint u_tex0_;
		GLuint a_texcoord_;

		GLuint u_lightposition_;
		GLuint u_lightpower_;
		GLuint u_shininess_;
		GLuint u_m_matrix_;
		GLuint u_v_matrix_;
		GLuint u_normal_;
		GLuint a_position_;
		GLuint a_color_;
		std::vector<glm::vec3> normals_;

		glm::mat4 model_;
	};

	typedef boost::intrusive_ptr<isomap> isomap_ptr;
	typedef boost::intrusive_ptr<const isomap> const_isomap_ptr;

	glm::ivec3 get_facing(const camera_callable_ptr& camera, const glm::vec3& coords);
}

#endif // USE_ISOMAP
