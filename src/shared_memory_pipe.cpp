#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>

#include <iostream>
#include <stdlib.h>

#include "asserts.hpp"
#include "shared_memory_pipe.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

using namespace boost::interprocess;

namespace {
std::vector<std::string> g_shm_objects;

static const size_t RegionSize = 1024*1024*10 - sizeof(uint32_t) - sizeof(interprocess_mutex);
static const size_t AllocSize = 1024*1024*10;

class RegionTryLock
{
	uint8_t* region_;
public:
	RegionTryLock(uint8_t* region) : region_(region) {
		interprocess_mutex* m = reinterpret_cast<interprocess_mutex*>(region_);
		bool res = m->try_lock();
		if(!res) {
			region_ = nullptr;
		}
	}

	~RegionTryLock() {
		if(region_ != nullptr) {
			interprocess_mutex* m = reinterpret_cast<interprocess_mutex*>(region_);
			m->unlock();
		}
	}

	bool hasLock() const { return region_ != nullptr; }
};

void formatSharedMemory(uint8_t* ptr)
{
	uint32_t len = 0;
	new (ptr) interprocess_mutex;
	ptr += sizeof(interprocess_mutex);
	memcpy(ptr, &len, sizeof(len));
}

}

void SharedMemoryPipeManager::createNamedPipe(const std::string& name)
{
	shared_memory_object shm(create_only, name.c_str(), read_write);

	shm.truncate(AllocSize);

	mapped_region region(shm, read_write, 0, 1024);

	uint8_t* ptr = reinterpret_cast<uint8_t*>(region.get_address());
	formatSharedMemory(ptr);
}

SharedMemoryPipeManager::~SharedMemoryPipeManager()
{
	for(auto s : g_shm_objects) {
		shared_memory_object::remove(s.c_str());
	}

	g_shm_objects.clear();
}

std::pair<SharedMemoryPipePtr,SharedMemoryPipePtr> SharedMemoryPipe::makeInMemoryPair()
{
	SharedMemoryPipePtr a(new SharedMemoryPipe);
	SharedMemoryPipePtr b(new SharedMemoryPipe(*a));
	std::swap(b->in_addr_, b->out_addr_);
	return std::pair<SharedMemoryPipePtr,SharedMemoryPipePtr>(a,b);
}

SharedMemoryPipe::SharedMemoryPipe()
{
	for(int i = 0; i != 2; ++i) {
		buffers_.push_back(std::shared_ptr<std::vector<uint8_t>>(new std::vector<uint8_t>));
		buffers_.back()->resize(AllocSize);
		formatSharedMemory(&(*buffers_.back())[0]);
	}

	in_addr_ = &(*buffers_[0])[0];
	out_addr_ = &(*buffers_[1])[0];
}

SharedMemoryPipe::SharedMemoryPipe(const std::string& name, bool server)
{
	if(server) {
		SharedMemoryPipeManager::createNamedPipe(name + ".read");
		SharedMemoryPipeManager::createNamedPipe(name + ".write");

		in_.reset(new shared_memory_object(open_only, (name + ".read").c_str(), read_write));
		out_.reset(new shared_memory_object(open_only, (name + ".write").c_str(), read_write));

	} else {
		out_.reset(new shared_memory_object(open_only, (name + ".read").c_str(), read_write));
		in_.reset(new shared_memory_object(open_only, (name + ".write").c_str(), read_write));
	}

	in_region_.reset(new mapped_region(*in_, read_write));
	out_region_.reset(new mapped_region(*out_, read_write));

	in_addr_ = reinterpret_cast<uint8_t*>(in_region_->get_address());
	out_addr_ = reinterpret_cast<uint8_t*>(out_region_->get_address());
}

void SharedMemoryPipe::write(const std::string& msg)
{
	out_queue_.push_back(msg);
	fprintf(stderr, "WRITE: QUEUE SIZE: %d\n", (int)out_queue_.size());
	ASSERT_LE(out_queue_.back().size(), RegionSize);
}

