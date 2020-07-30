#include "control.hpp"

namespace control {
std::string moveCursor(const Vec& pos)
{
    char buf[32];
    snprintf(
        buf, sizeof(buf), "\x1b[%d;%dH", static_cast<int>(pos.y + 1), static_cast<int>(pos.x + 1));
    return std::string(&buf[0]);
}

namespace sgr {
    std::string fgColor(uint8_t color)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[38;5;%um", color);
        return std::string(&buf[0]);
    }

    std::string fgColor(uint8_t r, uint8_t g, uint8_t b)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[38;2;%u;%u;%um", r, g, b);
        return std::string(&buf[0]);
    }

    std::string bgColor(uint8_t color)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "\x1b[48;5;%um", color);
        return std::string(&buf[0]);
    }

    std::string bgColor(uint8_t r, uint8_t g, uint8_t b)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[48;2;%u;%u;%um", r, g, b);
        return std::string(&buf[0]);
    }
}
}
