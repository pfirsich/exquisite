#include "editor.hpp"

#include <cassert>
#include <string_view>

using namespace std::literals;

#include "control.hpp"
#include "debug.hpp"
#include "terminal.hpp"

namespace detail {
std::string_view getControlString(char ch)
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
        return ""; // "LF"sv;
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

void drawBuffer(const Buffer& buf)
{
    terminal::bufferWrite(control::sgr::reset);

    const auto lineCount = buf.text.getLineCount();
    const auto size = terminal::getSize();
    assert(buf.scrollY <= lineCount - 1);
    const auto firstLine = buf.scrollY;
    const auto lastLine = firstLine + std::min(static_cast<size_t>(size.y), lineCount);

    for (size_t l = firstLine; l < lastLine; ++l) {
        const auto line = buf.text.getLine(l);
        bool faint = false;

        for (size_t i = line.offset; i < line.offset + line.length; ++i) {
            const auto ch = buf.text[i];

            if (std::iscntrl(ch)) {
                if (!faint) {
                    terminal::bufferWrite(control::sgr::faint);
                    faint = true;
                }
                terminal::bufferWrite(detail::getControlString(ch));
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
}

Vec getDrawCursor(const Buffer& buffer)
{
    Vec cur = { 0, buffer.cursorY - buffer.scrollY };
    std::vector<char> v;
    const auto line = buffer.getCurrentLine();
    for (size_t i = line.offset; i < line.offset + buffer.getCursorX(); ++i) {
        const auto ch = buffer.text[i];
        v.push_back(ch);
        if (std::iscntrl(ch)) {
            cur.x += detail::getControlString(ch).size();
        } else {
            // Somewhat hacky, but we just don't count utf8 continuation bytes
            if ((ch & 0b11000000) != 0b10000000)
                cur.x += 1;
        }
    }
    debug("line before cursor (", buffer.cursorX, "): ", hexString(v.data(), v.size()));
    return cur;
}

void redraw()
{
    terminal::bufferWrite(control::hideCursor);
    terminal::bufferWrite(control::resetCursor);

    drawBuffer(buffer);

    const auto drawCursor = getDrawCursor(buffer);
    debug("draw cursor: ", drawCursor.x, ", ", drawCursor.y);
    terminal::bufferWrite(control::moveCursor(drawCursor));
    terminal::bufferWrite(control::showCursor);

    terminal::flushWrite();
}
}
