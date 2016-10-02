#include <sys/time.h>

#include "asserts.hpp"
#include "formula_callable.hpp"
#include "formula_profiler.hpp"
#include "tbs_ipc_client.hpp"
#include "wml_formula_callable.hpp"

namespace tbs
{

ipc_client::ipc_client(SharedMemoryPipePtr pipe) : pipe_(pipe), in_flight_(0)
{
	ASSERT_LOG(pipe_.get() != nullptr, "Invalid pipe passed to ipc_client");	
}

void ipc_client::send_request(variant request)
{
	ASSERT_LOG(pipe_.get() != nullptr, "Invalid pipe in ipc_client");	
	pipe_->write(request.write_json());
	pipe_->process();

	++in_flight_;
}

void ipc_client::process()
{
	std::vector<std::string> msg;
	{
	formula_profiler::Instrument instrumentation("IPC_PROCESS");
	ASSERT_LOG(pipe_.get() != nullptr, "Invalid pipe in ipc_client");	
	pipe_->process();

	if(!callable_) {
		return;
	}

	pipe_->read(msg);

	}
	for(const std::string& m : msg) {
		--in_flight_;

		variant v;
		
		{
		timeval tv_begin, tv_end;
		formula_profiler::Instrument instrumentation("IPC_DESERIALIZE");
		const uint64_t ntaken_before = instrumentation.get_ns();
		gettimeofday(&tv_begin, nullptr);
		v = game_logic::deserialize_doc_with_objects(m);
		gettimeofday(&tv_end, nullptr);

		const int us = (tv_end.tv_sec - tv_begin.tv_sec)*1000000 + (tv_end.tv_usec - tv_begin.tv_usec);

		const uint64_t ntaken = instrumentation.get_ns();
		if(ntaken > 1000000LL) {
			fprintf(stderr, "ZZZ: TOOK: %d %dms: VS %dms (((%s)))\n", (int)(ntaken_before/1000000LL), (int)(ntaken/1000000LL), us/1000, m.c_str());
		}
		}

		callable_->add("message", v);
		{
		formula_profiler::Instrument instrumentation("IPC_MESSAGE_RECEIVED");
		handler_("message_received");
		}
	}
}

	BEGIN_DEFINE_CALLABLE_NOBASE(ipc_client)
		DEFINE_FIELD(in_flight, "int")
			return variant(obj.in_flight_);
	END_DEFINE_CALLABLE(ipc_client)


}
