/*
	Copyright (C) 2003-2013 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "geometry.hpp"

#include "DisplayDeviceFwd.hpp"
#include "ScopeableValue.hpp"
#include "SceneFwd.hpp"
#include "WindowManagerFwd.hpp"

namespace KRE
{
	class SceneTree;
	typedef std::shared_ptr<SceneTree> SceneTreePtr;
	typedef std::weak_ptr<SceneTree> WeakSceneTreePtr;

	typedef std::function<void(SceneTree*)> prerender_fn;

	class SceneTree : public std::enable_shared_from_this<SceneTree>
	{
	public:
		virtual ~SceneTree() {}
		SceneTreePtr getParent() const { return parent_.lock(); }
		SceneTreePtr getRoot() const { return root_.lock(); }

		void addObject(const SceneObjectPtr& obj) { objects_.emplace_back(obj); }
		void addEndObject(const SceneObjectPtr& obj) { objects_end_.emplace_back(obj); }
		void clearObjects() { objects_.clear(); objects_end_.clear(); }
		void removeObject(const SceneObjectPtr& obj);
		void addChild(const SceneTreePtr& child) { children_.emplace_back(child); }

		void preRender(const WindowPtr& wnd);
		void render(const WindowPtr& wnd) const;

		void setPosition(const glm::vec3& position);
		void setPosition(float x, float y, float z=0.0f);
		void setPosition(int x, int y, int z=0);
		const glm::vec3& getPosition() const { return position_; }

		void offsetPosition(const glm::vec3& position);
		void offsetPosition(float x, float y, float z=0.0f);
		void offsetPosition(int x, int y, int z=0);

		void setRotation(float angle, const glm::vec3& axis);
		void setRotation(const glm::quat& rot);
		const glm::quat& getRotation() const { return rotation_; }

		void setScale(float xs, float ys, float zs=1.0f);
		void setScale(const glm::vec3& scale);
		const glm::vec3& getScale() const { return scale_; }

		void clearRenderTargets() { render_targets_.clear(); }
		void addRenderTarget(const RenderTargetPtr& render_target) { render_targets_.emplace_back(render_target); }
		const std::vector<RenderTargetPtr>& getRenderTargets() const { return render_targets_; }

		void setCamera(const CameraPtr& cam) { camera_ = cam; }
		const CameraPtr& getCamera() const { return camera_; }

		// This is a third party matrix set, it is applied to content *before* any translation/rotation/scaling set on us.
		const glm::mat4& getModelMatrix() const { return model_matrix_; }
		void setModelMatrix(const glm::mat4& m) { model_matrix_ = m; model_changed_ = true; }

		// recursively clear objects and render targets.
		void clear();

		void setClipRect(const rect& r) { clip_rect_.reset(new rect(r)); }
		const rect& getClipRect() const { static rect empty_rect; return clip_rect_ ? *clip_rect_ : empty_rect; }
		void clearClipRect() { clip_rect_.reset(); }

		void setClipShape(const RenderablePtr& r) { clip_shape_ = r; }
		void clearClipShape() { clip_shape_ = nullptr; }

		static SceneTreePtr create(SceneTreePtr parent);

		// callback used during pre-render.
		prerender_fn setOnPreRenderFunction(prerender_fn fn) { auto current_fn = pre_render_fn_ ; pre_render_fn_ = fn; return current_fn; }
	protected:
		explicit SceneTree(const SceneTreePtr& parent);
	private:

		WeakSceneTreePtr root_;
		WeakSceneTreePtr parent_;
		std::vector<SceneTreePtr> children_;
		std::vector<SceneObjectPtr> objects_;
		std::vector<SceneObjectPtr> objects_end_;

		ScopeableValue scopeable_;
		CameraPtr camera_;
		std::vector<RenderTargetPtr> render_targets_;
		
		// arbitrary shape to used as for clipping.
		RenderablePtr clip_shape_;
		// rectangle to use for clipping.
		std::unique_ptr<rect> clip_rect_;

		glm::vec3 position_;
		glm::quat rotation_;
		glm::vec3 scale_;

		glm::vec3 offset_position_;

		mutable bool model_changed_;
		glm::mat4 model_matrix_;
		mutable glm::mat4 cached_model_matrix_;

		ColorPtr color_;

		prerender_fn pre_render_fn_;
	};

	const glm::vec3& get_xaxis();
	const glm::vec3& get_yaxis();
	const glm::vec3& get_zaxis();
	const glm::mat4& get_identity_matrix();
}
