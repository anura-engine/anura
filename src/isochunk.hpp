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

	struct tile_editor_info
	{
		std::string name;
		std::string group;
		variant id;
		graphics::texture tex;
		rect area;
	};

	class chunk : public game_logic::formula_callable
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

		chunk();
		explicit chunk(const variant& node);
		virtual ~chunk();
		
		void init();
		void build();
		void draw() const;
		void do_draw() const;
		variant write();

		virtual bool is_solid(int x, int y, int z) const = 0;
		bool is_xedge(int x) const;
		bool is_yedge(int y) const;
		bool is_zedge(int z) const;
		virtual variant get_tile_type(int x, int y, int z) const = 0;
		static variant get_tile_info(const std::string& type);
		pathfinding::directed_graph_ptr create_directed_graph(bool allow_diagonals=false);

		void set_tile(int x, int y, int z, const variant& type);
		void del_tile(int x, int y, int z);

		bool textured() const { return textured_; }
		int size_x() const { return size_x_; }
		int size_y() const { return size_y_; }
		int size_z() const { return size_z_; }
		void set_size(int mx, int my, int mz);

		float gamma() const { return gamma_; }
		void set_gamma(float g);

		bool lighting_enabled() const { return lighting_enabled_; }
		bool skip_lighting() const { return skip_lighting_; }

		static const std::vector<tile_editor_info>& get_editor_tiles();
	protected:
		enum {
			FRONT_FACE,
			RIGHT_FACE,
			TOP_FACE,
			BACK_FACE,
			LEFT_FACE,
			BOTTOM_FACE,
			MAX_FACES,
		};

		virtual void handle_build() = 0;
		virtual void handle_draw() const = 0;
		virtual void handle_set_tile(int x, int y, int z, const variant& type) = 0;
		virtual void handle_del_tile(int x, int y, int z) = 0;
		virtual variant handle_write() = 0;
		virtual std::vector<variant> create_dg_vertex_list(std::map<std::pair<int,int>, int>& vlist) = 0;

		void add_vertex_data(int face, GLfloat x, GLfloat y, GLfloat z, GLfloat size, std::vector<GLfloat>& varray);
		std::vector<std::vector<GLfloat> >& get_vertex_data() { return varray_; }
		void add_vertex_vbo_data();
		void clear_vertex_data() { varray_.clear(); }
		const graphics::vbo_array& vbo() const { return vbos_; }
		const std::vector<size_t>& get_vertex_attribute_offsets() const { return vattrib_offsets_; }
		const std::vector<size_t>& get_num_vertices() const { return num_vertices_; }
		const std::vector<glm::vec3>& normals() const { return normals_; }

		GLuint mvp_uniform() const { return u_mvp_matrix_; }
		GLuint light_position_uniform() const { return u_lightposition_; }
		GLuint light_power_uniform() const { return u_lightpower_; }
		GLuint shininess_uniform() const { return u_shininess_; }
		GLuint m_matrix_uniform() const { return u_m_matrix_; }
		GLuint v_matrix_uniform() const { return u_v_matrix_; }
		GLuint normal_uniform() const { return u_normal_; }
		GLuint position_uniform() const { return a_position_; }
		GLuint gamma_uniform() const { return u_gamma_; }
		
		gles2::program_ptr shader() { return shader_; }
		const glm::vec3& worldspace_position() const {return worldspace_position_; }
	private:
		DECLARE_CALLABLE(chunk);

		// Is this a coloured or textured chunk
		bool textured_;
		// VBO's to draw the chunk.
		graphics::vbo_array vbos_;
		// Vertex array data for the chunk
		std::vector<std::vector<GLfloat> > varray_;
		// Vertex attribute offsets
		std::vector<size_t> vattrib_offsets_;
		// Number of vertices to be drawn.
		std::vector<size_t> num_vertices_;

		bool lighting_enabled_;
		bool skip_lighting_;

		int size_x_;
		int size_y_;
		int size_z_;

		float gamma_;

		void get_uniforms_and_attributes();
		gles2::program_ptr shader_;
		GLuint u_mvp_matrix_;
		GLuint u_lightposition_;
		GLuint u_lightpower_;
		GLuint u_shininess_;
		GLuint u_m_matrix_;
		GLuint u_v_matrix_;
		GLuint u_normal_;
		GLuint a_position_;
		GLuint u_gamma_;
		std::vector<glm::vec3> normals_;

		glm::vec3 worldspace_position_;
	};

	class chunk_colored : public chunk
	{
	public:
		chunk_colored();
		explicit chunk_colored(const variant& node);
		virtual ~chunk_colored();
		bool is_solid(int x, int y, int z) const;
		variant get_tile_type(int x, int y, int z) const;
	protected:
		void handle_build();
		void handle_draw() const;
		variant handle_write();
		void handle_set_tile(int x, int y, int z, const variant& type);
		void handle_del_tile(int x, int y, int z);
		std::vector<variant> create_dg_vertex_list(std::map<std::pair<int,int>, int>& vlist);
	private:
		void add_face_left(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_face_right(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_face_front(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_face_back(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_face_top(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);
		void add_face_bottom(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const SDL_Color& col);

		void add_carray_data(const SDL_Color& col, std::vector<uint8_t>& carray);

		std::vector<std::vector<uint8_t> > carray_;
		std::vector<size_t> cattrib_offsets_;
		boost::unordered_map<position, SDL_Color> tiles_;

		GLuint a_color_;
	};

	class chunk_textured : public chunk
	{
	public:
		chunk_textured();
		explicit chunk_textured(const variant& node);
		virtual ~chunk_textured();
		bool is_solid(int x, int y, int z) const;
		variant get_tile_type(int x, int y, int z) const;
	protected:
		void handle_build();
		void handle_draw() const;
		variant handle_write();
		void handle_set_tile(int x, int y, int z, const variant& type);
		void handle_del_tile(int x, int y, int z);
		std::vector<variant> create_dg_vertex_list(std::map<std::pair<int,int>, int>& vlist);
	private:
		void add_face_left(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_right(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_front(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_back(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_top(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_bottom(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);

		void add_tarray_data(int face, const rectf& area, std::vector<GLfloat>& tarray);

		std::vector<std::vector<GLfloat> > tarray_;
		std::vector<size_t> tattrib_offsets_;
		boost::unordered_map<position, std::string> tiles_;

		GLuint u_texture_;
		GLuint a_texcoord_;
	};

	typedef boost::intrusive_ptr<chunk> chunk_ptr;
	typedef boost::intrusive_ptr<const chunk> const_chunk_ptr;

	namespace chunk_factory 
	{
		chunk_ptr create(const variant& v);
	}

	glm::ivec3 get_facing(const camera_callable_ptr& camera, const glm::vec3& coords);
}

#endif // USE_ISOMAP
