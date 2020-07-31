#include "editor.hpp"

#include <cassert>
#include <string_view>

using namespace std::literals;

#include "control.hpp"
#include "debug.hpp"
#include "terminal.hpp"

namespace editor {
namespace {
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

    StatusMessage statusMessage;
}

Buffer buffer;
std::unique_ptr<Prompt> currentPrompt;

Vec getDrawCursor(const Buffer& buffer)
{
    Vec cur = { 0, buffer.cursorY - buffer.scrollY };
    std::vector<char> v;
    const auto line = buffer.getCurrentLine();
    for (size_t i = line.offset; i < line.offset + buffer.getCursorX(); ++i) {
        const auto ch = buffer.text[i];
        v.push_back(ch);
        if (std::iscntrl(ch)) {
            cur.x += getControlString(ch).size();
        } else {
            // Somewhat hacky, but we just don't count utf8 continuation bytes
            if ((ch & 0b11000000) != 0b10000000)
                cur.x += 1;
        }
    }
    debug("line before cursor (", buffer.cursorX, "): ", hexString(v.data(), v.size()));
    return cur;
}

Vec drawBuffer(const Buffer& buf, const Vec& size)
{
    terminal::bufferWrite(control::sgr::reset);

    const auto lineCount = buf.text.getLineCount();
    assert(buf.scrollY <= lineCount - 1);
    const auto firstLine = buf.scrollY;
    const auto lastLine = firstLine + std::min(static_cast<size_t>(size.y), lineCount);
    terminal::flushWrite();
    const auto pos = terminal::getCursorPosition();
    debug("pos: ", pos.x, ", ", pos.y);

    for (size_t l = firstLine; l < lastLine; ++l) {
        const auto line = buf.text.getLine(l);
        debug("draw line ", l, " with ", line.length, " bytes");
        bool faint = false;

        if (l > firstLine && pos.x > 0)
            terminal::bufferWrite(control::moveCursorForward(pos.x));

        for (size_t i = line.offset; i < line.offset + line.length; ++i) {
            const auto ch = buf.text[i];

            if (std::iscntrl(ch)) {
                if (!faint) {
                    terminal::bufferWrite(control::sgr::faint);
                    faint = true;
                }
                terminal::bufferWrite(getControlString(ch));
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
        else
            debug("don't draw newline for ", l);
    }

    for (size_t y = lastLine + 1; y < size.y; ++y) {
        if (pos.x > 0)
            terminal::bufferWrite(control::moveCursorForward(pos.x));
        terminal::bufferWrite("~");
        terminal::bufferWrite(control::clearLine);
        if (y - (lastLine + 1) < size.y - 1)
            terminal::bufferWrite("\r\n");
    }

    const auto drawCursor = getDrawCursor(buf);
    debug("drawCursor in drawBuffer: ", drawCursor.x, ", ", drawCursor.y);
    return Vec { drawCursor.x + pos.x, drawCursor.y + pos.y };
}

void drawStatusBar()
{
    const auto size = terminal::getSize();
    std::stringstream ss;
    ss << buffer.cursorY + 1 << "/" << buffer.text.getLineCount() << ' ';
    const auto lines = ss.str();

    std::string status;
    status.reserve(size.x);
    status.append(buffer.name);
    status.append(size.x - lines.size() - status.size(), ' ');
    status.append(lines);

    terminal::bufferWrite(control::sgr::invert);
    terminal::bufferWrite(status);
    terminal::bufferWrite(control::sgr::resetInvert);
    terminal::bufferWrite(control::clearLine);
    terminal::bufferWrite("\r\n");
}

void redraw()
{
    terminal::bufferWrite(control::hideCursor);
    terminal::bufferWrite(control::resetCursor);

    const auto size = terminal::getSize();
    auto drawCursor = drawBuffer(buffer, Vec { size.x, size.y - 2 });
    terminal::bufferWrite("\r\n");

    drawStatusBar();

    if (currentPrompt) {
        terminal::bufferWrite(currentPrompt->prompt);
        assert(currentPrompt->input.text.getLineCount() == 1);
        drawCursor = drawBuffer(currentPrompt->input, Vec { size.x, 1 });
        terminal::bufferWrite(control::clearLine);
    } else {
        switch (statusMessage.type) {
        case StatusMessage::Type::Normal:
            terminal::bufferWrite(control::sgr::reset);
            break;
        case StatusMessage::Type::Error:
            terminal::bufferWrite(control::sgr::fgColor(1));
            break;
        }
        terminal::bufferWrite(statusMessage.message);
        terminal::bufferWrite(control::clearLine);
    }

    debug("draw cursor: ", drawCursor.x, ", ", drawCursor.y);
    terminal::bufferWrite(control::moveCursor(drawCursor));
    terminal::bufferWrite(control::showCursor);
    terminal::flushWrite();
}

void confirmPrompt()
{
    // Move from currentPrompt first, so callback can set another prompt
    const auto prompt = std::move(currentPrompt);
    const auto msg = prompt->callback(prompt->input.text.getString());
    setStatusMessage(msg.message, msg.type);
}

void setStatusMessage(const std::string& message, StatusMessage::Type type)
{
    statusMessage = StatusMessage { message, type };
}

StatusMessage getStatusMessage()
{
    return statusMessage;
}
}
