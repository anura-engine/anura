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

#include <map>
#include <memory>
#include <string>

namespace KRE
{
	class UniformHardwareInterface
	{
	public:
		explicit UniformHardwareInterface(const std::string& name);
		virtual ~UniformHardwareInterface();
		virtual void update(void* buffer, int size) = 0;
		const std::string& getName() const { return name_; }
	private:
		std::string name_;
		UniformHardwareInterface();
	};

	typedef std::map<std::string, std::ptrdiff_t> uniform_mapping;

	class UniformBufferBase
	{
	public:
		explicit UniformBufferBase(const std::string& name);
		virtual ~UniformBufferBase();
		void setHardware(std::unique_ptr<UniformHardwareInterface>&& impl) { impl_ = std::move(impl); }
		const std::string& getName() const { return name_; }
		UniformBufferBase(UniformBufferBase&&);
	private:
		UniformBufferBase();
		UniformBufferBase(const UniformBufferBase&);
		void operator=(const UniformBufferBase&);

		std::unique_ptr<UniformHardwareInterface> impl_;
		std::string name_;
	};

	template<typename T>
	class UniformBuffer : public UniformBufferBase
	{
	public:
		UniformBuffer(const std::string& name, const T& u) : UniformBufferBase(name), uniforms_(u) {}
		void setMapping(const uniform_mapping& map) { mapping_ = map; }
		void setMapping(uniform_mapping* map) { mapping_.swap(*map); }
	private:
		const T& uniforms_;
		uniform_mapping mapping_;
	};
}
