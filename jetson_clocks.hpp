#ifndef JETSON_CLOCKS_HPP_
#define JETSON_CLOCKS_HPP_

//--------------------------------------------------------//
//                   DOCUMENTATION                        //
//--------------------------------------------------------//
//
// Author: Jordan Ford
// Contact: jsford@andrew.cmu.edu
// Date: September 2019
//
// Description:
// jetson_clocks.hpp is a single header C++ library for
// controlling the various power states of the NVidia
// Jetson Nano, Jetson TX1, Jetson TX2(i),
// and Jetson AGX Xavier.
//
// Notes:
//
// Thus far, this has only been tested on the Jetson TX2 and
// Jetson Nano, but I will happily accept pull requests to
// fix bugs on any platform.
//
// This code is not thread safe (yet?). Do not manipulate power
// state across multiple threads without implementing your own
// synchronization.
//
// License:
//   Copyright (c) 2019 Jordan Ford
//
//   Permission is hereby granted, free of charge, to any person obtaining a
//   copy
//   of this software and associated documentation files (the "Software"), to
//   deal
//   in the Software without restriction, including without limitation the
//   rights
//   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//   copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions:
//
//   The above copyright notice and this permission notice shall be included in
//   all
//   copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//   FROM,
//   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//   THE
//   SOFTWARE.

//--------------------------------------------------------//
//                    INTERFACE                           //
//--------------------------------------------------------//

#include <string>
#include <vector>

namespace jetson_clocks {

/// Check if this process is running with root user permissions.
bool running_as_root();

/// Determine the SOC family of this board.
std::string get_soc_family();

/// Determine the machine type of this board.
std::string get_machine();

/// Set the fan pwm speed of this board.
bool set_fan_speed(unsigned char speed);

/// Get the fan pwm speed of this board.
unsigned char get_fan_speed();

/// Get all available GPU clock frequencies.
std::vector<long int> get_gpu_available_freqs();

/// Set the GPU min and max frequencies.
bool set_gpu_freq_range(long int min_freq, long int max_freq);

/// Get the min and max allowed EMC clock freqs.
std::pair<long int, long int> get_emc_available_freq_range();

/// Set the EMC clock freq.
bool set_emc_freq(long int freq);

/// Get the EMC clock freq.
long int get_emc_freq();

/// Get the ids of all cpus.
std::vector<int> get_cpu_ids();

/// Get the available governors for a given cpu.
std::vector<std::string> get_cpu_available_governors(int cpu_id);

/// Get the available clock frequencies for a given cpu.
std::vector<long int> get_cpu_available_freqs(int cpu_id);

/// Get the current clock governor for a given cpu.
std::string get_cpu_governor(int cpu_id);

/// Get the current minimum clock frequency for a given cpu.
long int get_cpu_min_speed(int cpu_id);

/// Get the current maximum clock frequency for a given cpu.
long int get_cpu_max_speed(int cpu_id);

/// Get the current clock frequency for a given cpu.
long int get_cpu_cur_speed(int cpu_id);

/// Set the clock governor for a given cpu.
bool set_cpu_governor(int cpu_id, const std::string &gov);

/// Set the minimum clock frequency for a given cpu.
bool set_cpu_min_freq(int cpu_id, long int min_freq);

/// Set the maximum clock frequency for a given cpu.
bool set_cpu_max_freq(int cpu_id, long int max_freq);

} // namespace jetson_clocks

//--------------------------------------------------------//
//                    IMPLEMENTATION                      //
//--------------------------------------------------------//

#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace jetson_clocks {

template <typename T> std::string to_string(T value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

bool running_as_root() { return (geteuid() == 0); }

bool file_exists(const std::string &name) {
  std::ifstream f(name.c_str());
  return f.good();
}

bool file_writable(const std::string &name) {
  if (!file_exists(name)) {
    return false;
  }
  FILE *fp = fopen(name.c_str(), "w");
  return fp != NULL;
}

std::string read_file(const std::string &name) {
  std::ifstream t(name.c_str());
  std::stringstream buffer;
  buffer << t.rdbuf();
  return buffer.str();
}

bool write_file(const std::string &name, const std::string &str) {
  if (!file_writable(name)) {
    return false;
  }
  std::ofstream out(name.c_str());
  out << str;
  return true;
}

std::string strip_newline(const std::string &input) {
  std::string output = input;
  output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
  return output;
}

std::vector<std::string> list_subdirs(const std::string &path) {
  int dir_count = 0;
  struct dirent *dent;
  DIR *srcdir = opendir(path.c_str());

  if (srcdir == NULL) {
    return {};
  }

  std::vector<std::string> dirs;

  while ((dent = readdir(srcdir)) != NULL) {
    struct stat st;

    if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
      continue;

    if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      dirs.push_back(dent->d_name);
    }
  }
  closedir(srcdir);
  return dirs;
}

