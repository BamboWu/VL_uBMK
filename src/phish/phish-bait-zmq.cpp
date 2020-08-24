#include "phish-bait.h"
#include "phish-bait-common.h"

#include <zmq.h>

#include <iostream>
#include <cstring>
#include <iterator>
#include <map>
#include <stdexcept>

#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>

struct zmq_minnow
{
  std::string control_port_internal;
  //std::string control_port_external; // IPC never remote
  std::string input_port_internal;
  //std::string input_port_external; // IPC never remote
};

const std::string quoted_string(const std::string& string)
{
  return "\"" + string + "\"";
}

extern "C"
{

int phish_bait_start()
{
  try
  {
    const bool verbose = (g_settings.count("verbose") != 0) && (g_settings["verbose"] == "true");
    int next_port = 5555;

    // Setup temporary storage for zmq-specific information
    std::vector<zmq_minnow> zmq_minnows(g_minnows.size());

    // Assign control port addresses ...
    for(unsigned int i = 0; i != zmq_minnows.size(); ++i)
    {
      //const school& school = g_schools[g_minnows[i].school_index];
      //const minnow& minnow = g_minnows[i];

      zmq_minnows[i].control_port_internal = "ipc:///tmp/endpt" + string_cast(next_port);
      // IPC never remote
      //zmq_minnows[i].control_port_external = "tcp://" + school.hosts[minnow.local_id] + ":" + string_cast(next_port);
      ++next_port;
    }

    // Assign input port addresses ...
    for(unsigned int i = 0; i != zmq_minnows.size(); ++i)
    {
      //const school& school = g_schools[g_minnows[i].school_index];
      //const minnow& minnow = g_minnows[i];

      zmq_minnows[i].input_port_internal = "ipc:///tmp/endpt" + string_cast(next_port);
      // IPC never remote
      //zmq_minnows[i].input_port_external = "tcp://" + school.hosts[minnow.local_id] + ":" + string_cast(next_port);
      ++next_port;
    }

    std::vector<pid_t> processes;

    // Create each of the minnow processes ...
    if(verbose)
      std::cerr << "BAIT ZMQ: Creating minnows." << std::endl;

    for(unsigned int i = 0; i != zmq_minnows.size(); ++i)
    {
      const school& school = g_schools[g_minnows[i].school_index];
      const minnow& minnow = g_minnows[i];
      const std::string host = school.hosts[minnow.local_id];

      const bool remote = false; // With ipc protocal, never remote

      std::vector<std::string> arguments;
      arguments.insert(arguments.end(), school.arguments.begin(), school.arguments.end());
      arguments.push_back("--phish-backend");
      arguments.push_back("zmq");
      arguments.push_back("--phish-host");
      arguments.push_back(remote ? quoted_string(host) : host);
      arguments.push_back("--phish-school-id");
      arguments.push_back(remote ? quoted_string(school.id) : school.id);
      arguments.push_back("--phish-local-id");
      arguments.push_back(string_cast(minnow.local_id));
      arguments.push_back("--phish-local-count");
      arguments.push_back(string_cast(school.count));
      arguments.push_back("--phish-global-id");
      arguments.push_back(string_cast(i));
      arguments.push_back("--phish-global-count");
      arguments.push_back(string_cast(g_minnows.size()));
      arguments.push_back("--phish-memory");
      arguments.push_back(g_settings.count("memory") ? g_settings["memory"] : "1024");
      arguments.push_back("--phish-high-water-mark");
      arguments.push_back(g_settings.count("high-water-mark") ? g_settings["high-water-mark"] : "1000");
      arguments.push_back("--phish-control-port");
      arguments.push_back(remote ? quoted_string(zmq_minnows[i].control_port_internal) : zmq_minnows[i].control_port_internal);
      arguments.push_back("--phish-input-port");
      arguments.push_back(remote ? quoted_string(zmq_minnows[i].input_port_internal) : zmq_minnows[i].input_port_internal);

      for(std::map<int, int>::const_iterator incoming = minnow.incoming.begin(); incoming != minnow.incoming.end(); ++incoming)
      {
        const int port = incoming->first;
        const int count = incoming->second;

        std::ostringstream buffer;
        buffer << port << "+" << count;

        arguments.push_back("--phish-input-connections");
        arguments.push_back(remote ? quoted_string(buffer.str()) : buffer.str());
      }

      for(std::vector<connection>::const_iterator connection = minnow.outgoing.begin(); connection != minnow.outgoing.end(); ++connection)
      {
        std::ostringstream buffer;
        for(std::vector<int>::const_iterator i = connection->input_indices.begin(); i != connection->input_indices.end(); ++i)
          buffer << "+" << zmq_minnows[*i].input_port_internal;  // IPC never remote

        std::ostringstream buffer2;
        buffer2 << string_cast(connection->output_port) << "+" << connection->send_pattern << "+" << string_cast(connection->input_port) << buffer.str();

        arguments.push_back("--phish-output-connection");
        arguments.push_back(remote ? quoted_string(buffer2.str()) : buffer2.str());
      }

      if(remote)
      {
        arguments.insert(arguments.begin(), host);
        arguments.insert(arguments.begin(), "-x");
        arguments.insert(arguments.begin(), "ssh");
      }

      if(verbose)
      {
        std::cerr << "BAIT ZMQ Command: ";
        std::copy(arguments.begin(), arguments.end(), std::ostream_iterator<std::string>(std::cerr, " "));
        std::cerr << std::endl;
      }

      switch(int pid = ::fork())
      {
        case -1:
        {
          throw std::runtime_error(strerror(errno));
        }
        case 0:
        {
          std::vector<char*> argv;
          for(unsigned int i = 0; i != arguments.size(); ++i)
            argv.push_back(const_cast<char*>(arguments[i].c_str()));
          argv.push_back(0);

          ::execvp(arguments[0].c_str(), argv.data());
          throw std::runtime_error(strerror(errno)); // Only reached if execvp() fails.
        }
        default:
        {
          processes.push_back(pid);

          // Horrible hack to prevent remote hosts from thinking that our ssh connections are a DoS attack.
          ::sleep(1);

          continue;
        }
      }
    }

    // Tell each minnow to begin processing ...
    if(verbose)
      std::cerr << "BAIT ZMQ: Starting minnows." << std::endl;

    void* const context = zmq_init(2);
    for(unsigned int i = 0; i != processes.size(); ++i)
    {
      void* const socket = zmq_socket(context, ZMQ_REQ);
      zmq_connect(socket, zmq_minnows[i].control_port_internal.c_str()); // IPC never remote
      zmq_send(socket, "start", strlen("start"), 0);

      std::string response(2, '\0');
      zmq_recv(socket, &response[0], response.size(), 0);
      if(response != "ok")
        throw std::runtime_error("Minnow failed to start.");
      zmq_close(socket);
    }
    if(verbose)
      std::cerr << "BAIT ZMQ: Minnows started." << std::endl;


    // Wait for processes to terminate ...
    for(unsigned int i = 0; i != processes.size(); ++i)
    {
      int status = 0;
      int options = 0;
      ::waitpid(processes[i], &status, options);
    }
    zmq_term(context);
  }
  catch(std::exception& e)
  {
    std::cerr << "PHISH BAIT ERROR: " << e.what() << std::endl;
    return -1;
  }
  return 0;
}

} // extern "C"
