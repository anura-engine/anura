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
		formula_profiler::Instrument instrumentation("IPC_DESERIALIZE");
		v = game_logic::deserialize_doc_with_objects(m);
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
