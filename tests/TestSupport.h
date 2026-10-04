#pragma once

#include <cmath>
#include <stdexcept>
#include <string>

inline void check(bool value, const char* expression, const char* file, int line) {
    if (!value) {
        throw std::runtime_error(std::string(file) + ':' + std::to_string(line) + ": " + expression);
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
inline bool near(float a, float b, float tolerance = 1e-5f) {
    return std::abs(a - b) <= tolerance;
}
template <class Function> void checkThrows(Function&& function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}
