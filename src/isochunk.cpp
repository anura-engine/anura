#if defined(USE_ISOMAP)

#include <boost/lexical_cast.hpp>
#include <boost/shared_array.hpp>
#include <boost/regex.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/algorithm/string.hpp>
#include <limits>
#include <sstream>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>
#if defined(_MSC_VER)
#include <boost/math/special_functions/round.hpp>
#define bmround	boost::math::round
#else
#define bmround	round
#endif

#include "base64.hpp"
#include "compress.hpp"
#include "foreach.hpp"
#include "isochunk.hpp"
#include "json_parser.hpp"
#include "level.hpp"
#include "preferences.hpp"
#include "profile_timer.hpp"
#include "simplex_noise.hpp"
#include "texture.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

namespace isometric
{
	namespace 
	{
		const int debug_draw_faces = chunk::FRONT | chunk::RIGHT | chunk::TOP | chunk::BACK | chunk::LEFT | chunk::BOTTOM;

		boost::random::mt19937 rng(uint32_t(std::time(0)));

		std::vector<tile_editor_info>& get_editor_tile_info()
		{
			static std::vector<tile_editor_info> res;
			return res;
		}

		struct tile_info
		{
			std::string name;
			std::string abbreviation;
			int faces;
			rectf area[6];
			bool transparent;
		};

		class terrain_info
		{
		public:
			terrain_info() {}
			virtual ~terrain_info() {}
			void load(const variant& node)
			{
				ASSERT_LOG(node.has_key("image") && node["image"].is_string(), 
					"terrain info must have 'image' attribute that is a string.");
				tex_ = graphics::texture::get(node["image"].as_string());
				ASSERT_LOG(node.has_key("blocks") && node["blocks"].is_list(),
					"terrain info must have 'blocks' attribute that is a list.");
				for(int i = 0; i != node["blocks"].num_elements(); ++i) {
					const variant& block = node["blocks"][i];
					tile_info ti;
					ti.faces = 0;
					ASSERT_LOG(block.has_key("name") && block["name"].is_string(), 
						"Each block in list must have a 'name' attribute of type string.");
					ti.name = block["name"].as_string();
					ASSERT_LOG(block.has_key("id") && block["id"].is_string(),
						"Each block in list must have an 'id' attribute of type string. Block name: " << ti.name);
					ti.abbreviation = block["id"].as_string();
					if(block.has_key("area")) {
						ASSERT_LOG(block["area"].is_list() && block["area"].num_elements() == 4,
							"Block " << ti.name << " must have an 'area' attribute that is a list of four elements.");
						ti.faces = chunk::FRONT;
						ti.area[0] = rectf(block["area"]);
					} else {
						ASSERT_LOG(block.has_key("front") && block["front"].is_list() && block["front"].num_elements() == 4,
							"Block " << ti.name << " must have an 'front' attribute that is a list of four elements.");
						ti.faces |= chunk::FRONT;
						ti.area[0] = rectf(block["front"]);

						if(block.has_key("right")) {
							ASSERT_LOG(block["right"].is_list() && block["right"].num_elements() == 4,
								"Block " << ti.name << " must have an 'right' attribute that is a list of four elements.");
							ti.faces |= chunk::RIGHT;
							ti.area[1] = rectf(block["right"]);
						}
						if(block.has_key("top")) {
							ASSERT_LOG(block["top"].is_list() && block["top"].num_elements() == 4,
								"Block " << ti.name << " must have an 'top' attribute that is a list of four elements.");
							ti.faces |= chunk::TOP;
							ti.area[2] = rectf(block["top"]);
						}
						if(block.has_key("back")) {
							ASSERT_LOG(block["back"].is_list() && block["back"].num_elements() == 4,
								"Block " << ti.name << " must have an 'back' attribute that is a list of four elements.");
							ti.faces |= chunk::BACK;
							ti.area[3] = rectf(block["back"]);
						}
						if(block.has_key("left")) {
							ASSERT_LOG(block["left"].is_list() && block["left"].num_elements() == 4,
								"Block " << ti.name << " must have an 'left' attribute that is a list of four elements.");
							ti.faces |= chunk::LEFT;
							ti.area[4] = rectf(block["left"]);
						}
						if(block.has_key("bottom")) {
							ASSERT_LOG(block["bottom"].is_list() && block["bottom"].num_elements() == 4,
								"Block " << ti.name << " must have an 'bottom' attribute that is a list of four elements.");
							ti.faces |= chunk::BOTTOM;
							ti.area[5] = rectf(block["bottom"]);
						}
						ti.transparent = block["transparent"].as_bool(false);
					}
					tile_data_[ti.abbreviation] = ti;

					// Set up some data for the editor
					tile_editor_info te;
					te.tex = tex_;
					te.name = ti.name;
					te.id = variant(ti.abbreviation);
					te.group = block.has_key("group") ? block["group"].as_string() : "unspecified";
					te.area = rect::from_coordinates(int(ti.area[0].xf() * tex_.width()), 
						int(ti.area[0].yf() * tex_.height()),
						int(ti.area[0].x2f() * tex_.width()),
						int(ti.area[0].y2f() * tex_.height()));
					get_editor_tile_info().push_back(te);
				}
			}

			std::map<std::string, tile_info>::const_iterator find(const std::string& s)
			{
				return tile_data_.find(s);
			}
			std::map<std::string, tile_info>::const_iterator end()
			{
				return tile_data_.end();
			}
			std::map<std::string, tile_info>::const_iterator random()
			{
				boost::random::uniform_int_distribution<> dist(0, tile_data_.size()-1);
				auto it = tile_data_.begin();
				std::advance(it, dist(rng));
				return it;
			}
			const graphics::texture& get_tex()
			{
				return tex_;
			}
			void clear()
			{
				tile_data_.clear();
				get_editor_tile_info().clear();
			}
		private:
			graphics::texture tex_;
			std::map<std::string, tile_info> tile_data_;
		};
		
		terrain_info& get_terrain_info()
		{
			static terrain_info res;
			return res;
		}
	}

	bool operator==(position const& p1, position const& p2)
	{
		return p1.x == p1.x && p1.y == p2.y && p1.z == p2.z;
	}

