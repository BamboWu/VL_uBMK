#ifndef PHISH_COMMON_H
#define PHISH_COMMON_H

#include <string>
#include <vector>

/// Stores the current backend ...
extern std::string g_backend;
/// Stores the host for this minnow ...
extern std::string g_host;
/// Stores the name of the minnow executable ...
extern std::string g_executable;
/// Stores the school ID for this minnow ...
extern std::string g_school_id;
/// This minnow's ID within the local school ...
extern int g_local_id;
/// Number of minnows in the local school ...
extern int g_local_count;
/// This minnow's ID within the global net ...
extern int g_global_id;
/// Number of minnows in the global net ...
extern int g_global_count;
/// Set to true after phish_init() is called ...
extern bool g_initialized;
/// Set to true after phish_check() is called ...
extern bool g_checked;
/// Keeps track of the number of datums received ...
extern uint64_t g_received_count;
/// Keeps track of the number of datums sent ...
extern uint64_t g_sent_count;

/// Stores a callback to be called when the last input port is closed ...
extern void(*g_all_input_ports_closed)();
/// Stores an optional callback to be called by phish_abort() ...
extern void(*g_at_abort)(int*);

/// Removes an argument from the beginning of a collection of arguments.  Preconditions: arguments.empty() == false.
const std::string pop_argument(std::vector<std::string>& arguments);
/// Returns a value suitable for assignment to argc
int get_argc(const std::vector<std::string>& arguments);
/// Returns a value suitable for assignment to argv
char** get_argv(const std::vector<std::string>& arguments);

/// Returns zero to cancel phish_abort(), nonzero to allow the process to exit.
int phish_abort_internal();

/// Prints a message using a standardized format.
void phish_message(const char* type, const char* message);

/// Prints a message about current minnow statistics.
void phish_stats();

#define phish_return_error(message, code) { phish_error(message); return code; }
#define phish_assert_initialized() { if (!g_initialized) { phish_return_error("phish_init() has not been called.", -2); } }
#define phish_assert_not_initialized() { if (g_initialized) { phish_return_error("phish_init() has already been called.", -3); } }
#define phish_assert_checked() { if (!g_checked) { phish_return_error("phish_check() has not been called.", -4); } }
#define phish_assert_not_checked() { if (g_checked) { phish_return_error("phish_check() has already been called.", -5); } }

#endif // !PHISH_COMMON_H
