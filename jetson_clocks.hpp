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

#include <stdexcept>
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
void set_fan_speed(unsigned char speed);

/// Get the fan pwm speed of this board.
unsigned char get_fan_speed();

/// Get all available GPU clock frequencies.
std::vector<long int> get_gpu_available_freqs();

/// Set the GPU min and max frequencies.
void set_gpu_freq_range(long int min_freq, long int max_freq);

/// Get the current GPU clock freq.
long int get_gpu_cur_freq();

/// Get the minimum GPU clock freq.
long int get_gpu_min_speed();

/// Get the maximum GPU clock freq.
long int get_gpu_max_speed();

/// Get the current GPU usage.
int get_gpu_current_usage();

/// Get the allowed EMC clock freqs.
std::vector<long int> get_emc_available_freqs();

/// Set the EMC clock freq.
void set_emc_freq(long int freq);

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
long int get_cpu_min_freq(int cpu_id);

/// Get the current maximum clock frequency for a given cpu.
long int get_cpu_max_freq(int cpu_id);

/// Get the current clock frequency for a given cpu.
long int get_cpu_cur_freq(int cpu_id);

/// Set the clock governor for a given cpu.
void set_cpu_governor(int cpu_id, const std::string &gov);

/// Set the minimum clock frequency for a given cpu.
void set_cpu_min_freq(int cpu_id, long int min_freq);

/// Set the maximum clock frequency for a given cpu.
void set_cpu_max_freq(int cpu_id, long int max_freq);

/// Functions will throw this exception if they cannot fulfill their purpose.
struct JetsonClocksException : public virtual std::runtime_error {
  explicit JetsonClocksException(const char *message)
      : std::runtime_error(message) {}
  explicit JetsonClocksException(const std::string &message)
      : std::runtime_error(message.c_str()) {}
};

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

  std::vector<std::string> dirs;

  if (srcdir == NULL) {
    return dirs;
  }

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
    if (compat_file.find("nvidia,tegra210") != std::string::npos) { // Nano
      soc_family = "tegra210";
    } else if (compat_file.find("nvidia,tegra186") != std::string::npos) {
      soc_family = "tegra186";
    } else if (compat_file.find("nvidia,tegra194") != std::string::npos) {
      soc_family = "tegra194";
    }
  } else {
    throw JetsonClocksException("SOC family cannot be found.");
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
  } else {
    throw JetsonClocksException("machine type cannot be found.");
  }
  return machine;
}

void set_fan_speed(unsigned char speed) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "fan speed can not be set without root permissions.");
  }

  // Jetson-TK1 CPU fan is always ON.
  if (get_machine() == "jetson-tk1") {
    return;
  }

  std::string path = "";

  if (file_writable("/sys/kernel/debug/tegra_fan/target_pwm")) {
    path = "/sys/kernel/debug/tegra_fan/target_pwm";
  } else if (file_writable("/sys/devices/pwm-fan/target_pwm")) {
    path = "/sys/devices/pwm-fan/target_pwm";
  } else {
    throw JetsonClocksException("fan speed file not found.");
  }

  std::string speed_string = to_string(static_cast<int>(speed));
  write_file(path, speed_string);
}

unsigned char get_fan_speed() {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "fan speed cannot be read without root permissions.");
  }

  // Jetson-TK1 CPU fan is always ON.
  if (get_machine() == "jetson-tk1") {
    return 255;
  }

  std::string path = "";

  if (file_writable("/sys/kernel/debug/tegra_fan/target_pwm")) {
    path = "/sys/kernel/debug/tegra_fan/target_pwm";
  } else if (file_writable("/sys/devices/pwm-fan/target_pwm")) {
    path = "/sys/devices/pwm-fan/target_pwm";
  } else {
    throw JetsonClocksException("fan speed file not found.");
  }

  return std::stoi(read_file(path));
}