std::string get_soc_family() {
  std::string soc_family = "";
  if (file_exists("/sys/devices/soc0/family")) {
    soc_family = read_file("/sys/devices/soc0/family");
  } else if (file_exists("/proc/device-tree/compatible")) {
    std::string compat_file = read_file("/proc/device-tree/compatible");
    if (compat_file.find("nvidia,tegra210") != std::string::npos) {
      soc_family = "tegra210";
    } else if (compat_file.find("nvidia,tegra186") != std::string::npos) {
      soc_family = "tegra186";
    }
  }
  return soc_family;
}

std::string get_machine() {
  std::string machine = "";
  if (file_exists("/sys/devices/soc0/family")) {
    if (file_exists("/sys/devices/soc0/machine")) {
      machine = read_file("/sys/devices/soc0/machine");
    }
  } else if (file_exists("/proc/device-tree/model")) {
    machine = read_file("/proc/device-tree/model");
  }
  return machine;
}

bool set_fan_speed(unsigned char speed) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot change fan speed without running as root."
              << std::endl;
    return false;
  }

  // Jetson-TK1 CPU fan is always ON.
  if (get_machine() == "jetson-tk1") {
    return true;
  }

  if (!file_writable("/sys/kernel/debug/tegra_fan/target_pwm")) {
    std::cout << "Can't access Fan!" << std::endl;
    return false;
  }

  std::string speed_string = to_string(static_cast<int>(speed));
  return write_file("/sys/kernel/debug/tegra_fan/target_pwm", speed_string);
}

unsigned char get_fan_speed() {
  if (!running_as_root()) {
    std::cout << "Error: Cannot read fan speed without running as root."
              << std::endl;
    return false;
  }

  // Jetson-TK1 CPU fan is always ON.
  if (get_machine() == "jetson-tk1") {
    return 255;
  }

  if (!file_exists("/sys/kernel/debug/tegra_fan/target_pwm")) {
    std::cout << "Can't access Fan!" << std::endl;
    return 0;
  }

  return std::stoi(read_file("/sys/kernel/debug/tegra_fan/target_pwm"));
}

std::vector<long int> get_gpu_available_freqs() {
  if (!running_as_root()) {
    std::cout
        << "Error: Cannot look up gpu available freqs without running as root."
        << std::endl;
    return {}; // NOTE(Jordan): This causes segfaults I think.
  }

  std::string soc_family = get_soc_family();

  std::string GPU_AVAILABLE_FREQS = "";

  if (soc_family == "tegra186") {
    GPU_AVAILABLE_FREQS = "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/"
                          "available_frequencies";
  } else if (soc_family == "tegra210") {
    GPU_AVAILABLE_FREQS =
        "/sys/devices/57000000.gpu/devfreq/57000000.gpu/available_frequencies";
  } else {
    std::cout << "Error: unsupported SOC " << soc_family << std::endl;
    return {}; // NOTE(Jordan): This causes segfaults I think.
  }

  std::string speedstr = read_file(GPU_AVAILABLE_FREQS);
  std::istringstream iss(speedstr);

  long int s;
  std::vector<long int> speeds;
  while (iss >> s) {
    speeds.push_back(s);
  }

  std::sort(speeds.begin(), speeds.end(), std::less<long int>());
  return speeds;
}

