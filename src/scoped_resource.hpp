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

#include <cstdio>

#include <functional>

namespace util
{
	struct scope_manager
	{
		scope_manager(std::function<void()> on_enter, std::function<void()> on_exit)
		  : on_exit_(on_exit)
		{
			on_enter();
		}

		~scope_manager() {
			on_exit_();
		}

		std::function<void()> on_exit_;
	};

	/**
	* A class template, scoped_resource, designed to
	* implement the Resource Acquisition Is Initialization (RAII) approach
	* to resource management. scoped_resource is designed to be used when
	* a resource is initialized at the beginning or middle of a scope,
	* and released at the end of the scope. The template argument
	* ReleasePolicy is a functor which takes an argument of the
	* type of the resource, and releases it.
	*
	* Usage example, for working with files:
	*
	* @code
	* struct close_file { void operator()(int fd) const {close(fd);} };
	* ...
	* {
	*    const scoped_resource<int,close_file> file(open("file.txt",O_RDONLY));
	*    read(file, buf, 1000);
	* } // file is automatically closed here
	* @endcode
	*
	* Note that scoped_resource has an explicit constructor, and prohibits
	* copy-construction, and thus the initialization syntax, rather than
	* the assignment syntax must be used when initializing.
	*
	* i.e. using scoped_resource<int,close_file> file = open("file.txt",O_RDONLY);
	* in the above example is illegal.
	*
	*/
	template<typename T,typename ReleasePolicy>
	class scoped_resource
	{
		T resource;

		//prohibited operations
		scoped_resource(const scoped_resource&);
		scoped_resource& operator=(const scoped_resource&);
	public:
		typedef T resource_type;
		typedef ReleasePolicy release_type;

	  /**
	  * Constructor
		*
		* @ param res This is the resource to be managed
	  */
		scoped_resource(resource_type res = resource_type())
				: resource(res) {}

	  /**
	  * The destructor is the main point in this class. It takes care of proper
		* deletion of the resource, using the provided release policy.
		*/
		~scoped_resource()
		{
			release_type()(resource);
		}

	  /**
	  * This operator makes sure you can access and use the scoped_resource
		* just like you were using the resource itself.
	  *
	  * @ret the underlying resource
		*/
		operator resource_type() const { return resource; }

	  /**
	  * This function provides explicit access to the resource. Its behaviour
	  * is identical to operator resource_type()
		*
		* @ret the underlying resource
	  */
		resource_type get() const { return resource; }

		/**
	  * This function provides convenient direct access to the -> operator
	  * if the underlying resource is a pointer. Only call this function
	  * if resource_type is a pointer type.
	  */
		resource_type operator->() const { return resource; }

		void assign(const resource_type& o) {
			release_type()(resource);
			resource = o;
		}
	};

	/**
	* A helper policy for scoped_ptr.
	* It will call the delete operator on a pointer, and assign the pointer to 0
	*/
	struct delete_item {
		template<typename T>
		void operator()(T*& p) const { delete p; p = 0; }
	};
	/**
	* A helper policy for scoped_array.
	* It will call the delete[] operator on a pointer, and assign the pointer to 0
	*/
	struct delete_array {
		template<typename T>
		void operator()(T*& p) const { delete [] p; p = 0; }
	};

	/**
	* A class which implements an approximation of
	* template<typename T>
	* typedef scoped_resource<T*,delete_item> scoped_ptr<T>;
	*
	* It is a convenient synonym for a common usage of @ref scoped_resource.
	* See scoped_resource for more details on how this class behaves.
	*
	* Usage example:
	* @code
	* {
	*    const scoped_ptr<Object> ptr(new Object);
	*    ...use ptr as you would a normal Object*...
	* } // ptr is automatically deleted here
	* @endcode
	*
	* NOTE: use this class only to manage a single object, *never* an array.
	* Use scoped_array to manage arrays. This distinction is because you
	* may call delete only on objects allocated with new, delete[] only
	* on objects allocated with new[].
	*/
	template<typename T>
	struct scoped_ptr : public scoped_resource<T*,delete_item>
	{
		explicit scoped_ptr(T* p) : scoped_resource<T*,delete_item>(p) {}
	};

	/**
	* This class has identical behaviour to @ref scoped_ptr, except it manages
	* heap-allocated arrays instead of heap-allocated single objects
	*
	* Usage example:
	* @code
	* {
	*    const scoped_array<char> ptr(new char[n]);
	*    ...use ptr as you would a normal char*...
	* } // ptr is automatically deleted here
	* @endcode
	*
	*/
	template<typename T>
	struct scoped_array : public scoped_resource<T*,delete_array>
	{
		explicit scoped_array(T* p) : scoped_resource<T*,delete_array>(p) {}
	};

	/**
	 * This class specializes the scoped_resource to implement scoped FILEs. Not
	 * sure this is the best place to place such an utility, though.
	 */
	struct close_FILE
	{
		void operator()(FILE* f) const { if(f != nullptr) { fclose(f); } }
	};
	typedef scoped_resource<FILE*,close_FILE> scoped_FILE;
}
