#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

struct Vec {
    // for terminal cells 0, 0 is top left
    size_t x, y;

    bool operator==(const Vec& other) const
    {
        return x == other.x && y == other.y;
    }
};

struct Range {
    size_t offset;
    size_t length;

    bool contains(size_t index) const
    {
        return index >= offset && index - offset < length;
    }

    auto end() const
    {
        return offset + length;
    }
};

template <typename T>
T subClamp(T a, T b)
{
    if (a < b)
        return 0;
    return a - b;
}

[[noreturn]] void die(std::string_view msg);

std::string hexString(const void* data, size_t size);

std::unique_ptr<FILE, decltype(&fclose)> uniqueFopen(const char* path, const char* modes);

std::optional<std::string> readFile(const fs::path& path);

std::string readAll(int fd);
