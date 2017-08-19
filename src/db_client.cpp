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

#include <deque>

#include "db_client.hpp"
#include "filesystem.hpp"
#include "json_parser.hpp"
#include "preferences.hpp"
#include "unit_test.hpp"
#include "variant_utils.hpp"

PREF_STRING(db_json_file, "", "The file to output database content to when using a file to simulate a database");
PREF_STRING(db_key_prefix, "", "Prefix to put before all requests for keys.");

BEGIN_DEFINE_CALLABLE_NOBASE(DbClient)
BEGIN_DEFINE_FN(read_modify_write, "(string, function(any)->any) ->commands")
	std::string key = FN_ARG(0).as_string();
	variant fn = FN_ARG(1);
	DbClient* cli = const_cast<DbClient*>(&obj);
	variant v(new game_logic::FnCommandCallable("db::read_modify_write", [=]() {
	  cli->get(key, [=](variant doc) {
		if(doc.is_null()) {
			return;
		}

		std::vector<variant> args;
		args.push_back(doc);
		variant new_doc = fn(args);

		cli->put(key, new_doc, [](){}, [](){});
		});
	}
	));

	return v;
END_DEFINE_FN

BEGIN_DEFINE_FN(remove, "(string) ->commands")
	std::string key = FN_ARG(0).as_string();
	DbClient* cli = const_cast<DbClient*>(&obj);
	variant v(new game_logic::FnCommandCallable("db::remove", [=]() {
		cli->remove(key);
	}
	));

	return v;
END_DEFINE_FN
BEGIN_DEFINE_FN(get, "(string) ->any")
	std::string key = FN_ARG(0).as_string();
	DbClient* cli = const_cast<DbClient*>(&obj);

	variant result;
	bool done = false;

	cli->get(key, [&done,&result](variant res) { result = res; done = true; });
	while(!done) {
		cli->process();
	}

	return result;
		
END_DEFINE_FN
END_DEFINE_CALLABLE(DbClient)

DbClient::error::error(const std::string& message) : msg(message)
{}

DbClient::~DbClient() {}

namespace
{
	std::map<std::string, variant> db_client_cache;
	class FileBackedDbClient : public DbClient
	{
	public:
		explicit FileBackedDbClient(const std::string& fname, const std::string& prefix) : fname_(fname), dirty_(false), prefix_(prefix) {
			if(db_client_cache.count(fname)) {
				doc_ = db_client_cache[fname];
			} else if(sys::file_exists(fname_)) {
				doc_ = json::parse(sys::read_file(fname_), json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			}
			
			if(!doc_.is_map()) {
				std::map<variant,variant> m;
				doc_ = variant(&m);
			}

			db_client_cache[fname] = doc_;
		}

		~FileBackedDbClient() {
		}

		bool process(int timeout_us) override {
			if(dirty_) {
				sys::write_file(fname_, doc_.write_json());
				dirty_ = false;
			}
			return false;
		}

		void put(const std::string& key, variant doc, std::function<void()> on_done, std::function<void()> on_error, PUT_OPERATION op=PUT_SET) override
		{
			if(op == PUT_APPEND) {
				variant existing = doc_[variant(prefix_ + key)];
				std::vector<variant> val;
				if(existing.is_list()) {
					val = existing.as_list();
				}

				val.push_back(doc);
				doc = variant(&val);
			}

			doc_.add_attr_mutation(variant(prefix_ + key), doc);
			dirty_ = true;
			on_done();
		}

		void get(const std::string& key, std::function<void(variant)> on_done, int lock_seconds, GET_OPERATION op) override {
			on_done(doc_[prefix_ + key]);
		}

		void remove(const std::string& key) override {
			doc_.remove_attr_mutation(variant(prefix_ + key));
			dirty_ = true;
		}

		void getKeysWithPrefix(const std::string& key, std::function<void(std::vector<variant>)> on_done) override {
		}

	private:
		std::string fname_;
		variant doc_;
		bool dirty_;
		std::string prefix_;
	};
}

#ifndef USE_DBCLIENT

DbClientPtr DbClient::create(const char* prefix) 
{
	if(prefix == nullptr) {
		prefix = g_db_key_prefix.c_str();
	}
	return DbClientPtr(new FileBackedDbClient(g_db_json_file.empty() ? "db.json" : g_db_json_file.c_str(), prefix));
}

#else

#include "json_parser.hpp"
#include "preferences.hpp"

#include <libcouchbase/couchbase.h>

namespace 
{
	PREF_STRING(couchbase_host, "localhost", "");
	PREF_STRING(couchbase_user, "", "");
	PREF_STRING(couchbase_bucket, "default", "");
	PREF_STRING(couchbase_passwd, "", "");

