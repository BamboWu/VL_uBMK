#ifndef PHISH_BAIT_COMMON_H
#define PHISH_BAIT_COMMON_H

#include <map>
#include <sstream>
#include <string>
#include <vector>

#define PHISH_BAIT_SEND_PATTERN_BROADCAST "broadcast"
#define PHISH_BAIT_SEND_PATTERN_DIRECT "direct"
#define PHISH_BAIT_SEND_PATTERN_HASHED "hashed"
#define PHISH_BAIT_SEND_PATTERN_ROUND_ROBIN "roundrobin"

/// Converts any type to std::string.
template<typename T>
std::string string_cast(const T& value)
{
  std::ostringstream buffer;
  buffer << value;
  return buffer.str();
}

/// Storage for a school.
struct school
{
  school(const std::string& _id, const std::vector<std::string>& _hosts, const std::vector<std::string>& _arguments, int _first_global_id);

  /// Stores a unique identifier for the school.
  std::string id;
  /// Stores the set of hostnames for the school.
  std::vector<std::string> hosts;
  /// Stores the command line arguments for the school.
  std::vector<std::string> arguments;

  /// Stores the number of minnows in this school.
  int count;
  /// Stores the zero-based global index of the first minnow in this school.
  int first_global_id;
};

/// Storage for a hook.
struct hook
{
  hook(const std::string& _output_id, int _output_port, const std::string& _style, int _input_port, const std::string& _input_id);

  /// Stores the output school id.
  std::string output_id;
  /// Stores the output port.
  int output_port;
  /// Stores the hook style, which will be one of the PHISH_BAIT_XXXX constants defined in phish-bait.h.
  std::string style;
  /// Stores the input port.
  int input_port;
  /// Stores the input school id.
  std::string input_id;
};

/// Storage for an individual connection.  Note that connections are derived from hook information as a convenience
/// for backend implementations.
struct connection
{
  connection(int _output_port, const std::string& _send_pattern, int _input_port, const std::vector<int>& _input_indices);

  /// Stores the zero-based output port.
  int output_port;
  /// Stores the send pattern for the connection, which will be one of the PHISH_BAIT_SEND_PATTERN_XXXX constants.  Note
  /// that send pattern is not the same as the hook style.
  std::string send_pattern;
  /// Stores the zero-based input port.
  int input_port;
  /// Stores a collection of input minnow indices.
  std::vector<int> input_indices;
};

/// Storage for an individual minnow with connectivity information.  Note that minnows are derived from school and hook information
/// as a convenience for backend implementations.
struct minnow
{
  minnow(int _school_index, int _local_id);

  /// Stores the zero-based index for the school of which this minnow is a part.
  int school_index;
  /// Stores the zero-based local index of this minnow within its school.
  int local_id;
  /// Stores the number of incoming connections per input port.
  std::map<int, int> incoming;
  /// Stores outgoing connections.
  std::vector<connection> outgoing;
};

/// Stores a collection of backend-specific name-value settings.
extern std::map<std::string, std::string> g_settings;
/// Stores a mapping from school id to school index.
extern std::map<std::string, int> g_school_index_map;
/// Stores a mapping from school id to minnow index.
extern std::map<std::string, std::vector<int> > g_minnow_index_map;
/// Stores the current set of schools.
extern std::vector<school> g_schools;
/// Stores the current set of hooks.
extern std::vector<hook> g_hooks;
/// Stores the current set of minnows.
extern std::vector<minnow> g_minnows;

#endif // !PHISH_BAIT_COMMON_H
