/* ----------------------------------------------------------------------
   PHISH = Parallel Harness for Informatic Stream Hashing
   http://www.sandia.gov/~sjplimp/phish.html
   Steve Plimpton, sjplimp@sandia.gov, Sandia National Laboratories

   Copyright (2012) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under 
   the modified Berkeley Software Distribution (BSD) License.

   See the README file in the top-level PHISH directory.
------------------------------------------------------------------------- */

#include <iostream>
#include <cstring>
#include <map>
#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <unistd.h>
#include <string.h>
#include <zmq.h>

#include "phish/hashlittle.h"
#include "phish/phish.h"
#include "phish/phish-bait-common.h"
#include "phish/phish-common.h"

#define phish_return_error(message, code) { phish_error(message); return code; }

///////////////////////////////////////////////////////////////////////////////////
// Internal state

// ZMQ context and incoming sockets ...
static void* g_context = 0;
static void* g_control_port = 0;
static void* g_input_port = 0;

// Input port configuration ...

// Stores a message callback for each input port ...
static std::map<int, void(*)(int)> g_input_port_message_callback;
// Stores a port-closed callback for each input port ...
static std::map<int, void(*)()> g_input_port_closed_callback;
// Stores whether an input port is optional ...
static std::map<int, bool> g_input_port_required;
// Stores the number of incoming connections for each input port ...
static std::map<int, int> g_input_port_connection_count;

// Stores the maximum size of a datum, in bytes ...
static int g_datum_size = 1024;
// Keeps pointers to every allocated datum ...
static std::vector<char*> g_datum_pool;
// Keeps pointers to unused datums ...
static std::vector<char*> g_unused_datum_pool;

char* use_datum()
{
  if(g_unused_datum_pool.empty())
  {
    g_datum_pool.push_back(new char[g_datum_size]());
    return g_datum_pool.back();
  }

  char* const result = g_unused_datum_pool.back();
  g_unused_datum_pool.pop_back();
  return result;
}

void release_datum(char* datum)
{
  g_unused_datum_pool.push_back(datum);
}

// Temporary storage for packing outgoing datums ...
static char* g_pack_begin = 0;
static char* g_pack_end = 0;

inline uint32_t& pack_count()
{
  return *reinterpret_cast<uint32_t*>(g_pack_begin);
}

// Temporary storage for packing incoming datums ...
static char* g_unpack_begin = 0;
static char* g_unpack_current = 0;
static char* g_unpack_end = 0;

inline uint32_t& unpack_count()
{
  return *reinterpret_cast<uint32_t*>(g_unpack_begin);
}

// Keeps track of whether a message loop is running ...
static bool g_running = false;

/// Defines a collection of message recipients (zmq sockets).
typedef std::vector<void*> recipients_t;
/// Abstract interface for classes that route messages to their destination(s).
class output_connection
{
public:
  output_connection(int input_port, const recipients_t& recipients);
  virtual ~output_connection();
  virtual void send() = 0;
  virtual void send_hashed(char* key, int key_length) = 0;
  virtual void send_direct(int destination) = 0;

  int recipient_count();

protected:
  const int m_input_port;
  const recipients_t m_recipients;
  void raw_send(void* recipient);
};

static std::map<int, std::vector<output_connection*> > g_output_connections;
static std::set<int> g_defined_output_ports;

//////////////////////////////////////////////////////////////////////////////////
// Internal implementation details

template<typename T>
static inline void pack_value(const T& data, uint8_t type)
{
  if(g_pack_end + sizeof(uint8_t) + sizeof(T) > g_pack_begin + g_datum_size)
    phish_error("Send buffer overflow.");
  pack_count() += 1;
  *reinterpret_cast<uint8_t*>(g_pack_end) = type;
  g_pack_end += sizeof(uint8_t);
  *reinterpret_cast<T*>(g_pack_end) = data;
  g_pack_end += sizeof(T);
}

