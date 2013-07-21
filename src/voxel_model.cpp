/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef USE_GLES2

#include <boost/shared_ptr.hpp>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

#include "json_parser.hpp"
#include "variant_utils.hpp"
#include "voxel_model.hpp"


namespace voxel {

void get_voxel_pos_adjacent(const VoxelPos& pos, VoxelPos* adj)
{
	for(int n = 0; n != 6; ++n) {
		adj[n] = pos;
	}

	adj[0].x++;
	adj[1].x--;
	adj[2].y++;
	adj[3].y--;
	adj[4].z++;
	adj[5].z--;
}

namespace
{
	std::vector<glm::vec3>& normal_vectors()
	{
		static std::vector<glm::vec3> res;
		if(res.empty()) {
			res.push_back(glm::vec3(-1.0f, 0.0f, 0.0f));
			res.push_back(glm::vec3( 1.0f, 0.0f, 0.0f));
			res.push_back(glm::vec3( 0.0f, 1.0f, 0.0f));
			res.push_back(glm::vec3( 0.0f,-1.0f, 0.0f));
			res.push_back(glm::vec3( 0.0f, 0.0f,-1.0f));
			res.push_back(glm::vec3( 0.0f, 0.0f, 1.0f));
		}
		return res;
	}
}

bool operator==(VoxelPos const& p1, VoxelPos const& p2)
{
	return p1.x == p2.x && p1.y == p2.y && p1.z == p2.z;
}

bool operator<(VoxelPos const& p1, VoxelPos const& p2)
{
	return std::lexicographical_compare(&p1[0], &p1[0]+3, &p2[0], &p2[0]+3);
}

bool VoxelPosLess::operator()(const VoxelPos& p1, const VoxelPos& p2) const
{
	return p1 < p2;
}

bool VoxelArea::voxel_in_area(const VoxelPos& pos) const
{
	bool result = true;
	for(int n = 0; n != 3 && result; ++n) {
		result = pos[n] >= top_left[n] && pos[n] < bot_right[n];
	}

	return result;
}

variant write_voxels(const std::vector<VoxelPos>& positions, const Voxel& voxel) {
	std::map<variant,variant> m;
	std::vector<variant> pos;
	for(const VoxelPos& p : positions) {
		for(int n = 0; n != 3; ++n) {
			pos.push_back(variant(p[n]));
		}
	}
	m[variant("loc")] = variant(&pos);
	m[variant("color")] = voxel.color.write();
	return variant(&m);
}

VoxelPos read_voxel_pos(const variant& v) {
	std::vector<int> res = v.as_list_int();
	ASSERT_LOG(res.size() == 3, "Illegal voxel pos: " << v.write_json() << " " << v.debug_location());
	VoxelPos result;
	result[0] = res[0];
	result[1] = res[1];
	result[2] = res[2];
	return result;
}

variant write_voxel_pos(const VoxelPos& pos) {
	std::vector<variant> result;
	result.push_back(variant(pos[0]));
	result.push_back(variant(pos[1]));
	result.push_back(variant(pos[2]));
	return variant(&result);
}

void read_voxels(const variant& v, VoxelMap* out) {
	std::vector<int> pos = v["loc"].as_list_int();
	ASSERT_LOG(pos.size()%3 == 0, "Bad location: " << v.write_json() << v.debug_location());
	graphics::color color(v["color"]);

	while(pos.empty() == false) {
		VoxelPair result;

		std::copy(pos.end()-3, pos.end(), &result.first[0]);
		result.second.color = color;
		out->insert(result);

		pos.resize(pos.size() - 3);
	}
}

LayerType read_layer_type(const variant& v) {
	LayerType result;
	result.last_edited_variation = v["last_edited_variation"].as_string_default();
	result.symmetric = v["symmetric"].as_bool(false);
	variant layers_node = v["variations"];
	if(layers_node.is_null()) {
		Layer default_layer;
		default_layer.name = "default";
		result.variations["default"] = default_layer;
		return result;
	}

	for(const std::pair<variant,variant>& p : layers_node.as_map()) {
		Layer layer;
		layer.name = p.first.as_string();
		variant layer_node = p.second;
		if(layer_node["voxels"].is_list()) {
			for(variant v : layer_node["voxels"].as_list()) {
				read_voxels(v, &layer.map);
			}
		}

		result.variations[layer.name] = layer;
	}

	const variant pivots_node = v["pivots"];
	if(!pivots_node.is_null()) {
		for(const variant::map_pair& p : pivots_node.as_map()) {
			std::vector<int> pos = p.second.as_list_int();
			ASSERT_LOG(pos.size() == 3, "Invalid pivot position: " << p.second.write_json() << " " << p.second.debug_location());
			VoxelPos voxel_pos;
			std::copy(pos.begin(), pos.end(), &voxel_pos[0]);
			result.pivots[p.first.as_string()] = voxel_pos;
		}
	}

	return result;
}

std::map<std::string, AttachmentPoint> read_attachment_points(const variant& v)
{
	std::map<std::string, AttachmentPoint> result;
	for(auto p : v.as_map()) {
		AttachmentPoint point;
		point.name = p.first.as_string();
		point.layer = p.second["layer"].as_string();
		point.pivot = p.second["pivot"].as_string();

		if(p.second["rotations"].is_list()) {
			for(auto rotation_node : p.second["rotations"].as_list()) {
				AttachmentPointRotation rotation;
				rotation.direction = variant_to_vec3(rotation_node["direction"]);
				rotation.amount = rotation_node["rotation"].as_decimal().as_float();
				point.rotations.push_back(rotation);
			}
		}

		result[point.name] = point;
	}

	return result;
}

variant write_attachment_points(const std::map<std::string, AttachmentPoint>& m)
{
	std::map<variant, variant> result;
	for(auto p : m) {
		std::map<variant, variant> node;
		node[variant("layer")] = variant(p.second.layer);
		node[variant("pivot")] = variant(p.second.pivot);

		if(!p.second.rotations.empty()) {
			std::vector<variant> rotations;

			for(const AttachmentPointRotation& r : p.second.rotations) {
				std::map<variant, variant> rotation_node;
				rotation_node[variant("direction")] = vec3_to_variant(r.direction);
				rotation_node[variant("rotation")] = variant(decimal(r.amount));
				rotations.push_back(variant(&rotation_node));
			}

			node[variant("rotations")] = variant(&rotations);
		}

		result[variant(p.first)] = variant(&node);
	}

	return variant(&result);
}

Model read_model(const variant& v) {
	Model model;

	if(v.has_key("feet")) {
		model.feet_position = read_voxel_pos(v["feet"]);
	} else {
		model.feet_position[0] = model.feet_position[1] = model.feet_position[2] = 0;
	}

	if(v.has_key("scale")) {
		model.scale = v["scale"].as_decimal();
	} else {
		model.scale = decimal::from_int(1);
	}

	for(const std::pair<variant,variant>& p : v["layers"].as_map()) {
		LayerType layer_type = read_layer_type(p.second);
		layer_type.name = p.first.as_string();
		model.layer_types.push_back(layer_type);
	}

	for(const std::pair<variant,variant>& p : v["animations"].as_map()) {
		Animation anim = read_animation(p.second);
		anim.name = p.first.as_string();
		model.animations.push_back(anim);
	}

	if(v.has_key("attachment_points")) {
		model.attachment_points = read_attachment_points(v["attachment_points"]);
	}

	return model;
}

variant write_model(const Model& model) {
	std::map<variant,variant> layers_node;
	for(const LayerType& layer_type : model.layer_types) {
		std::map<variant,variant> layer_type_node;
		layer_type_node[variant("name")] = variant(layer_type.name);
		layer_type_node[variant("last_edited_variation")] = variant(layer_type.last_edited_variation);
		if(layer_type.symmetric) {
			layer_type_node[variant("symmetric")] = variant::from_bool(true);
		}

		if(!layer_type.pivots.empty()) {
			std::map<variant,variant> pivots;
			for(const std::pair<std::string,VoxelPos>& p : layer_type.pivots) {
				std::vector<variant> value;
				value.push_back(variant(p.second.x));
				value.push_back(variant(p.second.y));
				value.push_back(variant(p.second.z));

				pivots[variant(p.first)] = variant(&value);
			}

			layer_type_node[variant("pivots")] = variant(&pivots);
		}

		std::map<variant,variant> variations_node;
		for(const std::pair<std::string, Layer>& p : layer_type.variations) {
			std::map<variant,variant> layer_node;
			layer_node[variant("name")] = variant(p.first);

			std::vector<std::pair<std::vector<VoxelPos>, Voxel> > grouped_voxels;
			for(const VoxelPair& vp : p.second.map) {
				bool found = false;
				for(std::pair<std::vector<VoxelPos>, Voxel>& group : grouped_voxels) {
					if(vp.second == group.second) {
						found = true;
						group.first.push_back(vp.first);
						break;
					}
				}

				if(!found) {
					std::vector<VoxelPos> pos;
					pos.push_back(vp.first);
					grouped_voxels.push_back(std::pair<std::vector<VoxelPos>, Voxel>(pos, vp.second));
				}
			}

			std::vector<variant> voxels;
			for(const std::pair<std::vector<VoxelPos>, Voxel>& group : grouped_voxels) {
				voxels.push_back(write_voxels(group.first, group.second));
			}

			layer_node[variant("voxels")] = variant(&voxels);
			variations_node[variant(p.first)] = variant(&layer_node);
		}

		layer_type_node[variant("variations")] = variant(&variations_node);
		layers_node[variant(layer_type.name)] = variant(&layer_type_node);
	}

	std::map<variant, variant> animations_node;
	for(const Animation& anim : model.animations) {
		animations_node[variant(anim.name)] = write_animation(anim);
	}

	std::map<variant,variant> result_node;
	result_node[variant("feet")] = write_voxel_pos(model.feet_position);
	result_node[variant("scale")] = variant(model.scale);
	result_node[variant("layers")] = variant(&layers_node);
	result_node[variant("animations")] = variant(&animations_node);

	if(!model.attachment_points.empty()) {
		result_node[variant("attachment_points")] = write_attachment_points(model.attachment_points);
	}

	return variant(&result_node);
}

Animation read_animation(const variant& v)
{
	Animation result;
	if(v.has_key("duration")) {
		result.duration = v["duration"].as_decimal().as_float();
	} else {
		result.duration = -1.0;
	}

	for(variant t : v["transforms"].as_list()) {
		AnimationTransform transform;
		transform.children_only = t["children_only"].as_bool(false);
		transform.layer = t["layer"].as_string();
		if(t.has_key("pivots")) {
			std::vector<std::string> pivots = t["pivots"].as_list_string();
			ASSERT_LOG(pivots.size() == 2, "Must have two pivots in animation: " << t.to_debug_string());
			transform.pivot_src = pivots[0];
			transform.pivot_dst = pivots[1];
			transform.rotation_formula = game_logic::formula::create_optional_formula(t["rotation"]);
		}
		transform.translation_formula = game_logic::formula::create_optional_formula(t["translation"]);
		result.transforms.push_back(transform);
	}

	return result;
}

variant write_animation(const Animation& anim)
{
	std::vector<variant> t;
	for(const AnimationTransform& transform : anim.transforms) {
		std::map<variant,variant> node;
		node[variant("layer")] = variant(transform.layer);

		if(transform.children_only) {
			node[variant("children_only")] = variant::from_bool(true);
		}

		if(transform.rotation_formula) {
			std::vector<variant> pivot_vec;
			pivot_vec.push_back(variant(transform.pivot_src));
			pivot_vec.push_back(variant(transform.pivot_dst));

			node[variant("pivots")] = variant(&pivot_vec);
			node[variant("rotation")] = transform.rotation_formula->str_var();
		}

		if(transform.translation_formula) {
			node[variant("translation")] = transform.translation_formula->str_var();
		}

		t.push_back(variant(&node));
	}

	std::map<variant,variant> result;
	result[variant("transforms")] = variant(&t);
	if(anim.duration > 0.0) {
		result[variant("duration")] = variant(decimal(anim.duration));
	}

	return variant(&result);
}

voxel_model::voxel_model(const variant& node)
  : name_(node["model"].as_string()), anim_time_(0.0), old_anim_time_(0.0),
    invalidated_(false), model_(1.0f), proto_model_(1.0f)
{
	Model base(read_model(json::parse_from_file(name_)));

	attachment_points_ = base.attachment_points;

	for(const LayerType& layer_type : base.layer_types) {
		variant variation_name = node[layer_type.name];
		if(variation_name.is_null()) {
			variation_name = variant("default");
		}

		auto variation_itor = layer_type.variations.find(variation_name.as_string());
		ASSERT_LOG(variation_itor != layer_type.variations.end(), "Could not find variation of layer " << layer_type.name << " name " << variation_name.as_string() << " in model " << name_);

		if(layer_type.symmetric) {
			Layer left;
			Layer right;
			left.name = right.name = variation_itor->second.name;
			for(const VoxelPair& p : variation_itor->second.map) {
				if(p.first[0] < 0) {
					left.map.insert(p);
				} else {
					right.map.insert(p);
				}
			}

			children_.push_back(voxel_model_ptr(new voxel_model(left, layer_type)));
			children_.back()->name_ = "left_" + layer_type.name;
			children_.push_back(voxel_model_ptr(new voxel_model(right, layer_type)));
			children_.back()->name_ = "right_" + layer_type.name;

		} else {
			children_.push_back(voxel_model_ptr(new voxel_model(variation_itor->second, layer_type)));
		}
	}

	for(const Animation& anim : base.animations) {
		animations_[anim.name].reset(new Animation(anim));
	}
}

voxel_model::voxel_model(const Layer& layer, const LayerType& layer_type)
  : name_(layer_type.name), invalidated_(false), model_(1.0f), proto_model_(1.0f)
{
	for(const std::pair<std::string, VoxelPos>& pivot : layer_type.pivots) {
		glm::vec3 point = glm::vec3(pivot.second) + glm::vec3(0.5f);

		pivots_.push_back(std::pair<std::string, glm::vec3>(pivot.first, point));
	}

	vbo_id_.reset(new GLuint, [](GLuint* id){glDeleteBuffers(1, id); delete id;});
	glGenBuffers(1, vbo_id_.get());

	std::vector<GLfloat> varray[6];
	std::vector<GLubyte> carray[6];

	for(auto p : layer.map) {
		for(int n = FACE_LEFT; n != MAX_FACES; ++n) {
			glm::ivec3 pos = glm::ivec3(normal_vectors()[n]) + p.first;
			if(layer.map.find(pos) == layer.map.end()) {
				add_face(n, p, varray[n], carray[n]);
			}
		}
	}

	size_t total_size = 0;
	for(int n = FACE_LEFT; n != MAX_FACES; ++n) {
		vattrib_offsets_[n] = total_size;
		total_size += varray[n].size() * sizeof(GLfloat);
		num_vertices_[n] = varray[n].size() / 3;
	}
	for(int n = FACE_LEFT; n != MAX_FACES; ++n) {
		cattrib_offsets_[n] = total_size;
		total_size += carray[n].size() * sizeof(uint8_t);
	}
	glBindBuffer(GL_ARRAY_BUFFER, *vbo_id_);
	glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
	for(int n = FACE_LEFT; n != MAX_FACES; ++n) {
		glBufferSubData(GL_ARRAY_BUFFER, vattrib_offsets_[n], varray[n].size()*sizeof(GLfloat), &varray[n][0]);
	}
	for(int n = FACE_LEFT; n != MAX_FACES; ++n) {
		glBufferSubData(GL_ARRAY_BUFFER, cattrib_offsets_[n], carray[n].size()*sizeof(uint8_t), &carray[n][0]);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void voxel_model::add_face(int face, const VoxelPair& p, std::vector<GLfloat>& varray, std::vector<GLubyte>& carray)
{
	add_vertex_data(face, GLfloat(p.first.x), GLfloat(p.first.y), GLfloat(p.first.z), varray);
	// colors are all the same per vertex.
	for(int n = 0; n != 6; ++n) {
		carray.push_back(p.second.color.r());
		carray.push_back(p.second.color.g());
		carray.push_back(p.second.color.b());
		carray.push_back(p.second.color.a());
	}
}

void voxel_model::add_vertex_data(int face, GLfloat x, GLfloat y, GLfloat z, std::vector<GLfloat>& varray)
{
	switch(face) {
	case FACE_FRONT:
		varray.push_back(x); varray.push_back(y); varray.push_back(z+1);
		varray.push_back(x+1); varray.push_back(y); varray.push_back(z+1);
		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z+1);

		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z+1);
		varray.push_back(x); varray.push_back(y+1); varray.push_back(z+1);
		varray.push_back(x); varray.push_back(y); varray.push_back(z+1);
		break;
	case FACE_RIGHT:
		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z+1);
		varray.push_back(x+1); varray.push_back(y); varray.push_back(z+1);
		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z);

		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z);
		varray.push_back(x+1); varray.push_back(y); varray.push_back(z+1);
		varray.push_back(x+1); varray.push_back(y); varray.push_back(z);
		break;
	case FACE_TOP:
		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z+1);
		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z);
		varray.push_back(x); varray.push_back(y+1); varray.push_back(z+1);

		varray.push_back(x); varray.push_back(y+1); varray.push_back(z+1);
		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z);
		varray.push_back(x); varray.push_back(y+1); varray.push_back(z);
		break;
	case FACE_BACK:
		varray.push_back(x+1); varray.push_back(y); varray.push_back(z);
		varray.push_back(x); varray.push_back(y); varray.push_back(z);
		varray.push_back(x); varray.push_back(y+1); varray.push_back(z);

		varray.push_back(x); varray.push_back(y+1); varray.push_back(z);
		varray.push_back(x+1); varray.push_back(y+1); varray.push_back(z);
		varray.push_back(x+1); varray.push_back(y); varray.push_back(z);
		break;
	case FACE_LEFT:
		varray.push_back(x); varray.push_back(y+1); varray.push_back(z+1);
		varray.push_back(x); varray.push_back(y+1); varray.push_back(z);
		varray.push_back(x); varray.push_back(y); varray.push_back(z+1);

		varray.push_back(x); varray.push_back(y); varray.push_back(z+1);
		varray.push_back(x); varray.push_back(y+1); varray.push_back(z);
		varray.push_back(x); varray.push_back(y); varray.push_back(z);
		break;
	case FACE_BOTTOM:
		varray.push_back(x+1); varray.push_back(y); varray.push_back(z+1);
		varray.push_back(x); varray.push_back(y); varray.push_back(z+1);
		varray.push_back(x+1); varray.push_back(y); varray.push_back(z);

		varray.push_back(x+1); varray.push_back(y); varray.push_back(z);
		varray.push_back(x); varray.push_back(y); varray.push_back(z+1);
		varray.push_back(x); varray.push_back(y); varray.push_back(z);
		break;
	default: ASSERT_LOG(false, "voxel_model::add_vertex_data unexpected facing value: " << face);
	}
}

