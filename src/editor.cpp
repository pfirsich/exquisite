#include "editor.hpp"

#include <cassert>
#include <string_view>

using namespace std::literals;

#include "control.hpp"
#include "debug.hpp"
#include "terminal.hpp"

namespace detail {
std::optional<std::string_view> getControlString(char ch)
{
    assert(std::iscntrl(ch));
    switch (ch) {
    case 0:
        return "NUL"sv;
    case 1:
        return "SOH"sv;
    case 2:
        return "STX"sv;
    case 3:
        return "ETX"sv;
    case 4:
        return "EOT"sv;
    case 5:
        return "ENQ"sv;
    case 6:
        return "ACK"sv;
    case 7:
        return "BEL"sv;
    case 8:
        return "BS"sv;
    case 9:
        return "TAB"sv;
    case 10:
        return std::nullopt; // "LF"sv;
    case 11:
        return "VT"sv;
    case 12:
        return "FF"sv;
    case 13:
        return "CR"sv;
    case 14:
        return "SO"sv;
    case 15:
        return "SI"sv;
    case 16:
        return "DLE"sv;
    case 17:
        return "DC1"sv;
    case 18:
        return "DC2"sv;
    case 19:
        return "DC3"sv;
    case 20:
        return "DC4"sv;
    case 21:
        return "NAK"sv;
    case 22:
        return "SYN"sv;
    case 23:
        return "ETB"sv;
    case 24:
        return "CAN"sv;
    case 25:
        return "EM"sv;
    case 26:
        return "SUB"sv;
    case 27:
        return "ESC"sv;
    case 28:
        return "FS"sv;
    case 29:
        return "GS"sv;
    case 30:
        return "RS"sv;
    case 31:
        return "US"sv;
    case 127:
        return "DEL"sv;
    default:
        assert(false && "Unhandled control character");
    }
}
}

namespace editor {
Buffer buffer;

Vec drawBuffer(const Buffer& buf)
{
    terminal::bufferWrite(control::sgr::reset);

    const auto lineCount = buf.text.getLineCount();
    const auto size = terminal::getSize();
    assert(buf.scrollY <= lineCount - 1);
    const auto firstLine = buf.scrollY;
    const auto lastLine = firstLine + std::min(static_cast<size_t>(size.y), lineCount);

    Vec drawCursor = { 0, 0 };
    for (size_t l = firstLine; l < lastLine; ++l) {
        const auto line = buf.text.getLine(l);
        bool faint = false;

        if (buffer.cursorY == l) {
            drawCursor = terminal::getCursorPosition();
            drawCursor.x = line.length;
        }

        for (size_t i = line.offset; i < line.offset + line.length; ++i) {
            const auto ch = buf.text[i];

            // This is not a nice way to do it, but it's easy
            if (buffer.cursorY == l && buffer.cursorX == i - line.offset) {
                terminal::flushWrite();
                drawCursor = terminal::getCursorPosition();
            }

            const bool control = std::iscntrl(ch);
            if (control) {
                const auto str = detail::getControlString(ch);
                if (str) {
                    if (!faint) {
                        terminal::bufferWrite(control::sgr::faint);
                        faint = true;
                    }
                    terminal::bufferWrite(*str);
                }
            } else {
                if (faint) {
                    terminal::bufferWrite(control::sgr::resetIntensity);
                    faint = false;
                }
                terminal::bufferWrite(std::string_view(&ch, 1));
            }
        }
        if (faint) {
            terminal::bufferWrite(control::sgr::resetIntensity);
            faint = false;
        }
        terminal::bufferWrite(control::clearLine);
        if (l - firstLine < size.y - 1)
            terminal::bufferWrite("\r\n");
    }
    for (size_t y = lastLine + 1; y < size.y; ++y) {
        terminal::bufferWrite("~");
        terminal::bufferWrite(control::clearLine);
        if (y < size.y - 1)
            terminal::bufferWrite("\r\n");
    }

    return drawCursor;
}

void redraw()
{
    terminal::bufferWrite(control::hideCursor);
    terminal::bufferWrite(control::resetCursor);

    const auto drawCursor = drawBuffer(buffer);

    terminal::bufferWrite(control::moveCursor(drawCursor));
    terminal::bufferWrite(control::showCursor);

    terminal::flushWrite();
}
}
