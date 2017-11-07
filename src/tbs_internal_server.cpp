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

#include <SDL.h>

#include <boost/interprocess/sync/named_semaphore.hpp>
#include <boost/exception/diagnostic_information.hpp>

#if !defined(_MSC_VER)
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#endif

#include "asserts.hpp"
#include "background_task_pool.hpp"
#include "formatter.hpp"
#include "json_parser.hpp"
#include "module.hpp"
#include "preferences.hpp"
#include "shared_memory_pipe.hpp"
#include "string_utils.hpp"
#include "tbs_internal_server.hpp"
#include "uuid.hpp"
#include "variant_utils.hpp"
#include "wml_formula_callable.hpp"

PREF_STRING(tbs_server_child_args, "", "Arguments to pass along to the tbs spawned child");

extern std::string g_anura_exe_name;

namespace tbs
{
	using std::placeholders::_1;

	namespace 
	{
		internal_server_ptr server_ptr;
	}

	boost::asio::io_service internal_server::io_service_;

	internal_server::internal_server()
		: server_base(io_service_)
	{
	}

	internal_server::~internal_server()
	{
	}

	void internal_server::send_request(const variant& request, 
		int session_id,
		game_logic::MapFormulaCallablePtr callable, 
		std::function<void(const std::string&)> handler)
	{
		ASSERT_LOG(server_ptr != nullptr, "Internal server pointer is nullptr");
		send_function send_fn = std::bind(&internal_server::send_msg, server_ptr.get(), _1, session_id, handler, callable);
		server_ptr->write_queue(send_fn, request, session_id);
	}

	void internal_server::send_msg(const variant& resp, 
		int session_id,
		std::function<void(const std::string&)> handler, 
		game_logic::MapFormulaCallablePtr callable)
	{
		if(handler) {
			callable->add("message", resp);
			handler("message_received");
		}
		disconnect(session_id);
	}

