#include "phish-bait.h"
#include "phish-bait-common.h"

#include <iostream>
#include <stdexcept>

std::map<std::string, std::string> g_settings;
std::map<std::string, int> g_school_index_map;
std::map<std::string, std::vector<int> > g_minnow_index_map;
std::vector<school> g_schools;
std::vector<hook> g_hooks;
std::vector<minnow> g_minnows;

school::school(const std::string& _id, const std::vector<std::string>& _hosts, const std::vector<std::string>& _arguments, int _first_global_id) :
  id(_id),
  hosts(_hosts),
  arguments(_arguments),
  count(_hosts.size()),
  first_global_id(_first_global_id)
{
}

hook::hook(const std::string& _output_id, int _output_port, const std::string& _style, int _input_port, const std::string& _input_id) :
  output_id(_output_id),
  output_port(_output_port),
  style(_style),
  input_port(_input_port),
  input_id(_input_id)
{
}

connection::connection(int _output_port, const std::string& _send_pattern, int _input_port, const std::vector<int>& _input_indices) :
  output_port(_output_port),
  send_pattern(_send_pattern),
  input_port(_input_port),
  input_indices(_input_indices)
{
  for(std::vector<int>::iterator i = input_indices.begin(); i != input_indices.end(); ++i)
  {
    g_minnows[*i].incoming.insert(std::make_pair(input_port, 0));
    g_minnows[*i].incoming[input_port] += 1;
  }
}

minnow::minnow(int _school_index, int _local_id) :
  school_index(_school_index),
  local_id(_local_id)
{
}

extern "C"
{

void phish_bait_reset()
{
  g_settings.clear();
  g_school_index_map.clear();
  g_minnow_index_map.clear();
  g_schools.clear();
  g_hooks.clear();
  g_minnows.clear();
}

void phish_bait_set(const char* name, const char* value)
{
  g_settings[name] = value;
}

int phish_bait_school(const char* id, int count, const char** hosts, int argc, const char** argv)
{
  try
  {
    if(g_school_index_map.count(id))
      throw std::runtime_error("Duplicate minnow id");
    if(count < 1)
      throw std::runtime_error("Minnow count must be >= 1");

    const int first_global_id = g_minnows.size();

    g_minnow_index_map[id] = std::vector<int>();
    for(int i = 0; i != count; ++i)
    {
      g_minnow_index_map[id].push_back(g_minnows.size());
      g_minnows.push_back(minnow(g_schools.size(), i));
    }

    g_school_index_map[id] = g_schools.size();
    g_schools.push_back(school(id, std::vector<std::string>(hosts, hosts + count), std::vector<std::string>(argv, argv + argc), first_global_id));

    return 0;
  }
  catch(std::exception& e)
  {
    std::cerr << "PHISH BAIT ERROR: " << e.what() << std::endl;
    return -1;
  }
}

int phish_bait_hook(const char* output_id, int output_port, const char* style, int input_port, const char* input_id)
{
  try
  {
    if(0 == g_school_index_map.count(output_id))
    {
      std::ostringstream message;
      message << "Unknown output id: " << output_id;
      throw std::runtime_error(message.str());
    }
    if(0 == g_school_index_map.count(input_id))
    {
      std::ostringstream message;
      message << "Unknown input id: " << input_id;
      throw std::runtime_error(message.str());
    }

    const std::vector<int>& output_minnows = g_minnow_index_map[output_id];
    const std::vector<int>& input_minnows = g_minnow_index_map[input_id];

    if(std::string(style) == PHISH_BAIT_SINGLE)
    {
      if(input_minnows.size() != 1)
        throw std::runtime_error("Single hook style requires exactly one input minnow.");

      for(unsigned int i = 0; i != output_minnows.size(); ++i)
      {
        g_minnows[output_minnows[i]].outgoing.push_back(connection(output_port, PHISH_BAIT_SEND_PATTERN_BROADCAST, input_port, input_minnows));
      }
    }
    else if(std::string(style) == PHISH_BAIT_PAIRED)
    {
      if(output_minnows.size() != input_minnows.size())
        throw std::runtime_error("Paired hook style requires equal numbers of output and input minnows.");

      for(unsigned int i = 0; i != output_minnows.size(); ++i)
      {
        g_minnows[output_minnows[i]].outgoing.push_back(connection(output_port, PHISH_BAIT_SEND_PATTERN_BROADCAST, input_port, std::vector<int>(1, input_minnows[i])));
      }
    }
    else if(std::string(style) == PHISH_BAIT_HASHED)
    {
      for(unsigned int i = 0; i != output_minnows.size(); ++i)
      {
        g_minnows[output_minnows[i]].outgoing.push_back(connection(output_port, PHISH_BAIT_SEND_PATTERN_HASHED, input_port, input_minnows));
      }
    }
    else if(std::string(style) == PHISH_BAIT_ROUND_ROBIN)
    {
      for(unsigned int i = 0; i != output_minnows.size(); ++i)
      {
        g_minnows[output_minnows[i]].outgoing.push_back(connection(output_port, PHISH_BAIT_SEND_PATTERN_ROUND_ROBIN, input_port, input_minnows));
      }
    }
    else if(std::string(style) == PHISH_BAIT_DIRECT)
    {
      for(unsigned int i = 0; i != output_minnows.size(); ++i)
      {
        g_minnows[output_minnows[i]].outgoing.push_back(connection(output_port, PHISH_BAIT_SEND_PATTERN_DIRECT, input_port, input_minnows));
      }
    }
    else if(std::string(style) == PHISH_BAIT_BROADCAST)
    {
      for(unsigned int i = 0; i != output_minnows.size(); ++i)
      {
        g_minnows[output_minnows[i]].outgoing.push_back(connection(output_port, PHISH_BAIT_SEND_PATTERN_BROADCAST, input_port, input_minnows));
      }
    }
    else if(std::string(style) == PHISH_BAIT_CHAIN)
    {
      if(std::string(output_id) != std::string(input_id))
        throw std::runtime_error("Chain hook style requires the same id for output and input minnows.");

      for(unsigned int i = 0; i+1 < output_minnows.size(); ++i)
      {
        g_minnows[output_minnows[i]].outgoing.push_back(connection(output_port, PHISH_BAIT_SEND_PATTERN_BROADCAST, input_port, std::vector<int>(1, input_minnows[i+1])));
      }
    }
    else if(std::string(style) == PHISH_BAIT_RING)
    {
      if(std::string(output_id) != std::string(input_id))
        throw std::runtime_error("Ring hook style requires the same id for output and input minnows.");

      for(unsigned int i = 0; i != output_minnows.size(); ++i)
      {
        g_minnows[output_minnows[i]].outgoing.push_back(connection(output_port, PHISH_BAIT_SEND_PATTERN_BROADCAST, input_port, std::vector<int>(1, input_minnows[(i+1) % input_minnows.size()])));
      }
    }
    else if(std::string(style) == PHISH_BAIT_PUBLISH)
    {
      throw std::runtime_error("Not implemented.");
    }
    else if(std::string(style) == PHISH_BAIT_SUBSCRIBE)
    {
      throw std::runtime_error("Not implemented.");
    }
    else
    {
      throw std::runtime_error("Unknown hook style");
    }

    g_hooks.push_back(hook(output_id, output_port, style, input_port, input_id));

    return 0;
  }
  catch(std::exception& e)
  {
    std::cerr << "PHISH BAIT ERROR: " << e.what() << std::endl;
    return -1;
  }
}

} // extern "C"
