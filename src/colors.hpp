#pragma once

#include <cstdint>
#include <variant>

struct RgbColor {
    uint8_t r, g, b;
};
using ColorIndex = uint8_t;
using Color = std::variant<ColorIndex, RgbColor>;

struct ColorScheme {
    Color statusErrorColor = 1;
    Color highlightLine = 238;
    Color matchHighlightColor = 243;
};

extern ColorScheme colorScheme;
