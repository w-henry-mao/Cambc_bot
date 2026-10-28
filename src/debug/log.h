#pragma once
#include <iostream>
namespace debug {
constexpr bool kLogging = true;
inline void log(const char* msg) {
    if constexpr (kLogging) {
        std::cout << msg << std::endl;
    }
}
}
