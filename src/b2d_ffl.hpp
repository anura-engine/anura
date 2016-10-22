/*
	Copyright (C) 2012-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#ifdef USE_BOX2D

#include <Box2D/Box2D.h>
#include "intrusive_ptr.hpp"

#include <vector>
#include <map>

#include "entity.hpp"
#include "formula_callable.hpp"
#include "geometry.hpp"
#include "variant.hpp"

namespace box2d
{
	class manager
	{
	public:
		manager();
		~manager();
	};

	class world;
	typedef ffl::IntrusivePtr<world> world_ptr;
	typedef ffl::IntrusivePtr<const world> const_world_ptr;
	class body;
	typedef ffl::IntrusivePtr<body> body_ptr;
	typedef ffl::IntrusivePtr<const body> const_body_ptr;
	class joint;
	typedef ffl::IntrusivePtr<joint> joint_ptr;
	typedef ffl::IntrusivePtr<const joint> const_joint_ptr;

	class destruction_listener : public b2DestructionListener
	{
	public:
		destruction_listener();
		void SayGoodbye(b2Joint* joint);
		void SayGoodbye(b2Fixture* fix);
	};

	class debug_draw : public b2Draw
	{
	public:
		debug_draw();
		void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color);
		void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color);
		void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color);
		void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color);
		void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color);
		void DrawTransform(const b2Transform& xf);
		void DrawPoint(const b2Vec2& p, float32 size, const b2Color& color);
		void DrawString(int x, int y, const char* string, ...); 
		void DrawAABB(b2AABB* aabb, const b2Color& color);
	};

	class body : public game_logic::FormulaCallable
	{
	public:
		explicit body(const variant& b);
		virtual ~body();
		const b2Body& get_body() const { return *body_; }
		b2Body& get_body() { return *body_; }
		const std::shared_ptr<const b2Body> get_body_ptr() const { return body_; }
		std::shared_ptr<b2Body> get_body_ptr() { return body_; }
		b2Body* get_raw_body_ptr() { return body_.get(); }
		const b2BodyDef* get_body_definition() const { return &body_def_; }

		virtual variant getValue(const std::string&) const;
		virtual void setValue(const std::string& key, const variant& value);

		bool active() const;
		void set_active(bool actv=true);

		void finishLoading(EntityPtr e=nullptr);
		std::shared_ptr<b2FixtureDef> create_fixture(const variant& fix);

		variant write();
		variant fix_write();
		variant shape_write(const b2Shape* shape);
	protected:
	private:
		b2BodyDef body_def_;
		std::vector<std::shared_ptr<b2FixtureDef> > fix_defs_;
		std::vector<std::shared_ptr<b2Shape> > shape_list_;
		std::shared_ptr<b2Body> body_;
	};

	class joint : public game_logic::FormulaCallable
	{
	public:
		explicit joint(b2Joint* j);
		virtual variant getValue(const std::string& key) const;
		virtual void setValue(const std::string& key, const variant& value);
		
		b2Joint* get_b2Joint() { return joint_; }
	private:
		b2Joint* joint_;
	};

	class world : public game_logic::FormulaCallable
	{
	public:
		world(const variant& w);
		virtual ~world();
		const b2World& get_world() const { return world_; }
		b2World& get_world() { return world_; }

		virtual variant getValue(const std::string&) const;
		virtual void setValue(const std::string& key, const variant& value);

		void finishLoading();
		void step(float time_step);

		joint_ptr find_joint_by_id(const std::string& key) const;

		float x1() const { return world_x1_; }
		float x2() const { return world_x2_; }
		float y1() const { return world_y1_; }
		float y2() const { return world_y2_; }

		float last_dt() const { return last_dt_; }
		float last_inv_dt() const { return last_inv_dt_; }
		void set_dt(float time_step);

		variant write();

		static b2World& current();
		static b2World* getCurrentPtr();
		static const world& our_world();
		static world_ptr our_world_ptr();

		void set_as_current_world();
		static void clear_current_world();

		b2Body* create_body(body*);

		int scale() const { return pixel_scale_; }
		void set_scale(int scale) { pixel_scale_ = scale; }

		bool draw_debug_data() const { return draw_debug_data_; }
		void enable_draw_debug_data(bool draw=true) { draw_debug_data_ = draw; }
	protected:
	private:
		int velocity_iterations_;
		int position_iterations_;
		b2World world_;

		float world_x1_, world_y1_;
		float world_x2_, world_y2_;

		float last_dt_;
		float last_inv_dt_;

		int pixel_scale_;

		bool draw_debug_data_;
		debug_draw debug_draw_;

		destruction_listener destruction_listener_;
	};
}

#endif
