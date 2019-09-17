#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unistd.h>

namespace jetson_clocks {

static const std::string ENV_HOME = std::getenv("HOME");
static const std::string DEFAULT_CONF_FILE = ENV_HOME + "/l4t_dfs.conf";

bool running_as_root() {
    return (geteuid() == 0);
}

bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

bool file_writable(const std::string& name) {
    FILE *fp = fopen(name.c_str(), "w");
    return fp != NULL;
}

std::string read_file(const std::string& name) {
    std::ifstream t(name.c_str());
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

bool write_file(const std::string& name, const std::string& str) {
    if(!file_writable(name)) { return false; }
    std::ofstream out(name.c_str());
    out << str;
    return true;
}

std::string get_soc_family() {
    std::string soc_family = "";
    if(file_exists("/sys/devices/soc0/family")) {
        soc_family = read_file("/sys/devices/soc0/family");
    } else if(file_exists("/proc/device-tree/compatible")) {
       std::string compat_file = read_file("/proc/device-tree/compatible");
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
            machine = read_file("/sys/devices/soc0/machine");
        }
    } else if(file_exists("/proc/device-tree/model")) {
        machine = read_file("/proc/device-tree/model");
    }
    return machine;
}

bool set_fan_speed(unsigned char speed) {
    // Jetson-TK1 CPU fan is always ON.
    if(get_machine() == "jetson-tk1") {
        return true;
    }

    if(!file_writable("/sys/kernel/debug/tegra_fan/target_pwm")) {
        std::cout << "Can't access Fan!" << std::endl;
        return false;
    }

    std::string speed_string = std::to_string(speed);
    return write_file("/sys/kernel/debug/tegra_fan/target_pwm", speed_string);
}


}   // namespace jetson_clocks