voxel_model_ptr voxel_model::get_child(const std::string& id) const
{
	for(const voxel_model_ptr& child : children_) {
		if(child->name() == id) {
			return child;
		}
	}

	ASSERT_LOG(false, "Could not find child in model: " << id);
	return voxel_model_ptr();
}

namespace
{
	void print_mat4(const std::string& s, const glm::mat4& m)
	{
		std::cerr << s;
		for(int i = 0; i != 4; i++) {
			std::cerr << std::endl;
			for(int j = 0; j != 4; j++) {
				std::cerr << m[i][j] << " ";
			}
		}
		std::cerr << std::endl;
	}
}

void voxel_model::attach_child(voxel_model_ptr child, const std::string& src_attachment, const std::string& dst_attachment)
{
	auto src_attach_itor = child->attachment_points_.find(src_attachment);
	auto dst_attach_itor = attachment_points_.find(dst_attachment);

	ASSERT_LOG(src_attach_itor != child->attachment_points_.end(), "Could not find attachment point: " << src_attachment);
	ASSERT_LOG(dst_attach_itor != attachment_points_.end(), "Could not find attachment point: " << dst_attachment);

	const AttachmentPoint& src_attach = src_attach_itor->second;
	const AttachmentPoint& dst_attach = dst_attach_itor->second;

	voxel_model_ptr src_model = child->get_child(src_attach.layer);
	voxel_model_ptr dst_model = get_child(dst_attach.layer);

	ASSERT_LOG(src_model, "Could not find source model: " << src_attach.layer);
	ASSERT_LOG(dst_model, "Could not find dest model: " << dst_attach.layer);

	const std::pair<std::string, glm::vec3>* src_pivot = NULL;
	const std::pair<std::string, glm::vec3>* dst_pivot = NULL;

	for(const std::pair<std::string, glm::vec3>& p : src_model->pivots_) {
		if(p.first == src_attach.pivot) {
			src_pivot = &p;
			break;
		}
	}

	ASSERT_LOG(src_pivot, "Could not find source pivot: " << src_pivot);

	for(const std::pair<std::string, glm::vec3>& p : dst_model->pivots_) {
		if(p.first == dst_attach.pivot) {
			dst_pivot = &p;
			break;
		}
	}

	ASSERT_LOG(dst_pivot, "Could not find source pivot: " << dst_pivot);

	child->clear_transforms();

	child->translate_geometry(dst_pivot->second - src_pivot->second);
	for(const AttachmentPointRotation& r : dst_attach_itor->second.rotations) {
		child->rotate_geometry(dst_pivot->second, dst_pivot->second + r.direction, r.amount);
	}
	
	child->set_prototype();
	dst_model->children_.push_back(child);
}

