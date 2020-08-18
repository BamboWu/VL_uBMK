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

/* PHISH library API */

#ifndef PHISH_BAIT_HPP
#define PHISH_BAIT_HPP

#include <stdexcept>
#include <vector>

#include "phish-bait.h"

namespace phish
{

namespace bait
{

class exception :
  public std::exception
{
public:
  exception(int e) :
    error(e)
  {
  }

  const char* what() const throw()
  {
    switch(error)
    {
    default:
      return "Unknown error.";
    }
  }

  const int error;

  static int test(int code)
  {
    if(code < 0)
      throw exception(code);
    return code;
  }
};

void reset()
{
  ::phish_bait_reset();
}

void set(const std::string& name, const std::string& value)
{
  ::phish_bait_set(name.c_str(), value.c_str());
}

void school(const std::string& id, const std::vector<std::string>& hosts, const std::vector<std::string>& arguments)
{
  std::vector<const char*> temp_hosts;
  for(int i = 0; (size_t)i != hosts.size(); ++i)
    temp_hosts.push_back(hosts[i].c_str());

  std::vector<const char*> temp_arguments;
  for(int i = 0; (size_t)i != arguments.size(); ++i)
    temp_arguments.push_back(arguments[i].c_str());

  exception::test(::phish_bait_school(id.c_str(), temp_hosts.size(), temp_hosts.data(), temp_arguments.size(), temp_arguments.data()));
}

void hook(const std::string& output_id, int output_port, const std::string& style, int input_port, const std::string& input_id)
{
  exception::test(::phish_bait_hook(output_id.c_str(), output_port, style.c_str(), input_port, input_id.c_str()));
}

void start()
{
  exception::test(::phish_bait_start());
}

} // namespace bait

} // namespace phish

#endif // !PHISH_BAIT_HPP