	void couchbase_error_handler(lcb_t instance, lcb_error_t error, const char* errinfo)
	{
		LOG_ERROR("Database error: " << errinfo);
		ASSERT_LOG(false, "Database Error");
	}

	void store_callback(lcb_t instance, const void *cookie,
						lcb_storage_t operation,
						lcb_error_t error,
						const lcb_store_resp_t *item);

	void get_callback(lcb_t instance, const void *cookie, lcb_error_t error,
					  const lcb_get_resp_t *item);
	void http_data_callback(lcb_http_request_t request, lcb_t instance, const void *cookie, lcb_error_t error, const lcb_http_resp_t *resp);
	void http_complete_callback(lcb_http_request_t request, lcb_t instance, const void *cookie, lcb_error_t error, const lcb_http_resp_t *resp);

	struct CouchbaseHTTPInfo {
		std::function<void(std::vector<variant>)> callback;
		mutable int* prequests;
		std::string request;
	};

void remove_callback(lcb_t instance, const void* cookie, lcb_error_t error, const lcb_remove_resp_t* resp);

	void timer_callback(lcb_timer_t timer, lcb_t instance, const void* cookie);

	struct PutInfo {
		PutInfo() : retry_client(nullptr) {}
		DbClient* retry_client;
		std::string retry_key;
		variant retry_doc;
		std::function<void()> on_done;
		std::function<void()> on_error;
	};

	struct GetInfo {
		GetInfo() : op(DbClient::GET_NORMAL) {}
		DbClient::GET_OPERATION op;
		std::function<void(variant)> on_done;
	};


	class CouchbaseDbClient : public DbClient
	{
	public:
		CouchbaseDbClient(const std::string& prefix) : outstanding_requests_(0), prefix_(prefix) {
			struct lcb_create_st create_options;
			create_options.v.v0.host = g_couchbase_host.c_str();

			if(!g_couchbase_user.empty()) {
				create_options.v.v0.user = g_couchbase_user.c_str();
			}

			if(!g_couchbase_passwd.empty()) {
				create_options.v.v0.passwd = g_couchbase_passwd.c_str();
			}

			create_options.v.v0.bucket = g_couchbase_bucket.c_str();

			lcb_error_t err = lcb_create(&instance_, &create_options);
			ASSERT_LOG(err == LCB_SUCCESS, "Could not connect to couchbase server: " << lcb_strerror(nullptr, err));

			//lcb_set_bootstrap_callback(instance_, couchbase_error_handler);

			err = lcb_connect(instance_);
			ASSERT_LOG(err == LCB_SUCCESS, "Failed to connect to couchbase server: " << lcb_strerror(nullptr, err));

			lcb_set_get_callback(instance_, get_callback);
			lcb_set_remove_callback(instance_, remove_callback);
			lcb_set_store_callback(instance_, store_callback);
			lcb_set_http_data_callback(instance_, http_data_callback);
			lcb_set_http_complete_callback(instance_, http_complete_callback);

			lcb_wait(instance_);
		}

		~CouchbaseDbClient() {}

