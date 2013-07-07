#ifndef VOXEL_MODEL_HPP_INCLUDED
#define VOXEL_MODEL_HPP_INCLUDED

#include <map>
#include <vector>

#include "color_utils.hpp"
#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "formula.hpp"
#include "variant.hpp"

#include <boost/array.hpp>
#include <boost/intrusive_ptr.hpp>

#include <glm/glm.hpp>

namespace voxel {

typedef boost::array<int, 3> VoxelPos;
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
};

typedef std::map<VoxelPos, Voxel> VoxelMap;
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

	voxel_model_ptr build_instance() const;
	voxel_model_ptr get_child(const std::string& id) const;

	void attach_child(voxel_model_ptr child, const std::string& src_attachment, const std::string& dst_attachment);


	void set_animation(boost::shared_ptr<Animation> anim);
	void set_animation(const std::string& id);
	void process_animation(GLfloat advance=0.02);

	const std::map<std::string, boost::shared_ptr<Animation> >& animations() const { return animations_; }

	void accumulate_rotation(const std::string& pivot_a, const std::string& pivot_b, GLfloat rotation, bool children_only=false);
	void accumulate_translation(const glm::vec3& translate);
	void clear_transforms();

	const std::string& name() const { return name_; }

	void generate_geometry(std::vector<GLfloat>* vertexes, std::vector<GLfloat>* normals, std::vector<GLfloat>* colors);
private:
	DECLARE_CALLABLE(voxel_model);

	void calculate_transforms();
	void apply_transforms();

	void reset_geometry();
	void translate_geometry(const glm::vec3& amount);
	void rotate_geometry(const glm::vec3& p1, const glm::vec3& p2, GLfloat amount, bool children_only=false);

	std::string name_;

	std::vector<std::pair<std::string, glm::vec3> > pivots_;

	struct Rotation {
		int src_pivot, dst_pivot;
		GLfloat amount;
		bool children_only;
	};
	std::vector<Rotation> rotation_;
	glm::vec3 translation_;
	std::vector<glm::vec3> vertexes_;

	int add_vertex(const glm::vec3& vertex);

	struct Face {
		boost::array<int, 4> geometry;
		graphics::color color;
	};

	std::vector<Face> faces_;

	const_voxel_model_ptr prototype_;
	std::vector<voxel_model_ptr> children_;

	boost::shared_ptr<Animation> anim_, old_anim_;
	GLfloat anim_time_, old_anim_time_;

	std::map<std::string, boost::shared_ptr<Animation> > animations_;
	std::map<std::string, AttachmentPoint> attachment_points_;

	bool invalidated_;
};

}

#endif