template<typename T>
static inline void pack_array(const T* data, int32_t count, uint8_t type)
{
  if(g_pack_end + sizeof(uint8_t) + sizeof(uint32_t) + (count * sizeof(T)) > g_pack_begin + g_datum_size)
    phish_error("Send buffer overflow.");
  pack_count() += 1;
  *reinterpret_cast<uint8_t*>(g_pack_end) = type;
  g_pack_end += sizeof(uint8_t);
  *reinterpret_cast<uint32_t*>(g_pack_end) = count;
  g_pack_end += sizeof(uint32_t);
  ::memcpy(g_pack_end, data, count * sizeof(T));
  g_pack_end += count * sizeof(T);
}

template <typename T>
static inline void unpack_value(char** data, int32_t* count)
{
  *count = 1;

  *data = g_unpack_current;
  g_unpack_current += sizeof(T);
}

template <typename T>
static inline void unpack_array(char** data, int32_t* count)
{
  *count = *reinterpret_cast<uint32_t*>(g_unpack_current);
  g_unpack_current += sizeof(uint32_t);

  *data = g_unpack_current;
  g_unpack_current += *count * sizeof(T);
}

/// Defines a container for a collection of ports.
typedef std::vector<int> port_collection;
const port_collection output_ports()
{
  port_collection ports;
  for(std::map<int, std::vector<output_connection*> >::iterator i = g_output_connections.begin(); i != g_output_connections.end(); ++i)
    ports.push_back(i->first);
  return ports;
}

/// Defines constants for managing message framing & control.
enum message_frame
{
  PORT_MASK = 0x7f,
  TYPE_MASK = 0x80,
  CLOSE_MESSAGE = 0x80
};

/*
void zmq_assert(int rc)
{
  if(rc < 0)
    throw std::runtime_error(zmq_strerror(errno));
}
*/

// output_connection implementation.
output_connection::output_connection(int input_port, const recipients_t& recipients) :
  m_input_port(input_port),
  m_recipients(recipients)
{
}

output_connection::~output_connection()
{
  const uint8_t frame = (m_input_port & PORT_MASK) | CLOSE_MESSAGE;
  for(recipients_t::const_iterator recipient = m_recipients.begin(); recipient != m_recipients.end(); ++recipient)
  {
    zmq_send(*recipient, &frame, sizeof(frame), 0);
  }

/* Don't close the zmq sockets, see https://zeromq.jira.com/browse/LIBZMQ-229 */

  for(recipients_t::const_iterator recipient = m_recipients.begin(); recipient != m_recipients.end(); ++recipient)
  {
    zmq_close(*recipient);
  }
}

int output_connection::recipient_count()
{
  return m_recipients.size();
}

void output_connection::raw_send(void* recipient)
{
  const uint8_t frame = (m_input_port & PORT_MASK);
  if(pack_count())
  {
    zmq_send(recipient, &frame, sizeof(frame), ZMQ_SNDMORE);
    zmq_send(recipient, g_pack_begin, g_pack_end - g_pack_begin, 0);
  }
  else
  {
    zmq_send(recipient, &frame, sizeof(frame), 0);
  }
}

/// output_connection implementation that broadcasts messages to every recipient.
class broadcast_connection :
  public output_connection
{
public:
  broadcast_connection(int input_port, const recipients_t& recipients) :
    output_connection(input_port, recipients)
  {
  }

  void send()
  {
    for(recipients_t::const_iterator recipient = m_recipients.begin(); recipient != m_recipients.end(); ++recipient)
    {
      raw_send(*recipient);
    }
  }

  void send_hashed(char* key, int key_length)
  {
    phish_warn("Cannot send hashed with broadcast connection.");
  }

  void send_direct(int destination)
  {
    phish_warn("Cannot send direct with broadcast connection.");
  }
};

/// output_connection implementation that sends a message to one recipient, choosing recipients in round-robin order.
class round_robin_connection :
  public output_connection
{
  int m_index;

public:
  round_robin_connection(int input_port, const recipients_t& recipients) :
    output_connection(input_port, recipients),
    m_index(0)
  {
  }

  void send()
  {
    void* const recipient = m_recipients[m_index];
    m_index = (m_index + 1) % m_recipients.size();
    raw_send(recipient);
  }

  void send_hashed(char* key, int key_length)
  {
    phish_warn("Cannot send hashed with roundrobin connection.");
  }

  void send_direct(int destination)
  {
    phish_warn("Cannot send direct with roundrobin connection.");
  }
};