void voxel_model::set_prototype()
{
	proto_model_ = model_;
	for(auto child : children_) {
		child->set_prototype();
	}
}

void voxel_model::set_animation(const std::string& anim_str)
{
	boost::shared_ptr<Animation> anim = animations_[anim_str];
	ASSERT_LOG(anim, "Could not find animation " << anim_str);
	set_animation(anim);
}

void voxel_model::set_animation(boost::shared_ptr<Animation> anim)
{
	if(anim_) {
		old_anim_ = anim_;
		old_anim_time_ = anim_time_;
	} else {
		old_anim_.reset();
		old_anim_time_ = 0.0;
	}

	anim_ = anim;
	anim_time_ = 0.0;
}

void voxel_model::process_animation(GLfloat advance)
{
	if(!anim_) {
		return;
	}

	if(anim_->duration > 0 && anim_time_ > anim_->duration) {
		set_animation("stand");
	}

	anim_time_ += advance;

	const GLfloat TransitionTime = 0.5f;
	GLfloat ratio = 1.0f;

	if(old_anim_) {
		if(anim_time_ >= TransitionTime) {
			old_anim_.reset();
			old_anim_time_ = 0.0f;
		} else {
			old_anim_time_ += advance;
			if(old_anim_->duration > 0.0f && old_anim_time_ > old_anim_->duration) {
				old_anim_time_ = old_anim_->duration;
			}
			ratio = anim_time_ / TransitionTime;
		}
	}

	clear_transforms();

	game_logic::map_formula_callable_ptr callable(new game_logic::map_formula_callable);

	if(old_anim_) {
		callable->add("time", variant(decimal(old_anim_time_)));

		for(const AnimationTransform& transform : old_anim_->transforms) {
			if(transform.translation_formula) {
				const variant result = transform.translation_formula->execute(*callable);
				glm::vec3 translate = variant_to_vec3(result)*(1.0f - ratio);
				get_child(transform.layer)->accumulate_translation(translate);
			}

			if(!transform.rotation_formula) {
				continue;
			}

			GLfloat rotation = transform.rotation_formula->execute(*callable).as_decimal().as_float();
			get_child(transform.layer)->accumulate_rotation(transform.pivot_src, transform.pivot_dst, rotation*(1.0-ratio), transform.children_only);
		}
	}

	callable->add("time", variant(decimal(anim_time_)));

	for(const AnimationTransform& transform : anim_->transforms) {
		if(transform.translation_formula) {
			const variant result = transform.translation_formula->execute(*callable);
			glm::vec3 translate = variant_to_vec3(result) * ratio;
			get_child(transform.layer)->accumulate_translation(translate);
		}

		if(!transform.rotation_formula) {
			continue;
		}

		GLfloat rotation = GLfloat(transform.rotation_formula->execute(*callable).as_decimal().as_float());
		get_child(transform.layer)->accumulate_rotation(transform.pivot_src, transform.pivot_dst, rotation*ratio, transform.children_only);
	}

	generate_geometry();
}

