#pragma once

#include <string>
#include <string_view>

struct Vec {
    // top left is 0, 0
    size_t x, y;
};

[[noreturn]] void die(std::string_view msg);

std::string hexString(const void* data, size_t size);