/// output_connection implementation that sends a message to one recipient, choosing recipients using a hashed key supplied by the caller.
class hashed_connection :
  public output_connection
{
public:
  hashed_connection(int input_port, const recipients_t& recipients) :
    output_connection(input_port, recipients)
  {
  }

  void send()
  {
    phish_warn("Cannot send over hashed connection without key.");
  }

  void send_hashed(char* key, int key_length)
  {
    int index = hashlittle(key, key_length, 0) % m_recipients.size();
    void* const recipient = m_recipients[index];
    raw_send(recipient);
  }

  void send_direct(int destination)
  {
    phish_warn("Cannot send direct with hashed connection.");
  }
};

/// output_connection implementation that sends a two-part message to one recipient, which is specified by the caller.
class direct_connection :
  public output_connection
{
public:
  direct_connection(int input_port, const recipients_t& recipients) :
    output_connection(input_port, recipients)
  {
  }

  void send()
  {
    phish_warn("Cannot send over direct connection without recipient.");
  }

  void send_hashed(char* key, int key_length)
  {
    phish_warn("Cannot send hashed with direct connection.");
  }

  void send_direct(int destination)
  {
    int index = destination % m_recipients.size();
    void* const recipient = m_recipients[index];
    raw_send(recipient);
  }
};

///////////////////////////////////////////////////////////////////////////////////
// Public API

