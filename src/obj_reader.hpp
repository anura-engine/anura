#pragma once

#include <map>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

// XXX This all needs fixed up.

namespace obj
{
	struct mtl_data
	{
		std::string name;
		glm::vec3 ambient;			// Ka
		glm::vec3 diffuse;			// Kd
		glm::vec3 specular;			// Ks
		float specular_coef;		// Ns
		float alpha;				// d or Tr
		int illumination_model;		// illum

		std::string tex_ambient;	// map_Ka
		std::string tex_diffuse;	// map_Kd
		std::string tex_specular;	// map_Ks
		std::string tex_specular_coef;	// map_Ns
		std::string tex_alpha;		// map_d
	};

	struct obj_data
	{
		std::string name;
		std::vector<glm::vec4> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> uvs;

		// vertex, uv, normal
		std::vector<float> face_vertices;
		std::vector<float> face_normals;
		std::vector<float> face_uvs;

		std::vector<glm::vec3> parameter_space_vertices;

		mtl_data mtl;
	};

	void load_obj_file(const std::string& filename, std::vector<obj_data>& o);
}