bool set_gpu_freq_range(long int min_freq, long int max_freq) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot set GPU freq range without running as root."
              << std::endl;
    return false;
  }

  std::vector<long int> available_freqs = get_gpu_available_freqs();

  // Min freq must be available.
  if (std::find(available_freqs.begin(), available_freqs.end(), min_freq) ==
      available_freqs.end()) {
    std::cout << "Error: Selected gpu minimum frequency is not available. "
                 "Options are: \n";
    for (const auto &s : available_freqs) {
      std::cout << s << "\n";
    }
    return false;
  }

  // Max freq must be available.
  if (std::find(available_freqs.begin(), available_freqs.end(), max_freq) ==
      available_freqs.end()) {
    std::cout << "Error: Selected gpu maximum frequency is not available. "
                 "Options are: \n";
    for (const auto &s : available_freqs) {
      std::cout << s << "\n";
    }
    return false;
  }

  std::string soc_family = get_soc_family();

  std::string GPU_MIN_FREQ = "";
  std::string GPU_MAX_FREQ = "";
  std::string GPU_CUR_FREQ = "";
  std::string GPU_RAIL_GATE = "";

  if (soc_family == "tegra186") {
    GPU_MIN_FREQ =
        "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/min_freq";
    GPU_MAX_FREQ =
        "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/max_freq";
    GPU_RAIL_GATE = "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/device/"
                    "railgate_enable";
  } else if (soc_family == "tegra210") {
    GPU_MIN_FREQ = "/sys/devices/57000000.gpu/devfreq/57000000.gpu/min_freq";
    GPU_MAX_FREQ = "/sys/devices/57000000.gpu/devfreq/57000000.gpu/max_freq";
    GPU_RAIL_GATE =
        "/sys/devices/57000000.gpu/devfreq/57000000.gpu/device/railgate_enable";
  } else {
    std::cout << "Error: unsupported SOC " << soc_family << std::endl;
    return false;
  }

  bool success = true;
  success &= write_file(GPU_MIN_FREQ, to_string(min_freq));
  success &= write_file(GPU_MAX_FREQ, to_string(max_freq));
  success &= write_file(GPU_RAIL_GATE, "0");
  return success;
}

std::pair<long int, long int> get_emc_available_freq_range() {
  if (!running_as_root()) {
    std::cout << "Error: Cannot look up EMC freqs without running as root."
              << std::endl;
    return std::make_pair(0, 0);
  }

  std::string soc_family = get_soc_family();

  std::string EMC_ISO_CAP = "";
  std::string EMC_MIN_FREQ = "";
  std::string EMC_MAX_FREQ = "";

  if (soc_family == "tegra186") {
    EMC_ISO_CAP = "/sys/kernel/nvpmodel_emc_cap/emc_iso_cap";
    EMC_MIN_FREQ = "/sys/kernel/debug/bpmp/debug/clk/emc/min_rate";
    EMC_MAX_FREQ = "/sys/kernel/debug/bpmp/debug/clk/emc/max_rate";

    long int emc_cap = std::stoi(read_file(EMC_ISO_CAP));
    long int emc_fmax = std::stoi(read_file(EMC_MAX_FREQ));
    if (emc_cap > 0 && emc_cap < emc_fmax) {
      EMC_MAX_FREQ = EMC_ISO_CAP;
    }
  } else if (soc_family == "tegra210") {
    EMC_MIN_FREQ = "/sys/kernel/debug/tegra_bwmgr/min_rate";
    EMC_MAX_FREQ = "/sys/kernel/debug/tegra_bwmgr/max_rate";
  } else {
    std::cout << "Error: unsupported SOC " << soc_family << std::endl;
    return std::make_pair(0, 0);
  }

  long int min_freq = std::stoi(read_file(EMC_MIN_FREQ));
  long int max_freq = std::stoi(read_file(EMC_MAX_FREQ));
  return std::make_pair(min_freq, max_freq);
}

long int get_emc_freq() {
  if (!running_as_root()) {
    std::cout << "Error: Cannot read EMC freq without running as root."
              << std::endl;
    return false;
  }

  std::string soc_family = get_soc_family();

  std::string EMC_UPDATE_FREQ = "";
  std::string EMC_FREQ_OVERRIDE = "";

  if (soc_family == "tegra186") {
    EMC_UPDATE_FREQ = "/sys/kernel/debug/bpmp/debug/clk/emc/rate";
    EMC_FREQ_OVERRIDE = "/sys/kernel/debug/bpmp/debug/clk/emc/mrq_rate_locked";
  } else if (soc_family == "tegra210") {
    EMC_UPDATE_FREQ = "/sys/kernel/debug/clk/override.emc/clk_update_rate";
    EMC_FREQ_OVERRIDE = "/sys/kernel/debug/clk/override.emc/clk_state";
  } else {
    std::cout << "Error: unsupported SOC " << soc_family << std::endl;
    return false;
  }

  return std::stoi(read_file(EMC_UPDATE_FREQ));
}