void voxel_model::accumulate_rotation(const std::string& pivot_a, const std::string& pivot_b, GLfloat rotation, bool children_only)
{
	invalidated_ = true;

	int pivot_a_index = -1, pivot_b_index = -1;
	int index = 0;
	for(const std::pair<std::string, glm::vec3>& p : pivots_) {
		if(p.first == pivot_a) {
			pivot_a_index = index;
			break;
		}
		++index;
	}

	index = 0;
	for(const std::pair<std::string, glm::vec3>& p : pivots_) {
		if(p.first == pivot_b) {
			pivot_b_index = index;
			break;
		}
		++index;
	}

	ASSERT_LOG(pivot_a_index != -1 && pivot_b_index != -1, "Illegal pivot specification: " << pivot_a << " - " << pivot_b);

	if(pivot_a_index > pivot_b_index) {
		std::swap(pivot_a_index, pivot_b_index);
		rotation = -rotation;
	}

	for(Rotation& rotate : rotation_) {
		if(rotate.src_pivot == pivot_a_index && rotate.dst_pivot == pivot_b_index && rotate.children_only == children_only) {
			rotate.amount += rotation;
			return;
		}
	}

	Rotation new_rotation = { pivot_a_index, pivot_b_index, rotation, children_only };
	rotation_.push_back(new_rotation);
}

