#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>

extern "C" {

int printmap() {

    std::ifstream fi;
    std::string line;
    int pid = getpid();

    std::cout << "PID:" << pid << std::endl;

    fi.open("/proc/" + std::to_string(pid) + "/maps");
    if (fi.is_open()) {
        while (getline(fi, line)) {
            std::cout << line << std::endl;
        }
        fi.close();
    } else {
        std::cout << "Unable to open map file\n";
    }

    return 0;
}

}
