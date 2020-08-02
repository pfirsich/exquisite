#include "control.hpp"

#include <cassert>

namespace control {
std::string moveCursorForward(size_t num)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "\x1b[%dC", static_cast<int>(num));
    return std::string(&buf[0]);
}

std::string moveCursor(const Vec& pos)
{
    char buf[32];
    snprintf(
        buf, sizeof(buf), "\x1b[%d;%dH", static_cast<int>(pos.y + 1), static_cast<int>(pos.x + 1));
    return std::string(&buf[0]);
}

namespace sgr {
    std::string fgColor(ColorIndex color)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[38;5;%um", color);
        return std::string(&buf[0]);
    }

    std::string fgColor(const RgbColor& color)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[38;2;%u;%u;%um", color.r, color.g, color.b);
        return std::string(&buf[0]);
    }

    std::string fgColor(const Color& color)
    {
        if (const auto idx = std::get_if<ColorIndex>(&color))
            return fgColor(*idx);
        else if (const auto rgb = std::get_if<RgbColor>(&color))
            return fgColor(*rgb);
        assert(false && "Invalid variant state");
    }

    std::string bgColor(ColorIndex color)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[48;5;%um", color);
        return std::string(&buf[0]);
    }

    std::string bgColor(const RgbColor& color)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[48;2;%u;%u;%um", color.r, color.g, color.b);
        return std::string(&buf[0]);
    }

    std::string bgColor(const Color& color)
    {
        if (const auto idx = std::get_if<ColorIndex>(&color))
            return bgColor(*idx);
        else if (const auto rgb = std::get_if<RgbColor>(&color))
            return bgColor(*rgb);
        assert(false && "Invalid variant state");
    }
}
}
