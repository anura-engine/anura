#include "db_client.hpp"

BEGIN_DEFINE_CALLABLE_NOBASE(db_client)
BEGIN_DEFINE_FN(read_modify_write, "(string, function(any)->any) ->commands")
#ifndef USE_DB_CLIENT
	return variant();
#else
	std::string key = FN_ARG(0).as_string();
	variant fn = FN_ARG(1);
	db_client* cli = const_cast<db_client*>(&obj);
	variant v(new game_logic::fn_command_callable([=]() {
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
#endif
END_DEFINE_FN

BEGIN_DEFINE_FN(remove, "(string) ->commands")
#ifndef USE_DB_CLIENT
	return variant();
#else
	std::string key = FN_ARG(0).as_string();
	db_client* cli = const_cast<db_client*>(&obj);
	variant v(new game_logic::fn_command_callable([=]() {
		cli->remove(key);
	}
	));

	return v;
	
#endif
END_DEFINE_FN
BEGIN_DEFINE_FN(get, "(string) ->any")
#ifndef USE_DB_CLIENT
	return variant();
#else
	std::string key = FN_ARG(0).as_string();
	db_client* cli = const_cast<db_client*>(&obj);

	variant result;
	bool done = false;

	cli->get(key, [&done,&result](variant res) { result = res; done = true; });
	while(!done) {
		cli->process();
	}

	return result;
		
#endif
	
END_DEFINE_FN
END_DEFINE_CALLABLE(db_client)

db_client::error::error(const std::string& message) : msg(message)
{}

db_client::~db_client() {}

#ifndef USE_DB_CLIENT

db_client_ptr db_client::create() {
	throw error("No db_client supported");
}

#else

#include "json_parser.hpp"
#include "preferences.hpp"
#include "unit_test.hpp"

#include <libcouchbase/couchbase.h>

namespace {

PREF_STRING(couchbase_host, "localhost", "");
PREF_STRING(couchbase_user, "", "");
PREF_STRING(couchbase_bucket, "default", "");
PREF_STRING(couchbase_passwd, "", "");

void couchbase_error_handler(lcb_t instance, lcb_error_t error, const char* errinfo)
{
	fprintf(stderr, "Database error: %s\n", errinfo);
	ASSERT_LOG(false, "DB Error");
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


class couchbase_db_client : public db_client
{
public:
	couchbase_db_client() : outstanding_requests_(0) {
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
		ASSERT_LOG(err == LCB_SUCCESS, "Could not connect to couchbase server: " << lcb_strerror(NULL, err));

		lcb_set_error_callback(instance_, couchbase_error_handler);

		err = lcb_connect(instance_);
		ASSERT_LOG(err == LCB_SUCCESS, "Failed to connect to couchbase server: " << lcb_strerror(NULL, err));

		lcb_set_get_callback(instance_, get_callback);
		lcb_set_remove_callback(instance_, remove_callback);
		lcb_set_store_callback(instance_, store_callback);

		lcb_wait(instance_);
	}

	~couchbase_db_client() {}

	void put(const std::string& key, variant doc, std::function<void()> on_done, std::function<void()> on_error, PUT_OPERATION op) {
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
		ASSERT_LOG(err == LCB_SUCCESS, "Error in store: " << lcb_strerror(NULL, err));
	}

	virtual void remove(const std::string& key)
	{
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
		ASSERT_LOG(err == LCB_SUCCESS, "Error in remove: " << lcb_strerror(NULL, err));
	}

	void get(const std::string& key, std::function<void(variant)> on_done, int lock_seconds)
	{
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
		ASSERT_LOG(err == LCB_SUCCESS, "Error in get: " << lcb_strerror(NULL, err));
	}

	bool process(int timeout_us)
	{
		if(timeout_us > 0) {
			lcb_error_t error = LCB_SUCCESS;
			lcb_timer_t timer = lcb_timer_create(instance_, NULL, timeout_us, 0, timer_callback, &error);
			ASSERT_LOG(error == LCB_SUCCESS, "Failed to create lcb timer");
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

	ASSERT_LOG(err == LCB_SUCCESS, "Error in store callback: " << lcb_strerror(NULL, err));
	
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
	fprintf(stderr, "ZZZ: DB get_callback()\n");
	ASSERT_LOG(err == LCB_SUCCESS || err == LCB_KEY_ENOENT, "Error in get callback: " << lcb_strerror(NULL, err));
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
	db_client_ptr client = db_client::create();

	client->get("abc", [](variant value) { fprintf(stderr, "RESULT: %s\n", value.write_json().c_str()); });
	client->process();

	client->put("abc", variant(54), []() { fprintf(stderr, "DONE\n"); }, []() { fprintf(stderr, "ERROR\n"); } );

	client->process();
}

}

db_client_ptr db_client::create() {
	return db_client_ptr(new couchbase_db_client);
}

#endif //USE_DB_CLIENT
