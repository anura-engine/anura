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

#include <string>
#include <vector>

#include "RenderFwd.hpp"

#include "xhtml_fwd.hpp"
#include "xhtml_node.hpp"
#include "css_styles.hpp"

namespace xhtml
{
	class ObjectProxy : public std::enable_shared_from_this<ObjectProxy>
	{
	public:
		explicit ObjectProxy(const AttributeMap& attributes);
		virtual ~ObjectProxy() {}
		virtual void init() {}
		virtual void process(float dt) {}
		virtual KRE::SceneObjectPtr getRenderable() = 0;
		int width() const { return width_; }
		int height () const { return height_; }
		void setDimensions(int w, int h);
		rect getDimensions() { return rect(0, 0, width_, height_); }
		bool areDimensionsFixed() const { return dimensions_fixed_; }

		virtual bool mouseButtonUp(const point& p, int button, unsigned button_state, unsigned short ctrl_key_state) { return false; }
		virtual bool mouseButtonDown(const point& p, int button, unsigned button_state, unsigned short ctrl_key_state) { return false; }
		virtual bool mouseMove(const point& p, unsigned button_state, unsigned ctrl_key_state) { return false; }
		virtual bool keyDown(const point& p, const Keystate& keysym, bool pressed, bool repeat) { return false; }
		virtual bool keyUp(const point& p, const Keystate& keysym, bool pressed, bool repeat) { return false; }
	private:
		int width_;
		int height_;
		bool dimensions_fixed_;
	};

	typedef std::shared_ptr<ObjectProxy> ObjectProxyPtr;

	typedef std::function<ObjectProxyPtr(const AttributeMap& attributes)> object_create_fn;

	// XXX should cache class, id, xml:id, lang, dir in the class structure.
	class Element : public Node
	{
	public:
		virtual ~Element();
		static ElementPtr create(const std::string& name, WeakDocumentPtr owner=WeakDocumentPtr());
		std::string toString() const override;
		ElementId getElementId() const { return tag_; }
		const std::string& getTag() const override { return name_; }
		const std::string& getName() const { return name_; }
		bool hasTag(const std::string& tag) const { return tag == name_; }
		bool hasTag(ElementId tag) const { return tag == tag_; }

		static void registerObjectHandler(const std::string& content_type, object_create_fn fn);
	protected:
		explicit Element(ElementId id, const std::string& name, WeakDocumentPtr owner);
		std::string name_;
		ElementId tag_;
	};

	void add_custom_element(const std::string& e);

	struct ObjectProxyRegistrar
	{
		explicit ObjectProxyRegistrar(const std::string& type, object_create_fn fn)
		{
			Element::registerObjectHandler(type, fn);
		}
	};
}
