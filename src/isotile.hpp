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

	typedef boost::unordered_map<position, std::string> tile_type;

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
		void build();
		void rebuild();
		virtual void draw() const;
		variant write();

		bool is_solid(int x, int y, int z) const;
		bool is_xedge(int x) const;
		bool is_yedge(int y) const;
		bool is_zedge(int z) const;
		std::string get_tile_type(int x, int y, int z) const;
		static variant get_tile_info(const std::string& type);
		pathfinding::directed_graph_ptr create_directed_graph(bool allow_diagonals=false);

		void set_tile(int x, int y, int z, const std::string& type);
		void del_tile(int x, int y, int z);

		static const std::vector<tile_editor_info>& get_editor_tiles();
	protected:
		const GLfloat* model() const { return glm::value_ptr(model_); }

		void add_face_left(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_right(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_front(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_back(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_top(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
		void add_face_bottom(GLfloat x, GLfloat y, GLfloat z, GLfloat size, const std::string& bid);
	private:
		DECLARE_CALLABLE(isomap);

		tile_type tiles_;
		int size_x_;
		int size_y_;
		int size_z_;
		graphics::vbo_array arrays_;

		std::vector<GLfloat> vertices_left_;
		std::vector<GLfloat> vertices_right_;
		std::vector<GLfloat> vertices_top_;
		std::vector<GLfloat> vertices_bottom_;
		std::vector<GLfloat> vertices_front_;
		std::vector<GLfloat> vertices_back_;

		std::vector<GLfloat> tarray_left_;
		std::vector<GLfloat> tarray_right_;
		std::vector<GLfloat> tarray_top_;
		std::vector<GLfloat> tarray_bottom_;
		std::vector<GLfloat> tarray_front_;
		std::vector<GLfloat> tarray_back_;

		gles2::program_ptr shader_;
		gles2::actives_map_iterator mm_uniform_it_;
		gles2::actives_map_iterator pm_uniform_it_;
		gles2::actives_map_iterator vm_uniform_it_;
		gles2::actives_map_iterator a_position_it_;
		gles2::actives_map_iterator a_tex_coord_it_;
		gles2::actives_map_iterator tex0_it_;

		glm::mat4 model_;
	};

	typedef boost::intrusive_ptr<isomap> isomap_ptr;
	typedef boost::intrusive_ptr<const isomap> const_isomap_ptr;

	glm::ivec3 get_facing(const glm::vec3& coords);
}

#endif // USE_ISOMAP
