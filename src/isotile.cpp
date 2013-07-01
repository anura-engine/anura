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
#include "isotile.hpp"
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
		const int num_array_buffers = 12;

		const int debug_draw_faces = isomap::FRONT | isomap::RIGHT | isomap::TOP | isomap::BACK | isomap::LEFT | isomap::BOTTOM;
		//const int debug_draw_faces = isomap::FRONT;

		boost::random::mt19937 rng(std::time(0));

		boost::shared_array<GLuint>& tile_array_buffer()
		{
			static graphics::vbo_array res;
			if(res == NULL) {
				res = graphics::vbo_array(new GLuint[num_array_buffers], graphics::vbo_deleter(num_array_buffers));
				glGenBuffers(num_array_buffers, &res[0]);
			}
			return res;
		}

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
						ti.faces = isomap::FRONT;
						ti.area[0] = rectf(block["area"]);
					} else {
						ASSERT_LOG(block.has_key("front") && block["front"].is_list() && block["front"].num_elements() == 4,
							"Block " << ti.name << " must have an 'front' attribute that is a list of four elements.");
						ti.faces |= isomap::FRONT;
						ti.area[0] = rectf(block["front"]);

						if(block.has_key("right")) {
							ASSERT_LOG(block["right"].is_list() && block["right"].num_elements() == 4,
								"Block " << ti.name << " must have an 'right' attribute that is a list of four elements.");
							ti.faces |= isomap::RIGHT;
							ti.area[1] = rectf(block["right"]);
						}
						if(block.has_key("top")) {
							ASSERT_LOG(block["top"].is_list() && block["top"].num_elements() == 4,
								"Block " << ti.name << " must have an 'top' attribute that is a list of four elements.");
							ti.faces |= isomap::TOP;
							ti.area[2] = rectf(block["top"]);
						}
						if(block.has_key("back")) {
							ASSERT_LOG(block["back"].is_list() && block["back"].num_elements() == 4,
								"Block " << ti.name << " must have an 'back' attribute that is a list of four elements.");
							ti.faces |= isomap::BACK;
							ti.area[3] = rectf(block["back"]);
						}
						if(block.has_key("left")) {
							ASSERT_LOG(block["left"].is_list() && block["left"].num_elements() == 4,
								"Block " << ti.name << " must have an 'left' attribute that is a list of four elements.");
							ti.faces |= isomap::LEFT;
							ti.area[4] = rectf(block["left"]);
						}
						if(block.has_key("bottom")) {
							ASSERT_LOG(block["bottom"].is_list() && block["bottom"].num_elements() == 4,
								"Block " << ti.name << " must have an 'bottom' attribute that is a list of four elements.");
							ti.faces |= isomap::BOTTOM;
							ti.area[5] = rectf(block["bottom"]);
						}
						ti.transparent = block["transparent"].as_bool(false);
					}
					tile_data_[ti.abbreviation] = ti;

					// Set up some data for the editor
					tile_editor_info te;
					te.tex = tex_;
					te.name = ti.name;
					te.id = ti.abbreviation;
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

	isomap::isomap()
	{
		arrays_ = tile_array_buffer();
		get_terrain_info().clear();
		get_terrain_info().load(json::parse_from_file("data/terrain.cfg"));
	}

	isomap::isomap(variant node)
	{
		arrays_ = tile_array_buffer();

		get_terrain_info().clear();
		get_terrain_info().load(json::parse_from_file("data/terrain.cfg"));

		if(node.has_key("random")) {
			// Load in some random data.
			size_x_ = node["random"]["width"].as_int(32);
			size_y_ = node["random"]["height"].as_int(32);
			size_z_ = node["random"]["depth"].as_int(32);

			uint32_t seed = node["random"]["seed"].as_int(0);
			noise::simplex::init(seed);

			std::vector<float> vec;
			vec.resize(2);
			for(int x = 0; x != size_x_; ++x) {
				vec[0] = float(x)/float(size_x_);
				for(int z = 0; z != size_z_; ++z) {
					vec[1] = float(z)/float(size_z_);
					int h = int(noise::simplex::noise2(&vec[0]) * size_y_);
					h = std::max<int>(1, std::min<int>(size_y_-1, h));
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
			ASSERT_LOG(node.has_key("voxels") && node["voxels"].is_string(), "'voxels' attribute must be a string.");
			std::string decoded = base64::b64decode(node["voxels"].as_string());
			std::string voxels;
			if(!decoded.empty()) {
				std::vector<char> decomp = zip::decompress(std::vector<char>(decoded.begin(), decoded.end()));
				voxels = std::string(decomp.begin(), decomp.end());
			} else {
				voxels = node["voxels"].as_string();
			}
			int min_x, min_y, min_z;
			int max_x, max_y, max_z;
			min_x = min_y = min_z = std::numeric_limits<int>::max();
			max_x = max_y = max_z = std::numeric_limits<int>::min();
			std::vector<std::string> vlist;
			boost::split(vlist, voxels, boost::is_any_of("\t\n \r;:"));
			const boost::regex re("(-?\\d+),(-?\\d+),(-?\\d+),(\\w+)");
			foreach(auto s, vlist) {
				boost::match_results<std::string::const_iterator> m;
				if(s.empty() == false) {
					if(boost::regex_match(s, m, re)) {
						const int x = boost::lexical_cast<int>(std::string(m[1].first, m[1].second));
						const int y = boost::lexical_cast<int>(std::string(m[2].first, m[2].second));
						const int z = boost::lexical_cast<int>(std::string(m[3].first, m[3].second));
						if(min_x > x) { min_x = x; }
						if(max_x < x) { max_x = x; }
						if(min_y > y) { min_y = y; }
						if(max_y < y) { max_y = y; }
						if(min_z > z) { min_z = z; }
						if(max_z < z) { max_z = z; }
						tiles_[position(x,y,z)] = std::string(m[4].first, m[4].second);
					} else {
						std::cerr << "ISOMAP: Rejected voxel description: " << s << std::endl;
					}
				}
			}
			size_x_ = max_x - min_x + 1;
			size_y_ = max_y - min_y + 1;
			size_z_ = max_z - min_z + 1;
			std::cerr << "isomap: size_x( " << size_x_ << "), size_y(" << size_y_ << "), size_z(" << size_z_ << ")" << std::endl;
		}

		// Load shader.
		ASSERT_LOG(node.has_key("shader"), "Must have 'shader' attribute");
		if(node["shader"].is_map()) {
			ASSERT_LOG(node["shader"].has_key("vertex") && node["shader"].has_key("fragment"),
				"Must have 'shader' attribute with 'vertex' and 'fragment' child attributes.");
			gles2::shader v1(GL_VERTEX_SHADER, "iso_vertex_shader", node["shader"]["vertex"].as_string());
			gles2::shader f1(GL_FRAGMENT_SHADER, "iso_fragment_shader", node["shader"]["fragment"].as_string());
			shader_.reset(new gles2::program(node["shader"]["name"].as_string(), v1, f1));
		} else {
			ASSERT_LOG(node["shader"].is_string(), "'shader' attribute must be string or map");
			shader_ = gles2::program::find_program(node["shader"].as_string());
		}

		if(tiles_.empty()) {
			std::cerr << "ISOMAP: No tiles found, this is probably an error" << std::endl;
		} else {
			build();
		}
	}

	isomap::~isomap()
	{
	}

	const std::vector<tile_editor_info>& isomap::get_editor_tiles()
	{
		return get_editor_tile_info();
	}

	variant isomap::write()
	{
		variant_builder res;

		std::string s;
		for(auto t = tiles_.begin(); t != tiles_.end(); ++t) {
			int x = t->first.x;
			int y = t->first.y;
			int z = t->first.z;
			std::stringstream str;
			str << x << "," << y << "," << z << "," << t->second << " ";
			s += str.str();
		}
		std::vector<char> enc_and_comp(base64::b64encode(zip::compress(std::vector<char>(s.begin(), s.end()))));
		res.add("voxels", std::string(enc_and_comp.begin(), enc_and_comp.end()));

		variant_builder shader;
		shader.add("name", shader_->name());
		shader.add("vertex", shader_->vertex_shader().code());
		shader.add("fragment", shader_->fragment_shader().code());
		res.add("shader", shader.build());
		
		return res.build();
	}

	bool isomap::is_solid(int x, int y, int z) const
	{
		auto it = tiles_.find(position(x, y, z));
		if(it == tiles_.end()) {
			return false;
		}
		if(it->second.empty() != false) {
			return false;
		}
		auto ti = get_terrain_info().find(it->second);
		ASSERT_LOG(ti != get_terrain_info().end(), "is_solid: Terrain not found: " << it->second);
		return !ti->second.transparent;
	}

	void isomap::rebuild()
	{
		vertices_left_.clear();
		vertices_right_.clear();
		vertices_top_.clear();
		vertices_bottom_.clear();
		vertices_front_.clear();
		vertices_back_.clear();

		tarray_left_.clear();
		tarray_right_.clear();
		tarray_top_.clear();
		tarray_bottom_.clear();
		tarray_front_.clear();
		tarray_back_.clear();

		build();
	}

	void isomap::build()
	{
		profile::manager pman("isomap::build");

		for(auto t = tiles_.begin(); t != tiles_.end(); ++t) {
			int x = t->first.x;
			int y = t->first.y;
			int z = t->first.z;
			if(x > 0) {
				if(is_solid(x-1, y, z) == false) {
					add_face_left(x,y,z,1,t->second);
				}
			} else {
				add_face_left(x,y,z,1,t->second);
			}
			if(x < size_x_ - 1) {
				if(is_solid(x+1, y, z) == false) {
					add_face_right(x,y,z,1,t->second);
				}
			} else {
				add_face_right(x,y,z,1,t->second);
			}
			if(y > 0) {
				if(is_solid(x, y-1, z) == false) {
					add_face_bottom(x,y,z,1,t->second);
				}
			} else {
				add_face_bottom(x,y,z,1,t->second);
			}
			if(y < size_y_ - 1) {
				if(is_solid(x, y+1, z) == false) {
					add_face_top(x,y,z,1,t->second);
				}
			} else {
				add_face_top(x,y,z,1,t->second);
			}
			if(z > 0) {
				if(is_solid(x, y, z-1) == false) {
					add_face_back(x,y,z,1,t->second);
				}
			} else {
				add_face_back(x,y,z,1,t->second);
			}
			if(z < size_z_ - 1) {
				if(is_solid(x, y, z+1) == false) {
					add_face_front(x,y,z,1,t->second);
				}
			} else {
				add_face_front(x,y,z,1,t->second);
			}
		}
		
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[0]);
		glBufferData(GL_ARRAY_BUFFER, vertices_left_.size()*sizeof(GLfloat), &vertices_left_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[1]);
		glBufferData(GL_ARRAY_BUFFER, vertices_right_.size()*sizeof(GLfloat), &vertices_right_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[2]);
		glBufferData(GL_ARRAY_BUFFER, vertices_top_.size()*sizeof(GLfloat), &vertices_top_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[3]);
		glBufferData(GL_ARRAY_BUFFER, vertices_bottom_.size()*sizeof(GLfloat), &vertices_bottom_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[4]);
		glBufferData(GL_ARRAY_BUFFER, vertices_front_.size()*sizeof(GLfloat), &vertices_front_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[5]);
		glBufferData(GL_ARRAY_BUFFER, vertices_back_.size()*sizeof(GLfloat), &vertices_back_[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, arrays_[6]);
		glBufferData(GL_ARRAY_BUFFER, tarray_left_.size()*sizeof(GLfloat), &tarray_left_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[7]);
		glBufferData(GL_ARRAY_BUFFER, tarray_right_.size()*sizeof(GLfloat), &tarray_right_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[8]);
		glBufferData(GL_ARRAY_BUFFER, tarray_top_.size()*sizeof(GLfloat), &tarray_top_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[9]);
		glBufferData(GL_ARRAY_BUFFER, tarray_bottom_.size()*sizeof(GLfloat), &tarray_bottom_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[10]);
		glBufferData(GL_ARRAY_BUFFER, tarray_front_.size()*sizeof(GLfloat), &tarray_front_[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, arrays_[11]);
		glBufferData(GL_ARRAY_BUFFER, tarray_back_.size()*sizeof(GLfloat), &tarray_back_[0], GL_STATIC_DRAW);

		std::cerr << "Built " << vertices_left_.size()/3 << " left vertices" << std::endl;
		std::cerr << "Built " << vertices_right_.size()/3 << " right vertices" << std::endl;
		std::cerr << "Built " << vertices_top_.size()/3 << " top vertices" << std::endl;
		std::cerr << "Built " << vertices_bottom_.size()/3 << " bottom vertices" << std::endl;
		std::cerr << "Built " << vertices_front_.size()/3 << " front vertices" << std::endl;
		std::cerr << "Built " << vertices_back_.size()/3 << " back vertices" << std::endl;

		//mm_uniform_it_ = shader_->get_uniform_reference("mvp_matrix");
		mm_uniform_it_ = shader_->get_uniform_reference("MVP");
		//a_position_it_ = shader_->get_attribute_reference("a_position");
		a_position_it_ = shader_->get_attribute_reference("vertexPosition_modelspace");
		a_tex_coord_it_ = shader_->get_attribute_reference("a_tex_coord");
		tex0_it_ = shader_->get_uniform_reference("u_tex0");

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void isomap::add_face_left(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		vertices_left_.push_back(x); vertices_left_.push_back(y+s); vertices_left_.push_back(z+s);
		vertices_left_.push_back(x); vertices_left_.push_back(y+s); vertices_left_.push_back(z);
		vertices_left_.push_back(x); vertices_left_.push_back(y); vertices_left_.push_back(z+s);

		vertices_left_.push_back(x); vertices_left_.push_back(y); vertices_left_.push_back(z+s);
		vertices_left_.push_back(x); vertices_left_.push_back(y+s); vertices_left_.push_back(z);
		vertices_left_.push_back(x); vertices_left_.push_back(y); vertices_left_.push_back(z);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_left: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & LEFT ? it->second.area[4] : it->second.area[0];
		tarray_left_.push_back(area.x2f()); tarray_left_.push_back(area.yf()); 
		tarray_left_.push_back(area.xf());  tarray_left_.push_back(area.yf()); 
		tarray_left_.push_back(area.x2f());  tarray_left_.push_back(area.y2f()); 
		
		tarray_left_.push_back(area.x2f());  tarray_left_.push_back(area.y2f()); 
		tarray_left_.push_back(area.xf()); tarray_left_.push_back(area.yf()); 
		tarray_left_.push_back(area.xf()); tarray_left_.push_back(area.y2f()); 
	}

	void isomap::add_face_right(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		vertices_right_.push_back(x+s); vertices_right_.push_back(y+s); vertices_right_.push_back(z+s);
		vertices_right_.push_back(x+s); vertices_right_.push_back(y); vertices_right_.push_back(z+s);
		vertices_right_.push_back(x+s); vertices_right_.push_back(y+s); vertices_right_.push_back(z);

		vertices_right_.push_back(x+s); vertices_right_.push_back(y+s); vertices_right_.push_back(z);
		vertices_right_.push_back(x+s); vertices_right_.push_back(y); vertices_right_.push_back(z+s);
		vertices_right_.push_back(x+s); vertices_right_.push_back(y); vertices_right_.push_back(z);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_right: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & RIGHT ? it->second.area[1] : it->second.area[0];
		tarray_right_.push_back(area.x2f()); tarray_right_.push_back(area.yf()); 
		tarray_right_.push_back(area.x2f());  tarray_right_.push_back(area.y2f()); 
		tarray_right_.push_back(area.xf());  tarray_right_.push_back(area.yf()); 
		
		tarray_right_.push_back(area.xf());  tarray_right_.push_back(area.yf()); 
		tarray_right_.push_back(area.x2f()); tarray_right_.push_back(area.y2f()); 
		tarray_right_.push_back(area.xf()); tarray_right_.push_back(area.y2f()); 
	}

	void isomap::add_face_front(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		vertices_front_.push_back(x); vertices_front_.push_back(y); vertices_front_.push_back(z+s);
		vertices_front_.push_back(x+s); vertices_front_.push_back(y); vertices_front_.push_back(z+s);
		vertices_front_.push_back(x+s); vertices_front_.push_back(y+s); vertices_front_.push_back(z+s);

		vertices_front_.push_back(x+s); vertices_front_.push_back(y+s); vertices_front_.push_back(z+s);
		vertices_front_.push_back(x); vertices_front_.push_back(y+s); vertices_front_.push_back(z+s);
		vertices_front_.push_back(x); vertices_front_.push_back(y); vertices_front_.push_back(z+s);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_front: Unable to find tile type in list: " << bid);
		const rectf area = it->second.area[0];
		tarray_front_.push_back(area.x2f());  tarray_front_.push_back(area.y2f()); 
		tarray_front_.push_back(area.xf()); tarray_front_.push_back(area.y2f()); 
		tarray_front_.push_back(area.xf()); tarray_front_.push_back(area.yf()); 

		tarray_front_.push_back(area.xf());  tarray_front_.push_back(area.yf()); 
		tarray_front_.push_back(area.x2f()); tarray_front_.push_back(area.yf()); 
		tarray_front_.push_back(area.x2f());  tarray_front_.push_back(area.y2f()); 
	}

	void isomap::add_face_back(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		vertices_back_.push_back(x+s); vertices_back_.push_back(y); vertices_back_.push_back(z);
		vertices_back_.push_back(x); vertices_back_.push_back(y); vertices_back_.push_back(z);
		vertices_back_.push_back(x); vertices_back_.push_back(y+s); vertices_back_.push_back(z);

		vertices_back_.push_back(x); vertices_back_.push_back(y+s); vertices_back_.push_back(z);
		vertices_back_.push_back(x+s); vertices_back_.push_back(y+s); vertices_back_.push_back(z);
		vertices_back_.push_back(x+s); vertices_back_.push_back(y); vertices_back_.push_back(z);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_back: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & BACK ? it->second.area[3] : it->second.area[0];
		tarray_back_.push_back(area.xf()); tarray_back_.push_back(area.y2f()); 
		tarray_back_.push_back(area.x2f());  tarray_back_.push_back(area.y2f()); 
		tarray_back_.push_back(area.x2f());  tarray_back_.push_back(area.yf()); 
		
		tarray_back_.push_back(area.x2f());  tarray_back_.push_back(area.yf()); 
		tarray_back_.push_back(area.xf()); tarray_back_.push_back(area.yf()); 
		tarray_back_.push_back(area.xf()); tarray_back_.push_back(area.y2f()); 
	}

	void isomap::add_face_top(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		vertices_top_.push_back(x+s); vertices_top_.push_back(y+s); vertices_top_.push_back(z+s);
		vertices_top_.push_back(x+s); vertices_top_.push_back(y+s); vertices_top_.push_back(z);
		vertices_top_.push_back(x); vertices_top_.push_back(y+s); vertices_top_.push_back(z+s);

		vertices_top_.push_back(x); vertices_top_.push_back(y+s); vertices_top_.push_back(z+s);
		vertices_top_.push_back(x+s); vertices_top_.push_back(y+s); vertices_top_.push_back(z);
		vertices_top_.push_back(x); vertices_top_.push_back(y+s); vertices_top_.push_back(z);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_top: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & TOP ? it->second.area[2] : it->second.area[0];
		tarray_top_.push_back(area.x2f()); tarray_top_.push_back(area.y2f()); 
		tarray_top_.push_back(area.x2f());  tarray_top_.push_back(area.yf()); 
		tarray_top_.push_back(area.xf());  tarray_top_.push_back(area.y2f()); 
		
		tarray_top_.push_back(area.xf());  tarray_top_.push_back(area.y2f()); 
		tarray_top_.push_back(area.x2f()); tarray_top_.push_back(area.yf()); 
		tarray_top_.push_back(area.xf()); tarray_top_.push_back(area.yf()); 
	}

	void isomap::add_face_bottom(GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		vertices_bottom_.push_back(x+s); vertices_bottom_.push_back(y); vertices_bottom_.push_back(z+s);
		vertices_bottom_.push_back(x); vertices_bottom_.push_back(y); vertices_bottom_.push_back(z+s);
		vertices_bottom_.push_back(x+s); vertices_bottom_.push_back(y); vertices_bottom_.push_back(z);

		vertices_bottom_.push_back(x+s); vertices_bottom_.push_back(y); vertices_bottom_.push_back(z);
		vertices_bottom_.push_back(x); vertices_bottom_.push_back(y); vertices_bottom_.push_back(z+s);
		vertices_bottom_.push_back(x); vertices_bottom_.push_back(y); vertices_bottom_.push_back(z);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_bottom: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & BOTTOM ? it->second.area[5] : it->second.area[0];
		tarray_bottom_.push_back(area.x2f()); tarray_bottom_.push_back(area.y2f()); 
		tarray_bottom_.push_back(area.xf());  tarray_bottom_.push_back(area.y2f()); 
		tarray_bottom_.push_back(area.x2f());  tarray_bottom_.push_back(area.yf()); 
		
		tarray_bottom_.push_back(area.x2f());  tarray_bottom_.push_back(area.yf()); 
		tarray_bottom_.push_back(area.xf()); tarray_bottom_.push_back(area.y2f()); 
		tarray_bottom_.push_back(area.xf()); tarray_bottom_.push_back(area.yf()); 
	}

	void isomap::draw() const
	{
		glClear(GL_DEPTH_BUFFER_BIT);
		// Cull triangles which normal is not towards the camera
		glEnable(GL_CULL_FACE);
		// Enable depth test
		glEnable(GL_DEPTH_TEST);

		glUseProgram(shader_->get());
		glActiveTexture(GL_TEXTURE0);
		get_terrain_info().get_tex().set_as_current_texture();
		glUniform1i(tex0_it_->second.location, 0);

		glm::mat4 mvp = level::current().projection_mat() * level::current().view_mat() * model_;
		shader_->set_uniform(mm_uniform_it_, 1, glm::value_ptr(mvp));

		//////////////////////////////////////////////////////////////////////
		// Lighting test section
		static GLint V = -1;
		if(V == -1) {
			V = shader_->get_uniform("V");
		}
		glUniformMatrix4fv(V, 1, GL_FALSE, glm::value_ptr(level::current().view_mat()));
		static GLint M = -1;
		if(M == -1) {
			M = shader_->get_uniform("M");
		}
		glUniformMatrix4fv(M, 1, GL_FALSE, glm::value_ptr(model_));
		static GLint LightPosition_worldspace = -1;
		if(LightPosition_worldspace == -1) {
			LightPosition_worldspace = shader_->get_uniform("LightPosition_worldspace");
		}
		glUniform3f(LightPosition_worldspace, 48.0f, 48.0f, 48.0f);
		static GLint vertexNormal_modelspace = -1;
		if(vertexNormal_modelspace == -1) {
			vertexNormal_modelspace = shader_->get_uniform("vertexNormal_modelspace");
		}
		//////////////////////////////////////////////////////////////////////

		glEnableVertexAttribArray(a_position_it_->second.location);
		glEnableVertexAttribArray(a_tex_coord_it_->second.location);

		if(debug_draw_faces & FRONT) {
			glUniform3f(vertexNormal_modelspace, 0, 0, 1);
			// front
			glBindBuffer(GL_ARRAY_BUFFER, arrays_[4]);
			glVertexAttribPointer(
				a_position_it_->second.location, // The attribute we want to configure
				3,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glBindBuffer(GL_ARRAY_BUFFER, arrays_[10]);
			glVertexAttribPointer(
				a_tex_coord_it_->second.location, // The attribute we want to configure
				2,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glDrawArrays(GL_TRIANGLES, 0, vertices_front_.size()/3);
		}

		if(debug_draw_faces & BACK) {
			// back
			glUniform3f(vertexNormal_modelspace, 0, 0, -1);
			glBindBuffer(GL_ARRAY_BUFFER, arrays_[5]);
			glVertexAttribPointer(
				a_position_it_->second.location, // The attribute we want to configure
				3,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glBindBuffer(GL_ARRAY_BUFFER, arrays_[11]);
			glVertexAttribPointer(
				a_tex_coord_it_->second.location, // The attribute we want to configure
				2,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glDrawArrays(GL_TRIANGLES, 0, vertices_back_.size()/3);
		}

		if(debug_draw_faces & LEFT) {
			// left
			glUniform3f(vertexNormal_modelspace, -1, 0, 0);
			glBindBuffer(GL_ARRAY_BUFFER, arrays_[0]);
			glVertexAttribPointer(
				a_position_it_->second.location, // The attribute we want to configure
				3,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glBindBuffer(GL_ARRAY_BUFFER, arrays_[6]);
			glVertexAttribPointer(
				a_tex_coord_it_->second.location, // The attribute we want to configure
				2,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glDrawArrays(GL_TRIANGLES, 0, vertices_left_.size()/3);
		}

		if(debug_draw_faces & RIGHT) {
			// right
			glUniform3f(vertexNormal_modelspace, 1, 0, 0);
			glBindBuffer(GL_ARRAY_BUFFER, arrays_[1]);
			glVertexAttribPointer(
				a_position_it_->second.location, // The attribute we want to configure
				3,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glBindBuffer(GL_ARRAY_BUFFER, arrays_[7]);
			glVertexAttribPointer(
				a_tex_coord_it_->second.location, // The attribute we want to configure
				2,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glDrawArrays(GL_TRIANGLES, 0, vertices_right_.size()/3);
		}

		if(debug_draw_faces & TOP) {
			// top
			glUniform3f(vertexNormal_modelspace, 0, 1, 0);
			glBindBuffer(GL_ARRAY_BUFFER, arrays_[2]);
			glVertexAttribPointer(
				a_position_it_->second.location, // The attribute we want to configure
				3,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glBindBuffer(GL_ARRAY_BUFFER, arrays_[8]);
			glVertexAttribPointer(
				a_tex_coord_it_->second.location, // The attribute we want to configure
				2,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glDrawArrays(GL_TRIANGLES, 0, vertices_top_.size()/3);
		}

		if(debug_draw_faces & BOTTOM) {
			// bottom
			glUniform3f(vertexNormal_modelspace, 0, -1, 0);
			glBindBuffer(GL_ARRAY_BUFFER, arrays_[3]);
			glVertexAttribPointer(
				a_position_it_->second.location, // The attribute we want to configure
				3,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glBindBuffer(GL_ARRAY_BUFFER, arrays_[9]);
			glVertexAttribPointer(
				a_tex_coord_it_->second.location, // The attribute we want to configure
				2,                  // size
				GL_FLOAT,           // type
				GL_FALSE,           // normalized?
				0,					// stride
				0					// array buffer offset
			);

			glDrawArrays(GL_TRIANGLES, 0, vertices_bottom_.size()/3);
		}

		glDisableVertexAttribArray(a_position_it_->second.location);
		glDisableVertexAttribArray(a_tex_coord_it_->second.location);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glUseProgram(0);
	}

	std::string isomap::get_tile_type(int x, int y, int z) const
	{
		auto it = tiles_.find(position(x, y, z));
		if(it == tiles_.end()) {
			return "";
		}
		return it->second;
	}

	variant isomap::get_tile_info(const std::string& type)
	{
		return variant(); // -- todo
	}

	bool isomap::is_xedge(int x) const
	{
		if(x >= 0 && x < size_x_) {
			return false;
		}
		return true;
	}

	bool isomap::is_yedge(int y) const
	{
		if(y >= 0 && y < size_y_) {
			return false;
		}
		return true;
	}

	bool isomap::is_zedge(int z) const
	{
		if(z >= 0 && z < size_z_) {
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

	pathfinding::directed_graph_ptr isomap::create_directed_graph(bool allow_diagonals)
	{
		profile::manager pman("isomap::create_directed_graph");

		std::vector<variant> vertex_list;
		std::map<std::pair<int,int>, int> vlist;

		for(auto t = tiles_.begin(); t != tiles_.end(); ++t) {
			int x = t->first.x;
			int y = t->first.y;
			int z = t->first.z;

			if(y < size_y_ - 1) {
				if(is_solid(x, y+1, z) == false) {
					vertex_list.push_back(variant_list_from_xyz(x,y+1,z));
					vlist[std::make_pair(x,z)] = y+1;
				}
			} else {
				vertex_list.push_back(variant_list_from_xyz(x,y+1,z));
				vlist[std::make_pair(x,z)] = y+1;
			}
		}
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

	void isomap::set_tile(int x, int y, int z, const std::string& type)
	{
		tiles_[position(x,y,z)] = type;
		rebuild();
	}

	void isomap::del_tile(int x, int y, int z)
	{
		auto it = tiles_.find(position(x,y,z));
		ASSERT_LOG(it != tiles_.end(), "del_tile: no tile found at position(" << x << "," << y << "," << z << ") to delete");
		tiles_.erase(it);
		rebuild();
	}

	BEGIN_DEFINE_CALLABLE_NOBASE(isomap)
	DEFINE_FIELD(dummy, "null") //you need to define at least one field right
		return variant();       //now, or else shit goes sideways.
	END_DEFINE_CALLABLE(isomap)


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

}

	

#endif
