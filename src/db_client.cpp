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

PREF_STRING(db_json_file, "", "The file to output database content to when using a file to simulate a database");
PREF_STRING(db_key_prefix, "", "Prefix to put before all requests for keys.");

BEGIN_DEFINE_CALLABLE_NOBASE(DbClient)
BEGIN_DEFINE_FN(read_modify_write, "(string, function(any)->any) ->commands")
	std::string key = FN_ARG(0).as_string();
	variant fn = FN_ARG(1);
	DbClient* cli = const_cast<DbClient*>(&obj);
	variant v(new game_logic::FnCommandCallable([=]() {
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
	variant v(new game_logic::FnCommandCallable([=]() {
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
	class FileBackedDbClient : public DbClient
	{
	public:
		explicit FileBackedDbClient(const std::string& fname) : fname_(fname), dirty_(false) {
			if(sys::file_exists(fname_)) {
				doc_ = json::parse(sys::read_file(fname_), json::JSON_PARSE_OPTIONS::NO_PREPROCESSOR);
			}
			
			if(!doc_.is_map()) {
				std::map<variant,variant> m;
				doc_ = variant(&m);
			}
		}

		~FileBackedDbClient() {
		}

		bool process(int timeout_us) {
			if(dirty_) {
				sys::write_file(fname_, doc_.write_json());
				dirty_ = false;
			}
			return false;
		}

		void put(const std::string& key, variant doc, std::function<void()> on_done, std::function<void()> on_error, PUT_OPERATION op=PUT_SET)
		{
			doc_.add_attr_mutation(variant(g_db_key_prefix + key), doc);
			dirty_ = true;
			on_done();
		}

		void get(const std::string& key, std::function<void(variant)> on_done, int lock_seconds) {
			on_done(doc_[g_db_key_prefix + key]);
		}

		void remove(const std::string& key) {
			doc_.remove_attr_mutation(variant(g_db_key_prefix + key));
			dirty_ = true;
		}

	private:
		std::string fname_;
		variant doc_;
		bool dirty_;
	};
}

#ifndef USE_DBCLIENT

DbClientPtr DbClient::create() 
{
	return DbClientPtr(new FileBackedDbClient(g_db_json_file.empty() ? "db.json" : g_db_json_file.c_str()));
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

void remove_callback(lcb_t instance, const void* cookie, lcb_error_t error, const lcb_remove_resp_t* resp);

	void timer_callback(lcb_timer_t timer, lcb_t instance, const void* cookie);

	struct PutInfo {
		std::function<void()> on_done;
		std::function<void()> on_error;
	};

	struct GetInfo {
		std::function<void(variant)> on_done;
	};


	class CouchbaseDbClient : public DbClient
	{
	public:
		CouchbaseDbClient() : outstanding_requests_(0) {
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

			lcb_wait(instance_);
		}

		~CouchbaseDbClient() {}

		void put(const std::string& rkey, variant doc, std::function<void()> on_done, std::function<void()> on_error, PUT_OPERATION op) {
			const std::string key = g_db_key_prefix + rkey;

			std::string doc_str = doc.write_json();
			lcb_store_cmd_t cmd;
			memset(&cmd, 0, sizeof(cmd));

			switch(op) {
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

			PutInfo* cookie = new PutInfo;
			cookie->on_done = [=]() { --*poutstanding; on_done(); };
			cookie->on_error = [=]() { --*poutstanding; on_error(); };

			lcb_error_t err = lcb_store(instance_, cookie, 1, commands);
			ASSERT_LOG(err == LCB_SUCCESS, "Error in store: " << lcb_strerror(nullptr, err));
		}

	virtual void remove(const std::string& rkey)
	{
		const std::string key = g_db_key_prefix + rkey;

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

		void get(const std::string& rkey, std::function<void(variant)> on_done, int lock_seconds)
		{
			const std::string key = g_db_key_prefix + rkey;

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
			cookie->on_done = [=](variant v) { --*poutstanding; on_done(v); };

			lcb_error_t err = lcb_get(instance_, cookie, 1, commands);
			ASSERT_LOG(err == LCB_SUCCESS, "Error in get: " << lcb_strerror(nullptr, err));
		}

		bool process(int timeout_us)
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

		ASSERT_LOG(err == LCB_SUCCESS, "Error in store callback: " << lcb_strerror(nullptr, err));
	
		if(cookie) {
			const PutInfo* info = reinterpret_cast<const PutInfo*>(cookie);
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
			variant v;
			if(err == LCB_SUCCESS) {
				std::string doc(reinterpret_cast<const char*>(item->v.v0.bytes),reinterpret_cast<const char*>( item->v.v0.bytes) + item->v.v0.nbytes);
				v = json::parse(doc);
			}
			const GetInfo* info = reinterpret_cast<const GetInfo*>(cookie);
			if(info->on_done) {
				info->on_done(v);
			}

		delete info;
	}
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

DbClientPtr DbClient::create() 
{
	if(g_db_json_file.empty()) {
		return DbClientPtr(new CouchbaseDbClient);
	} else {
		return DbClientPtr(new FileBackedDbClient(g_db_json_file));
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