void voxel_model::accumulate_translation(const glm::vec3& translate)
{
	translation_ += translate;
	//std::cerr << "TRANSLATION: " << translation_[1] << "\n";
}

void voxel_model::generate_geometry()
{
	calculate_transforms();
	for(auto child : children_) {
		child->calculate_transforms();
	}
}

void voxel_model::clear_transforms()
{
	rotation_.clear();
	translation_ = glm::vec3(0.0f);
	invalidated_ = true;
	for(auto child : children_) {
		child->clear_transforms();
	}
}

void voxel_model::draw(graphics::lighting_ptr lighting, camera_callable_ptr camera, const glm::mat4& model) const
{
	for(auto child : children_) {
		child->draw(lighting, camera, model);
	}
	if(vbo_id_) {
		GLint cur_program;
		glGetIntegerv(GL_CURRENT_PROGRAM, &cur_program);

		static GLuint u_mvp = -1;
		if(u_mvp == -1) {
			u_mvp = glGetUniformLocation(cur_program, "mvp_matrix");
		}
		static GLuint u_normal = -1;
		if(u_normal == -1) {
			u_normal = glGetUniformLocation(cur_program, "u_normal");
		}
		static GLuint a_position = -1;
		if(a_position == -1) {
			a_position = glGetAttribLocation(cur_program, "a_position");
		}
		static GLuint a_color = -1;
		if(a_color == -1) {
			a_color = glGetAttribLocation(cur_program, "a_color");
		}

		glm::mat4 mdl = model * model_;
		glm::mat4 mvp = camera->projection_mat() * camera->view_mat() * mdl;
		glUniformMatrix4fv(u_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

		if(lighting) {
			lighting->set_modelview_matrix(mdl, camera->view_mat());
		}

		glBindBuffer(GL_ARRAY_BUFFER, *vbo_id_);
		glEnableVertexAttribArray(a_position);
		glEnableVertexAttribArray(a_color);
		for(int n = FACE_LEFT; n != MAX_FACES; ++n) {
			if(u_normal != -1) {
				glUniform3fv(u_normal, 1, glm::value_ptr(normal_vectors()[n]));
			}
			glVertexAttribPointer(a_position, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLfloat*>(vattrib_offsets_[n]));
			glVertexAttribPointer(a_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, reinterpret_cast<const GLfloat*>(cattrib_offsets_[n]));
			glDrawArrays(GL_TRIANGLES, 0, num_vertices_[n]);
		}
		glDisableVertexAttribArray(a_color);
		glDisableVertexAttribArray(a_position);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

void voxel_model::calculate_transforms()
{
	if(!invalidated_) {
		return;
	}
	reset_geometry();
	apply_transforms();
}

void voxel_model::reset_geometry()
{
	model_ = proto_model_;
	for(const voxel_model_ptr& child : children_) {
		child->reset_geometry();
	}
}

void voxel_model::apply_transforms()
{
	invalidated_ = false;

	translate_geometry(translation_);
	for(const Rotation& rotate : rotation_) {
		rotate_geometry(pivots_[rotate.src_pivot].second, pivots_[rotate.dst_pivot].second, rotate.amount, rotate.children_only);
	}

	for(const voxel_model_ptr& child : children_) {
		child->apply_transforms();
	}
}


void voxel_model::translate_geometry(const glm::vec3& amount)
{
	for(const voxel_model_ptr& child : children_) {
		child->translate_geometry(amount);
	}

	model_ = glm::translate(model_, amount);
}

void voxel_model::rotate_geometry(const glm::vec3& p1, const glm::vec3& p2, GLfloat amount, bool children_only)
{
	for(const voxel_model_ptr& child : children_) {
		child->rotate_geometry(p1, p2, amount);
	}

	if(children_only) {
		return;
	}

	model_ = glm::translate(glm::mat4(1.0f), p1) 
		* glm::rotate(glm::mat4(1.0f), amount, glm::normalize(p2 - p1)) 
		* glm::translate(glm::mat4(1.0f), -p1) 
		* model_;
}

BEGIN_DEFINE_CALLABLE_NOBASE(voxel_model)
DEFINE_FIELD(rotation, "null")
	return variant();
END_DEFINE_CALLABLE(voxel_model)

}

#endif
