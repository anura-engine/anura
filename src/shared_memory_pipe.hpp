#pragma once

#include <deque>

#include <memory>
#include <boost/interprocess/shared_memory_object.hpp>

#include <string>
#include <vector>

class SharedMemoryPipeManager
{
public:
	static void createNamedPipe(const std::string& name);
	SharedMemoryPipeManager() {}
	~SharedMemoryPipeManager();
private:
};

class SharedMemoryPipe;
typedef std::shared_ptr<SharedMemoryPipe> SharedMemoryPipePtr;

class SharedMemoryPipe
{
public:
	//make a pair of pipes which are just within this process.
	static std::pair<SharedMemoryPipePtr,SharedMemoryPipePtr> makeInMemoryPair();

	SharedMemoryPipe(const std::string& name, bool server);


	void process();

	void write(const std::string& msg);
	void read(std::vector<std::string>& msg);
private:

	SharedMemoryPipe();

	typedef std::shared_ptr<boost::interprocess::shared_memory_object> ShmPtr;
	ShmPtr in_, out_;

	typedef std::shared_ptr<boost::interprocess::mapped_region> MappedRegionPtr;
	MappedRegionPtr in_region_, out_region_;

	uint8_t* in_addr_, *out_addr_;

	std::vector<std::shared_ptr<std::vector<uint8_t>>> buffers_;

	std::deque<std::string> in_queue_, out_queue_;
};
