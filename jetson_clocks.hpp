#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace jetson_clocks {

static const std::string ENV_HOME = std::getenv("HOME");

static const std::string RED="\e[0;31m";
static const std::string GREEN="\e[0;32m";
static const std::string BLUE="\e[0;34m";
static const std::string BRED="\e[1;31m";
static const std::string BGREEN="\e[1;32m";
static const std::string BBLUE="\e[1;34m";
static const std::string NC="\e[0m";

bool running_as_root() {
    return (geteuid() == 0);
}

bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

std::string cat_file(const std::string& name) {
    std::ifstream t(name.c_str());
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

std::string get_soc_family() {
    std::string soc_family = "";
    if(file_exists("/sys/devices/soc0/family")) {
        soc_family = cat_file("/sys/devices/soc0/family");
    } else if(file_exists("/proc/device-tree/compatible")) {
       std::string compat_file = cat_file("/proc/device-tree/compatible");
       if(compat_file.find("nvidia,tegra210") != std::string::npos) {
        soc_family = "tegra210";
       } else if(compat_file.find("nvidia,tegra186") != std::string::npos) {
        soc_family = "tegra186";
       }
    }
    return soc_family;
}

std::string get_machine() {
    std::string machine = "";
    if(file_exists("/sys/devices/soc0/family")) {
        if(file_exists("/sys/devices/soc0/machine")) {
            machine = cat_file("/sys/devices/soc0/machine");
        }
    } else if(file_exists("/proc/device-tree/model")) {
        machine = cat_file("/proc/device-tree/model");
    }
    return machine;
}

bool store(const std::string& conf_file) {}
bool restore(const std::string& conf_file) {}

bool do_fan() {}
bool do_clusterswitch() {}
bool do_hotplug() {}
bool do_cpu() {}
bool do_gpu() {}
bool do_emc() {}

}   // namespace jetson_clocks
