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
#include "lighting.hpp"
#include "pathfinding.hpp"
#include "raster.hpp"
#include "shaders.hpp"
#include "variant.hpp"

namespace voxel
{
	struct position
	{
		position(int xx, int yy, int zz) : x(xx), y(yy), z(zz) {}
		int x, y, z;
	};
	bool operator==(position const& p1, position const& p2);
	std::size_t hash_value(position const& p);

	struct textured_tile_editor_info
	{
		std::string name;
		std::string group;
		variant id;
		graphics::texture tex;
		rect area;
	};

	struct colored_tile_editor_info
	{
		std::string name;
		std::string group;
		variant id;
		graphics::color color;
	};

	class logical_world;
	typedef boost::intrusive_ptr<logical_world> logical_world_ptr;

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
		explicit chunk(gles2::program_ptr shader, logical_world_ptr logic, const variant& node);
		virtual ~chunk();
		
		void init();
		void build();
		void draw(const graphics::lighting_ptr lighting, const camera_callable_ptr& camera) const;
		variant write();

		virtual bool is_solid(int x, int y, int z) const = 0;
		virtual variant get_tile_type(int x, int y, int z) const = 0;
		static variant get_tile_info(const std::string& type);

		void set_tile(int x, int y, int z, const variant& type);
		void del_tile(int x, int y, int z);

		bool textured() const { return textured_; }
		int size_x() const { return size_x_; }
		int size_y() const { return size_y_; }
		int size_z() const { return size_z_; }
		void set_size(int mx, int my, int mz);

		size_t scale_x() const { return scale_x_; }
		size_t scale_y() const { return scale_y_; }
		size_t scale_z() const { return scale_z_; }

		static const std::vector<textured_tile_editor_info>& get_textured_editor_tiles();
		static const std::vector<colored_tile_editor_info>& get_colored_editor_tiles();
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
		virtual void handle_draw(const graphics::lighting_ptr lighting, const camera_callable_ptr& camera) const = 0;
		virtual void handle_set_tile(int x, int y, int z, const variant& type) = 0;
		virtual void handle_del_tile(int x, int y, int z) = 0;
		virtual variant handle_write() = 0;

		void add_vertex_data(int face, GLfloat x, GLfloat y, GLfloat z, GLfloat size, std::vector<GLfloat>& varray);
		std::vector<std::vector<GLfloat> >& get_vertex_data() { return varray_; }
		void add_vertex_vbo_data();
		void clear_vertex_data() { varray_.clear(); }
		const graphics::vbo_array& vbo() const { return vbos_; }
		const std::vector<size_t>& get_vertex_attribute_offsets() const { return vattrib_offsets_; }
		const std::vector<size_t>& get_num_vertices() const { return num_vertices_; }
		const std::vector<glm::vec3>& normals() const { return normals_; }

		GLuint mvp_uniform() const { return u_mvp_matrix_; }
		GLuint normal_uniform() const { return u_normal_; }
		GLuint position_uniform() const { return a_position_; }
		
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

		int size_x_;
		int size_y_;
		int size_z_;

		size_t scale_x_;
		size_t scale_y_;
		size_t scale_z_;

		void get_uniforms_and_attributes(gles2::program_ptr shader);
		GLuint u_mvp_matrix_;
		GLuint u_normal_;
		GLuint a_position_;
		std::vector<glm::vec3> normals_;

		glm::vec3 worldspace_position_;
	};

	class chunk_colored : public chunk
	{
	public:
		chunk_colored();
		explicit chunk_colored(gles2::program_ptr shader, logical_world_ptr logic, const variant& node);
		virtual ~chunk_colored();
		bool is_solid(int x, int y, int z) const;
		variant get_tile_type(int x, int y, int z) const;
	protected:
		void handle_build();
		void handle_draw(const graphics::lighting_ptr lighting, const camera_callable_ptr& camera) const;
		variant handle_write();
		void handle_set_tile(int x, int y, int z, const variant& type);
		void handle_del_tile(int x, int y, int z);
	private:
		void add_face_left(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const variant& col);
		void add_face_right(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const variant& col);
		void add_face_front(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const variant& col);
		void add_face_back(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const variant& col);
		void add_face_top(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const variant& col);
		void add_face_bottom(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const variant& col);

		void add_carray_data(int face, const graphics::color& color, std::vector<uint8_t>& carray);

		std::vector<std::vector<uint8_t> > carray_;
		std::vector<size_t> cattrib_offsets_;
		boost::unordered_map<position, variant> tiles_;

		GLuint a_color_;
	};

	class chunk_textured : public chunk
	{
	public:
		chunk_textured();
		explicit chunk_textured(gles2::program_ptr shader, logical_world_ptr logic, const variant& node);
		virtual ~chunk_textured();
		bool is_solid(int x, int y, int z) const;
		variant get_tile_type(int x, int y, int z) const;
	protected:
		void handle_build();
		void handle_draw(const graphics::lighting_ptr lighting, const camera_callable_ptr& camera) const;
		variant handle_write();
		void handle_set_tile(int x, int y, int z, const variant& type);
		void handle_del_tile(int x, int y, int z);
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
		chunk_ptr create(gles2::program_ptr shader, logical_world_ptr logic, const variant& v);
	}
}

#endif // USE_ISOMAP