bool set_emc_freq(long int freq) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot change EMC freq without running as root."
              << std::endl;
    return false;
  }

  long int min_freq, max_freq;
  std::tie(min_freq, max_freq) = get_emc_available_freq_range();
  if (freq < min_freq || freq > max_freq) {
    std::cout << "Error: EMC freq must be in the range [" << min_freq << ", "
              << max_freq << "]" << std::endl;
    return false;
  }

  std::string soc_family = get_soc_family();

  std::string EMC_UPDATE_FREQ = "";
  std::string EMC_FREQ_OVERRIDE = "";

  if (soc_family == "tegra186") {
    EMC_UPDATE_FREQ = "/sys/kernel/debug/bpmp/debug/clk/emc/rate";
    EMC_FREQ_OVERRIDE = "/sys/kernel/debug/bpmp/debug/clk/emc/mrq_rate_locked";
  } else if (soc_family == "tegra210") {
    EMC_UPDATE_FREQ = "/sys/kernel/debug/clk/override.emc/clk_update_rate";
    EMC_FREQ_OVERRIDE = "/sys/kernel/debug/clk/override.emc/clk_state";
  } else {
    std::cout << "Error: unsupported SOC " << soc_family << std::endl;
    return false;
  }

  bool success = true;
  success &= write_file(EMC_UPDATE_FREQ, to_string(freq));
  success &= write_file(EMC_FREQ_OVERRIDE, "1");
  return true;
}

std::vector<int> get_cpu_ids() {
  if (!running_as_root()) {
    std::cout << "Error: Cannot look up CPU ids without running as root."
              << std::endl;
    return {}; // NOTE(Jordan): This causes segfaults I think.
  }

  // Find all directories in /sys/devices/system/cpu/ ending in cpu[0-9].
  std::vector<std::string> cpu_subdirs;
  {
    std::vector<std::string> all_subdirs =
        list_subdirs("/sys/devices/system/cpu/");

    std::regex reg("(.*)cpu[0-9]");
    std::copy_if(
        all_subdirs.begin(), all_subdirs.end(), std::back_inserter(cpu_subdirs),
        [&reg](const std::string &str) { return std::regex_match(str, reg); });
  }

  std::vector<int> ids;
  for (const auto &dir : cpu_subdirs) {
    ids.push_back(dir[dir.size() - 1] - '0');
  }
  std::sort(ids.begin(), ids.end(), std::less<int>());
  return ids;
}

