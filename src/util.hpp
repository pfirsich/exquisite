#pragma once

#include <string_view>

struct Vec {
    // top left is 0, 0
    size_t x, y;
};

[[noreturn]] void die(std::string_view msg);
