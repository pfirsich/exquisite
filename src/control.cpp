#include "control.hpp"

#include <cassert>

#include <fmt/format.h>

namespace control {
std::string moveCursorForward(size_t num)
{
    return fmt::format("\x1b[{}C", num);
}

std::string moveCursor(const Vec& pos)
{
    return fmt::format("\x1b[{};{}H", pos.y + 1, pos.x + 1);
}

std::string setCursorStyle(int style)
{
    return fmt::format("\x1b[{} q", style);
}

namespace sgr {
    std::string color(ColorIndex idx)
    {
        return fmt::format("5;{}m", idx);
    }

    std::string color(const RgbColor& rgb)
    {
        return fmt::format("2;{};{};{}m", rgb.r, rgb.g, rgb.b);
    }

    std::string color(const Color& col)
    {
        if (const auto idx = std::get_if<ColorIndex>(&col))
            return color(*idx);
        else if (const auto rgb = std::get_if<RgbColor>(&col))
            return color(*rgb);
        die("Invalid variant state");
    }

    std::string fgColor(ColorIndex idx)
    {
        return fmt::format("\x1b[38;5;{}m", idx);
    }

    std::string fgColor(const RgbColor& rgb)
    {
        return fmt::format("\x1b[38;2;{};{};{}m", rgb.r, rgb.b, rgb.b);
    }

    std::string fgColor(const Color& color)
    {
        if (const auto idx = std::get_if<ColorIndex>(&color))
            return fgColor(*idx);
        else if (const auto rgb = std::get_if<RgbColor>(&color))
            return fgColor(*rgb);
        die("Invalid variant state");
    }

    std::string bgColor(ColorIndex idx)
    {
        return fmt::format("\x1b[48;5;{}m", idx);
    }

    std::string bgColor(const RgbColor& rgb)
    {
        return fmt::format("\x1b[48;2;{};{};{}m", rgb.r, rgb.g, rgb.b);
    }

    std::string bgColor(const Color& color)
    {
        if (const auto idx = std::get_if<ColorIndex>(&color))
            return bgColor(*idx);
        else if (const auto rgb = std::get_if<RgbColor>(&color))
            return bgColor(*rgb);
        die("Invalid variant state");
    }
}
}