// Compatibility API for minnows written in C ...
extern "C"
{

int phish_init(int* argc, char*** argv)
{
  try
  {
    phish_assert_not_initialized();
    g_initialized = true;

    std::vector<std::string> arguments(*argv, *argv + *argc);
    std::vector<std::string> kept_arguments;

    g_executable = arguments[0];

    uint64_t high_water_mark = 1000;

    g_context = zmq_init(2);
    while(arguments.size())
    {
      const std::string argument = pop_argument(arguments);
      if(argument == "--phish-backend")
      {
        g_backend = pop_argument(arguments);
        if(g_backend != "zmq")
        {
          std::ostringstream message;
          message << "Incompatible backend: expected zmq, using " << g_backend << ".";
          phish_return_error(message.str().c_str(), -1);
        }
      }
      else if(argument == "--phish-host")
      {
        g_host = pop_argument(arguments);
      }
      else if(argument == "--phish-school-id")
      {
        g_school_id = pop_argument(arguments);
      }
      else if(argument == "--phish-local-id")
      {
        std::istringstream stream(pop_argument(arguments));
        stream >> g_local_id;
      }
      else if(argument == "--phish-local-count")
      {
        std::istringstream stream(pop_argument(arguments));
        stream >> g_local_count;
      }
      else if(argument == "--phish-global-id")
      {
        std::istringstream stream(pop_argument(arguments));
        stream >> g_global_id;
      }
      else if(argument == "--phish-global-count")
      {
        std::istringstream stream(pop_argument(arguments));
        stream >> g_global_count;
      }
      else if(argument == "--phish-memory")
      {
        std::istringstream stream(pop_argument(arguments));
        int kilobytes = 1;
        stream >> kilobytes;

        g_datum_size = kilobytes * 1024;
      }
      else if(argument == "--phish-high-water-mark")
      {
        std::istringstream stream(pop_argument(arguments));
        stream >> high_water_mark;
      }
      else if(argument == "--phish-control-port")
      {
        const std::string address = pop_argument(arguments);
        g_control_port = zmq_socket(g_context, ZMQ_REP);
        zmq_bind(g_control_port, address.c_str());
      }
      else if(argument == "--phish-input-port")
      {
        const std::string address = pop_argument(arguments);
        g_input_port = zmq_socket(g_context, ZMQ_PULL);
        zmq_setsockopt(g_input_port, ZMQ_RCVHWM, &high_water_mark, sizeof(high_water_mark));
        zmq_bind(g_input_port, address.c_str());
      }
      else if(argument == "--phish-input-connections")
      {
        std::string spec = pop_argument(arguments);
        std::replace(spec.begin(), spec.end(), '+', ' ');
        int port = 0;
        int connection_count = 0;
        std::istringstream stream(spec);
        stream >> port >> connection_count;

        g_input_port_connection_count[port] = connection_count;

      }
      else if(argument == "--phish-output-connection")
      {
        std::string spec = pop_argument(arguments);
        std::replace(spec.begin(), spec.end(), '+', ' ');
        int output_port = 0;
        std::string pattern;
        int input_port = 0;
        std::vector<std::string> recipients;
        std::istringstream stream(spec);
        stream >> output_port >> pattern >> input_port;
        while(true)
        {
          std::string recipient;
          stream >> recipient;
          if(!stream)
            break;
          recipients.push_back(recipient);
        }

        if(!g_output_connections.count(output_port))
          g_output_connections[output_port] = std::vector<output_connection*>();

        recipients_t recipient_sockets;
        for(std::vector<std::string>::iterator recipient = recipients.begin(); recipient != recipients.end(); ++recipient)
        {
          void* const socket = zmq_socket(g_context, ZMQ_PUSH);
          zmq_setsockopt(socket, ZMQ_SNDHWM, &high_water_mark, sizeof(high_water_mark));
          zmq_connect(socket, recipient->c_str());
          recipient_sockets.push_back(socket);
        }

        if(pattern == PHISH_BAIT_SEND_PATTERN_BROADCAST)
        {
          g_output_connections[output_port].push_back(new broadcast_connection(input_port, recipient_sockets));
        }
        else if(pattern == PHISH_BAIT_SEND_PATTERN_ROUND_ROBIN)
        {
          g_output_connections[output_port].push_back(new round_robin_connection(input_port, recipient_sockets));
        }
        else if(pattern == PHISH_BAIT_SEND_PATTERN_HASHED)
        {
          g_output_connections[output_port].push_back(new hashed_connection(input_port, recipient_sockets));
        }
        else if(pattern == PHISH_BAIT_SEND_PATTERN_DIRECT)
        {
          g_output_connections[output_port].push_back(new direct_connection(input_port, recipient_sockets));
        }
        else
        {
          std::ostringstream message;
          message << "Unknown send pattern: " << pattern;
          phish_return_error(message.str().c_str(), -1);
        }
      }
      else
      {
        kept_arguments.push_back(argument);
      }
    }

    // Do some sanity checking on our command-line arguments ...
    if(g_backend.empty() || g_host.empty() || g_school_id.empty() || !g_control_port)
      phish_return_error("Missing required phish arguments.  You must use the phish bait system to execute a minnow.", -1);

    // Setup send and receive buffers ...
    g_pack_begin = use_datum();
    pack_count() = 0;
    g_pack_end = g_pack_begin + sizeof(uint32_t);

    g_unpack_begin = use_datum();
    g_unpack_current = g_unpack_begin;
    g_unpack_end = g_unpack_begin;

    // Wait to hear from the school ...
    std::string request(5, '\0');
    zmq_recv(g_control_port, &request[0], request.size(), 0);
    if(request == "start")
    {
      zmq_send(g_control_port, "ok", strlen("ok"), 0);
    }
    else
    {
      std::ostringstream message;
      message << "Unexpected request: " << request;
      phish_return_error(message.str().c_str(), -1);
    }

    // Cleanup argc & argv ...
    *argc = get_argc(kept_arguments);
    *argv = get_argv(kept_arguments);

    return 0;
  }
  catch(std::exception& e)
  {
    std::ostringstream message;
    message << "Uncaught exception: " << e.what();
    phish_return_error(message.str().c_str(), -1);
  }
}

int phish_exit()
{
  phish_assert_initialized();
  phish_assert_checked();

  // Close output ports ...
  const port_collection ports = output_ports();
  for(port_collection::const_iterator port = ports.begin(); port != ports.end(); ++port)
    phish_close(*port);

  // Cancel any running loop ...
  g_running = false;

  // Delete input port ...
  zmq_close(g_input_port);
  g_input_port = 0;

  // Delete control port ...
  zmq_close(g_control_port);
  g_control_port = 0;

  std::ostringstream message;
  message << "Allocated " << g_datum_pool.size() << " datums.";
  phish_message("Stats", message.str().c_str());
  phish_stats();

  // Cleanup the datum pool ...
  for(std::vector<char*>::iterator datum = g_datum_pool.begin(); datum != g_datum_pool.end(); ++datum)
    delete [] *datum;

  // Shut-down zmq ...
/* Don't try to shutdown zmq, see https://zeromq.jira.com/browse/LIBZMQ-229
  delete g_context;
*/
  //zmq_term(g_context);
  ::sleep(5);
  g_context = 0;

  return 0;
}

void phish_abort()
{
  if(!phish_abort_internal())
    return;

  phish_warn("Currently, phish_abort() doesn't shut-down the entire school.");
  exit(-1);
}

int phish_input(int port, void(*message_callback)(int), void(*port_closed_callback)(), int required)
{
  phish_assert_initialized();
  phish_assert_not_checked();

  g_input_port_message_callback[port] = message_callback;
  g_input_port_closed_callback[port] = port_closed_callback;
  g_input_port_required[port] = required;

  return 0;
}

int phish_output(int port)
{
  phish_assert_initialized();
  phish_assert_not_checked();

  g_defined_output_ports.insert(port);

  return 0;
}

int phish_check()
{
  phish_assert_initialized();
  phish_assert_not_checked();
  g_checked = true;

  for(std::map<int, int>::iterator port = g_input_port_connection_count.begin(); port != g_input_port_connection_count.end(); ++port)
  {
    if(!g_input_port_message_callback.count(port->first))
    {
      std::ostringstream message;
      message << g_school_id << ": unexpected connection to undefined input port " << port->first;
      phish_return_error(message.str().c_str(), -1);
    }
  }
  for(std::map<int, void(*)(int)>::iterator port = g_input_port_message_callback.begin(); port != g_input_port_message_callback.end(); ++port)
  {
    if(!g_input_port_connection_count.count(port->first) && g_input_port_required[port->first])
    {
      std::ostringstream message;
      message << g_school_id << ": required input port " << port->first << " does not have a connection.";
      phish_return_error(message.str().c_str(), -1);
    }
  }
  for(std::map<int, std::vector<output_connection*> >::iterator port = g_output_connections.begin(); port != g_output_connections.end(); ++port)
  {
    if(!g_defined_output_ports.count(port->first))
    {
      std::ostringstream message;
      message << g_school_id << ": unexpected connection from undefined output port " << port->first;
      phish_return_error(message.str().c_str(), -1);
    }
  }

  return 0;
}

int phish_close(int port)
{
  phish_assert_initialized();
  phish_assert_checked();

  if(!g_output_connections.count(port))
    return 0;
  for(unsigned int i = 0; i != g_output_connections[port].size(); ++i)
    delete g_output_connections[port][i];
  g_output_connections.erase(port);

  return 0;
}

int phish_loop()
{
  phish_assert_initialized();
  phish_assert_checked();

  if(g_running)
  {
    phish_warn("Cannot call phish_loop() while a loop is already running.");
    return 0;
  }
  g_running = true;

  while(g_running)
  {
    try
    {
      uint8_t frame = 0;
      zmq_recv(g_input_port, &frame, sizeof(frame), 0);
      const int port = (frame & PORT_MASK);

      if((frame & TYPE_MASK) == CLOSE_MESSAGE)
      {
        g_input_port_connection_count[port] -= 1;
        if(g_input_port_connection_count[port] == 0)
        {
          g_input_port_connection_count.erase(port);
          if(g_input_port_closed_callback.count(port) && g_input_port_closed_callback[port])
          {
            g_input_port_closed_callback[port]();
          }
          if(g_input_port_connection_count.size() == 0)
          {
            g_running = false;
            if(g_all_input_ports_closed)
              g_all_input_ports_closed();
          }
        }
      }
      else
      {
        g_received_count += 1;

        unpack_count() = 0;
        g_unpack_current = g_unpack_begin + sizeof(uint32_t);
        g_unpack_end = g_unpack_begin + sizeof(uint32_t);

        int64_t more_parts = 0;
        size_t more_parts_size = sizeof(more_parts);
        zmq_getsockopt(g_input_port, ZMQ_RCVMORE, &more_parts, &more_parts_size);
        if(more_parts)
        {
          const int received = zmq_recv(g_input_port, g_unpack_begin, g_datum_size, 0);
          if(received > g_datum_size)
            phish_return_error("Receive buffer overflow.", -1);
          g_unpack_end = g_unpack_begin + received;
        }

        if(g_input_port_message_callback[port])
          g_input_port_message_callback[port](unpack_count());
      }
    }
    catch(std::exception& e)
    {
      phish_warn(e.what());
    }
  }

  return 0;
}

int phish_probe(void (*idle_callback)())
{
  phish_assert_initialized();
  phish_assert_checked();

  if(g_running)
  {
    phish_warn("Cannot call phish_probe() while a loop is already running.");
    return 0;
  }
  g_running = true;

  while(g_running)
  {
    try
    {
      uint8_t frame = 0;
      const int received = zmq_recv(g_input_port, &frame, sizeof(frame), ZMQ_NOBLOCK);
      if(1 == received)
      {
        const int port = (frame & PORT_MASK);

        if((frame & TYPE_MASK) == CLOSE_MESSAGE)
        {
          g_input_port_connection_count[port] -= 1;
          if(g_input_port_connection_count[port] == 0)
          {
            g_input_port_connection_count.erase(port);
            if(g_input_port_closed_callback.count(port))
            {
              g_input_port_closed_callback[port]();
            }
            if(g_input_port_connection_count.size() == 0)
            {
              g_running = false;
              if(g_all_input_ports_closed)
                g_all_input_ports_closed();
            }
          }
        }
        else
        {
          g_received_count += 1;

          unpack_count() = 0;
          g_unpack_current = g_unpack_begin + sizeof(uint32_t);
          g_unpack_end = g_unpack_begin + sizeof(uint32_t);

          int64_t more_parts = 0;
          size_t more_parts_size = sizeof(more_parts);
          zmq_getsockopt(g_input_port, ZMQ_RCVMORE, &more_parts, &more_parts_size);
          if(more_parts)
          {
            const int received = zmq_recv(g_input_port, g_unpack_begin, g_datum_size, 0);
            if(received > g_datum_size)
              phish_return_error("Receive buffer overflow.", -1);
            g_unpack_end = g_unpack_begin + received;
          }

          if(g_input_port_message_callback[port])
            g_input_port_message_callback[port](unpack_count());
        }
      }
      else if(errno == EAGAIN)
      {
        idle_callback();
      }
    }
    catch(std::exception& e)
    {
      phish_warn(e.what());
    }
  }

  return 0;
}

int phish_recv()
{
  phish_assert_initialized();
  phish_assert_checked();

  if(g_running)
  {
    phish_warn("Cannot call phish_recv() while a loop is running.");
    return 0;
  }

  uint8_t frame = 0;
  if(zmq_recv(g_input_port, &frame, sizeof(frame), ZMQ_NOBLOCK) > 0)
  {
    const int port = (frame & PORT_MASK);

    if((frame & TYPE_MASK) == CLOSE_MESSAGE)
    {
      g_input_port_connection_count[port] -= 1;
      if(g_input_port_connection_count[port] == 0)
      {
        g_input_port_connection_count.erase(port);
        if(g_input_port_closed_callback.count(port) && g_input_port_closed_callback[port])
        {
          g_input_port_closed_callback[port]();
        }
        if(g_input_port_connection_count.size() == 0)
        {
          if(g_all_input_ports_closed)
            g_all_input_ports_closed();
          return -1;
        }
      }
    }
    else
    {
      g_received_count += 1;

      unpack_count() = 0;
      g_unpack_current = g_unpack_begin + sizeof(uint32_t);
      g_unpack_end = g_unpack_begin + sizeof(uint32_t);

      int64_t more_parts = 0;
      size_t more_parts_size = sizeof(more_parts);
      zmq_getsockopt(g_input_port, ZMQ_RCVMORE, &more_parts, &more_parts_size);
      if(more_parts)
      {
        const int received = zmq_recv(g_input_port, g_unpack_begin, g_datum_size, 0);
        if(received > g_datum_size)
          phish_return_error("Receive buffer overflow.", -1);
        g_unpack_end = g_unpack_begin + received;
      }

      if(g_input_port_message_callback[port])
        g_input_port_message_callback[port](unpack_count());
      return unpack_count();
    }
  }

  return 0;
}

void phish_send(int port)
{
  if(!g_output_connections.count(port))
  {
    std::ostringstream message;
    message << "Cannot send message to closed port: " << port;
    phish_warn(message.str().c_str());
    return;
  }

  const int end = g_output_connections[port].size();
  for(int i = 0; i != end; ++i)
    g_output_connections[port][i]->send();

  g_sent_count += 1;

  pack_count() = 0;
  g_pack_end = g_pack_begin + sizeof(uint32_t);
}

void phish_send_key(int port, char* key, int key_length)
{
  if(!g_output_connections.count(port))
  {
    std::ostringstream message;
    message << "Cannot send message to closed port: " << port;
    phish_warn(message.str().c_str());
    return;
  }

  const int end = g_output_connections[port].size();
  for(int i = 0; i != end; ++i)
    g_output_connections[port][i]->send_hashed(key, key_length);

  g_sent_count += 1;

  pack_count() = 0;
  g_pack_end = g_pack_begin + sizeof(uint32_t);
}

void phish_send_direct(int port, int receiver)
{
  if(!g_output_connections.count(port))
  {
    std::ostringstream message;
    message << "Cannot send message to closed port: " << port;
    phish_warn(message.str().c_str());
    return;
  }

  const int end = g_output_connections[port].size();
  for(int i = 0; i != end; ++i)
    g_output_connections[port][i]->send_direct(receiver);

  g_sent_count += 1;

  pack_count() = 0;
  g_pack_end = g_pack_begin + sizeof(uint32_t);
}

void phish_reset_receiver(int, int)
{
  phish_warn("phish_reset_receiver() Not implemented.");
}

void phish_repack()
{
  release_datum(g_pack_begin);

  g_pack_begin = g_unpack_begin;
  g_pack_end = g_unpack_end;

  g_unpack_begin = use_datum();
  g_unpack_current = g_unpack_begin;
  g_unpack_end = g_unpack_begin;
}

void phish_pack_raw(char* data, int32_t length)
{
  pack_array(data, length, PHISH_RAW);
}

void phish_pack_char(char value)
{
  pack_value(value, PHISH_CHAR);
}

void phish_pack_int8(int8_t value)
{
  pack_value(value, PHISH_INT8);
}

void phish_pack_int16(int16_t value)
{
  pack_value(value, PHISH_INT16);
}

void phish_pack_int32(int32_t value)
{
  pack_value(value, PHISH_INT32);
}

void phish_pack_int64(int64_t value)
{
  pack_value(value, PHISH_INT64);
}

void phish_pack_uint8(uint8_t value)
{
  pack_value(value, PHISH_UINT8);
}

void phish_pack_uint16(uint16_t value)
{
  pack_value(value, PHISH_UINT16);
}

void phish_pack_uint32(uint32_t value)
{
  pack_value(value, PHISH_UINT32);
}

void phish_pack_uint64(uint64_t value)
{
  pack_value(value, PHISH_UINT64);
}

void phish_pack_float(float value)
{
  pack_value(value, PHISH_FLOAT);
}

void phish_pack_double(double value)
{
  pack_value(value, PHISH_DOUBLE);
}

void phish_pack_string(char* value)
{
  pack_array(value, strlen(value) + 1, PHISH_STRING);
}

void phish_pack_int8_array(int8_t* array, int32_t count)
{
  pack_array(array, count, PHISH_INT8_ARRAY);
}

void phish_pack_int16_array(int16_t* array, int32_t count)
{
  pack_array(array, count, PHISH_INT16_ARRAY);
}

void phish_pack_int32_array(int32_t* array, int32_t count)
{
  pack_array(array, count, PHISH_INT32_ARRAY);
}

void phish_pack_int64_array(int64_t* array, int32_t count)
{
  pack_array(array, count, PHISH_INT64_ARRAY);
}

void phish_pack_uint8_array(uint8_t* array, int32_t count)
{
  pack_array(array, count, PHISH_UINT8_ARRAY);
}

void phish_pack_uint16_array(uint16_t* array, int32_t count)
{
  pack_array(array, count, PHISH_UINT16_ARRAY);
}

void phish_pack_uint32_array(uint32_t* array, int32_t count)
{
  pack_array(array, count, PHISH_UINT32_ARRAY);
}

void phish_pack_uint64_array(uint64_t* array, int32_t count)
{
  pack_array(array, count, PHISH_UINT64_ARRAY);
}

void phish_pack_float_array(float* array, int32_t count)
{
  pack_array(array, count, PHISH_FLOAT_ARRAY);
}

void phish_pack_double_array(double* array, int32_t count)
{
  pack_array(array, count, PHISH_DOUBLE_ARRAY);
}

void phish_pack_pickle(char* data, int32_t count)
{
  pack_array(data, count, PHISH_PICKLE);
}

int phish_unpack(char** data, int32_t* count)
{
  if(g_unpack_current >= g_unpack_end)
    phish_return_error("No data to unpack.", -1);

  const uint8_t type = *reinterpret_cast<uint8_t*>(g_unpack_current);
  g_unpack_current += sizeof(uint8_t);

  switch(type)
  {
    case PHISH_CHAR:
      unpack_value<char>(data, count);
      return type;

    case PHISH_INT8:
      unpack_value<int8_t>(data, count);
      return type;

    case PHISH_INT16:
      unpack_value<int16_t>(data, count);
      return type;

    case PHISH_INT32:
      unpack_value<int32_t>(data, count);
      return type;

    case PHISH_INT64:
      unpack_value<int64_t>(data, count);
      return type;

    case PHISH_UINT8:
      unpack_value<uint8_t>(data, count);
      return type;

    case PHISH_UINT16:
      unpack_value<uint16_t>(data, count);
      return type;

    case PHISH_UINT32:
      unpack_value<uint32_t>(data, count);
      return type;

    case PHISH_UINT64:
      unpack_value<uint64_t>(data, count);
      return type;

    case PHISH_FLOAT:
      unpack_value<float>(data, count);
      return type;

    case PHISH_DOUBLE:
      unpack_value<double>(data, count);
      return type;

    case PHISH_RAW:
      unpack_array<char>(data, count);
      return type;

    case PHISH_STRING:
      unpack_array<char>(data, count);
      return type;

    case PHISH_INT8_ARRAY:
      unpack_array<int8_t>(data, count);
      return type;

    case PHISH_INT16_ARRAY:
      unpack_array<int16_t>(data, count);
      return type;

    case PHISH_INT32_ARRAY:
      unpack_array<int32_t>(data, count);
      return type;

    case PHISH_INT64_ARRAY:
      unpack_array<int64_t>(data, count);
      return type;

    case PHISH_UINT8_ARRAY:
      unpack_array<uint8_t>(data, count);
      return type;

    case PHISH_UINT16_ARRAY:
      unpack_array<uint16_t>(data, count);
      return type;

    case PHISH_UINT32_ARRAY:
      unpack_array<uint32_t>(data, count);
      return type;

    case PHISH_UINT64_ARRAY:
      unpack_array<uint64_t>(data, count);
      return type;

    case PHISH_FLOAT_ARRAY:
      unpack_array<float>(data, count);
      return type;

    case PHISH_DOUBLE_ARRAY:
      unpack_array<double>(data, count);
      return type;

    case PHISH_PICKLE:
      unpack_array<char>(data, count);
      return type;
  }

  phish_return_error("Unknown datum type.", -1);
}

int phish_query(const char* kw, int flag1, int flag2)
{
  const std::string keyword(kw);

  if(keyword == "idlocal")
    return g_local_id;
  else if(keyword == "nlocal")
    return g_local_count;
  else if(keyword == "idglobal")
    return g_global_id;
  else if(keyword == "nglobal")
    return g_global_count;
  else if(keyword == "outport/connections")
  {
    if(0 == g_output_connections.count(flag1))
      phish_return_error("Invalid phish_query flags", -3);
    return g_output_connections[flag1].size(); 
  }
  else if(keyword == "outport/nminnows")
  {
    if(0 == g_output_connections.count(flag1))
      phish_return_error("Invalid phish_query port", -3);
    if(flag2 < 0 || (unsigned)flag2 >= g_output_connections[flag1].size())
      phish_return_error("Invalid phish_query connection", -3);
    return g_output_connections[flag1][flag2]->recipient_count();
  }
  else
    phish_return_error("Invalid phish_query keyword.", -1);
}

} // extern "C"
