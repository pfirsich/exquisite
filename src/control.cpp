#include "control.hpp"

#include <cassert>

namespace control {
std::string moveCursorForward(size_t num)
{
    char buf[16];
    const auto n = snprintf(buf, sizeof(buf), "\x1b[%dC", static_cast<int>(num));
    assert(n > 0 && static_cast<size_t>(n) < sizeof(buf));
    return std::string(&buf[0], n);
}

std::string moveCursor(const Vec& pos)
{
    char buf[32];
    const auto n = snprintf(
        buf, sizeof(buf), "\x1b[%d;%dH", static_cast<int>(pos.y + 1), static_cast<int>(pos.x + 1));
    assert(n > 0 && static_cast<size_t>(n) < sizeof(buf));
    return std::string(&buf[0], n);
}

namespace sgr {
    std::string color(ColorIndex idx)
    {
        char buf[16];
        const auto n = snprintf(buf, sizeof(buf), "5;%um", idx);
        assert(n > 0 && static_cast<size_t>(n) < sizeof(buf));
        return std::string(&buf[0], n);
    }

    std::string color(const RgbColor& rgb)
    {
        char buf[16];
        const auto n = snprintf(buf, sizeof(buf), "2;%u;%u;%um", rgb.r, rgb.g, rgb.b);
        assert(n > 0 && static_cast<size_t>(n) < sizeof(buf));
        return std::string(&buf[0], n);
    }

    std::string color(const Color& col)
    {
        if (const auto idx = std::get_if<ColorIndex>(&col))
            return color(*idx);
        else if (const auto rgb = std::get_if<RgbColor>(&col))
            return color(*rgb);
        assert(false && "Invalid variant state");
    }

    std::string fgColor(ColorIndex idx)
    {
        char buf[16];
        const auto n = snprintf(buf, sizeof(buf), "\x1b[38;5;%um", idx);
        assert(n > 0 && static_cast<size_t>(n) < sizeof(buf));
        return std::string(&buf[0], n);
    }

    std::string fgColor(const RgbColor& rgb)
    {
        char buf[32];
        const auto n = snprintf(buf, sizeof(buf), "\x1b[38;2;%u;%u;%um", rgb.r, rgb.g, rgb.b);
        assert(n > 0 && static_cast<size_t>(n) < sizeof(buf));
        return std::string(&buf[0], n);
    }

    std::string fgColor(const Color& color)
    {
        if (const auto idx = std::get_if<ColorIndex>(&color))
            return fgColor(*idx);
        else if (const auto rgb = std::get_if<RgbColor>(&color))
            return fgColor(*rgb);
        assert(false && "Invalid variant state");
    }

    std::string bgColor(ColorIndex idx)
    {
        char buf[16];
        const auto n = snprintf(buf, sizeof(buf), "\x1b[48;5;%um", idx);
        assert(n > 0 && static_cast<size_t>(n) < sizeof(buf));
        return std::string(&buf[0], n);
    }

    std::string bgColor(const RgbColor& rgb)
    {
        char buf[32];
        const auto n = snprintf(buf, sizeof(buf), "\x1b[48;2;%u;%u;%um", rgb.r, rgb.g, rgb.b);
        assert(n > 0 && static_cast<size_t>(n) < sizeof(buf));
        return std::string(&buf[0], n);
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
