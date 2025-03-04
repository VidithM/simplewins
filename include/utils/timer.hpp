#pragma once

#include <iostream>
#include <chrono>
#include <string>

namespace swins {
    namespace utils {
        class Timer {
            private:
                std::chrono::time_point<std::chrono::high_resolution_clock> start;
            public:
                Timer();
                void reset();
                void now(uint64_t *res, std::string msg);
        };
    }
}