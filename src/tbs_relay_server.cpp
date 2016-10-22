#include "intrusive_ptr.hpp"

#include "http_client.hpp"
#include "tbs_server.hpp"
#include "unit_test.hpp"

class tbs_relay_server : public http::web_server
{
	struct OutgoingSocketInfo {
		explicit OutgoingSocketInfo(boost::asio::io_service& service)
		  : socket(service)
		{
		}

		boost::asio::ip::tcp::socket socket;
	};

	typedef std::shared_ptr<OutgoingSocketInfo> OutgoingSocketPtr;
	typedef boost::array<char, 4> OutgoingHeader;
	typedef std::shared_ptr<OutgoingHeader> OutgoingHeaderPtr;

	struct SessionRequestPair {
		OutgoingSocketPtr outgoing_socket;
		socket_ptr incoming_socket;
		std::string request;
	};

	struct SessionInfo {
		std::vector<SessionRequestPair> requests;

		SessionRequestPair& getRequestPair(OutgoingSocketPtr& outgoing_socket) {
			for(SessionRequestPair& p : requests) {
				if(p.outgoing_socket == outgoing_socket) {
					return p;
				}
			}

			for(SessionRequestPair& p : requests) {
				if(!p.outgoing_socket) {
					p.outgoing_socket = outgoing_socket;
					return p;
				}
			}

			requests.push_back(SessionRequestPair());
			LOG_INFO("Add new pair: " << requests.size());
			requests.back().outgoing_socket = outgoing_socket;
			return requests.back();
		}

		SessionRequestPair& getRequestPair(socket_ptr& incoming_socket) {
			for(SessionRequestPair& p : requests) {
				if(p.incoming_socket == incoming_socket) {
					return p;
				}
			}

			for(SessionRequestPair& p : requests) {
				if(!p.incoming_socket) {
					p.incoming_socket = incoming_socket;
					return p;
				}
			}

			requests.push_back(SessionRequestPair());
			LOG_INFO("Add new pair: " << requests.size());
			requests.back().incoming_socket = incoming_socket;
			return requests.back();
		}
	};

	std::map<uint32_t, SessionInfo> sessions_;

public:
	tbs_relay_server(boost::asio::io_service& io_service, int incoming_port, int outgoing_port)
	  : http::web_server(io_service, incoming_port),
	    acceptor_(io_service, tcp::endpoint(tcp::v4(), outgoing_port))
	{
		start_accept_outgoing();
	}

	~tbs_relay_server()
	{
		acceptor_.close();
	}

	void start_accept_outgoing()
	{
		OutgoingSocketPtr socket(new OutgoingSocketInfo(acceptor_.get_io_service()));
		acceptor_.async_accept(socket->socket, std::bind(&tbs_relay_server::handle_accept_outgoing, this, socket, std::placeholders::_1));
	}

	void handle_accept_outgoing(OutgoingSocketPtr socket, const boost::system::error_code& error)
	{
		ASSERT_LOG(!error, "Error in accept");

		start_receive_outgoing(socket);
		start_accept_outgoing();
	}

	void start_receive_outgoing(OutgoingSocketPtr socket)
	{
		OutgoingHeaderPtr buf(new OutgoingHeader);
		socket->socket.async_read_some(boost::asio::buffer(*buf), std::bind(&tbs_relay_server::handle_receive_outgoing_header, this, socket, buf, std::placeholders::_1, std::placeholders::_2));
	}

	void handle_receive_outgoing_header(OutgoingSocketPtr socket, OutgoingHeaderPtr header, const boost::system::error_code& e, size_t nbytes)
	{
		if(e || nbytes != 4) {
			LOG_ERROR("Socket ERROR: " << e.message() << " / " << nbytes);
			socket->socket.close();
			return;
		}

		uint32_t session_id = 0;
		memcpy(&session_id, header->data(), 4);

		LOG_INFO("Received connection from server for session " << session_id);

		handle_outgoing_connection(session_id, socket);

		std::shared_ptr<boost::array<char, 65536> > buf(new boost::array<char, 65536>);
		socket->socket.async_read_some(boost::asio::buffer(*buf), std::bind(&tbs_relay_server::handle_receive_outgoing_message, this, socket, buf, session_id, std::placeholders::_1, std::placeholders::_2));
	}

	void handle_receive_outgoing_message(OutgoingSocketPtr socket, std::shared_ptr<boost::array<char, 65536> > buf, uint32_t session_id, const boost::system::error_code& e, size_t nbytes)
	{
		SessionRequestPair& session = sessions_[session_id].getRequestPair(socket);
		if(e) {
			LOG_ERROR("Socket ERROR: " << e.message() << " / " << nbytes);

			disconnectOutgoing(session_id, socket);
			return;
		}

		if(!session.incoming_socket) {
			LOG_ERROR("Received reply without request");
			disconnectOutgoing(session_id, socket);
		}

		std::shared_ptr<std::string> str(new std::string(buf->data(), buf->data()+nbytes));
		LOG_INFO("Send to client: " << str);

		boost::asio::async_write(session.incoming_socket->socket, boost::asio::buffer(*str),
		  std::bind(&tbs_relay_server::handle_send_to_client, this, session_id, session, socket, str, std::placeholders::_1, std::placeholders::_2, str->size()));

		socket->socket.async_read_some(boost::asio::buffer(*buf), std::bind(&tbs_relay_server::handle_receive_outgoing_message, this, socket, buf, session_id, std::placeholders::_1, std::placeholders::_2));
	}

