/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
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

#include <boost/intrusive_ptr.hpp>

namespace ffl
{

#ifdef DEBUG_GARBAGE_COLLECTOR
	void registerIntrusivePtr(void*);
	void unregisterIntrusivePtr(void*);
#endif

	template<typename T>
	class IntrusivePtr : public boost::intrusive_ptr<T>
	{
	public:
		IntrusivePtr() : boost::intrusive_ptr<T>()
		{
#ifdef DEBUG_GARBAGE_COLLECTOR
			registerIntrusivePtr(this);
#endif
		}

		IntrusivePtr(T* p) : boost::intrusive_ptr<T>(p)
		{
#ifdef DEBUG_GARBAGE_COLLECTOR
			registerIntrusivePtr(this);
#endif
		}

		template<typename Y>
		IntrusivePtr(const IntrusivePtr<Y>& p) : boost::intrusive_ptr<T>(p)
		{
#ifdef DEBUG_GARBAGE_COLLECTOR
			registerIntrusivePtr(this);
#endif
		}

		template<typename Y>
		IntrusivePtr(const boost::intrusive_ptr<Y>& p) : boost::intrusive_ptr<T>(p)
		{
#ifdef DEBUG_GARBAGE_COLLECTOR
			registerIntrusivePtr(this);
#endif
		}

		~IntrusivePtr()
		{
#ifdef DEBUG_GARBAGE_COLLECTOR
			unregisterIntrusivePtr(this);
#endif
		}

		template<typename Y>
		IntrusivePtr& operator=(const IntrusivePtr<Y>& p) {
			const boost::intrusive_ptr<Y>& o = p;
			this->boost::intrusive_ptr<T>::operator=(o);
			return *this;
		}

		template<typename Y>
		IntrusivePtr& operator=(const boost::intrusive_ptr<Y>& p) {
			this->boost::intrusive_ptr<T>::operator=(p);
			return *this;
		}
	};
}
