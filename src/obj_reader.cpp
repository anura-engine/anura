#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <sstream>

#include "filesystem.hpp"
#include "module.hpp"
#include "obj_reader.hpp"
#include "string_utils.hpp"

namespace obj
{
	namespace 
	{
		static const boost::regex re_v("v"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"(?:\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?))?");
		static const boost::regex re_vt("vt"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"(?:\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?))?");
		static const boost::regex re_vn("vn"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)");
		static const boost::regex re_vp("vp"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"(?:\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"(?:\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?))?)?");
		static const boost::regex re_f_sub("(\\d+)(?:/(\\d+)?(?:/(\\d+)?)?)?");
		static const boost::regex re_k("K[asd]"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)"
			"\\s+([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)");
	}

	void load_mtl_file(const boost::filesystem::path& filename, std::map<std::string, mtl_data>& mtl) 
	{
		std::string mtl_file_data = sys::read_file(module::map_file(filename.generic_string()));
		std::vector<std::string> lines = util::split(mtl_file_data, '\n');
		boost::scoped_ptr<mtl_data> m;
		for(auto line : lines) {
			std::stringstream ss(line);
			std::string symbol;
			ss >> symbol;
			if(symbol == "newmtl") {
				if(m != NULL) {
					mtl[m->name] = *m;
				}
				m.reset(new mtl_data);
				ss >> m->name;
			} else if(symbol == "Ka" || symbol == "Kd" || symbol == "Ks") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				glm::vec3 k(1.0f, 1.0f, 1.0f);
				boost::smatch what;
				if(boost::regex_match(line, what, re_k)) {
					for(int n = 1; n !=	what.size(); ++n) {
						if(what[n].str().empty() == false) {
							k[n-1] = boost::lexical_cast<float>(what[n]);
						}
					}
				}
				if(symbol == "Ka") {
					m->ambient = k;
				} else if(symbol == "Kd") {
					m->diffuse = k;
				} else {
					m->specular = k;
				}
			} else if(symbol == "Ns") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				ss >> m->specular_coef;
			} else if(symbol == "d" || symbol == "Tr") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				ss >> m->alpha;
			} else if(symbol == "illum") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				ss >> m->illumination_model;
			} else if(symbol == "map_Ka") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				ss >> m->tex_ambient;
			} else if(symbol == "map_Kd") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				ss >> m->tex_diffuse;
			} else if(symbol == "map_Ks") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				ss >> m->tex_specular;
			} else if(symbol == "map_Ns") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				ss >> m->tex_specular_coef;
			} else if(symbol == "map_alpha") {
				ASSERT_LOG(m != NULL, "Error no 'newmtl' definition found before data.");
				ss >> m->tex_alpha;
			}
		}
		if(m != NULL) {
			mtl[m->name] = *m;
		}
	}

	void load_obj_file(const std::string& filename, std::vector<obj_data>& odata)
	{
		boost::filesystem::path p(filename);
		std::string obj_file_data = sys::read_file(module::map_file(filename));
		std::vector<std::string> lines = util::split(obj_file_data, '\n');
		boost::scoped_ptr<obj_data> o(new obj_data);
		std::map<std::string, mtl_data> mtl;

		for(auto line : lines) {
			std::stringstream ss(line);
			std::string symbol;
			ss >> symbol;
			if(symbol == "o") {
				if(!o->name.empty()) {
					odata.push_back(*o);
					o.reset(new obj_data);
				}
				ss >> o->name;
			} else if(symbol == "v") {
				ASSERT_LOG(o != NULL, "Error no 'o' definition found before data.");
				glm::vec4 vertex(0.0f, 0.0f, 0.0f, 1.0f);
				boost::smatch what;
				if(boost::regex_match(line, what, re_v)) {
					for(int n = 1; n !=	what.size(); ++n) {
						if(what[n].str().empty() == false) {
							vertex[n-1] = boost::lexical_cast<float>(what[n]);
						}
					}
				}
				o->vertices.push_back(vertex);
			} else if(symbol == "vt") {
				ASSERT_LOG(o != NULL, "Error no 'o' definition found before data.");
				glm::vec3 uvw(0.0f, 0.0f, 0.0f);
				boost::smatch what;
				if(boost::regex_match(line, what, re_vt)) {
					for(int n = 1; n !=	what.size(); ++n) {
						if(what[n].str().empty() == false) {
							uvw[n-1] = boost::lexical_cast<float>(what[n]);
						}
					}
				}
				o->uvs.push_back(glm::vec2(uvw[0], uvw[1]));
			} else if(symbol == "vn") {
				ASSERT_LOG(o != NULL, "Error no 'o' definition found before data.");
				glm::vec3 normal(0.0f, 0.0f, 0.0f);
				boost::smatch what;
				if(boost::regex_match(line, what, re_vn)) {
					for(int n = 1; n !=	what.size(); ++n) {
						if(what[n].str().empty() == false) {
							normal[n-1] = boost::lexical_cast<float>(what[n]);
						}
					}
				}
				o->normals.push_back(normal);
			} else if(symbol == "vp") {
				ASSERT_LOG(o != NULL, "Error no 'o' definition found before data.");
				glm::vec3 psv(0.0f, 0.0f, 0.0f);
				boost::smatch what;
				if(boost::regex_match(line, what, re_vn)) {
					for(int n = 1; n !=	what.size(); ++n) {
						if(what[n].str().empty() == false) {
							psv[n-1] = boost::lexical_cast<float>(what[n]);
						}
					}
				}
				o->parameter_space_vertices.push_back(psv);
			} else if(symbol == "f") {
				ASSERT_LOG(o != NULL, "Error no 'o' definition found before 'f' data.");
				std::string group;
				do {
					ss >> group;
					boost::smatch what;
					if(boost::regex_match(group, what, re_f_sub)) {
						for(int n = 1; n !=	what.size(); ++n) {
							if(what[n].str().empty() == false) {
								ASSERT_LOG(n > 0 && n < 4, "'f' sub-expression outside number of allowable elements: " << n);
								if(n == 1) {
									size_t index = boost::lexical_cast<size_t>(what[1])-1;
									ASSERT_LOG(index < o->vertices.size(), "index outside number of vertices: " << index << " >= " << o->vertices.size());
									o->face_vertices.push_back(o->vertices[index][0]);
									o->face_vertices.push_back(o->vertices[index][1]);
									o->face_vertices.push_back(o->vertices[index][2]);
								} else if(n == 2) {
									size_t index = boost::lexical_cast<size_t>(what[2])-1;
									ASSERT_LOG(index < o->uvs.size(), "index outside number of uvs: " << index << " >= " << o->uvs.size());
									o->face_uvs.push_back(o->uvs[index][0]);
									o->face_uvs.push_back(1.0f - o->uvs[index][1]);
								} else if(n == 3) {
									size_t index = boost::lexical_cast<size_t>(what[3])-1;
									ASSERT_LOG(index < o->normals.size(), "index outside number of normals: " << index << " >= " << o->normals.size());
									o->face_normals.push_back(o->normals[index][0]);
									o->face_normals.push_back(o->normals[index][1]);
									o->face_normals.push_back(o->normals[index][2]);
								}
							}
						}
					}
				} while(ss.eof() == false && group.empty() == false);
			} else if(symbol == "mtllib") {
				std::string mtl_file_name;
				ss >> mtl_file_name;
				load_mtl_file(p.parent_path() / mtl_file_name, mtl);
			} else if(symbol == "usemtl") {
				ASSERT_LOG(o != NULL, "Error no 'o' definition found before data.");
				std::string mtl_name;
				ss >> mtl_name;
				auto it = mtl.find(mtl_name);
				ASSERT_LOG(it != mtl.end(), "Unable to find material(" << mtl_name << "( in mtl_file");
				o->mtl = it->second;
			}
		}
		if(!o->name.empty()) {
			odata.push_back(*o);
		}
	}
}