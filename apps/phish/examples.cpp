#include <iostream>
#include <string>
#include "phish/phish-bait.hpp"

#ifndef MINNOW_DIR
#define MINNOW_DIR "./minnow/"
#endif

int main(int argc, char *argv[]) {
#ifdef PHISH_EXAMPLE_FILELIST
  std::vector<std::string> hosts = {"localhost"};
  std::vector<std::string> args0 = {
    std::string(MINNOW_DIR) + std::string("filegen"), std::string(argv[1])};
  std::vector<std::string> args1 = {
    std::string(MINNOW_DIR) + std::string("print")};
  phish::bait::school("1", hosts, args0);
  phish::bait::school("2", hosts, args1);
  phish::bait::hook("1", 0, "single", 0, "2");
#elif PHISH_EXAMPLE_WORDCOUNT
  std::vector<std::string> hosts = {"localhost"};
  std::vector<std::string> args0 = {
    std::string(MINNOW_DIR) + std::string("filegen"), std::string(argv[1])};
  std::vector<std::string> args1 = {
    std::string(MINNOW_DIR) + std::string("file2words")};
  std::vector<std::string> args2 = {
    std::string(MINNOW_DIR) + std::string("count")};
  std::vector<std::string> args3 = {
    std::string(MINNOW_DIR) + std::string("sort"), "10"};
  std::vector<std::string> args4 = {
    std::string(MINNOW_DIR) + std::string("print")};
  phish::bait::school("filegen", hosts, args0);
  phish::bait::school("file2words", hosts, args1);
  phish::bait::school("count", hosts, args2);
  phish::bait::school("sort", hosts, args3);
  phish::bait::school("print", hosts, args4);
  phish::bait::hook("filegen", 0, "roundrobin", 0, "file2words");
  phish::bait::hook("file2words", 0, "hashed", 0, "count");
  phish::bait::hook("count", 0, "single", 0, "sort");
  phish::bait::hook("sort", 0, "single", 0, "print");
#endif
  phish::bait::start();
  std::cout << "Done\n";
}
