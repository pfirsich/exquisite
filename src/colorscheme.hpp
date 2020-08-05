#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "util.hpp"

class ColorScheme {
public:
    struct Entry {
        std::string name;
        std::string color;
    };

    ColorScheme(std::initializer_list<std::pair<std::string, Color>> colors);

    const std::vector<Entry>& getColors() const;
    std::optional<size_t> getEntryId(std::string_view name) const;

    const std::string& getColor(size_t entryId) const;
    const std::string& getColor(std::string_view name) const;

    const std::string& operator[](size_t entryId) const;
    const std::string& operator[](std::string_view name) const;

private:
    std::vector<Entry> colors_;
};

extern ColorScheme colorScheme;
