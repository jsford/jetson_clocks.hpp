#include "jetson_clocks.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace std;
using namespace jetson_clocks;

int main(int argc, char* argv[]) {

    std::cout << get_soc_family() << std::endl;
    std::cout << get_machine() << std::endl;
    set_fan_speed(255);

    return 0;
}
