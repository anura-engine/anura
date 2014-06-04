#include "../asserts.hpp"
#include "logger.hpp"

const char* log_level_names[] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"FATAL",
};

const char* get_log_level_as_string(LogLevel l)
{
	if(l <= LOG_LEVEL_FATAL && l >= LOG_LEVEL_DEBUG) {
		return log_level_names[l];
	}
	ASSERT_LOG(false, "Log level " << l << " is outside valid range");
	return NULL;
}
