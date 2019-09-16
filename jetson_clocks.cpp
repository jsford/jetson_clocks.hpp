#include "jetson_clocks.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace std;
using namespace jetson_clocks;

void usage() {
    std::cout << "Maximize jetson performance by setting static max frequency to CPU, GPU, and EMC clocks.\n"
              << "Usage:\n"
              << "jetson_clocks [options]\n"
              << "   options,\n"
              << "   --show              display current settings\n"
              << "   --store [file]      store current settings to a file (default: ${HOME}/l4t_dfs.conf)\n"
              << "   --restore [file]    restore saved settings from a file (default: ${HOME}/l4t_dfs.conf)\n"
              << "   run jetson_clocks without any option to set static max frequency to CPU, GPU, and EMC clocks.\n";
}

int main(int argc, char* argv[]) {

    enum ACTION {NO_OP, SHOW, STORE, RESTORE} action = NO_OP;
    std::string conf_file = ENV_HOME + "/l4t_dfs.conf";

    if(argc == 1) {
        usage();
        return 0;
    }

    for(int i=1; i<argc; ++i) {
        std::string arg = argv[i];
        if(arg.find("--show") != string::npos) {
            std::cout << "SOC family:" << get_soc_family() << "   Machine:" << get_machine() << '\n';
            action = SHOW;
        } else if(arg.find("--store") != string::npos && i+1 != argc) {
            std::string next_arg = argv[i+1];
            if(next_arg.size() != 0) { 
                conf_file = next_arg;
            }
            action = STORE;
            ++i;
        } else if(arg.find("--restore") != string::npos && i+1 != argc) {
            std::string next_arg = argv[i+1];
            if(next_arg.size() != 0) { 
                conf_file = next_arg;
            }
            action = RESTORE;
            ++i;
        } else {
            usage();
            return 1;
        }
    }   

    if(!running_as_root()) {
        std::cout << RED << "Error: Run this script as a root user." << NC << std::endl;
        return 1;
    }

    switch(action) {
        case STORE:
            if(file_exists(conf_file)) {
                std::cout << "File " << conf_file << " already exists. Can I overwrite it? Y/N:";
                char overwrite = 'N';
                do {
                    std::cin >> overwrite;
                    if(overwrite == 'n') { overwrite = 'N'; }
                    if(overwrite == 'y') { overwrite = 'Y'; }
                } while( !cin.fail() && overwrite != 'Y' && overwrite != 'N');
                if(overwrite == 'N') {
                    std::cout << "Error: " << conf_file << " already exists!" << std::endl;
                    return 1;
                }
                store(conf_file);
            }
            break;
        case RESTORE:
            if(!file_exists(conf_file)) {
                std::cout << "Error: " << conf_file << " file not found!" << std::endl;
                return 1;
            }
            restore(conf_file);
            return 0;
            break;
        default:
            break;
    }

    do_hotplug();
    do_clusterswitch();
    do_cpu();
    do_gpu();
    do_emc();
    do_fan();

    return 0;
}
