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
		const int debug_draw_faces = isomap::FRONT | isomap::RIGHT | isomap::TOP | isomap::BACK | isomap::LEFT | isomap::BOTTOM;
		//const int debug_draw_faces = isomap::FRONT;

		boost::random::mt19937 rng(std::time(0));

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
		: u_tex0_(-1), u_lightposition_(-1), u_lightpower_(-1), u_mvp_matrix_(-1),
		u_shininess_(-1), u_m_matrix_(-1), u_v_matrix_(-1), u_normal_(-1)
	{
		// Call init *before* doing anything else
		init();
	}

	isomap::isomap(variant node)
		: u_tex0_(-1), u_lightposition_(-1), u_lightpower_(-1), u_mvp_matrix_(-1),
		u_shininess_(-1), u_m_matrix_(-1), u_v_matrix_(-1), u_normal_(-1)
	{
		// Call init *before* doing anything else
		init();

		if(node.has_key("random")) {
			// Load in some random data.
			int size_x = node["random"]["width"].as_int(32);
			int size_y = node["random"]["height"].as_int(32);
			int size_z = node["random"]["depth"].as_int(32);

			uint32_t seed = node["random"]["seed"].as_int(0);
			noise::simplex::init(seed);

			bool textured = node["random"].has_key("colored") && node["random"]["colored"].as_bool() == true ? false : true;
			int ndx = textured ? 0 : 1;
			shader_data_[ndx].size_x_ = size_x;
			shader_data_[ndx].size_y_ = size_y;
			shader_data_[ndx].size_z_ = size_z;
			shader_data_[ndx].textured_ = textured;

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
							if(textured) {
								shader_data_[0].tiles_[position(x,y,z)] = tile_data(node["random"]["type"].as_string());
							} else {
								shader_data_[1].tiles_[position(x,y,z)] = tile_data(graphics::color(node["random"]["type"]).as_sdl_color());
							}
						} else {
							if(textured) {
								shader_data_[0].tiles_[position(x,y,z)] = tile_data(get_terrain_info().random()->first);
							} else {
								shader_data_[1].tiles_[position(x,y,z)] = tile_data(random_color.as_sdl_color());
							}
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
						shader_data_[0].tiles_[position(x,y,z)] = tile_data(std::string(m[4].first, m[4].second));
					} else {
						std::cerr << "ISOMAP: Rejected voxel description: " << s << std::endl;
					}
				}
			}
			shader_data_[0].size_x_ = max_x - min_x + 1;
			shader_data_[0].size_y_ = max_y - min_y + 1;
			shader_data_[0].size_z_ = max_z - min_z + 1;
			std::cerr << "isomap: size_x( " << shader_data_[0].size_x_ << "), size_y(" << shader_data_[0].size_y_ << "), size_z(" << shader_data_[0].size_z_ << ")" << std::endl;
		}

		// Load shader.
		ASSERT_LOG(node.has_key("shader"), "Must have 'shader' attribute");
		if(node["shader"].is_map()) {
			ASSERT_LOG(node["shader"].has_key("vertex") && node["shader"].has_key("fragment"),
				"Must have 'shader' attribute with 'vertex' and 'fragment' child attributes.");
			gles2::shader v1(GL_VERTEX_SHADER, "iso_vertex_shader", node["shader"]["vertex"]);
			gles2::shader f1(GL_FRAGMENT_SHADER, "iso_fragment_shader", node["shader"]["fragment"]);
			shader_.reset(new gles2::program(node["shader"]["name"].as_string(), v1, f1));
		} else {
			ASSERT_LOG(node["shader"].is_string(), "'shader' attribute must be string or map");
			//shader_ = gles2::program::find_program(node["shader"].as_string());
			if(shader_ == NULL) {
				shader_ = gles2::shader_program::get_global(node["shader"].as_string())->shader();
			}
		}

		bool found_data = false;
		for(auto sd : shader_data_) {
			found_data |= sd.tiles_.empty() == false;
		}
		ASSERT_LOG(found_data != false, "ISOMAP: No tiles found");

		u_mvp_matrix_ = shader_->get_uniform("mvp_matrix");
		ASSERT_LOG(u_mvp_matrix_ != -1, "isomap::build_colored(): u_mvp_matrix_ == -1");
		u_lightposition_ = shader_->get_uniform("LightPosition_worldspace");
		ASSERT_LOG(u_lightposition_ != -1, "isomap::build_colored(): u_lightposition_ == -1");
		u_lightpower_ = shader_->get_uniform("LightPower");
		ASSERT_LOG(u_lightpower_ != -1, "isomap::build_colored(): u_lightpower_ == -1");
		u_shininess_ = shader_->get_uniform("Shininess");
		ASSERT_LOG(u_shininess_ != -1, "isomap::build_colored(): u_shininess_ == -1");
		u_m_matrix_ = shader_->get_uniform("m_matrix");
		ASSERT_LOG(u_m_matrix_ != -1, "isomap::build_colored(): u_m_matrix_ == -1");
		u_v_matrix_ = shader_->get_uniform("v_matrix");
		ASSERT_LOG(u_v_matrix_ != -1, "isomap::build_colored(): u_v_matrix_ == -1");
		u_normal_ = shader_->get_uniform("u_normal");
		ASSERT_LOG(u_normal_ != -1, "isomap::build_colored(): u_normal_ == -1");
		a_position_ = shader_->get_attribute("a_position");
		ASSERT_LOG(a_position_ != -1, "isomap::build_colored(): a_position_ == -1");

		a_color_ = shader_->get_attribute("a_color");
		a_texcoord_ = shader_->get_attribute("a_tex_coord");
		u_tex0_ = shader_->get_uniform("u_tex0");
		/*
		//mm_uniform_it_ = shader_->get_uniform_reference("mvp_matrix");
		u_mvp_matrix_ = shader_->get_uniform("MVP");
		//a_position_it_ = shader_->get_attribute_reference("a_position");
		a_position_it_ = shader_->get_attribute_reference("vertexPosition_modelspace");
		a_tex_coord_it_ = shader_->get_attribute_reference("a_tex_coord");
		u_tex0_ = shader_->get_uniform("u_tex0");

		u_m_matrix_ = shader_->get_uniform("M");
		u_v_matrix_ = shader_->get_uniform("V");
		u_lightposition_ = shader_->get_uniform("LightPosition_worldspace");
		u_normal_ = shader_->get_uniform("vertexNormal_modelspace");
		*/

		build();
	}

	void isomap::init()
	{
		shader_data_.resize(2);

		for(int n = 0; n != 2; ++n) {
			shader_data_[n].vbos_ = boost::shared_array<GLuint>(new GLuint[2], [](GLuint* id) {glDeleteBuffers(2,id); delete id;});
			glGenBuffers(2, &shader_data_[n].vbos_[0]);
		}

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

/*		std::string s;
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
	*/
		return res.build();
	}

	bool isomap::is_solid(int x, int y, int z) const
	{
		/*profile::manager pman("isomap::is_solid");
		for(auto sd : shader_data_) {
			auto it = sd.tiles_.find(position(x,y,z));
			if(it != sd.tiles_.end()) {
				if(it->second.name.empty() == false) {
					auto ti = get_terrain_info().find(it->second.name);
					ASSERT_LOG(ti != get_terrain_info().end(), "is_solid: Terrain not found: " << it->second.name);
					return !ti->second.transparent;
				} else {
					return it->second.color.a == 255;
				}
			}
		}
		return false;*/
		return false;
	}

	void isomap::rebuild()
	{
		build();
	}

	void isomap::build()
	{
		for(auto& sd : shader_data_) {
			sd.varray_.clear();
			sd.tarray_.clear();
			sd.carray_.clear();
			std::cerr << "cleared sd.vattrib_offsets_" << std::endl;
			sd.vattrib_offsets_.clear();
			sd.tcattrib_offsets_.clear();
			sd.num_vertices_.clear();

			sd.varray_.resize(6);
			sd.carray_.resize(6);
			sd.vattrib_offsets_.resize(6);
			sd.tcattrib_offsets_.resize(6);
			sd.num_vertices_.resize(6);

			if(sd.tiles_.size() > 0) {
				if(sd.textured_) {
					build_textured(sd);
				} else {
					build_colored(sd);
				}
			}
		}
	}

	void isomap::build_colored(shader_data& sd)
	{
		profile::manager pman("isomap::build_colored");

		for(auto t : sd.tiles_) {
			int x = t.first.x;
			int y = t.first.y;
			int z = t.first.z;

			if(x > 0) {
				if(is_solid(x-1, y, z) == false) {
					add_colored_face_left(sd,x,y,z,1,t.second.color);
				}
			} else {
				add_colored_face_left(sd,x,y,z,1,t.second.color);
			}
			if(x < sd.size_x_ - 1) {
				if(is_solid(x+1, y, z) == false) {
					add_colored_face_right(sd,x,y,z,1,t.second.color);
				}
			} else {
				add_colored_face_right(sd,x,y,z,1,t.second.color);
			}
			if(y > 0) {
				if(is_solid(x, y-1, z) == false) {
					add_colored_face_bottom(sd,x,y,z,1,t.second.color);
				}
			} else {
				add_colored_face_bottom(sd,x,y,z,1,t.second.color);
			}
			if(y < sd.size_y_ - 1) {
				if(is_solid(x, y+1, z) == false) {
					add_colored_face_top(sd,x,y,z,1,t.second.color);
				}
			} else {
				add_colored_face_top(sd,x,y,z,1,t.second.color);
			}
			if(z > 0) {
				if(is_solid(x, y, z-1) == false) {
					add_colored_face_back(sd,x,y,z,1,t.second.color);
				}
			} else {
				add_colored_face_back(sd,x,y,z,1,t.second.color);
			}
			if(z < sd.size_z_ - 1) {
				if(is_solid(x, y, z+1) == false) {
					add_colored_face_front(sd,x,y,z,1,t.second.color);
				}
			} else {
				add_colored_face_front(sd,x,y,z,1,t.second.color);
			}
		}

		size_t total_size = 0;
		int n = 0;
		for(auto vec : sd.varray_) {
			sd.vattrib_offsets_[n] = total_size;
			total_size += vec.size() * sizeof(GLfloat);
			sd.num_vertices_[n] = vec.size() / 3;
			++n;
		}
		glBindBuffer(GL_ARRAY_BUFFER, sd.vbos_[0]);
		glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
		for(int n=0; n != 6; ++n) {
			glBufferSubData(GL_ARRAY_BUFFER, sd.vattrib_offsets_[n], sd.varray_[n].size()*sizeof(GLfloat), &sd.varray_[n][0]);
		}

		total_size = 0;
		n = 0;
		for(auto vec : sd.carray_) {
			sd.tcattrib_offsets_[n] = total_size;
			total_size += vec.size() * sizeof(uint8_t);
			++n;
		}
		glBindBuffer(GL_ARRAY_BUFFER, sd.vbos_[1]);
		glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
		for(int n=0; n != 6; ++n) {
			glBufferSubData(GL_ARRAY_BUFFER, sd.tcattrib_offsets_[n], sd.carray_[n].size()*sizeof(uint8_t), &sd.carray_[n][0]);
		}
		std::cerr << "Built " << sd.varray_[FRONT_FACE].size()/3 << " front faces" << std::endl;
		std::cerr << "Built " << sd.varray_[BACK_FACE].size()/3 << " back faces" << std::endl;
		std::cerr << "Built " << sd.varray_[TOP_FACE].size()/3 << " top faces" << std::endl;
		std::cerr << "Built " << sd.varray_[BOTTOM_FACE].size()/3 << " bottom faces" << std::endl;
		std::cerr << "Built " << sd.varray_[LEFT_FACE].size()/3 << " left faces" << std::endl;
		std::cerr << "Built " << sd.varray_[RIGHT_FACE].size()/3 << " right faces" << std::endl;

		sd.varray_.clear();
		sd.carray_.clear();

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void isomap::build_textured(shader_data& sd)
	{
		profile::manager pman("isomap::build");

		for(auto t : sd.tiles_) {
			int x = t.first.x;
			int y = t.first.y;
			int z = t.first.z;
			if(x > 0) {
				if(is_solid(x-1, y, z) == false) {
					add_face_left(sd,x,y,z,1,t.second.name);
				}
			} else {
				add_face_left(sd,x,y,z,1,t.second.name);
			}
			if(x < sd.size_x_ - 1) {
				if(is_solid(x+1, y, z) == false) {
					add_face_right(sd,x,y,z,1,t.second.name);
				}
			} else {
				add_face_right(sd,x,y,z,1,t.second.name);
			}
			if(y > 0) {
				if(is_solid(x, y-1, z) == false) {
					add_face_bottom(sd,x,y,z,1,t.second.name);
				}
			} else {
				add_face_bottom(sd,x,y,z,1,t.second.name);
			}
			if(y < sd.size_y_ - 1) {
				if(is_solid(x, y+1, z) == false) {
					add_face_top(sd,x,y,z,1,t.second.name);
				}
			} else {
				add_face_top(sd,x,y,z,1,t.second.name);
			}
			if(z > 0) {
				if(is_solid(x, y, z-1) == false) {
					add_face_back(sd,x,y,z,1,t.second.name);
				}
			} else {
				add_face_back(sd,x,y,z,1,t.second.name);
			}
			if(z < sd.size_z_ - 1) {
				if(is_solid(x, y, z+1) == false) {
					add_face_front(sd,x,y,z,1,t.second.name);
				}
			} else {
				add_face_front(sd,x,y,z,1,t.second.name);
			}
		}
		
		size_t total_size = 0;
		int n = 0;
		for(auto vec : sd.varray_) {
			sd.vattrib_offsets_[n] = total_size;
			total_size += vec.size() * sizeof(GLfloat);
			sd.num_vertices_[n] = vec.size() / 3;
			++n;
		}
		glBindBuffer(GL_ARRAY_BUFFER, sd.vbos_[0]);
		glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
		for(int n=0; n != 6; ++n) {
			glBufferSubData(GL_ARRAY_BUFFER, sd.vattrib_offsets_[n], sd.varray_[n].size()*sizeof(GLfloat), &sd.varray_[n][0]);
		}

		total_size = 0;
		n = 0;
		for(auto vec : sd.tarray_) {
			sd.tcattrib_offsets_[n] = total_size;
			total_size += vec.size() * sizeof(GLfloat);
			++n;
		}
		glBindBuffer(GL_ARRAY_BUFFER, sd.vbos_[1]);
		glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
		for(int n=0; n != 6; ++n) {
			glBufferSubData(GL_ARRAY_BUFFER, sd.tcattrib_offsets_[n], sd.tarray_[n].size()*sizeof(uint8_t), &sd.tarray_[n][0]);
		}
		sd.varray_.clear();
		sd.tarray_.clear();

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void isomap::add_carray_data(const SDL_Color& col, std::vector<uint8_t>& carray)
	{
		for(int n = 0; n != 6; ++n) {
			carray.push_back(col.r);
			carray.push_back(col.g);
			carray.push_back(col.b);
			carray.push_back(col.a);
		}
	}

	void isomap::add_tarray_data(int face, const rectf& area, std::vector<GLfloat>& tarray)
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

	void isomap::add_vertex_data(int face, GLfloat x, GLfloat y, GLfloat z, GLfloat s, std::vector<GLfloat>& varray)
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

	void isomap::add_colored_face_left(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(LEFT_FACE, x, y, z, s, sd.varray_[LEFT_FACE]);
		add_carray_data(col, sd.carray_[LEFT_FACE]);
	}

	void isomap::add_colored_face_right(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(RIGHT_FACE, x, y, z, s, sd.varray_[RIGHT_FACE]);
		add_carray_data(col, sd.carray_[RIGHT_FACE]);
	}

	void isomap::add_colored_face_front(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(FRONT_FACE, x, y, z, s, sd.varray_[FRONT_FACE]);
		add_carray_data(col, sd.carray_[FRONT_FACE]);
	}

	void isomap::add_colored_face_back(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(BACK_FACE, x, y, z, s, sd.varray_[BACK_FACE]);
		add_carray_data(col, sd.carray_[BACK_FACE]);
	}

	void isomap::add_colored_face_top(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(TOP_FACE, x, y, z, s, sd.varray_[TOP_FACE]);
		add_carray_data(col, sd.carray_[TOP_FACE]);
	}

	void isomap::add_colored_face_bottom(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const SDL_Color& col)
	{
		add_vertex_data(BOTTOM_FACE, x, y, z, s, sd.varray_[BOTTOM_FACE]);
		add_carray_data(col, sd.carray_[BOTTOM_FACE]);
	}

	void isomap::add_face_left(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(LEFT_FACE, x, y, z, s, sd.varray_[LEFT_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_left: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & LEFT ? it->second.area[4] : it->second.area[0];
		add_tarray_data(LEFT_FACE, area, sd.tarray_[LEFT_FACE]);
	}

	void isomap::add_face_right(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(RIGHT_FACE, x, y, z, s, sd.varray_[RIGHT_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_right: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & RIGHT ? it->second.area[1] : it->second.area[0];
		add_tarray_data(RIGHT_FACE, area, sd.tarray_[RIGHT_FACE]);
	}

	void isomap::add_face_front(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(FRONT_FACE, x, y, z, s, sd.varray_[FRONT_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_front: Unable to find tile type in list: " << bid);
		const rectf area = it->second.area[0];
		add_tarray_data(FRONT_FACE, area, sd.tarray_[FRONT_FACE]);
	}

	void isomap::add_face_back(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(BACK_FACE, x, y, z, s, sd.varray_[BACK_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_back: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & BACK ? it->second.area[3] : it->second.area[0];
		add_tarray_data(BACK_FACE, area, sd.tarray_[BACK_FACE]);
	}

	void isomap::add_face_top(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(TOP_FACE, x, y, z, s, sd.varray_[TOP_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_top: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & TOP ? it->second.area[2] : it->second.area[0];
		add_tarray_data(TOP_FACE, area, sd.tarray_[TOP_FACE]);
	}

	void isomap::add_face_bottom(shader_data& sd, GLfloat x, GLfloat y, GLfloat z, GLfloat s, const std::string& bid)
	{
		add_vertex_data(BOTTOM_FACE, x, y, z, s, sd.varray_[BOTTOM_FACE]);

		auto it = get_terrain_info().find(bid);
		ASSERT_LOG(it != get_terrain_info().end(), "add_face_bottom: Unable to find tile type in list: " << bid);
		const rectf area = it->second.faces & BOTTOM ? it->second.area[5] : it->second.area[0];
		add_tarray_data(BOTTOM_FACE, area, sd.tarray_[BOTTOM_FACE]);
	}

	void isomap::draw() const
	{
		glClear(GL_DEPTH_BUFFER_BIT);
		// Cull triangles which normal is not towards the camera
		glEnable(GL_CULL_FACE);
		// Enable depth test
		glEnable(GL_DEPTH_TEST);

		for(auto sd : shader_data_) {
			if(sd.tiles_.size() > 0) {
				if(sd.textured_) {
					draw_textured(sd);
				} else {
					draw_colored(sd);
				}
			}
		}
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glUseProgram(0);
	}

	void isomap::draw_colored(const shader_data& sd) const
	{
		ASSERT_LOG(sd.vattrib_offsets_.size() != 0, "sd.vattrib_offsets_.size() == 0");
		ASSERT_LOG(sd.tcattrib_offsets_.size() != 0, "sd.vattrib_offsets_.size() == 0");
		glUseProgram(shader_->get());

		glm::mat4 mvp = level::current().projection_mat() * level::current().view_mat() * model_;
		glUniformMatrix4fv(u_mvp_matrix_, 1, GL_FALSE, glm::value_ptr(mvp));

		glUniformMatrix4fv(u_v_matrix_, 1, GL_FALSE, glm::value_ptr(level::current().view_mat()));
		glUniformMatrix4fv(u_m_matrix_, 1, GL_FALSE, glm::value_ptr(model_));
		glUniform3f(u_lightposition_, sd.size_x_/2.0f, 200.0f, sd.size_z_/2.0f);
		glUniform1f(u_lightpower_, 15000.0f);
		glUniform1f(u_shininess_, 5.0f);

		glEnableVertexAttribArray(a_position_);
		glEnableVertexAttribArray(a_color_);
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			if(debug_draw_faces & (1 << n)) {
				glUniform3fv(u_normal_, 1, glm::value_ptr(normals_[n]));
				glBindBuffer(GL_ARRAY_BUFFER, sd.vbos_[0]);
				glVertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLfloat*>(sd.vattrib_offsets_[n]));
				glBindBuffer(GL_ARRAY_BUFFER, sd.vbos_[1]);
				glVertexAttribPointer(a_color_, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, reinterpret_cast<const GLfloat*>(sd.tcattrib_offsets_[n]));
				glDrawArrays(GL_TRIANGLES, 0, sd.num_vertices_[n]);
			}
		}
		glDisableVertexAttribArray(a_position_);
		glDisableVertexAttribArray(a_color_);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void isomap::draw_textured(const shader_data& sd) const
	{
		ASSERT_LOG(sd.vattrib_offsets_.size() != 0, "sd.vattrib_offsets_.size() == 0");
		ASSERT_LOG(sd.tcattrib_offsets_.size() != 0, "sd.vattrib_offsets_.size() == 0");
		glUseProgram(shader_->get());

		glActiveTexture(GL_TEXTURE0);
		get_terrain_info().get_tex().set_as_current_texture();
		glUniform1i(u_tex0_, 0);

		glm::mat4 mvp = level::current().projection_mat() * level::current().view_mat() * model_;
		glUniformMatrix4fv(u_mvp_matrix_, 1, GL_FALSE, glm::value_ptr(mvp));

		glUniformMatrix4fv(u_v_matrix_, 1, GL_FALSE, glm::value_ptr(level::current().view_mat()));
		glUniformMatrix4fv(u_m_matrix_, 1, GL_FALSE, glm::value_ptr(model_));
		glUniform3f(u_lightposition_, sd.size_x_/2.0f, 200.0f, sd.size_z_/2.0f);
		glUniform1f(u_lightpower_, 15000.0f);
		glUniform1f(u_shininess_, 5.0f);

		glEnableVertexAttribArray(a_position_);
		glEnableVertexAttribArray(a_texcoord_);
		for(int n = FRONT_FACE; n != MAX_FACES; ++n) {
			if(debug_draw_faces & (1 << n)) {
				glUniform3fv(u_normal_, 1, glm::value_ptr(normals_[n]));
				glBindBuffer(GL_ARRAY_BUFFER, sd.vbos_[0]);
				glVertexAttribPointer(a_position_, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLfloat*>(sd.vattrib_offsets_[n]));
				glBindBuffer(GL_ARRAY_BUFFER, sd.vbos_[1]);
				glVertexAttribPointer(a_texcoord_, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLfloat*>(sd.tcattrib_offsets_[n]));
				glDrawArrays(GL_TRIANGLES, 0, sd.num_vertices_[n]);
			}
		}
		glDisableVertexAttribArray(a_position_);
		glDisableVertexAttribArray(a_texcoord_);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	std::string isomap::get_tile_type(int x, int y, int z) const
	{
		auto it = shader_data_[0].tiles_.find(position(x, y, z));
		if(it == shader_data_[0].tiles_.end()) {
			return "";
		}
		return it->second.name;
	}

	variant isomap::get_tile_info(const std::string& type)
	{
		return variant(); // -- todo
	}

	bool isomap::is_xedge(int x, int size_x) const
	{
		if(x >= 0 && x < size_x) {
			return false;
		}
		return true;
	}

	bool isomap::is_yedge(int y, int size_y) const
	{
		if(y >= 0 && y < size_y) {
			return false;
		}
		return true;
	}

	bool isomap::is_zedge(int z, int size_z) const
	{
		if(z >= 0 && z < size_z) {
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
		/*
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
		*/
		return pathfinding::directed_graph_ptr();
	}

	void isomap::set_tile(int x, int y, int z, const std::string& type)
	{
		//tiles_[position(x,y,z)] = type;
		//rebuild();
	}

	void isomap::del_tile(int x, int y, int z)
	{
		//auto it = tiles_.find(position(x,y,z));
		//ASSERT_LOG(it != tiles_.end(), "del_tile: no tile found at position(" << x << "," << y << "," << z << ") to delete");
		//tiles_.erase(it);
		//rebuild();
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