std::vector<long int> get_gpu_available_freqs() {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot read gpu available freqs without root permissions.");
  }

  std::string soc_family = get_soc_family();

  std::string GPU_AVAILABLE_FREQS = "";

  if (soc_family == "tegra186") {
    GPU_AVAILABLE_FREQS = "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/"
                          "available_frequencies";
  } else if (soc_family == "tegra210") {
    GPU_AVAILABLE_FREQS =
        "/sys/devices/57000000.gpu/devfreq/57000000.gpu/available_frequencies";
  } else if (soc_family == "tegra194") {
    GPU_AVAILABLE_FREQS = "/sys/devices/17000000.gv11b/devfreq/17000000.gv11b/"
                          "available_frequencies";
  }else {
    throw JetsonClocksException(
        "cannot read gpu available freqs with unsupported SOC family " +
        soc_family + ".");
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

void set_gpu_freq_range(long int min_freq, long int max_freq) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot set gpu freq range without root permissions.");
  }

  std::vector<long int> available_freqs = get_gpu_available_freqs();

  // Min freq must be available.
  if (std::find(available_freqs.begin(), available_freqs.end(), min_freq) ==
      available_freqs.end()) {
    throw JetsonClocksException(
        "selected gpu minimum frequency is not available.");
  }

  // Max freq must be available.
  if (std::find(available_freqs.begin(), available_freqs.end(), max_freq) ==
      available_freqs.end()) {
    throw JetsonClocksException(
        "selected gpu maximum frequency is not available.");
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
  } else if (soc_family == "tegra194") {
    GPU_MIN_FREQ =
        "/sys/devices/17000000.gv11b/devfreq/17000000.gv11b/min_freq";
    GPU_MAX_FREQ =
        "/sys/devices/17000000.gv11b/devfreq/17000000.gv11b/max_freq";
    GPU_RAIL_GATE = "/sys/devices/17000000.gv11b/devfreq/17000000.gv11b/device/"
                    "railgate_enable";
  } else {
    throw JetsonClocksException(
        "cannot gpu frequency range with unsupported SOC family " + soc_family +
        ".");
  }

  write_file(GPU_MIN_FREQ, to_string(min_freq));
  write_file(GPU_MAX_FREQ, to_string(max_freq));
  write_file(GPU_RAIL_GATE, "0");
}

long int get_gpu_cur_freq() {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot get gpu current freq. without root permissions.");
  }

  std::string soc_family = get_soc_family();

  std::string PATH = "";

  if (soc_family == "tegra186") { // TODO
    PATH = "";
  } else if (soc_family == "tegra210") { // TODO
    PATH = "";
  } else if (soc_family == "tegra194") {
    PATH = "/sys/devices/17000000.gv11b/devfreq/17000000.gv11b/cur_freq";
  } else {
    throw JetsonClocksException(
        "cannot get current gpu frequency with unsupported SOC family " + soc_family +
        ".");
  }

  if (!file_exists(PATH)) {
    throw JetsonClocksException("cannot get current gpu freq. because " + PATH +
                                " does not exist.");
  }

  long int cur_freq = std::stoi(read_file(PATH));

  return cur_freq;
}

long int get_gpu_min_freq() {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot get gpu min freq. without root permissions.");
  }

  std::string soc_family = get_soc_family();

  std::string PATH = "";

  if (soc_family == "tegra186") { // TODO
    PATH = "";
  } else if (soc_family == "tegra210") { // TODO
    PATH = "";
  } else if (soc_family == "tegra194") {
    PATH = "/sys/devices/17000000.gv11b/devfreq/17000000.gv11b/min_freq";
  } else {
    throw JetsonClocksException(
        "cannot get gpu min frequency with unsupported SOC family " + soc_family +
        ".");
  }

  if (!file_exists(PATH)) {
    throw JetsonClocksException("cannot get min gpu freq. because " + PATH +
                                " does not exist.");
  }

  long int min_freq = std::stoi(read_file(PATH));

  return min_freq;
}

long int get_gpu_max_freq() {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot get gpu max freq. without root permissions.");
  }

  std::string soc_family = get_soc_family();

  std::string PATH = "";

  if (soc_family == "tegra186") { // TODO
    PATH = "";
  } else if (soc_family == "tegra210") { // TODO
    PATH = "";
  } else if (soc_family == "tegra194") {
    PATH = "/sys/devices/17000000.gv11b/devfreq/17000000.gv11b/max_freq";
  } else {
    throw JetsonClocksException(
        "cannot get gpu max frequency with unsupported SOC family " + soc_family +
        ".");
  }

  if (!file_exists(PATH)) {
    throw JetsonClocksException("cannot get max gpu freq. because " + PATH +
                                " does not exist.");
  }

  long int max_freq = std::stoi(read_file(PATH));

  return max_freq;
}

int get_gpu_current_usage() {
  std::string soc_family = get_soc_family();

  std::string GPU_USAGE = "";
  if (soc_family == "tegra210") {
    GPU_USAGE = "/sys/devices/gpu.0/load";
  } else {
    throw JetsonClocksException("cannot get current GPU usage. SOC family unsupported.");
  }

  return std::stoi(read_file(GPU_USAGE));
}