	std::size_t hash_value(position const& p)
	{
		std::size_t seed = 0;
		boost::hash_combine(seed, p.x);
		boost::hash_combine(seed, p.y);
		boost::hash_combine(seed, p.z);
		return seed;
	}

	chunk::chunk()
		: u_mvp_matrix_(-1), u_lightposition_(-1), lighting_enabled_(false),
		u_lightpower_(-1), u_shininess_(-1), u_m_matrix_(-1), u_v_matrix_(-1), 
		u_normal_(-1), a_position_(-1), u_gamma_(-1), textured_(true), gamma_(1.0f),
		skip_lighting_(false), worldspace_position_(0.0f)
	{
		// Call init *before* doing anything else
		init();
	}

	chunk::chunk(const variant& node)
		: u_mvp_matrix_(-1), u_lightposition_(-1), lighting_enabled_(false),
		u_lightpower_(-1), u_shininess_(-1), u_m_matrix_(-1), u_v_matrix_(-1), 
		u_normal_(-1), a_position_(-1), u_gamma_(-1), textured_(true), gamma_(1.0f),
		skip_lighting_(node["skip_lighting_uniforms"].as_bool(false)), worldspace_position_(0.0f)
	{
		// Call init *before* doing anything else
		init();

		// using textured or colored data
		bool textured = node.has_key("colored") && node["colored"].as_bool() == true ? false : true;

		// Load shader.
		ASSERT_LOG(node.has_key("shader"), "Must have 'shader' attribute");
		ASSERT_LOG(node["shader"].is_string(), "'shader' attribute must be a string");
		shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();
		get_uniforms_and_attributes();

		if(node.has_key("worldspace_position")) {
			const variant& wp = node["worldspace_position"];
			ASSERT_LOG(wp.is_list() && wp.num_elements() == 3, "'worldspace_position' attribute must be a list of 3 integers");
			worldspace_position_.x = float(wp[0].as_decimal().as_float());
			worldspace_position_.y = float(wp[1].as_decimal().as_float());
			worldspace_position_.z = float(wp[2].as_decimal().as_float());
		}
	}

	void chunk::init()
	{
		vbos_ = boost::shared_array<GLuint>(new GLuint[2], [](GLuint* id) {glDeleteBuffers(2,id); delete id;});
		glGenBuffers(2, &vbos_[0]);

		get_terrain_info().clear();
		get_terrain_info().load(json::parse_from_file("data/terrain.cfg"));

		normals_.clear();
		normals_.push_back(glm::vec3(0,0,1));	// front
		normals_.push_back(glm::vec3(1,0,0));	// right
		normals_.push_back(glm::vec3(0,1,0));	// top
		normals_.push_back(glm::vec3(0,0,-1));	// back
		normals_.push_back(glm::vec3(-1,0,0));	// left
		normals_.push_back(glm::vec3(0,-1,0));	// bottom
	}

	chunk::~chunk()
	{
	}

	void chunk::get_uniforms_and_attributes()
	{
		u_mvp_matrix_ = shader_->get_fixed_uniform("mvp_matrix");
		ASSERT_LOG(u_mvp_matrix_ != -1, "chunk: mvp_matrix_ == -1");
		a_position_ = shader_->get_fixed_attribute("vertex");
		ASSERT_LOG(a_position_ != -1, "chunk: vertex == -1");

		u_lightposition_ = shader_->get_fixed_uniform("light_position");
		u_lightpower_ = shader_->get_fixed_uniform("light_power");
		u_shininess_ = shader_->get_fixed_uniform("shininess");
		u_m_matrix_ = shader_->get_fixed_uniform("m_matrix");
		u_v_matrix_ = shader_->get_fixed_uniform("v_matrix");
		u_normal_ = shader_->get_fixed_uniform("normal");
		u_gamma_ = shader_->get_fixed_uniform("gamma");

		lighting_enabled_ = u_lightposition_ != -1 
			&& u_lightpower_ != -1
			&& u_shininess_ != -1
			&& u_m_matrix_ != -1
			&& u_v_matrix_ != -1
			&& u_normal_ != -1;

		std::cerr << "chunk::get_uniforms_and_attributes Lighting is " << (lighting_enabled_ ? "enabled" : "disabled") << std::endl;
		if(!lighting_enabled_) {
			std::cerr << "light_position: " << u_lightposition_ << std::endl;
			std::cerr << "light_power: " << u_lightpower_ << std::endl;
			std::cerr << "shininess: " << u_shininess_ << std::endl;
			std::cerr << "m_matrix: " << u_m_matrix_ << std::endl;
			std::cerr << "v_matrix: " << u_v_matrix_ << std::endl;
			std::cerr << "normal: " << u_normal_ << std::endl;
		}
	}

	const std::vector<tile_editor_info>& chunk::get_editor_tiles()
	{
		return get_editor_tile_info();
	}

	variant chunk::write()
	{
		variant_builder res;

		res.add("shader", shader_->name());
		res.add("colored", textured() == false);
		res.merge_object(handle_write());
		return res.build();
	}

	void chunk::build()
	{
		varray_.clear();
		vattrib_offsets_.clear();
		num_vertices_.clear();

		varray_.resize(MAX_FACES);
		vattrib_offsets_.resize(MAX_FACES);
		num_vertices_.resize(MAX_FACES);

		handle_build();
	}

