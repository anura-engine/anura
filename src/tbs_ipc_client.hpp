#pragma once

#include <memory>
#include <string>

#include "formula_callable.hpp"
#include "formula_callable_definition.hpp"
#include "shared_memory_pipe.hpp"
#include "variant.hpp"

namespace tbs
{
	class ipc_client : public game_logic::FormulaCallable
	{
	public:
		ipc_client(SharedMemoryPipePtr pipe);

		void set_handler(std::function<void(std::string)> fn) { handler_ = fn; }
		void set_callable(game_logic::MapFormulaCallablePtr callable) { callable_ = callable; }
		void send_request(variant request);
		void process();
	private:
		DECLARE_CALLABLE(ipc_client);

		SharedMemoryPipePtr pipe_;

		game_logic::MapFormulaCallablePtr callable_;
		std::function<void(std::string)> handler_;

		int in_flight_;
	};
}