		void put(const std::string& rkey, variant doc, std::function<void()> on_done, std::function<void()> on_error, PUT_OPERATION op) override {
			const std::string key = prefix_ + rkey;

			std::string doc_str;
			if(op == PUT_APPEND) {
				doc_str += ",";
			}

			doc_str += doc.write_json();
			lcb_store_cmd_t cmd;
			memset(&cmd, 0, sizeof(cmd));

			PutInfo* cookie = new PutInfo;

			switch(op) {
				case PUT_APPEND:
					cmd.v.v0.operation = LCB_APPEND;
					cookie->retry_client = this;
					cookie->retry_key = rkey;
					cookie->retry_doc = doc;
					break;
				case PUT_ADD:
					cmd.v.v0.operation = LCB_ADD;
					break;
				case PUT_REPLACE:
					cmd.v.v0.operation = LCB_REPLACE;
					break;
				case PUT_SET:
				default:
					cmd.v.v0.operation = LCB_SET;
					break;
			}

			cmd.v.v0.key = key.c_str();
			cmd.v.v0.nkey = key.size();
			cmd.v.v0.bytes = doc_str.c_str();
			cmd.v.v0.nbytes = doc_str.size();

			const lcb_store_cmd_t* commands[1];
			commands[0] = &cmd;

			++outstanding_requests_;
			int* poutstanding = &outstanding_requests_;

			cookie->on_done = [=]() { --*poutstanding; on_done(); };
			cookie->on_error = [=]() { --*poutstanding; on_error(); };

			lcb_error_t err = lcb_store(instance_, cookie, 1, commands);
			ASSERT_LOG(err == LCB_SUCCESS, "Error in store: " << lcb_strerror(nullptr, err));
		}

	virtual void remove(const std::string& rkey) override
	{
		const std::string key = prefix_ + rkey;

		lcb_remove_cmd_t cmd;
		memset(&cmd, 0, sizeof(cmd));

		cmd.v.v0.key = key.c_str();
		cmd.v.v0.nkey = key.size();
		
		const lcb_remove_cmd_t* commands[1];
		commands[0] = &cmd;

		++outstanding_requests_;
		int* poutstanding = &outstanding_requests_;

		PutInfo* cookie = new PutInfo;
		cookie->on_done = [=]() { --*poutstanding; };
		cookie->on_error = [=]() { --*poutstanding; };

		lcb_error_t err = lcb_remove(instance_, cookie, 1, commands);
		ASSERT_LOG(err == LCB_SUCCESS, "Error in remove: " << lcb_strerror(nullptr, err));
	}

		void get(const std::string& rkey, std::function<void(variant)> on_done, int lock_seconds, GET_OPERATION op) override
		{
			const std::string key = prefix_ + rkey;

			lcb_get_cmd_t cmd;
			memset(&cmd, 0, sizeof(cmd));
			cmd.v.v0.key = key.c_str();
			cmd.v.v0.nkey = key.size();

			if(lock_seconds) {
				cmd.v.v0.lock = 1;
				cmd.v.v0.exptime = lock_seconds;
			}

			++outstanding_requests_;
			int* poutstanding = &outstanding_requests_;

			const lcb_get_cmd_t* commands[1];
			commands[0] = &cmd;

			GetInfo* cookie = new GetInfo;
			cookie->op = op;
			cookie->on_done = [=](variant v) { --*poutstanding; on_done(v); };

			lcb_error_t err = lcb_get(instance_, cookie, 1, commands);
			ASSERT_LOG(err == LCB_SUCCESS, "Error in get: " << lcb_strerror(nullptr, err));
		}