	void internal_server::process()
	{
		ASSERT_LOG(server_ptr != NULL, "Internal server pointer is NULL");
		server_ptr->handle_process();
	}

boost::interprocess::named_semaphore* g_termination_semaphore;

std::string g_termination_semaphore_name;

#if defined(_MSC_VER)
const std::string shared_sem_name = "anura_tbs_sem";
HANDLE child_process;
HANDLE child_thread;
HANDLE child_stderr;
HANDLE child_stdout;
#else
const std::string shared_sem_name = "/anura_tbs_sem";
pid_t g_child_pid;
#endif

std::string get_semaphore_name(std::string id, int sem_id) {
	return formatter() << shared_sem_name << id << sem_id;
}

bool create_utility_process(const std::string& app, const std::vector<std::string>& argv)
{

#if defined(_MSC_VER)
	char app_np[MAX_PATH];
	// Grab the full path name
	DWORD chararacters_copied = GetModuleFileNameA(NULL, app_np,  MAX_PATH);
	ASSERT_LOG(chararacters_copied > 0, "Failed to get module name: " << GetLastError());
	std::string app_name_and_path(app_np, chararacters_copied);

	// windows version
	std::string command_line_params;
	command_line_params += "\"" + app_name_and_path + "\" ";
	for(size_t n = 0; n != argv.size(); ++n) {
		command_line_params += "\"" + argv[n] + "\" ";
	}
	std::vector<char> child_args;
	child_args.resize(command_line_params.size()+1);
	memset(&child_args[0], 0, command_line_params.size()+1);
	memcpy(&child_args[0], &command_line_params[0], command_line_params.size());

	STARTUPINFOA siStartupInfo; 
	PROCESS_INFORMATION piProcessInfo;
	SECURITY_ATTRIBUTES saFileSecurityAttributes;
	memset(&siStartupInfo, 0, sizeof(siStartupInfo));
	memset(&piProcessInfo, 0, sizeof(piProcessInfo));
	siStartupInfo.cb = sizeof(siStartupInfo);
	saFileSecurityAttributes.nLength = sizeof(saFileSecurityAttributes);
	saFileSecurityAttributes.lpSecurityDescriptor = NULL;
	saFileSecurityAttributes.bInheritHandle = true;
	child_stderr = siStartupInfo.hStdError = CreateFileA("stderr_server.txt", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &saFileSecurityAttributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	ASSERT_LOG(siStartupInfo.hStdError != INVALID_HANDLE_VALUE, 
		"Unable to open stderr_server.txt for child process.");
	child_stdout = siStartupInfo.hStdOutput = CreateFileA("stdout_server.txt", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &saFileSecurityAttributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	ASSERT_LOG(siStartupInfo.hStdOutput != INVALID_HANDLE_VALUE, 
		"Unable to open stdout_server.txt for child process.");
	siStartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	siStartupInfo.dwFlags = STARTF_USESTDHANDLES;
	std::cerr << "CREATE CHILD PROCESS: " << app_name_and_path << std::endl;
	ASSERT_LOG(CreateProcessA(app_name_and_path.c_str(), &child_args[0], NULL, NULL, true, CREATE_DEFAULT_ERROR_MODE, 0, 0, &siStartupInfo, &piProcessInfo),
		"Unable to create child process for utility: " << GetLastError());
	child_process = piProcessInfo.hProcess;
	child_thread = piProcessInfo.hThread;
#else
	// everyone else version using fork()
	//...
	g_child_pid = fork();
	if(g_child_pid == 0) {
		FILE* fout = std::freopen("stdout_server.txt","w", stdout);
		FILE* ferr = std::freopen("stderr_server.txt","w", stderr);
		std::cerr.sync_with_stdio(true);

		std::vector<char*> args;
		args.push_back(const_cast<char*>(app.c_str()));
		for(const std::string& a : argv) {
			args.push_back(const_cast<char*>(a.c_str()));
		}
		args.push_back(NULL);

		execv(app.c_str(), &args[0]);

		const char* error = NULL;
		switch(errno) {
		case E2BIG: error = "E2BIG"; break;
		case EACCES: error = "EACCES"; break;
		case EFAULT: error = "EFAULT"; break;
		case EIO: error = "EIO"; break;
		case ENOENT: error = "ENOENT"; break;
		default:
			error = "Unk"; break;
		}
		ASSERT_LOG(false, "execv FAILED: " << error);
	}
	ASSERT_LOG(g_child_pid >= 0, "Unable to fork process: " << errno);

#endif
	// Create a semaphore to signal termination.
#if defined(_MSC_VER)
	return false;
#else
	return g_child_pid == 0;
#endif
}

bool is_utility_process_running()
{
	if(!g_termination_semaphore) {
		return false;
	}

#if defined(_MSC_VER)
	if(WaitForSingleObject(child_process, 0) == WAIT_TIMEOUT) {
		return true;
	}
	CloseHandle(child_process);
	CloseHandle(child_thread);
	CloseHandle(child_stderr);
	CloseHandle(child_stdout);

	child_process = 0;
#else
	int status;
	if(waitpid(g_child_pid, &status, WNOHANG) != g_child_pid) {
		return true;
	}

	g_child_pid = 0;
#endif

	return false;
}

void terminate_utility_process(bool* complete=nullptr)
{
	if(!g_termination_semaphore) {
		if(complete) {
			*complete = true;
		}
		return;
	}

	g_termination_semaphore->post();

#if defined(_MSC_VER)
	HANDLE local_child_process = child_process;
	HANDLE local_child_thread = child_thread;
	HANDLE local_child_stderr = child_stderr;
	HANDLE local_child_stdout = child_stdout;
	std::string named_semaphore = g_termination_semaphore_name;
	boost::interprocess::named_semaphore* sem = g_termination_semaphore;
	
	background_task_pool::submit([=]() {
		WaitForSingleObject(local_child_process, INFINITE);
		CloseHandle(local_child_process);
		CloseHandle(local_child_thread);
		CloseHandle(local_child_stderr);
		CloseHandle(local_child_stdout);
		if(complete) {
			*complete = true;
		}

		boost::interprocess::named_semaphore::remove(named_semaphore.c_str());
		delete sem;
	}, 

	[=]() {
		if(complete != nullptr) {
			*complete = true;
		}
	});

#else
	if(!g_child_pid) {
		if(complete) {
			*complete = true;
		}
		return;
	}

	pid_t child_pid = g_child_pid;
	std::string named_semaphore = g_termination_semaphore_name;
	boost::interprocess::named_semaphore* sem = g_termination_semaphore;

	background_task_pool::submit([=]() {
		int status;
		if(waitpid(child_pid, &status, 0) != child_pid) {
			std::cerr << "Error waiting for child process to finish: " << errno << std::endl;
		} else {
			LOG_INFO("Child process " << child_pid << " reaped");
		}

		boost::interprocess::named_semaphore::remove(named_semaphore.c_str());
		delete sem;
	},

	[=]() {
		if(complete != nullptr) {
			*complete = true;
		}
	});

	g_child_pid = 0;
#endif

	g_termination_semaphore = nullptr;
}

	namespace {
		int g_local_server_port;
		SharedMemoryPipePtr g_current_ipc_pipe;
	}

	int get_server_on_localhost(SharedMemoryPipePtr* ipc_pipe) {
		if(ipc_pipe != nullptr) {
			*ipc_pipe = g_current_ipc_pipe;
		}
		return g_local_server_port;
	}

	int spawn_server_on_localhost(SharedMemoryPipePtr* ipc_pipe) {

		terminate_utility_process(nullptr);

		delete g_termination_semaphore;
		g_termination_semaphore = NULL;

		boost::interprocess::named_semaphore* startup_semaphore = NULL;
		std::string startup_semaphore_name;


		int sem_id = 0;
		for(int i = 0; i != 64; ++i) {
			sem_id = rand()%65536;
			try {
				g_termination_semaphore_name = get_semaphore_name("term", sem_id);
				startup_semaphore_name = get_semaphore_name("start", sem_id);
				g_termination_semaphore = new boost::interprocess::named_semaphore(boost::interprocess::create_only_t(), g_termination_semaphore_name.c_str(), 0);
				startup_semaphore = new boost::interprocess::named_semaphore(boost::interprocess::create_only_t(), startup_semaphore_name.c_str(), 0);
			} catch(boost::interprocess::interprocess_exception&) {
				delete g_termination_semaphore;
				g_termination_semaphore = NULL;
				continue;
			}

			break;
		}

		std::string pipe_name;
		if(ipc_pipe != nullptr) {
			std::string error_msg;
			for(int i = 0; i != 4 && pipe_name.empty(); ++i) {
				std::string uuid_str = write_uuid(generate_uuid());
				uuid_str.resize(16);
				pipe_name = formatter() << "anura_tbs." << uuid_str;
				try {
					SharedMemoryPipeManager::createNamedPipe(pipe_name);
					g_current_ipc_pipe.reset(new SharedMemoryPipe(pipe_name, true));
					*ipc_pipe = g_current_ipc_pipe;
				} catch(boost::exception& e) {
					error_msg = diagnostic_information(e);
					error_msg = "boost exception: " + error_msg;
				} catch(std::exception& e) {
					ASSERT_LOG(false, "Error creating local server. Try restarting your computer to resolve this problem: " << e.what());
				} catch(...) {
					pipe_name = "";
					error_msg = "Unknown error";
				}
			}

			ASSERT_LOG(*ipc_pipe, "Could not create named pipe after 64 attempts: " << error_msg);
		}

		ASSERT_LOG(startup_semaphore, "Could not create semaphore");

		bool started_server = false;
		for(int attempt = 0; attempt != 4 && started_server == false; ++attempt) {
			g_local_server_port = 4096 + rand()%20000;

			std::vector<std::string> args;

			if(g_tbs_server_child_args.empty() == false) {
				args = util::split(g_tbs_server_child_args, ' ');
			}

			args.push_back(formatter() << "--module=" << module::get_module_name());
			args.push_back(formatter() << "--tbs-server-save-replay-file=" << preferences::user_data_path() << "/local-replays.cfg");
			args.push_back("--tbs-server-local=true");
			args.push_back("--log-file=server-log.txt");
			args.push_back("--log-level=debug");
			args.push_back("--no-tbs-server");
			args.push_back("--quit-server-after-game");
			args.push_back("--quit-server-on-parent-exit");
			args.push_back("--tbs-server-timeout=0");
			args.push_back(formatter() << "--tbs-server-semaphore=" << sem_id);
			args.push_back("--utility=tbs_server");
			args.push_back("--port");
			args.push_back(formatter() << g_local_server_port);

			if(pipe_name.empty() == false) {
				args.push_back("--sharedmem");
				args.push_back(pipe_name);
				args.push_back("1");
			}

			create_utility_process(g_anura_exe_name, args);

			while(started_server == false && is_utility_process_running()) {
				if(startup_semaphore->try_wait()) {
					started_server = true;
				}
			}

			if(!started_server) {
				LOG_ERROR("Failed to start server process attempt " << (attempt+1) << "\nSERVER OUTPUT: " << sys::read_file("stderr_server.txt") << "\n--END OUTPUT--\n");
			}
		}

		ASSERT_LOG(started_server, "Could not start server process. Server output: " << sys::read_file("stderr_server.txt") << " -- server log: " << sys::read_file("server-log.txt"));

		delete startup_semaphore;
		startup_semaphore = NULL;

		boost::interprocess::named_semaphore::remove(startup_semaphore_name.c_str());

		return g_local_server_port;
	}


	internal_server_manager::internal_server_manager(bool use_internal_server)
	{
		if(use_internal_server) {
			server_ptr = internal_server_ptr(new internal_server);

		}
	}

	internal_server_manager::~internal_server_manager()
	{
		if(g_termination_semaphore) {
			bool complete = false;
			terminate_utility_process(&complete);
			while(!complete) {
				background_task_pool::pump();
				SDL_Delay(1);
			}
		}

		server_ptr.reset();
	}

	int internal_server::requests_in_flight(int session_id)
	{
		int result = 0;
		for(auto i = server_ptr->connections_.begin(); i != server_ptr->connections_.end(); ++i) {
			if(i->second.session_id == session_id) {
				//implementing this causes problems. Investigate.
			}
		}

		return result;
	}

	server_base::socket_info& internal_server::create_socket_info(send_function send_fn)
	{
		connections_.push_back(std::pair<send_function, socket_info>(send_fn, socket_info()));
		return connections_.back().second;
	}

	void internal_server::disconnect(int session_id)
	{
		if(session_id == -1) {
			return;
		}

		for(auto i = connections_.begin(); i != connections_.end(); ++i) {
			if(i->second.session_id == session_id) {
				connections_.erase(i);
				return;
			}
		}
		ASSERT_LOG(false, "Trying to erase unknown session_id: " << session_id);
	}

	void internal_server::heartbeat_internal(int send_heartbeat, std::map<int, client_info>& clients)
	{
		std::vector<std::pair<send_function, variant> > messages;

		for(auto i = connections_.begin(); i != connections_.end(); ++i) {
			send_function send_fn = i->first;
			socket_info& info = i->second;
			ASSERT_LOG(info.session_id != -1, "UNKNOWN SOCKET");

			client_info& cli_info = clients[info.session_id];
			if(cli_info.msg_queue.empty() == false) {
				messages.push_back(std::pair<send_function,variant>(send_fn, game_logic::deserialize_doc_with_objects(cli_info.msg_queue.front())));
				cli_info.msg_queue.pop_front();
			} else if(send_heartbeat) {
				if(!cli_info.game) {
					variant_builder v;
					v.add("type", variant("heartbeat"));
					messages.push_back(std::pair<send_function,variant>(send_fn, v.build()));
				} else {
					variant v = create_heartbeat_packet(cli_info);
					messages.push_back(std::pair<send_function,variant>(send_fn, v));
				}
			}
		}

		for(int i = 0; i != messages.size(); ++i) {
			messages[i].first(messages[i].second);
		}
	}

	void internal_server::handle_process()
	{
		send_function send_fn;
		variant request;
		int session_id;
		while(read_queue(&send_fn, &request, &session_id)) {
			server_ptr->handle_message(
				send_fn,
				std::bind(&internal_server::finish_socket, this, send_fn, _1),
				std::bind(&internal_server::create_socket_info, server_ptr.get(), send_fn),
				session_id, 
				request);
		}
		io_service_.poll();
		io_service_.reset();
	}

	void internal_server::queue_msg(int session_id, const std::string& msg, bool has_priority)
	{
		if(session_id == -1) {
			return;
		}

		server_base::queue_msg(session_id, msg, has_priority);
	}

	void internal_server::write_queue(send_function send_fn, const variant& v, int session_id)
	{
		msg_queue_.push_back(boost::make_tuple(send_fn,v,session_id));
	}

	bool internal_server::read_queue(send_function* send_fn, variant* v, int *session_id)
	{
		ASSERT_LOG(send_fn != nullptr && v != nullptr && session_id != nullptr,
			"read_queue called with nullptr parameter.");
		if(msg_queue_.empty()) {
			return false;
		}
		boost::tie(*send_fn, *v, *session_id) = msg_queue_.front();
		msg_queue_.pop_front();
		return true;
	}

	void internal_server::finish_socket(send_function send_fn, client_info& cli_info)
	{
		if(cli_info.msg_queue.empty() == false) {
			const std::string msg = cli_info.msg_queue.front();
			cli_info.msg_queue.pop_front();
			send_fn(game_logic::deserialize_doc_with_objects(msg));
		}
	}

	int internal_server::connection_timeout_ticks() const
	{
		return -1;
	}
}
