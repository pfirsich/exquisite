#pragma once

#include <string>
#include <string_view>

using namespace std::literals;

#include "colors.hpp"
#include "util.hpp"

namespace control {
inline constexpr auto clear = "\x1b[2J"; // clear whole screen
inline constexpr auto clearLine = "\x1b[K"sv;

inline constexpr auto hideCursor = "\x1b[?25l"sv;
inline constexpr auto showCursor = "\x1b[?25h"sv;
inline constexpr auto resetCursor = "\x1b[H"sv; // set cursor to 0, 0
std::string moveCursorForward(size_t num);
std::string moveCursor(const Vec& position);

// SGR
namespace sgr {
    inline constexpr auto reset = "\x1b[0m"sv;
    inline constexpr auto bold = "\x1b[1m"sv;
    inline constexpr auto faint = "\x1b[2m"sv;
    inline constexpr auto resetIntensity = "\x1b[22m"sv; // reset bold/faint
    inline constexpr auto italic = "\x1b[3m"sv; // not widely supported
    inline constexpr auto resetItalic = "\x1b[23m"sv;
    inline constexpr auto underline = "\x1b[4m"sv;
    inline constexpr auto resetUnderline = "\x1b[24m"sv;
    inline constexpr auto slowBlink = "\x1b[5m"sv;
    inline constexpr auto fastBlink = "\x1b[6m"sv;
    inline constexpr auto resetBlink = "\x1b[25m"sv;
    inline constexpr auto invert = "\x1b[7m"sv;
    inline constexpr auto resetInvert = "\x1b[27m"sv;
    inline constexpr auto crossedOut = "\x1b[9m"sv;
    inline constexpr auto resetCrossedOut = "\x1b[29m"sv;

    std::string fgColor(ColorIndex color);
    std::string fgColor(const RgbColor& color);
    std::string fgColor(const Color& color);
    inline constexpr auto resetFgColor = "\x1b[39m"sv;

    std::string bgColor(ColorIndex color);
    std::string bgColor(const RgbColor& color);
    std::string bgColor(const Color& color);
    inline constexpr auto resetBgColor = "\x1b[49m"sv;
}
}