		void getKeysWithPrefix(const std::string& key, std::function<void(std::vector<variant>)> on_done) override
		{
			CouchbaseHTTPInfo* cookie = new CouchbaseHTTPInfo;
			cookie->callback = on_done;
			cookie->prequests = &outstanding_requests_;
			cookie->request = "_design/all/_view/all" + (key.empty() ? "" : "?startkey=\"" + (prefix_ + key) + ":\"&endkey=\"" + (prefix_ + key) + "A\"");

			lcb_http_request_t req;
			lcb_http_cmd_t *cmd = new lcb_http_cmd_t;
			memset(cmd, 0, sizeof(*cmd));
			cmd->version = 0;
			cmd->v.v0.path = cookie->request.c_str();
			cmd->v.v0.npath = cookie->request.size();
			cmd->v.v0.body = nullptr;
			cmd->v.v0.nbody = 0;
			cmd->v.v0.method = LCB_HTTP_METHOD_GET;
			cmd->v.v0.chunked = 0;
			cmd->v.v0.content_type = "application/json";
			lcb_error_t err = lcb_make_http_request(instance_, cookie, LCB_HTTP_TYPE_VIEW, cmd, &req);
			ASSERT_LOG(err == LCB_SUCCESS, "Error in http request: " << lcb_strerror(nullptr, err));

			++outstanding_requests_;
		}

		bool process(int timeout_us) override
		{
			if(timeout_us > 0) {
				/*
				lcb_error_t error = LCB_SUCCESS;
				lcb_timer_t timer = lcb_timer_create(instance_, nullptr, timeout_us, 0, timer_callback, &error);
				ASSERT_LOG(error == LCB_SUCCESS, "Failed to create lcb timer");
			*/
			}
			lcb_wait(instance_);

			return outstanding_requests_ != 0;
		}
	private:
		lcb_t instance_;

		int outstanding_requests_;

		std::string prefix_;
	};

	void store_callback(lcb_t instance, const void *cookie,
						lcb_storage_t operation,
						lcb_error_t err,
						const lcb_store_resp_t *item)
	{
		if(cookie && (err == LCB_KEY_EEXISTS || err == LCB_KEY_ENOENT)) {
			const PutInfo* info = reinterpret_cast<const PutInfo*>(cookie);
			if(info->on_error) {
				info->on_error();
			}
		}

		const PutInfo* info = reinterpret_cast<const PutInfo*>(cookie);
		if(err != LCB_SUCCESS && info && info->retry_client != nullptr) {
			info->retry_client->put(info->retry_key, info->retry_doc, info->on_done, info->on_error, DbClient::PUT_ADD);
			return;
		}

		ASSERT_LOG(err == LCB_SUCCESS, "Error in store callback: " << lcb_strerror(nullptr, err));
	
		if(cookie) {
			if(info->on_done) {
				info->on_done();
			}

			delete info;
		}
	}

	void get_callback(lcb_t instance, const void *cookie, lcb_error_t err,
					  const lcb_get_resp_t *item)
	{
		ASSERT_LOG(err == LCB_SUCCESS || err == LCB_KEY_ENOENT, "Error in get callback: " << lcb_strerror(nullptr, err));
		if(cookie) {
			const GetInfo* info = reinterpret_cast<const GetInfo*>(cookie);
			variant v;
			if(err == LCB_SUCCESS) {
				std::string doc;
				if(info->op == DbClient::GET_LIST) {
					doc = "[";
				}
				doc.append(reinterpret_cast<const char*>(item->v.v0.bytes),reinterpret_cast<const char*>( item->v.v0.bytes) + item->v.v0.nbytes);
				if(info->op == DbClient::GET_LIST) {
					doc += "]";
				}

				try {
					if(info->op == DbClient::GET_RAW) {
						v = variant(doc);
					} else {
						assert_recover_scope scope;
						v = json::parse(doc);
					}
				} catch(json::ParseError& e) {
					LOG_ERROR("Error parsing database data to JSON: " << doc);
				} catch(validation_failure_exception& e) {
					LOG_ERROR("Error parsing database data to JSON: " << e.msg << ": " << doc);
				}
			}
			if(info->on_done) {
				info->on_done(v);
			}

			delete info;
		}
	}