	void handle_send_to_client(uint32_t session_id, SessionRequestPair& session_info, OutgoingSocketPtr socket, std::shared_ptr<std::string> str, const boost::system::error_code& e, size_t nbytes, size_t max_bytes)
	{
		if(e) {
			disconnectOutgoing(session_id, socket);
		} else if(nbytes == max_bytes) {
			keepalive_socket(session_info.incoming_socket);
			session_info.request.clear();
		}
	}

	void handle_send_to_server(uint32_t session_id, OutgoingSocketPtr socket, std::shared_ptr<std::string> str, const boost::system::error_code& e, size_t nbytes, size_t max_bytes)
	{
		if(e) {
			disconnectOutgoing(session_id, socket);
		}
	}

	void handle_outgoing_connection(uint32_t session_id, OutgoingSocketPtr socket)
	{
		SessionRequestPair& session = sessions_[session_id].getRequestPair(socket);
		session.outgoing_socket = socket;

		processSession(session_id, session);
	}

private:

	void disconnect(socket_ptr socket) override
	{
		http::web_server::disconnect(socket);
	}

	void disconnectOutgoing(uint32_t session_id, OutgoingSocketPtr socket)
	{
		socket->socket.close();
		auto& session = sessions_[session_id];
		auto& p = session.getRequestPair(socket);
		if(p.incoming_socket) {
			disconnect(p.incoming_socket);
		}

		for(auto i = session.requests.begin(); i != session.requests.end(); ++i) {
			if(&*i == &p) {
				session.requests.erase(i);
				LOG_INFO("Delete pair: " << session.requests.size());
				break;
			}
		}
	}

	void handlePost(socket_ptr socket, variant doc, const http::environment& env, const std::string& raw_msg) override {
		static const std::string CookieStr = "cookie";
		auto cookie_itor = env.find(CookieStr);
		if(cookie_itor == env.end()) {
			LOG_ERROR("Request without session ID");
			disconnect(socket);
			return;
		}

		const char* cookie_start = strstr(cookie_itor->second.c_str(), "session=");
		if(cookie_start == nullptr) {
			LOG_ERROR("Request without session ID");
			disconnect(socket);
			return;
		}

		const uint32_t session_id = atoi(cookie_start+8);
		SessionRequestPair& session = sessions_[session_id].getRequestPair(socket);
		session.request = raw_msg;

		LOG_INFO("Received post for session " << session_id << ": " << raw_msg);

		processSession(session_id, session);
	}

	void processSession(uint32_t session_id, SessionRequestPair& pair)
	{
		if(pair.request.empty() == false && pair.outgoing_socket) {
			std::shared_ptr<std::string> str(new std::string(pair.request));
			boost::asio::async_write(pair.outgoing_socket->socket, boost::asio::buffer(*str),
		  	std::bind(&tbs_relay_server::handle_send_to_server, this, session_id, pair.outgoing_socket, str, std::placeholders::_1, std::placeholders::_2, str->size()));
		}
	}

	void handleGet(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args) override {
	}

	boost::asio::ip::tcp::acceptor acceptor_;
};

COMMAND_LINE_UTILITY(tbs_relay_server)
{
	int incoming_port = 23458;
	int outgoing_port = 23459;

	if(args.size() > 0) {
		std::vector<std::string>::const_iterator it = args.begin();
		while(it != args.end()) {
			if(*it == "--incoming-port") {
				++it;
				ASSERT_LOG(it != args.end(), "need argument to incoming port");
				incoming_port = atoi(it->c_str());
				++it;
			} else if(*it == "--outgoing-port") {
				++it;
				ASSERT_LOG(it != args.end(), "need argument to outgoing port");
				outgoing_port = atoi(it->c_str());
				++it;
			} else {
				ASSERT_LOG(false, "Unrecognized argument: " << *it);
			}
		}
	}

	boost::asio::io_service io_service;
	
	tbs_relay_server server(io_service, incoming_port, outgoing_port);

	io_service.run();
}

namespace {
class test_web_server : public http::web_server
{
public:
	test_web_server(boost::asio::io_service& io_service, int port=23456)
	  : http::web_server(io_service, port)
	{}
		
	void handlePost(socket_ptr socket, variant doc, const http::environment& env, const std::string& raw_msg) override
	{
		LOG_INFO("handlePost, responding");
		const int a = doc["a"].as_int();
		const int b = doc["b"].as_int();
		std::map<variant,variant> msg;
		msg[variant("result")] = variant(a + b);
		send_msg(socket, "text/json", variant(&msg).write_json(), "");
	}

	void handleGet(socket_ptr socket, const std::string& url, const std::map<std::string, std::string>& args) override
	{
	}
};
}

COMMAND_LINE_UTILITY(test_tbs_relay_server)
{
	boost::asio::io_service io_service;
	
	test_web_server web_server(io_service, 23456);
	web_server.connect_proxy(1, "localhost", "23459");

	ffl::IntrusivePtr<http_client> client(new http_client("localhost", "23458", 1, &io_service));

	int x = 0;
	for(int count = 0; ; ++count) {
		io_service.poll();
		usleep(100000);

		if(count%10 == 0 && client->num_requests_in_flight() == 0) {
			std::map<variant,variant> m;
			m[variant("a")] = variant(++x);
			m[variant("b")] = variant(x);
			LOG_INFO("SENT REQUEST: " << (x+x));
			client->send_request("POST /request", variant(&m).write_json(),
			[](std::string s) {
				LOG_INFO("GOT RESPONSE: " << s);
			},
			[](std::string s) {
				LOG_INFO("GOT ERROR: " << s);
			},
			[](size_t a, size_t b, bool c) {
				LOG_INFO("SEND: " << a << ", " << b << ", " << c);
			});
		}

		client->process();

		
	}
	
	
}