void SharedMemoryPipe::read(std::vector<std::string>& msg)
{
	msg.reserve(in_queue_.size());
	for(const std::string& s : in_queue_) {
		msg.push_back(s);
	}

	in_queue_.clear();
}

void SharedMemoryPipe::process()
{
	if(out_queue_.empty() == false) {
		RegionTryLock try_lock(out_addr_);
		if(try_lock.hasLock()) {
			uint8_t* ptr = out_addr_;
			ptr += sizeof(interprocess_mutex);
			uint32_t len;
			memcpy(&len, ptr, sizeof(len));
			if(len == 0) {
				const std::string& s = out_queue_.front();
				memcpy(ptr + sizeof(len), s.c_str(), s.size());
				len = static_cast<uint32_t>(s.size());
				memcpy(ptr, &len, sizeof(len));

				out_queue_.pop_front();
			}
		}

	}

	std::vector<char> buf;
	{
		RegionTryLock try_lock(in_addr_);
		if(try_lock.hasLock()) {
			uint8_t* ptr = in_addr_;
			ptr += sizeof(interprocess_mutex);
			uint32_t len;
			memcpy(&len, ptr, sizeof(len));
			if(len != 0) {
				buf.resize(len);
				memcpy(&buf[0], ptr + sizeof(len), len);

				len = 0;
				memcpy(ptr, &len, sizeof(len));
			}
		}
	}

	if(!buf.empty()) {
		std::string s(buf.begin(), buf.end());
		in_queue_.push_back(s);
	}
}

UNIT_TEST(test_local_shared_memory_pipe)
{
	auto p = SharedMemoryPipe::makeInMemoryPair();

	p.first->write("abc");
	p.first->write("def");

	p.second->write("hij");
	p.second->write("jkl");
	p.second->write("xyz");

	for(int i = 0; i != 5; ++i) {
		p.first->process();
		p.second->process();
	}

	std::vector<std::string> msg;
	p.second->read(msg);

	CHECK_EQ(msg.size(), 2);
	CHECK_EQ(msg[0], "abc");
	CHECK_EQ(msg[1], "def");

	msg.clear();
	p.first->read(msg);
	CHECK_EQ(msg.size(), 3);
	CHECK_EQ(msg[0], "hij");
	CHECK_EQ(msg[1], "jkl");
	CHECK_EQ(msg[2], "xyz");
}

#ifdef __linux__
#include "json_parser.hpp"
#include "variant.hpp"
COMMAND_LINE_UTILITY(test_shared_memory_pipe)
{
	const std::string name = "anura_pipe";

	SharedMemoryPipeManager manager;

	SharedMemoryPipe pipe(name, true);

	const int pid = fork();
	if(pid == 0) {
		SharedMemoryPipe pipe(name, false);

		for(;;) {
			std::vector<std::string> msg;
			pipe.read(msg);
			for(auto s : msg) {
				variant v(json::parse(s));
				const int a = v["a"].as_int();
				const int b = v["b"].as_int();
				variant_builder builder;
				builder.add("a", a);
				builder.add("b", b);
				builder.add("c", a + b);
				pipe.write(builder.build().write_json());
			}

			pipe.process();
			usleep(100000);
		}
	} else if(pid > 0) {
		for(;;) {

			if(rand()%6 == 0) {
				const int num = rand()%4;
				for(int i = 0; i != num; ++i) {
					variant_builder builder;
					builder.add("a", rand()%20);
					builder.add("b", rand()%20);

					pipe.write(builder.build().write_json());
				}
			}

			std::vector<std::string> msg;
			pipe.read(msg);
			for(auto v : msg) {
				std::cerr << "RESPONSE: " << v << "\n";
			}

			pipe.process();
			usleep(100000);
		}
	}
}
#endif
