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

#include "xhtml.hpp"
#include "variant.hpp"

namespace xhtml
{
	enum class EventHandlerId {
		//CLICK,
		//DBL_CLICK,
		MOUSE_DOWN,
		MOUSE_UP,
		MOUSE_MOVE,
		MOUSE_ENTER,
		MOUSE_LEAVE,
		//MOUSE_OVER,
		//MOUSE_OUT,
		KEY_PRESS,
		KEY_UP,
		KEY_DOWN,
		LOAD,
		UNLOAD,
		RESIZE,
		WHEEL,
		//BLUR,
		//SCROLL,
		//FOCUS,
		//FOCUSIN,
		//FOCUSOUT,
		//SELECT,
		//ERROR,
		//COMPOSITIONSTART,
		//COMPOSITIONUPDATE,
		//COMPOSITIONEND,
		//ABORT,
		//BEFOREINPUT,
		//INPUT,

		MAX_EVENT_HANDLERS,
	};

	class Script : public std::enable_shared_from_this<Script>
	{
	public:
		Script();
		virtual ~Script() {}
		virtual void runScriptFile(const std::string& filename) = 0;
		virtual void runScript(const std::string& script) = 0;
		virtual void preProcess(const NodePtr& element, EventHandlerId evtname, const std::string& script) = 0;
		void addEventHandler(const NodePtr& element, EventHandlerId evtname, const std::string& script);
		virtual void runEventHandler(const NodePtr& element, EventHandlerId evtname, const variant& params) = 0;
	private:
	};
}