	void http_data_callback(lcb_http_request_t request, lcb_t instance, const void *cookie, lcb_error_t error, const lcb_http_resp_t *resp)
	{
	}

	void http_complete_callback(lcb_http_request_t request, lcb_t instance, const void *cookie, lcb_error_t error, const lcb_http_resp_t *resp)
	{
		const char* ptr = reinterpret_cast<const char*>(resp->v.v0.bytes);
		std::string response(ptr, ptr + resp->v.v0.nbytes);

		std::vector<variant> result;
		try {
			variant v = json::parse(response, json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);

			ASSERT_LOG(v.is_map(), "Invalid db http respones: " << response);

			variant rows = v["rows"];
			ASSERT_LOG(rows.is_list(), "Invalid db http response: " << response);

			for(variant item : rows.as_list()) {
				ASSERT_LOG(item.is_map(), "Invalid db http response: " << response);
				variant key = item["key"];
				result.push_back(key);
			}
		} catch(json::ParseError& e) {
			LOG_ERROR("Error parsing db http response: " << e.errorMessage());
		}

		const CouchbaseHTTPInfo* info = reinterpret_cast<const CouchbaseHTTPInfo*>(cookie);
		info->callback(result);
		--*info->prequests;
		delete info;
	}

	void remove_callback(lcb_t instance, const void* cookie, lcb_error_t error, const lcb_remove_resp_t* resp)
	{
		if(cookie) {
			const PutInfo* info = reinterpret_cast<const PutInfo*>(cookie);
			if(info->on_done) {
				info->on_done();
			}

			delete info;
		}
	}

	void timer_callback(lcb_timer_t timer, lcb_t instance, const void* cookie)
	{
		lcb_breakout(instance);
	}

	COMMAND_LINE_UTILITY(test_db)
	{
		DbClientPtr client = DbClient::create();

		client->get("abc", [](variant value) { LOG_DEBUG("RESULT: " << value.write_json()); });
		client->process();

		client->put("abc", variant(54), []() { LOG_DEBUG("DONE"); }, []() { LOG_ERROR("ERROR"); } );

		client->process();
	}

}

DbClientPtr DbClient::create(const char* prefix) 
{
	if(prefix == nullptr) {
		prefix = g_db_key_prefix.c_str();
	}
	if(g_db_json_file.empty()) {
		return DbClientPtr(new CouchbaseDbClient(prefix));
	} else {
		return DbClientPtr(new FileBackedDbClient(g_db_json_file, prefix));
	}
}

#endif //USE_DBCLIENT

COMMAND_LINE_UTILITY(query_db)
{
	DbClientPtr client = DbClient::create();
	std::deque<std::string> arguments(args.begin(), args.end());
	while(!arguments.empty()) {
		const std::string arg = arguments.front();
		arguments.pop_front();

		std::string result;
		client->get(arg, [&result](variant value) { result = value.write_json(); });
		while(result.empty()) {
			client->process();
		}

		printf("%s\n", result.c_str());
	}
}

COMMAND_LINE_UTILITY(delete_from_db)
{
	DbClientPtr client = DbClient::create();
	std::deque<std::string> arguments(args.begin(), args.end());
	while(!arguments.empty()) {
		const std::string arg = arguments.front();
		arguments.pop_front();

		client->remove(arg);
		while(client->process()) {
		}
	}
}

COMMAND_LINE_UTILITY(backup_db)
{
	variant_builder builder;
	variant_builder* builder_ptr = &builder;
	DbClientPtr client = DbClient::create();
	client->getKeysWithPrefix("", [=](std::vector<variant> v) {
		for(auto item : v) {
			client->get(item.as_string(), [=](variant doc) {
				builder_ptr->add(item.as_string(), doc);
			}, 0, DbClient::GET_RAW);
		}
	});
	while(client->process()) {
	}
	while(client->process()) {
	}

	variant res = builder.build();
	printf("%s\n", res.write_json().c_str());
}