std::vector<long int> get_cpu_available_freqs(int cpu_id) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot get CPU available frequencies without running "
                 "as root."
              << std::endl;
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_available_frequencies";

  if (!file_exists(path)) {
    std::cout << "Error: Cannot get cpu" << cpu_id
              << " available frequencies because " << path
              << " does not exist.";
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string speedstr = read_file(path);
  std::istringstream iss(speedstr);

  long int s;
  std::vector<long int> speeds;
  while (iss >> s) {
    speeds.push_back(s);
  }

  if (speeds.size() == 0) {
    std::cout << "Error: cpu" << cpu_id << " available frequencies not found."
              << std::endl;
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::sort(speeds.begin(), speeds.end(), std::less<long int>());
  return speeds;
}

std::vector<std::string> get_cpu_available_governors(int cpu_id) {
  if (!running_as_root()) {
    std::cout
        << "Error: Cannot get CPU available governors without running as root."
        << std::endl;
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_available_governors";

  if (!file_exists(path)) {
    std::cout << "Error: Cannot get cpu" << cpu_id
              << " available governors because " << path << " does not exist.";
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string govstr = strip_newline(read_file(path));

  std::istringstream iss(govstr);
  std::vector<std::string> governors((std::istream_iterator<std::string>(iss)),
                                     std::istream_iterator<std::string>());
  return governors;
}

std::string get_cpu_governor(int cpu_id) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot get CPU governor without running as root."
              << std::endl;
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_governor";

  if (!file_exists(path)) {
    std::cout << "Error: Cannot get cpu" << cpu_id << " governor because "
              << path << " does not exist.";
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string governor = strip_newline(read_file(path));
  return governor;
}

long int get_cpu_min_freq(int cpu_id) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot get CPU min. freq. without running as root."
              << std::endl;
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_min_freq";

  if (!file_exists(path)) {
    std::cout << "Error: Cannot get cpu" << cpu_id << " min. freq. because "
              << path << " does not exist.";
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  long int min_freq = std::stoi(read_file(path));

  return min_freq;
}

long int get_cpu_max_freq(int cpu_id) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot get CPU max. freq. without running as root."
              << std::endl;
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_max_freq";

  if (!file_exists(path)) {
    std::cout << "Error: Cannot get cpu" << cpu_id << " max. freq. because "
              << path << " does not exist.";
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  long int max_freq = std::stoi(read_file(path));

  return max_freq;
}

long int get_cpu_cur_freq(int cpu_id) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot get CPU cur. freq. without running as root."
              << std::endl;
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_cur_freq";

  if (!file_exists(path)) {
    std::cout << "Error: Cannot get cpu" << cpu_id << " cur. freq. because "
              << path << " does not exist.";
    return {}; // NOTE(Jordan): This probably causes segfaults.
  }

  long int cur_freq = std::stoi(read_file(path));

  return cur_freq;
}

bool set_cpu_min_freq(int cpu_id, long int min_freq) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot set CPU min. freq. without running as root."
              << std::endl;
    return false;
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_min_freq";
  if (!file_writable(path)) {
    std::cout << "Error: Cannot set cpu" << cpu_id << " min. freq. because "
              << path << " is not writable." << std::endl;
    return false;
  }

  auto available_freqs = get_cpu_available_freqs(cpu_id);
  if (available_freqs.size() == 0) {
    std::cout << "Error: cpu" << cpu_id << " available freqs not found.\n";
    return false;
  }

  if (std::find(available_freqs.begin(), available_freqs.end(), min_freq) ==
      available_freqs.end()) {
    std::cout << "Error: " << min_freq
              << " is not an available cpu frequency.\nOptions are: ";
    for (const auto &f : available_freqs) {
      std::cout << f << " ";
    }
    std::cout << std::endl;
    return false;
  }

  bool success = true;
  success &= write_file("/sys/module/qos/parameters/enable", "0");

  if (get_soc_family() == "tegra186") {
    success &=
        write_file("/sys/kernel/debug/tegra_cpufreq/M_CLUSTER/cc3/enable", "0");
    success &=
        write_file("/sys/kernel/debug/tegra_cpufreq/B_CLUSTER/cc3/enable", "0");
  }

  success &= write_file(path, to_string(min_freq));

  return true;
}

bool set_cpu_max_freq(int cpu_id, long int max_freq) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot set CPU max. freq. without running as root."
              << std::endl;
    return false;
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_max_freq";
  if (!file_writable(path)) {
    std::cout << "Error: Cannot set cpu" << cpu_id << " max. freq. because "
              << path << " is not writable." << std::endl;
    return false;
  }

  auto available_freqs = get_cpu_available_freqs(cpu_id);
  if (available_freqs.size() == 0) {
    std::cout << "Error: cpu" << cpu_id << " available freqs not found.\n";
    return false;
  }

  if (std::find(available_freqs.begin(), available_freqs.end(), max_freq) ==
      available_freqs.end()) {
    std::cout << "Error: " << max_freq
              << " is not an available cpu frequency.\nOptions are: ";
    for (const auto &f : available_freqs) {
      std::cout << f << " ";
    }
    std::cout << std::endl;
    return false;
  }

  bool success = true;
  success &= write_file("/sys/module/qos/parameters/enable", "0");

  if (get_soc_family() == "tegra186") {
    success &=
        write_file("/sys/kernel/debug/tegra_cpufreq/M_CLUSTER/cc3/enable", "0");
    success &=
        write_file("/sys/kernel/debug/tegra_cpufreq/B_CLUSTER/cc3/enable", "0");
  }

  success &= write_file(path, to_string(max_freq));

  return true;
}

bool set_cpu_governor(int cpu_id, const std::string &governor) {
  if (!running_as_root()) {
    std::cout << "Error: Cannot set CPU max. freq. without running as root."
              << std::endl;
    return false;
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_governor";
  if (!file_exists(path)) {
    std::cout << "Error: Cannot set cpu" << cpu_id << " governor because "
              << path << " does not exist." << std::endl;
    return false;
  }

  auto available_govs = get_cpu_available_governors(cpu_id);
  if (available_govs.size() == 0) {
    std::cout << "Error: cpu" << cpu_id << " available governors not found.\n";
    return false;
  }

  if (std::find(available_govs.begin(), available_govs.end(), governor) ==
      available_govs.end()) {
    std::cout << "Error: " << governor
              << " is not an available governor.\nOptions are: ";
    for (const auto &g : available_govs) {
      std::cout << g << " ";
    }
    std::cout << std::endl;
    return false;
  }

  bool success = true;
  success &= write_file("/sys/module/qos/parameters/enable", "0");

  if (get_soc_family() == "tegra186") {
    success &=
        write_file("/sys/kernel/debug/tegra_cpufreq/M_CLUSTER/cc3/enable", "0");
    success &=
        write_file("/sys/kernel/debug/tegra_cpufreq/B_CLUSTER/cc3/enable", "0");
  }

  success &= write_file(path, governor);

  return true;
}

} // namespace jetson_clock

#endif // JETSON_CLOCKS_HPP_
