#include "utils/timer.hpp"

void swins::utils::Timer::reset() {
    start = std::chrono::high_resolution_clock::now();
}

swins::utils::Timer::Timer() {
    reset();
}

void swins::utils::Timer::now(uint64_t *res, std::string msg) {
    auto now = std::chrono::high_resolution_clock::now();
    auto diff = now - start;
    uint64_t diff_val = std::chrono::duration_cast<std::chrono::milliseconds>
        (now - start).count();

    if (res == NULL) {
        std::cout << "Timer: " <<std::endl;
        if (msg.size()) {
            std::cout << "[MSG]: " << msg << ": ";
        }
        std::cout << diff_val << " (ms)" << std::endl;
    } else {
        (*res) = diff_val;
    }
}