std::vector<long int> get_emc_available_freqs() {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot read EMC available freqs without root permissions.");
  }

  std::string soc_family = get_soc_family();

  std::string EMC_ISO_CAP = "";
  std::string EMC_MIN_FREQ = "";
  std::string EMC_MAX_FREQ = "";

  if (soc_family == "tegra186" || soc_family == "tegra194") {
    EMC_ISO_CAP = "/sys/kernel/nvpmodel_emc_cap/emc_iso_cap";
    EMC_MIN_FREQ = "/sys/kernel/debug/bpmp/debug/clk/emc/min_rate";
    EMC_MAX_FREQ = "/sys/kernel/debug/bpmp/debug/clk/emc/max_rate";

    long int emc_cap = std::stoi(read_file(EMC_ISO_CAP));
    long int emc_fmax = std::stoi(read_file(EMC_MAX_FREQ));
    if (emc_cap > 0 && emc_cap < emc_fmax) {
      EMC_MAX_FREQ = EMC_ISO_CAP;
    }
  } else if (soc_family == "tegra210") {
    EMC_MIN_FREQ = "/sys/kernel/debug/tegra_bwmgr/emc_min_rate";
    EMC_MAX_FREQ = "/sys/kernel/debug/tegra_bwmgr/emc_max_rate";
  } else {
    throw JetsonClocksException(
        "cannot get emc available frequencies. SOC family unsupported.");
  }

  long int min_freq = std::stoi(read_file(EMC_MIN_FREQ));
  long int max_freq = std::stoi(read_file(EMC_MAX_FREQ));
  return {min_freq, max_freq};
}

long int get_emc_freq() {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot read EMC freq without root permissions.");
  }

  std::string soc_family = get_soc_family();

  std::string EMC_UPDATE_FREQ = "";
  std::string EMC_FREQ_OVERRIDE = "";

  if (soc_family == "tegra186" | soc_family == "tagra194") {
    EMC_UPDATE_FREQ = "/sys/kernel/debug/bpmp/debug/clk/emc/rate";
    EMC_FREQ_OVERRIDE = "/sys/kernel/debug/bpmp/debug/clk/emc/mrq_rate_locked";
  } else if (soc_family == "tegra210") {
    EMC_UPDATE_FREQ = "/sys/kernel/debug/clk/override.emc/clk_update_rate";
    EMC_FREQ_OVERRIDE = "/sys/kernel/debug/clk/override.emc/clk_state";
  } else {
    throw JetsonClocksException(
        "cannot get emc frequency. SOC family unsupported.");
  }

  return std::stoi(read_file(EMC_UPDATE_FREQ));
}

void set_emc_freq(long int freq) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot set EMC freq without root permissions.");
  }

  auto emc_freqs = get_emc_available_freqs();
  long int min_freq = emc_freqs[0];
  long int max_freq = emc_freqs[emc_freqs.size() - 1];
  if (freq < min_freq || freq > max_freq) {
    throw JetsonClocksException("emc frequency not in acceptable range.");
  }

  std::string soc_family = get_soc_family();

  std::string EMC_UPDATE_FREQ = "";
  std::string EMC_FREQ_OVERRIDE = "";

  if (soc_family == "tegra186" || soc_family == "tegra194") {
    EMC_UPDATE_FREQ = "/sys/kernel/debug/bpmp/debug/clk/emc/rate";
    EMC_FREQ_OVERRIDE = "/sys/kernel/debug/bpmp/debug/clk/emc/mrq_rate_locked";
  } else if (soc_family == "tegra210") {
    EMC_UPDATE_FREQ = "/sys/kernel/debug/clk/override.emc/clk_update_rate";
    EMC_FREQ_OVERRIDE = "/sys/kernel/debug/clk/override.emc/clk_state";
  } else {
    throw JetsonClocksException(
        "cannot set emc frequency with unsupported soc_family" + soc_family +
        ".");
  }

  write_file(EMC_UPDATE_FREQ, to_string(freq));
  write_file(EMC_FREQ_OVERRIDE, "1");
}

std::vector<int> get_cpu_ids() {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot look up CPU ids without root permissions.");
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
    throw JetsonClocksException(
        "cannot look up CPU available frequencies without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_available_frequencies";

  if (!file_exists(path)) {
    throw JetsonClocksException(
        "cannot get cpu available frequencies because " + path +
        " does not exist.");
  }

  std::string speedstr = read_file(path);
  std::istringstream iss(speedstr);

  long int s;
  std::vector<long int> speeds;
  while (iss >> s) {
    speeds.push_back(s);
  }

  std::sort(speeds.begin(), speeds.end(), std::less<long int>());
  return speeds;
}

std::vector<std::string> get_cpu_available_governors(int cpu_id) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot look up CPU available governors without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_available_governors";

  if (!file_exists(path)) {
    throw JetsonClocksException(
        "cannot look up CPU available governors because " + path +
        " does not exist.");
  }

  std::string govstr = strip_newline(read_file(path));

  std::istringstream iss(govstr);
  std::vector<std::string> governors((std::istream_iterator<std::string>(iss)),
                                     std::istream_iterator<std::string>());
  return governors;
}

std::string get_cpu_governor(int cpu_id) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot get cpu governor without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_governor";

  if (!file_exists(path)) {
    throw JetsonClocksException("cannot get cpu governor because " + path +
                                " does not exist.");
  }

  std::string governor = strip_newline(read_file(path));
  return governor;
}