	void chunk::add_vertex_data(int face, GLfloat x, GLfloat y, GLfloat z, GLfloat s, std::vector<GLfloat>& varray)
	{
		switch(face) {
		case FRONT_FACE:
			varray.push_back(x); varray.push_back(y); varray.push_back(z+s);
			varray.push_back(x+s); varray.push_back(y); varray.push_back(z+s);
			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z+s);

			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z+s);
			varray.push_back(x); varray.push_back(y+s); varray.push_back(z+s);
			varray.push_back(x); varray.push_back(y); varray.push_back(z+s);
			break;
		case RIGHT_FACE:
			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z+s);
			varray.push_back(x+s); varray.push_back(y); varray.push_back(z+s);
			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z);

			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z);
			varray.push_back(x+s); varray.push_back(y); varray.push_back(z+s);
			varray.push_back(x+s); varray.push_back(y); varray.push_back(z);
			break;
		case TOP_FACE:
			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z+s);
			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z);
			varray.push_back(x); varray.push_back(y+s); varray.push_back(z+s);

			varray.push_back(x); varray.push_back(y+s); varray.push_back(z+s);
			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z);
			varray.push_back(x); varray.push_back(y+s); varray.push_back(z);
			break;
		case BACK_FACE:
			varray.push_back(x+s); varray.push_back(y); varray.push_back(z);
			varray.push_back(x); varray.push_back(y); varray.push_back(z);
			varray.push_back(x); varray.push_back(y+s); varray.push_back(z);

			varray.push_back(x); varray.push_back(y+s); varray.push_back(z);
			varray.push_back(x+s); varray.push_back(y+s); varray.push_back(z);
			varray.push_back(x+s); varray.push_back(y); varray.push_back(z);
			break;
		case LEFT_FACE:
			varray.push_back(x); varray.push_back(y+s); varray.push_back(z+s);
			varray.push_back(x); varray.push_back(y+s); varray.push_back(z);
			varray.push_back(x); varray.push_back(y); varray.push_back(z+s);

			varray.push_back(x); varray.push_back(y); varray.push_back(z+s);
			varray.push_back(x); varray.push_back(y+s); varray.push_back(z);
			varray.push_back(x); varray.push_back(y); varray.push_back(z);
			break;
		case BOTTOM_FACE:
			varray.push_back(x+s); varray.push_back(y); varray.push_back(z+s);
			varray.push_back(x); varray.push_back(y); varray.push_back(z+s);
			varray.push_back(x+s); varray.push_back(y); varray.push_back(z);

			varray.push_back(x+s); varray.push_back(y); varray.push_back(z);
			varray.push_back(x); varray.push_back(y); varray.push_back(z+s);
			varray.push_back(x); varray.push_back(y); varray.push_back(z);
			break;
		default: ASSERT_LOG(false, "isomap::add_vertex_data unexpected facing value: " << face);
		}
	}

	void chunk::add_vertex_vbo_data()
	{
		size_t total_size = 0;
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			vattrib_offsets_[n] = total_size;
			total_size += varray_[n].size() * sizeof(GLfloat);
			num_vertices_[n] = varray_[n].size() / 3;
		}
		glBindBuffer(GL_ARRAY_BUFFER, vbos_[0]);
		glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			glBufferSubData(GL_ARRAY_BUFFER, vattrib_offsets_[n], varray_[n].size()*sizeof(GLfloat), &varray_[n][0]);
		}
	}

	void chunk::draw() const
	{
		glUseProgram(shader_->get());
		glClear(GL_DEPTH_BUFFER_BIT);
		// Cull triangles which normal is not towards the camera
		glEnable(GL_CULL_FACE);
		// Enable depth test
		glEnable(GL_DEPTH_TEST);

		handle_draw();

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
	}

	void chunk::do_draw() const
	{
		// Called by world, assumes everything is already setup.
		handle_draw();
	}

	void chunk::set_gamma(float g) 
	{ 
		gamma_ = std::max(0.001f, std::min(g, 100.0f)); 
	}

	bool chunk::is_xedge(int x) const
	{
		if(x >= 0 && x < size_x()) {
			return false;
		}
		return true;
	}

	bool chunk::is_yedge(int y) const
	{
		if(y >= 0 && y < size_y()) {
			return false;
		}
		return true;
	}

	bool chunk::is_zedge(int z) const
	{
		if(z >= 0 && z < size_z()) {
			return false;
		}
		return true;
	}

	namespace
	{
		variant variant_list_from_xyz(int x, int y, int z)
		{
			std::vector<variant> v;
			v.push_back(variant(x)); v.push_back(variant(y)); v.push_back(variant(z));
			return variant(&v);
		}
	}

	pathfinding::directed_graph_ptr chunk::create_directed_graph(bool allow_diagonals)
	{
		profile::manager pman("isomap::create_directed_graph");

		std::map<std::pair<int,int>, int> vlist;
		std::vector<variant> vertex_list = create_dg_vertex_list(vlist);
	
		pathfinding::graph_edge_list edges;
		for(auto p : vlist) {
			std::vector<variant> current_edges;
			const int x = p.first.first;
			const int z = p.first.second;
			
			auto it = vlist.find(std::make_pair(x+1,z));
			if(it != vlist.end() && !is_xedge(x+1) && !is_solid(x+1,it->second,z)) {
				current_edges.push_back(variant_list_from_xyz(x+1,it->second,z));
			}
			it = vlist.find(std::make_pair(x-1,z));
			if(it != vlist.end() && !is_xedge(x-1) && !is_solid(x-1,it->second,z)) {
				current_edges.push_back(variant_list_from_xyz(x-1,it->second,z));
			}
			it = vlist.find(std::make_pair(x,z+1));
			if(it != vlist.end() && !is_zedge(z+1) && !is_solid(x,it->second,z+1)) {
				current_edges.push_back(variant_list_from_xyz(x,it->second,z+1));
			}
			it = vlist.find(std::make_pair(x,z-1));
			if(it != vlist.end() && !is_zedge(z-1) && !is_solid(x,it->second,z-1)) {
				current_edges.push_back(variant_list_from_xyz(x,it->second,z-1));
			}
			if(allow_diagonals) {
				it = vlist.find(std::make_pair(x+1,z+1));
				if(it != vlist.end() && !is_xedge(x+1) && !is_zedge(z+1) && !is_solid(x+1,it->second,z+1)) {
					current_edges.push_back(variant_list_from_xyz(x+1,it->second,z+1));
				}
				it = vlist.find(std::make_pair(x+1,z-1));
				if(it != vlist.end() && !is_xedge(x+1) && !is_zedge(z-1) && !is_solid(x+1,it->second,z-1)) {
					current_edges.push_back(variant_list_from_xyz(x+1,it->second,z-1));
				}
				it = vlist.find(std::make_pair(x-1,z+1));
				if(it != vlist.end() && !is_xedge(x-1) && !is_zedge(z+1) && !is_solid(x-1,it->second,z+1)) {
					current_edges.push_back(variant_list_from_xyz(x-1,it->second,z+1));
				}
				it = vlist.find(std::make_pair(x-1,z-1));
				if(it != vlist.end() && !is_xedge(x-1) && !is_zedge(z-1) && !is_solid(x-1,it->second,z-1)) {
					current_edges.push_back(variant_list_from_xyz(x-1,it->second,z-1));
				}
			}
			edges[variant_list_from_xyz(p.first.first, p.second, p.first.second)] = current_edges;
		}
		return pathfinding::directed_graph_ptr(new pathfinding::directed_graph(&vertex_list, &edges));
	}

	variant chunk::get_tile_info(const std::string& type)
	{
		return variant(); // -- todo
	}

	void chunk::set_tile(int x, int y, int z, const variant& type)
	{
		handle_set_tile(x,y,z,type);
		build();
	}

	void chunk::del_tile(int x, int y, int z)
	{
		handle_del_tile(x,y,z);
		build();
	}

	void chunk::set_size(int mx, int my, int mz)
	{
		size_x_ = mx;
		size_y_ = my;
		size_z_ = mz;
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(chunk)
	DEFINE_FIELD(gamma, "decimal")
		return variant(obj.gamma());
	DEFINE_SET_FIELD
		obj.set_gamma(float(value.as_decimal().as_float()));
	END_DEFINE_CALLABLE(chunk)


	///////////////////////////////////////////////////////////////
	// Colored chunk functions
	chunk_colored::chunk_colored() : chunk()
	{
	}

	chunk_colored::~chunk_colored()
	{
	}

	chunk_textured::chunk_textured() : chunk()
	{
	}

	chunk_textured::~chunk_textured()
	{
	}

	chunk_colored::chunk_colored(const variant& node) : chunk(node)
	{
		a_color_ = shader()->get_fixed_attribute("color");
		ASSERT_LOG(a_color_ != -1, "chunk_colored: color == -1");	
		
		if(node.has_key("random")) {
			// Load in some random data.
			int size_x = node["random"]["width"].as_int(32);
			int size_y = node["random"]["height"].as_int(32);
			int size_z = node["random"]["depth"].as_int(32);
			set_size(size_x, size_y, size_z);

			int noise_height = node["noise_height"].as_int(size_y);

			uint32_t seed = node["random"]["seed"].as_int(0);
			noise::simplex::init(seed);

			boost::random::uniform_int_distribution<> dist(0,255);
			graphics::color random_color(dist(rng), dist(rng), dist(rng), 255);

			std::vector<float> vec;
			vec.resize(2);
			for(int x = 0; x != size_x; ++x) {
				vec[0] = float(x)/float(size_x);
				for(int z = 0; z != size_z; ++z) {
					vec[1] = float(z)/float(size_z);
					int h = int(noise::simplex::noise2(&vec[0]) * noise_height);
					h = std::max<int>(1, std::min<int>(size_y-1, h));
					for(int y = 0; y != h; ++y) {
						if(node["random"].has_key("type")) {
							tiles_[position(x,y,z)] = graphics::color(node["random"]["type"]).as_sdl_color();
						} else {
							tiles_[position(x,y,z)] = random_color.as_sdl_color();
						}
					}
				}
			}
		} else {
			ASSERT_LOG(node.has_key("voxels"), "'voxels' attribute must exist.");
			ASSERT_LOG(node["voxels"].is_string() || node["voxels"].is_map(), "'voxels' must be a string or map.");

			variant voxels;
			if(node["voxels"].is_string()) {
				std::string decoded = base64::b64decode(node["voxels"].as_string());
				ASSERT_LOG(decoded.empty() == false, "Error decoding voxel data.")
				std::vector<char> decomp = zip::decompress(std::vector<char>(decoded.begin(), decoded.end()));
				try {
					voxels = json::parse(std::string(decomp.begin(), decomp.end()));
				} catch (json::parse_error& e) {
					ASSERT_LOG(false, "Error parsing voxel data: " << e.error_message());
				}
			} else {
				voxels = node["voxels"];
			}
			int min_x, min_y, min_z;
			int max_x, max_y, max_z;
			min_x = min_y = min_z = std::numeric_limits<int>::max();
			max_x = max_y = max_z = std::numeric_limits<int>::min();

			const variant& voxel_keys = voxels.get_keys();
			for(int n = 0; n != voxel_keys.num_elements(); ++n) {
				ASSERT_LOG(voxel_keys[n].is_list() && voxel_keys[n].num_elements() == 3, "keys for voxels must be 3 elment lists.");
				const int x = voxel_keys[n][0].as_int();
				const int y = voxel_keys[n][1].as_int();
				const int z = voxel_keys[n][2].as_int();
				if(min_x > x) { min_x = x; }
				if(max_x < x) { max_x = x; }
				if(min_y > y) { min_y = y; }
				if(max_y < y) { max_y = y; }
				if(min_z > z) { min_z = z; }
				if(max_z < z) { max_z = z; }
				tiles_[position(x,y,z)] = graphics::color(voxels[voxel_keys[n]]).as_sdl_color();
			}
			set_size(max_x - min_x + 1, max_y - min_y + 1, max_z - min_z + 1);
		}

		ASSERT_LOG(tiles_.empty() == false, "ISOMAP: No tiles found");

		build();
	}

	chunk_textured::chunk_textured(const variant& node) : chunk(node)
	{
		a_texcoord_ = shader()->get_fixed_attribute("texcoord");
		ASSERT_LOG(a_texcoord_ != -1, "chunk_colored: texcoord == -1");	
		u_texture_ = shader()->get_fixed_uniform("texture");
		ASSERT_LOG(u_texture_ != -1, "chunk_colored: texture == -1");	
		
		if(node.has_key("random")) {
			// Load in some random data.
			int size_x = node["random"]["width"].as_int(32);
			int size_y = node["random"]["height"].as_int(32);
			int size_z = node["random"]["depth"].as_int(32);
			set_size(size_x, size_y, size_z);

			uint32_t seed = node["random"]["seed"].as_int(0);
			noise::simplex::init(seed);


			boost::random::uniform_int_distribution<> dist(0,255);
			graphics::color random_color(dist(rng), dist(rng), dist(rng), 255);

			std::vector<float> vec;
			vec.resize(2);
			for(int x = 0; x != size_x; ++x) {
				vec[0] = float(x)/float(size_x);
				for(int z = 0; z != size_z; ++z) {
					vec[1] = float(z)/float(size_z);
					int h = int(noise::simplex::noise2(&vec[0]) * size_y);
					h = std::max<int>(1, std::min<int>(size_y-1, h));
					for(int y = 0; y != h; ++y) {
						if(node["random"].has_key("type")) {
								tiles_[position(x,y,z)] = node["random"]["type"].as_string();
						} else {
								tiles_[position(x,y,z)] = get_terrain_info().random()->first;
						}
					}
				}
			}
		} else {
			ASSERT_LOG(node.has_key("voxels"), "'voxels' attribute must exist.");
			ASSERT_LOG(node["voxels"].is_string() || node["voxels"].is_map(), "'voxels' must be a string or map.");

			variant voxels;
			if(node["voxels"].is_string()) {
				std::string decoded = base64::b64decode(node["voxels"].as_string());
				ASSERT_LOG(decoded.empty() == false, "Error decoding voxel data.")
				std::vector<char> decomp = zip::decompress(std::vector<char>(decoded.begin(), decoded.end()));
				try {
					voxels = json::parse(std::string(decomp.begin(), decomp.end()));
				} catch (json::parse_error& e) {
					ASSERT_LOG(false, "Error parsing voxel data: " << e.error_message());
				}
			} else {
				voxels = node["voxels"];
			}
			int min_x, min_y, min_z;
			int max_x, max_y, max_z;
			min_x = min_y = min_z = std::numeric_limits<int>::max();
			max_x = max_y = max_z = std::numeric_limits<int>::min();

			const variant& voxel_keys = voxels.get_keys();
			for(int n = 0; n != voxel_keys.num_elements(); ++n) {
				ASSERT_LOG(voxel_keys[n].is_list() && voxel_keys[n].num_elements() == 3, "keys for voxels must be 3 elment lists.");
				const int x = voxel_keys[n][0].as_int();
				const int y = voxel_keys[n][1].as_int();
				const int z = voxel_keys[n][2].as_int();
				if(min_x > x) { min_x = x; }
				if(max_x < x) { max_x = x; }
				if(min_y > y) { min_y = y; }
				if(max_y < y) { max_y = y; }
				if(min_z > z) { min_z = z; }
				if(max_z < z) { max_z = z; }
				tiles_[position(x,y,z)] = voxels[voxel_keys[n]].as_string();
			}
			set_size(max_x - min_x + 1, max_y - min_y + 1, max_z - min_z + 1);
		}

		ASSERT_LOG(tiles_.empty() == false, "ISOMAP: No tiles found");

		build();
	}
	
	void chunk_colored::handle_build()
	{
		profile::manager pman("chunk_colored::handle_build");

		carray_.clear();
		carray_.resize(MAX_FACES);
		cattrib_offsets_.clear();
		cattrib_offsets_.resize(MAX_FACES);

		for(auto& t : tiles_) {
			int x = t.first.x;
			int y = t.first.y;
			int z = t.first.z;
			GLfloat xf = GLfloat(x);
			GLfloat yf = GLfloat(y);
			GLfloat zf = GLfloat(z);

			if(x > 0) {
				if(is_solid(x-1, y, z) == false) {
					add_face_left(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_left(xf,yf,zf,1,t.second);
			}
			if(x < size_x() - 1) {
				if(is_solid(x+1, y, z) == false) {
					add_face_right(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_right(xf,yf,zf,1,t.second);
			}
			if(y > 0) {
				if(is_solid(x, y-1, z) == false) {
					add_face_bottom(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_bottom(xf,yf,zf,1,t.second);
			}
			if(y < size_y() - 1) {
				if(is_solid(x, y+1, z) == false) {
					add_face_top(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_top(xf,yf,zf,1,t.second);
			}
			if(z > 0) {
				if(is_solid(x, y, z-1) == false) {
					add_face_back(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_back(xf,yf,zf,1,t.second);
			}
			if(z < size_z() - 1) {
				if(is_solid(x, y, z+1) == false) {
					add_face_front(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_front(xf,yf,zf,1,t.second);
			}
		}
		
		add_vertex_vbo_data();

		size_t total_size = 0;
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			cattrib_offsets_[n] = total_size;
			total_size += carray_[n].size() * sizeof(uint8_t);
		}
		glBindBuffer(GL_ARRAY_BUFFER, vbo()[1]);
		glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			glBufferSubData(GL_ARRAY_BUFFER, cattrib_offsets_[n], carray_[n].size()*sizeof(uint8_t), &carray_[n][0]);
		}
		std::cerr << "Built " << carray_[FRONT_FACE].size()/4 << " front faces" << std::endl;
		std::cerr << "Built " << carray_[BACK_FACE].size()/4 << " back faces" << std::endl;
		std::cerr << "Built " << carray_[TOP_FACE].size()/4 << " top faces" << std::endl;
		std::cerr << "Built " << carray_[BOTTOM_FACE].size()/4 << " bottom faces" << std::endl;
		std::cerr << "Built " << carray_[LEFT_FACE].size()/4 << " left faces" << std::endl;
		std::cerr << "Built " << carray_[RIGHT_FACE].size()/4 << " right faces" << std::endl;

		clear_vertex_data();
		carray_.clear();

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void chunk_textured::handle_build()
	{
		profile::manager pman("chunk_textured::handle_build");

		tarray_.clear();
		tarray_.resize(MAX_FACES);
		tattrib_offsets_.clear();
		tattrib_offsets_.resize(MAX_FACES);

		for(auto& t : tiles_) {
			int x = t.first.x;
			int y = t.first.y;
			int z = t.first.z;
			GLfloat xf = GLfloat(x);
			GLfloat yf = GLfloat(y);
			GLfloat zf = GLfloat(z);

			if(x > 0) {
				if(is_solid(x-1, y, z) == false) {
					add_face_left(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_left(xf,yf,zf,1,t.second);
			}
			if(x < size_x() - 1) {
				if(is_solid(x+1, y, z) == false) {
					add_face_right(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_right(xf,yf,zf,1,t.second);
			}
			if(y > 0) {
				if(is_solid(x, y-1, z) == false) {
					add_face_bottom(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_bottom(xf,yf,zf,1,t.second);
			}
			if(y < size_y() - 1) {
				if(is_solid(x, y+1, z) == false) {
					add_face_top(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_top(xf,yf,zf,1,t.second);
			}
			if(z > 0) {
				if(is_solid(x, y, z-1) == false) {
					add_face_back(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_back(xf,yf,zf,1,t.second);
			}
			if(z < size_z() - 1) {
				if(is_solid(x, y, z+1) == false) {
					add_face_front(xf,yf,zf,1,t.second);
				}
			} else {
				add_face_front(xf,yf,zf,1,t.second);
			}
		}
		
		add_vertex_vbo_data();

		size_t total_size = 0;
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			tattrib_offsets_[n] = total_size;
			total_size += tarray_[n].size() * sizeof(GLfloat);
		}
		glBindBuffer(GL_ARRAY_BUFFER, vbo()[1]);
		glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			glBufferSubData(GL_ARRAY_BUFFER, tattrib_offsets_[n], tarray_[n].size()*sizeof(GLfloat), &tarray_[n][0]);
		}
		std::cerr << "Built " << tarray_[FRONT_FACE].size()/2 << " front faces" << std::endl;
		std::cerr << "Built " << tarray_[BACK_FACE].size()/2 << " back faces" << std::endl;
		std::cerr << "Built " << tarray_[TOP_FACE].size()/2 << " top faces" << std::endl;
		std::cerr << "Built " << tarray_[BOTTOM_FACE].size()/2 << " bottom faces" << std::endl;
		std::cerr << "Built " << tarray_[LEFT_FACE].size()/2 << " left faces" << std::endl;
		std::cerr << "Built " << tarray_[RIGHT_FACE].size()/2 << " right faces" << std::endl;

		clear_vertex_data();
		tarray_.clear();

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void chunk_colored::add_carray_data(const SDL_Color& col, std::vector<uint8_t>& carray)
	{
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			carray.push_back(col.r);
			carray.push_back(col.g);
			carray.push_back(col.b);
			carray.push_back(col.a);
		}
	}

	void chunk_textured::add_tarray_data(int face, const rectf& area, std::vector<GLfloat>& tarray)
	{
		switch(face) {
		case FRONT_FACE:
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 

			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
			break;
		case RIGHT_FACE:
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 
		
			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
			break;
		case TOP_FACE:
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
		
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 
			break;
		case BACK_FACE:
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
		
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
			break;
		case LEFT_FACE:
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
		
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
			break;
		case BOTTOM_FACE:
			tarray.push_back(area.x2f()); tarray.push_back(area.y2f()); 
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
		
			tarray.push_back(area.x2f()); tarray.push_back(area.yf()); 
			tarray.push_back(area.xf());  tarray.push_back(area.y2f()); 
			tarray.push_back(area.xf());  tarray.push_back(area.yf()); 
			break;
		default: ASSERT_LOG(false, "isomap::add_vertex_data unexpected facing value: " << face);
		}
	}

	void chunk_colored::add_face_left(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(LEFT_FACE, x, y, z, s, get_vertex_data()[LEFT_FACE]);
		add_carray_data(col, carray_[LEFT_FACE]);
	}

	void chunk_colored::add_face_right(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(RIGHT_FACE, x, y, z, s, get_vertex_data()[RIGHT_FACE]);
		add_carray_data(col, carray_[RIGHT_FACE]);
	}

	void chunk_colored::add_face_front(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(FRONT_FACE, x, y, z, s, get_vertex_data()[FRONT_FACE]);
		add_carray_data(col, carray_[FRONT_FACE]);
	}

	void chunk_colored::add_face_back(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(BACK_FACE, x, y, z, s, get_vertex_data()[BACK_FACE]);
		add_carray_data(col, carray_[BACK_FACE]);
	}

	void chunk_colored::add_face_top(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(TOP_FACE, x, y, z, s, get_vertex_data()[TOP_FACE]);
		add_carray_data(col, carray_[TOP_FACE]);
	}

	void chunk_colored::add_face_bottom(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(BOTTOM_FACE, x, y, z, s, get_vertex_data()[BOTTOM_FACE]);
		add_carray_data(col, carray_[BOTTOM_FACE]);
	}

	void chunk_textured::add_face_left(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(LEFT_FACE, x, y, z, s, get_vertex_data()[LEFT_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_left: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & LEFT ? it->second.area[4] : it->second.area[0];
		add_tarray_data(LEFT_FACE, area, tarray_[LEFT_FACE]);
	}

	void chunk_textured::add_face_right(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(RIGHT_FACE, x, y, z, s, get_vertex_data()[RIGHT_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_right: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & RIGHT ? it->second.area[1] : it->second.area[0];
		add_tarray_data(RIGHT_FACE, area, tarray_[RIGHT_FACE]);
	}

	void chunk_textured::add_face_front(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(FRONT_FACE, x, y, z, s, get_vertex_data()[FRONT_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_front: Unable to find tile type in list: " << bid);
		const rectf area = it->second.area[0];
		add_tarray_data(FRONT_FACE, area, tarray_[FRONT_FACE]);
	}

	void chunk_textured::add_face_back(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(BACK_FACE, x, y, z, s, get_vertex_data()[BACK_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_back: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & BACK ? it->second.area[3] : it->second.area[0];
		add_tarray_data(BACK_FACE, area, tarray_[BACK_FACE]);
	}

	void chunk_textured::add_face_top(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(TOP_FACE, x, y, z, s, get_vertex_data()[TOP_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_top: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & TOP ? it->second.area[2] : it->second.area[0];
		add_tarray_data(TOP_FACE, area, tarray_[TOP_FACE]);
	}

	void chunk_textured::add_face_bottom(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(BOTTOM_FACE, x, y, z, s, get_vertex_data()[BOTTOM_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_bottom: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & BOTTOM ? it->second.area[5] : it->second.area[0];
		add_tarray_data(BOTTOM_FACE, area, tarray_[BOTTOM_FACE]);
	}

	void chunk_colored::handle_draw() const
	{
		ASSERT_LOG(get_vertex_attribute_offsets().size() != 0, "get_vertex_attribute_offsets().size() == 0");
		ASSERT_LOG(cattrib_offsets_.size() != 0, "cattrib_offsets_.size() == 0");

		glm::mat4 model = glm::translate(glm::mat4(1.0f), worldspace_position());
		glm::mat4 mvp = level::current().projection_mat() * level::current().view_mat() * model;
		glUniformMatrix4fv(mvp_uniform(), 1, GL_FALSE, glm::value_ptr(mvp));

		if(lighting_enabled()) {
			if(!skip_lighting()) {
				glUniform3f(light_position_uniform(), size_x()/2.0f, 200.0f, size_z()/2.0f);
				glUniform1f(light_power_uniform(), 15000.0f);
				glUniform1f(gamma_uniform(), gamma());
			}
			glUniform1f(shininess_uniform(), 5.0f);
			glUniformMatrix4fv(v_matrix_uniform(), 1, GL_FALSE, level::current().view());
			glUniformMatrix4fv(m_matrix_uniform(), 1, GL_FALSE, glm::value_ptr(model));
		}

		glEnableVertexAttribArray(position_uniform());
		glEnableVertexAttribArray(a_color_);
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			if(debug_draw_faces & (1 << n)) {
				if(lighting_enabled()) {
					glUniform3fv(normal_uniform(), 1, glm::value_ptr(normals()[n]));
				}
				glBindBuffer(GL_ARRAY_BUFFER, vbo()[0]);
				glVertexAttribPointer(position_uniform(), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLfloat*>(get_vertex_attribute_offsets()[n]));
				glBindBuffer(GL_ARRAY_BUFFER, vbo()[1]);
				glVertexAttribPointer(a_color_, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, reinterpret_cast<const GLfloat*>(cattrib_offsets_[n]));
				glDrawArrays(GL_TRIANGLES, 0, get_num_vertices()[n]);
			}
		}
		glDisableVertexAttribArray(position_uniform());
		glDisableVertexAttribArray(a_color_);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void chunk_textured::handle_draw() const
	{
		ASSERT_LOG(get_vertex_attribute_offsets().size() != 0, "get_vertex_attribute_offsets().size() == 0");
		ASSERT_LOG(tattrib_offsets_.size() != 0, "tattrib_offsets_.size() == 0");

		glActiveTexture(GL_TEXTURE0);
		get_terrain_info().get_tex().set_as_current_texture();
		glUniform1i(u_texture_, 0);

		//glm::mat4 model = glm::translate(glm::translate(glm::mat4(1.0f), glm::vec3(-size_x()/2, -size_y()/2, -size_z()/2)), worldspace_position());
		glm::mat4 model = glm::translate(glm::mat4(1.0f), worldspace_position());
		glm::mat4 mvp = level::current().projection_mat() * level::current().view_mat() * model;
		glUniformMatrix4fv(mvp_uniform(), 1, GL_FALSE, glm::value_ptr(mvp));

		if(lighting_enabled()) {
			if(!skip_lighting()) {
				glUniform3f(light_position_uniform(), size_x()/2.0f, 200.0f, size_z()/2.0f);
				glUniform1f(light_power_uniform(), 15000.0f);
				glUniform1f(gamma_uniform(), gamma());
			}
			glUniform1f(shininess_uniform(), 5.0f);
			glUniformMatrix4fv(v_matrix_uniform(), 1, GL_FALSE, level::current().view());
			glUniformMatrix4fv(m_matrix_uniform(), 1, GL_FALSE, glm::value_ptr(model));
		}

		glEnableVertexAttribArray(position_uniform());
		glEnableVertexAttribArray(a_texcoord_);
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			if(debug_draw_faces & (1 << n)) {
				if(lighting_enabled()) {
					glUniform3fv(normal_uniform(), 1, glm::value_ptr(normals()[n]));
				}
				glBindBuffer(GL_ARRAY_BUFFER, vbo()[0]);
				glVertexAttribPointer(position_uniform(), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLfloat*>(get_vertex_attribute_offsets()[n]));
				glBindBuffer(GL_ARRAY_BUFFER, vbo()[1]);
				glVertexAttribPointer(a_texcoord_, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLfloat*>(tattrib_offsets_[n]));
				glDrawArrays(GL_TRIANGLES, 0, get_num_vertices()[n]);
			}
		}
		glDisableVertexAttribArray(position_uniform());
		glDisableVertexAttribArray(a_texcoord_);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	variant chunk_textured::get_tile_type(int x, int y, int z) const
	{
		auto it = tiles_.find(position(x, y, z));
		if(it == tiles_.end()) {
			return variant();
		}
		return variant(it->second);
	}

	variant chunk_colored::get_tile_type(int x, int y, int z) const
	{
		auto it = tiles_.find(position(x, y, z));
		if(it == tiles_.end()) {
			return variant();
		}
		std::vector<variant> v;
		v.push_back(variant(it->second.r));
		v.push_back(variant(it->second.g));
		v.push_back(variant(it->second.b));
		v.push_back(variant(it->second.a));
		return variant(&v);
	}

	std::vector<variant> chunk_colored::create_dg_vertex_list(std::map<std::pair<int,int>, int>& vlist)
	{
		std::vector<variant> vertex_list;
		for(auto t = tiles_.begin(); t != tiles_.end(); ++t) {
			int x = t->first.x;
			int y = t->first.y;
			int z = t->first.z;

			if(y < size_y() - 1) {
				if(is_solid(x, y+1, z) == false) {
					vertex_list.push_back(variant_list_from_xyz(x,y+1,z));
					vlist[std::make_pair(x,z)] = y+1;
				}
			} else {
				vertex_list.push_back(variant_list_from_xyz(x,y+1,z));
				vlist[std::make_pair(x,z)] = y+1;
			}
		}
		return vertex_list;
	}

	std::vector<variant> chunk_textured::create_dg_vertex_list(std::map<std::pair<int,int>, int>& vlist)
	{
		std::vector<variant> vertex_list;
		for(auto t = tiles_.begin(); t != tiles_.end(); ++t) {
			int x = t->first.x;
			int y = t->first.y;
			int z = t->first.z;

			if(y < size_y() - 1) {
				if(is_solid(x, y+1, z) == false) {
					vertex_list.push_back(variant_list_from_xyz(x,y+1,z));
					vlist[std::make_pair(x,z)] = y+1;
				}
			} else {
				vertex_list.push_back(variant_list_from_xyz(x,y+1,z));
				vlist[std::make_pair(x,z)] = y+1;
			}
		}
		return vertex_list;
	}

	void chunk_colored::handle_set_tile(int x, int y, int z, const variant& type)
	{
		tiles_[position(x,y,z)] = graphics::color(type).as_sdl_color();
	}

	void chunk_colored::handle_del_tile(int x, int y, int z)
	{
		auto it = tiles_.find(position(x,y,z));
		if(it == tiles_.end()) {
			std::cerr << "chunk_colored::handle_del_tile(): No tile at " << x << "," << y << "," << z << " to delete" << std::endl;
		} else {
			tiles_.erase(it);
		}
	}

	void chunk_textured::handle_set_tile(int x, int y, int z, const variant& type)
	{
		tiles_[position(x,y,z)] = type.as_string();
	}

	void chunk_textured::handle_del_tile(int x, int y, int z)
	{
		auto it = tiles_.find(position(x,y,z));
		if(it == tiles_.end()) {
			std::cerr << "chunk_textured::handle_del_tile(): No tile at " << x << "," << y << "," << z << " to delete" << std::endl;
		} else {
			tiles_.erase(it);
		}
	}

	bool chunk_textured::is_solid(int x, int y, int z) const
	{
		auto it = tiles_.find(position(x,y,z));
		if(it != tiles_.end()) {
			if(it->second.empty() == false) {
				auto ti = get_terrain_info().find(it->second);
				ASSERT_LOG(ti != get_terrain_info().end(), "is_solid: Terrain not found: " << it->second);
				return !ti->second.transparent;
			}
		}
		return false;
	}

	bool chunk_colored::is_solid(int x, int y, int z) const
	{
		auto it = tiles_.find(position(x,y,z));
		if(it != tiles_.end()) {
			return it->second.a == 255;
		}
		return false;
	}

	variant chunk_colored::handle_write()
	{
		variant_builder res;
		std::map<variant,variant> vox;
		for(auto t : tiles_) {
			std::vector<variant> v;
			v.push_back(variant(t.first.x));
			v.push_back(variant(t.first.y));
			v.push_back(variant(t.first.z));
			std::vector<variant> rgba;
			rgba.push_back(variant(t.second.r));
			rgba.push_back(variant(t.second.g));
			rgba.push_back(variant(t.second.b));
			rgba.push_back(variant(t.second.a));
			vox[variant(&v)] = variant(&rgba);
		}
		std::string s = variant(&vox).write_json();
		std::vector<char> enc_and_comp(base64::b64encode(zip::compress(std::vector<char>(s.begin(), s.end()))));
		res.add("voxels", std::string(enc_and_comp.begin(), enc_and_comp.end()));
		return res.build();
	}
	
	variant chunk_textured::handle_write()
	{
		variant_builder res;
		std::map<variant,variant> vox;
		for(auto t : tiles_) {
			std::vector<variant> v;
			v.push_back(variant(t.first.x));
			v.push_back(variant(t.first.y));
			v.push_back(variant(t.first.z));
			vox[variant(&v)] = variant(t.second);
		}
		std::string s = variant(&vox).write_json();
		std::vector<char> enc_and_comp(base64::b64encode(zip::compress(std::vector<char>(s.begin(), s.end()))));
		res.add("voxels", std::string(enc_and_comp.begin(), enc_and_comp.end()));
		return res.build();
	}
	

	namespace
	{
		float dti(float val) 
		{
			return abs(val - bmround(val));
		}
	}

	glm::ivec3 get_facing(const camera_callable_ptr& camera, const glm::vec3& coords) 
	{
		ASSERT_LOG(camera != NULL, "get_facing: camera == NULL");
		const glm::vec3& lookat = camera->direction();
		if(dti(coords.x) < dti(coords.y)) {
			if(dti(coords.x) < dti(coords.z)) {
				if(lookat.x > 0) {
					return glm::ivec3(-1,0,0);
				} else {
					return glm::ivec3(1,0,0);
				}
			} else {
				if(lookat.z > 0) {
					return glm::ivec3(0,0,-1);
				} else {
					return glm::ivec3(0,0,1);
				}
			}
		} else {
			if(dti(coords.y) < dti(coords.z)) {
				if(lookat.y > 0) {
					return glm::ivec3(0,-1,0);
				} else {
					return glm::ivec3(0,1,0);
				}
			} else {
				if(lookat.z > 0) {
					return glm::ivec3(0,0,-1);
				} else {
					return glm::ivec3(0,0,1);
				}
			}
		}
	}

	namespace chunk_factory 
	{
		chunk_ptr create(const variant& v)
		{
			if(v.is_callable()) {
				chunk_ptr c = v.try_convert<chunk>();
				ASSERT_LOG(c != NULL, "Error converting chunk from callable.");
				return c;
			}
			ASSERT_LOG(v.has_key("type"), "No 'type' attribute found in definition.");
			const std::string& type = v["type"].as_string();
			if(type == "textured") {
				return chunk_ptr(new chunk_textured(v));
			} else if(type == "colored") {
				return chunk_ptr(new chunk_colored(v));
			} else {
				ASSERT_LOG(true, "Unable to create a chunk of type " << type);
			}
			return chunk_ptr();
		}
	}
}

#endif
