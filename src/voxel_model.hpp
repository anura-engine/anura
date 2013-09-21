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
#pragma once
#if defined(USE_SHADERS) && defined(USE_ISOMAP)

#include <map>
#include <vector>

#include "camera.hpp"
#include "color_utils.hpp"
#include "decimal.hpp"
#include "lighting.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula.hpp"
#include "variant.hpp"

#include <boost/array.hpp>
#include <boost/unordered_map.hpp>
#include <boost/intrusive_ptr.hpp>

#include <glm/glm.hpp>

namespace voxel {

typedef glm::ivec3 VoxelPos;
void get_voxel_pos_adjacent(const VoxelPos& pos, VoxelPos* adj);
struct Voxel {
	Voxel() : nlayer(-1) {}
	graphics::color color;
	int nlayer;
};

inline bool operator==(const Voxel& a, const Voxel& b) {
	return a.color.value() == b.color.value();
}

inline bool operator!=(const Voxel& a, const Voxel& b) {
	return a.color.value() != b.color.value();
}

struct VoxelArea {
	VoxelPos top_left, bot_right;
	bool voxel_in_area(const VoxelPos& pos) const;
};

bool operator==(VoxelPos const& p1, VoxelPos const& p2);
bool operator<(VoxelPos const& p1, VoxelPos const& p2);

struct VoxelPosLess
{
	bool operator()(const VoxelPos& a, const VoxelPos& b) const;
};
struct VoxelPosHash
{
    std::size_t operator()(VoxelPos const& p) const
    {
		std::size_t seed = 0;
		boost::hash_combine(seed, p.x);
		boost::hash_combine(seed, p.y);
		boost::hash_combine(seed, p.z);
		return seed;
    }
}; 
typedef std::map<VoxelPos, Voxel, VoxelPosLess> VoxelMap;
typedef std::pair<VoxelPos, Voxel> VoxelPair;

variant write_voxels(const std::vector<VoxelPos>& positions, const Voxel& voxel);
void read_voxels(const variant& v, VoxelMap* out);

struct AnimationTransform {
	std::string layer;
	std::string pivot_src, pivot_dst;
	game_logic::formula_ptr rotation_formula, translation_formula;
	bool children_only;
};

struct Animation {
	std::string name;
	std::vector<AnimationTransform> transforms;
	GLfloat duration;
};

Animation read_animation(const variant& v);
variant write_animation(const Animation& anim);

struct Layer {
	std::string name;
	VoxelMap map;
};

struct LayerType {
	LayerType() : symmetric(false) {}
	std::string name;
	std::string last_edited_variation;
	bool symmetric;
	std::map<std::string, Layer> variations;
	std::map<std::string, VoxelPos> pivots;
};

struct AttachmentPointRotation {
	glm::vec3 direction;
	GLfloat amount;
};

struct AttachmentPoint {
	std::string name, layer, pivot;
	std::vector<AttachmentPointRotation> rotations;
};

std::map<std::string, AttachmentPoint> read_attachment_points(const variant& v);
variant write_attachment_points(const std::map<std::string, AttachmentPoint>& m);

struct Model {
	std::vector<LayerType> layer_types;
	std::vector<Animation> animations;
	std::map<std::string, AttachmentPoint> attachment_points;
	VoxelPos feet_position;
	decimal scale;
};

LayerType read_layer_type(const variant& v);

Model read_model(const variant& v);
variant write_model(const Model& model);

class voxel_model;
typedef boost::intrusive_ptr<voxel_model> voxel_model_ptr;
typedef boost::intrusive_ptr<const voxel_model> const_voxel_model_ptr;

class voxel_model : public game_logic::formula_callable
{
public:
	explicit voxel_model(const variant& node);
	voxel_model(const Layer& layer, const LayerType& layer_type);

	voxel_model_ptr get_child(const std::string& id) const;

	void add_face(int face, const VoxelPair& p, std::vector<GLfloat>& varray, std::vector<GLubyte>& carray);
	void add_vertex_data(int face, GLfloat x, GLfloat y, GLfloat z, std::vector<GLfloat>& varray);

	void attach_child(voxel_model_ptr child, const std::string& src_attachment, const std::string& dst_attachment);

	std::string current_animation() const { return anim_ ? anim_->name : ""; }
	void set_animation(boost::shared_ptr<Animation> anim);
	void set_animation(const std::string& id);
	void process_animation(GLfloat advance=0.02);

	const std::map<std::string, boost::shared_ptr<Animation> >& animations() const { return animations_; }

	void accumulate_rotation(const std::string& pivot_a, const std::string& pivot_b, GLfloat rotation, glm::vec3 translation, bool children_only=false);
	void clear_transforms();

	void draw(graphics::lighting_ptr lighting, camera_callable_ptr camera, const glm::mat4& model) const;

	const std::string& name() const { return name_; }

	void get_bounding_box(glm::vec3& b1, glm::vec3& b2);
private:
	DECLARE_CALLABLE(voxel_model);

	enum {
		FACE_LEFT,
		FACE_RIGHT,
		FACE_TOP,
		FACE_BOTTOM,
		FACE_BACK,
		FACE_FRONT,
		MAX_FACES,
	};

	void calculate_transforms();
	void apply_transforms();

	void reset_geometry();
	void generate_geometry();
	void translate_geometry(const glm::vec3& amount, bool children_only=false);
	void rotate_geometry(const glm::vec3& p1, const glm::vec3& p2, GLfloat amount, bool children_only=false);

	void set_prototype();

	std::string name_;

	std::vector<std::pair<std::string, glm::vec3> > pivots_;

	struct Rotation {
		glm::vec3 translation_;
		int src_pivot, dst_pivot;
		GLfloat amount;
		bool children_only;
	};
	std::vector<Rotation> rotation_;
	std::vector<glm::vec3> vertexes_;

	std::vector<voxel_model_ptr> children_;

	boost::shared_ptr<Animation> anim_, old_anim_;
	GLfloat anim_time_, old_anim_time_;

	std::map<std::string, boost::shared_ptr<Animation> > animations_;
	std::map<std::string, AttachmentPoint> attachment_points_;

	bool invalidated_;

	typedef boost::shared_ptr<GLuint> vbo_ptr;
	vbo_ptr vbo_id_;
	size_t vattrib_offsets_[6];
	size_t cattrib_offsets_[6];
	size_t num_vertices_[6];

	glm::vec3 aabb_[2];

	glm::mat4 proto_model_;
	glm::mat4 model_;

	glm::vec3 feet_;
};

}

#endif