long int get_cpu_min_freq(int cpu_id) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot get cpu min. freq. without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_min_freq";

  if (!file_exists(path)) {
    throw JetsonClocksException("cannot get min. freq. because " + path +
                                " does not exist.");
  }

  long int min_freq = std::stoi(read_file(path));

  return min_freq;
}

long int get_cpu_max_freq(int cpu_id) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot get cpu max. freq. without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_max_freq";

  if (!file_exists(path)) {
    throw JetsonClocksException("cannot get max. freq. because " + path +
                                " does not exist.");
  }

  long int max_freq = std::stoi(read_file(path));

  return max_freq;
}

long int get_cpu_cur_freq(int cpu_id) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot get cpu current freq. without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_cur_freq";

  if (!file_exists(path)) {
    throw JetsonClocksException("cannot get current cpu freq. because " + path +
                                " does not exist.");
  }

  long int cur_freq = std::stoi(read_file(path));

  return cur_freq;
}

void set_cpu_min_freq(int cpu_id, long int min_freq) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot set CPU min. freq. without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_min_freq";
  if (!file_writable(path)) {
    throw JetsonClocksException("cannot set cpu" + to_string(cpu_id) +
                                " min. freq. because " + path +
                                " is not writable.");
  }

  auto available_freqs = get_cpu_available_freqs(cpu_id);

  if (std::find(available_freqs.begin(), available_freqs.end(), min_freq) ==
      available_freqs.end()) {
    throw JetsonClocksException(to_string(min_freq) +
                                " is not an available min. freq.");
  }

  write_file("/sys/module/qos/parameters/enable", "0");

  if (get_soc_family() == "tegra186") {
    write_file("/sys/kernel/debug/tegra_cpufreq/M_CLUSTER/cc3/enable", "0");
    write_file("/sys/kernel/debug/tegra_cpufreq/B_CLUSTER/cc3/enable", "0");
  }
  // The AGX (tegra194) has similar files at /sys/kernel/debug/tegra_cpufreq/CLUSTER[0-3]/cc3/enable.
  // On my machine, these are all '1', so I am not sure if they should be disabled.

  write_file(path, to_string(min_freq));
}

void set_cpu_max_freq(int cpu_id, long int max_freq) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot set CPU max. freq. without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_max_freq";
  if (!file_writable(path)) {
    throw JetsonClocksException("cannot set cpu" + to_string(cpu_id) +
                                " max. freq. because " + path +
                                " is not writable.");
  }

  auto available_freqs = get_cpu_available_freqs(cpu_id);

  if (std::find(available_freqs.begin(), available_freqs.end(), max_freq) ==
      available_freqs.end()) {
    throw JetsonClocksException(to_string(max_freq) +
                                " is not an available max. freq.");
  }

  write_file("/sys/module/qos/parameters/enable", "0");

  if (get_soc_family() == "tegra186") {
    write_file("/sys/kernel/debug/tegra_cpufreq/M_CLUSTER/cc3/enable", "0");
    write_file("/sys/kernel/debug/tegra_cpufreq/B_CLUSTER/cc3/enable", "0");
  }
  // The AGX (tegra194) has similar files at /sys/kernel/debug/tegra_cpufreq/CLUSTER[0-3]/cc3/enable.
  // On my machine, these are all '1', so I am not sure if they should be disabled.

  write_file(path, to_string(max_freq));
}

void set_cpu_governor(int cpu_id, const std::string &governor) {
  if (!running_as_root()) {
    throw JetsonClocksException(
        "cannot set CPU governor without root permissions.");
  }

  std::string path = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
                     "/cpufreq/scaling_governor";
  if (!file_exists(path)) {
    throw JetsonClocksException("cannot set cpu" + to_string(cpu_id) +
                                " governor because " + path +
                                " is not writable.");
  }

  auto available_govs = get_cpu_available_governors(cpu_id);

  if (std::find(available_govs.begin(), available_govs.end(), governor) ==
      available_govs.end()) {
    throw JetsonClocksException(governor + " is not an available governor.");
  }

  write_file("/sys/module/qos/parameters/enable", "0");

  if (get_soc_family() == "tegra186") {
    write_file("/sys/kernel/debug/tegra_cpufreq/M_CLUSTER/cc3/enable", "0");
    write_file("/sys/kernel/debug/tegra_cpufreq/B_CLUSTER/cc3/enable", "0");
  }
  // The AGX (tegra194) has similar files at /sys/kernel/debug/tegra_cpufreq/CLUSTER[0-3]/cc3/enable.
  // On my machine, these are all '1', so I am not sure if they should be disabled.

  write_file(path, governor);
}

} // namespace jetson_clock

#endif // JETSON_CLOCKS_HPP_
