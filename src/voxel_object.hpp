/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#pragma once

/* XXX This needs re-written.

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "variant.hpp"
#include "voxel_model.hpp"
#include "widget_factory.hpp"

class Level;

namespace voxel
{
	class voxel_object : public game_logic::FormulaCallable
	{
	public:
		voxel_object(const std::string& type, float x, float y, float z);
		explicit voxel_object(const variant& node);
		virtual ~voxel_object();

		const std::string& type() const { return type_; }
		bool isA(const std::string& type) const { return type_ == type; }

		void draw() const;
		virtual void process(Level& lvl);
		bool handle_sdl_event(const SDL_Event& event, bool claimed);

		const glm::vec3& translation() const { return translation_; } 
		const glm::vec3& rotation() const { return rotation_; } 
		const glm::vec3& scale() const { return scale_; } 

		float x() const { return translation_.x; }
		float y() const { return translation_.y; }
		float z() const { return translation_.z; }

		float rotation_x() const { return rotation_.x; }
		float rotation_y() const { return rotation_.y; }
		float rotation_z() const { return rotation_.z; }

		float scale_x() const { return scale_.x; }
		float scale_y() const { return scale_.y; }
		float scale_z() const { return scale_.z; }

		voxel_model_ptr& model() { return model_; }
		const voxel_model_ptr& model_const() { return model_; }

		void addWidget(const gui::WidgetPtr& w);
		void addWidgets(std::vector<gui::WidgetPtr>* widgets);
		void clearWidgets();
		void removeWidget(gui::WidgetPtr w);
		gui::WidgetPtr getWidgetById(const std::string& id);
		gui::ConstWidgetPtr getWidgetById(const std::string& id) const;
		std::vector<variant> getVariantWidgetList() const;

		bool paused() const { return paused_; }
		void set_paused(bool p=true);

		size_t cycle() const { return cycle_; }

		bool pt_in_object(const glm::vec3& pt);

		void set_event_arg(variant v);

		bool is_mouseover_object() const { return is_mouseover_; }
		void set_mouseover_object(bool mo=true) { is_mouseover_ = mo; }

		void addScheduledCommand(int cycle, variant cmd);
		std::vector<variant> popScheduledCommands();

	private:
		DECLARE_CALLABLE(voxel_object);

		std::string type_;

		bool paused_;

		size_t cycle_;

		glm::vec3 translation_;
		glm::vec3 rotation_;
		glm::vec3 scale_;

		voxel_model_ptr model_;

		typedef std::set<gui::WidgetPtr, gui::WidgetSortZOrder> widget_list;
		widget_list widgets_;	

		bool is_mouseover_;

		typedef std::pair<int, variant> ScheduledCommand;
		std::vector<ScheduledCommand> scheduled_commands_;

		// XXX hack
		variant event_arg_;

		voxel_object();
	};

	typedef ffl::IntrusivePtr<voxel_object> voxel_object_ptr;
	typedef ffl::IntrusivePtr<const voxel_object> const_voxel_object_ptr;
}

namespace voxel_object_factory
{
	voxel::voxel_object_ptr create(const variant& node);
}
*/