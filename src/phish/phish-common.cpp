#include "string.h"
#include "phish.h"
#include "phish-common.h"

#include <iostream>
#include <sstream>
#include <sys/time.h>

std::string g_backend = std::string();
std::string g_host = std::string();
std::string g_executable = std::string();
std::string g_school_id = std::string();
int g_local_id = 0;
int g_local_count = 0;
int g_global_id = 0;
int g_global_count = 0;
bool g_initialized = false;
bool g_checked = false;
uint64_t g_received_count = 0;
uint64_t g_sent_count = 0;

void(*g_all_input_ports_closed)() = 0;
void(*g_at_abort)(int*) = 0;

const std::string pop_argument(std::vector<std::string>& arguments)
{
  const std::string argument = arguments.front();
  arguments.erase(arguments.begin());
  return argument;
}

int get_argc(const std::vector<std::string>& arguments)
{
  return arguments.size();
}

char** get_argv(const std::vector<std::string>& arguments)
{
  char** argv = new char*[arguments.size()];
  for(unsigned int i = 0; i != arguments.size(); ++i)
  {
    argv[i] = new char[arguments[i].size() + 1];
    strcpy(argv[i], arguments[i].c_str());
  }
  return argv;
}

int phish_abort_internal()
{
  if (!g_at_abort) return true;

  int cancel = false;
  g_at_abort(&cancel);
  return !cancel;
}

void phish_message(const char* type, const char* message)
{
  std::cerr << "PHISH " << type << ": Minnow " << g_executable << " ID " 
            << g_school_id << " # " << g_global_id << ": " << message 
            << std::endl;
}

void phish_stats()
{
  std::ostringstream message;
  message << g_received_count << " " << g_sent_count << " datums recv/sent";
  phish_message("Stats", message.str().c_str());
}

// Provide default implementations for some parts of the PHISH API ...

extern "C"
{

void phish_callback(void (*done)(), void(*at_abort)(int*))
{
  g_all_input_ports_closed = done;
  g_at_abort = at_abort;
}

double phish_timer()
{
  timeval t;
  ::gettimeofday(&t, 0);
  return t.tv_sec + (t.tv_usec / 1000000.0);
}

const char* phish_host()
{
  return g_host.data();
}

void phish_error(const char* message)
{
  phish_message("ERROR", message);
  phish_abort();
}

void phish_warn(const char* message)
{
  phish_message("WARNING", message);
}

} // extern "C"
