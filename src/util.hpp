#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "config.hpp"

namespace fs = std::filesystem;

// I don't want to type this every time I use it (quite a bit)
inline constexpr size_t MaxSizeT = std::numeric_limits<size_t>::max();

struct Vec {
    // for terminal cells 0, 0 is top left
    size_t x = 0;
    size_t y = 0;

    bool operator==(const Vec& other) const
    {
        return x == other.x && y == other.y;
    }
};

struct Range {
    size_t offset = 0;
    size_t length = 0;

    bool contains(size_t index) const
    {
        return index >= offset && index - offset < length;
    }

    auto end() const
    {
        return offset + length;
    }
};

struct RgbColor {
    uint8_t r, g, b;
};
using ColorIndex = uint8_t;
using Color = std::variant<ColorIndex, RgbColor>;

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

size_t countNewlines(std::string_view str);
bool hasNewlines(std::string_view str);

size_t getNextLineOffset(std::string_view text, size_t offset);

// Returns the width in bytes and in characters (tabs align to tab stops)
std::pair<size_t, size_t> getIndentWidth(std::string_view line, size_t tabWidth);

Indentation detectIndentation(std::string_view text);

std::optional<int> toInt(const std::string& str, int base = 10);

std::string trimTrailingWhitespace(std::string_view str);

std::optional<std::vector<std::string>> walkDirectory(
    const fs::path& dirPath, size_t maxDepth = 5, size_t maxItems = 2000);
