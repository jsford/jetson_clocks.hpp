#include "jetson_clocks.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace std;
using namespace jetson_clocks;

int main(int argc, char *argv[]) {

  // Test Board Type Discovery
  std::cout << get_soc_family() << std::endl;
  std::cout << get_machine() << std::endl;

  // Test Fan Control
  set_fan_speed(255);

  // Test GPU Controls
  auto gpu_freqs = get_gpu_available_freqs();
  auto gpu_min_freq = gpu_freqs[0];
  auto gpu_max_freq = gpu_freqs[gpu_freqs.size()-1];
  set_gpu_freq_range(gpu_min_freq, gpu_max_freq);

  // Test EMC Controls
  long int min_emc_freq, max_emc_freq;
  std::tie(min_emc_freq, max_emc_freq) = get_emc_available_freq_range();
  std::cout << "EMC Range [" << min_emc_freq << ", " << max_emc_freq << "]\n";
  set_emc_freq((max_emc_freq+min_emc_freq)/2);

  // Test CPU Controls
  auto cpu_ids = get_cpu_ids();
  for(const auto& cpu_id : cpu_ids) {
    std::cout << "ID: " << cpu_id << std::endl;

    auto available_freqs = get_cpu_available_freqs(cpu_id);
    std::cout << "\tAVAILABLE FREQS: ";
    for(const auto& f : available_freqs) { std::cout << f << " "; }
    std::cout << std::endl;

    auto available_governors = get_cpu_available_governors(cpu_id);
    std::cout << "\tAVAILABLE GOVS: ";
    for(const auto& g : available_governors) { std::cout << g << " "; }
    std::cout << std::endl;
    
    if(available_freqs.size() != 0) {
        set_cpu_min_freq(cpu_id, available_freqs[0]);
        set_cpu_max_freq(cpu_id, available_freqs[available_freqs.size()-1]);
        set_cpu_governor(cpu_id, "performance");
    }
    std::cout << "MIN FREQ: " << get_cpu_min_freq(cpu_id) << std::endl;
    std::cout << "MAX FREQ: " << get_cpu_max_freq(cpu_id) << std::endl;
    std::cout << "CUR FREQ: " << get_cpu_cur_freq(cpu_id) << std::endl;
    std::cout << "CUR GOV:  " << get_cpu_governor(cpu_id) << std::endl;
  }

  return 0;
}
