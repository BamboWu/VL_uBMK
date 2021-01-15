/**
 * main.cpp
 * @author: James Wood
 * @version: Thu July 29 18:05:00 2020
 *
 * Copyright 2020 Jonathan Beard
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include "raftlib_src.hpp"
#include "fluidcmp.hpp"

static void print_usage(char* name) {
    std::cout << "Usages:\n";
    std::cout << "     1: " << name <<
        " 1 <threadnum> <framenum> <.fluid input> [.fluid output]\n";
    std::cout << "     2: " << name <<
        " 2 <FILE> <RFILE> [cmp options]\n";
    std::cout << "     3: " << name <<
        " 3 <threadnum> <framenum> <.fluid input> <.fluid output> "
        "<RFILE> [cmp options]\n";
    return;
}

int main(int argc, char* argv[])
{
    (void) timeStep;
    if (2 > argc) {
        print_usage(argv[0]);
        return 0;
    }
    int option = atoi(argv[1]);
    switch (option) {
        case 1: fluidanimate_raftlib(argc - 1, &argv[1]); break;
        case 2: fluidcmp(argc - 1, &argv[1]); break;
        case 3:
                fluidanimate_raftlib(argc - 1, &argv[1]);
                fluidcmp(argc - 4, &argv[4]);
                break;
        default:
                print_usage(argv[0]);
    }
    return 0;
}
