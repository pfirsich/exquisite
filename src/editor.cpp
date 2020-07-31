#include "editor.hpp"

#include <cassert>
#include <string_view>

using namespace std::literals;

#include "config.hpp"
#include "control.hpp"
#include "debug.hpp"
#include "terminal.hpp"

namespace editor {
namespace {
    std::string_view getControlString(char ch)
    {
        assert(std::iscntrl(ch));
        static constexpr std::array<std::string_view, 32> lut { "NUL"sv, "SOH"sv, "STX"sv, "ETX"sv,
            "EOT"sv, "ENQ"sv, "ACK"sv, "BEL"sv, "BS"sv, "TAB"sv, ""sv /*LF*/, "VT"sv, "FF"sv,
            "CR"sv, "SO"sv, "SI"sv, "DLE"sv, "DC1"sv, "DC2"sv, "DC3"sv, "DC4"sv, "NAK"sv, "SYN"sv,
            "ETB"sv, "CAN"sv, "EM"sv, "SUB"sv, "ESC"sv, "FS"sv, "GS"sv, "RS"sv, "US"sv };
        if (ch > 0 && ch < 32)
            return lut[ch];
        if (ch == 127)
            return "DEL"sv;
        assert(false && "Unhandled control character");
    }

    StatusMessage statusMessage;
}

Buffer buffer;
std::unique_ptr<Prompt> currentPrompt;

Vec drawBuffer(const Buffer& buf, const Vec& size)
{
    terminal::bufferWrite(control::sgr::reset);

    const auto lineCount = buf.text.getLineCount();
    assert(buf.scrollY <= lineCount - 1);
    const auto firstLine = buf.scrollY;
    const auto lastLine = firstLine + std::min(static_cast<size_t>(size.y), lineCount);
    terminal::flushWrite();
    const auto pos = terminal::getCursorPosition();
    auto drawCursor = Vec { pos.x, pos.y + buf.cursorY - buf.scrollY };

    bool faint = false;
    auto setFaint = [&faint](bool f) {
        if (f && !faint)
            terminal::bufferWrite(control::sgr::faint);
        if (!f && faint)
            terminal::bufferWrite(control::sgr::resetIntensity);
        faint = f;
    };

    for (size_t l = firstLine; l < lastLine; ++l) {
        const auto line = buf.text.getLine(l);

        if (l > firstLine && pos.x > 0)
            terminal::bufferWrite(control::moveCursorForward(pos.x));

        for (size_t i = line.offset; i < line.offset + line.length; ++i) {
            const auto ch = buf.text[i];

            // Move cursor if the line is correct and we are drawing characters
            // in front of the cursor
            const bool moveCursor = l == buf.cursorY && i < line.offset + buffer.getCursorX();

            if (ch == '\n') {
                if (config.renderWhitespace && config.newlineChar) {
                    setFaint(true);
                    terminal::bufferWrite(*config.newlineChar);
                }
                // do nothing otherwise (don't increase drawCursor)
            } else if (ch == ' ' && config.renderWhitespace && config.spaceChar) {
                // If whitespace is not rendered, this will fall into "else"
                setFaint(true);
                terminal::bufferWrite(*config.spaceChar);

                if (moveCursor)
                    drawCursor.x++;
            } else if (ch == '\t') {
                const bool tabChars = config.tabStartChar || config.tabMidChar || config.tabEndChar;
                assert(config.tabWidth > 0);
                if (config.renderWhitespace && tabChars) {
                    if (config.tabWidth >= 2)
                        terminal::bufferWrite(*config.tabStartChar);
                    for (size_t i = 0; i < config.tabWidth - 2; ++i)
                        terminal::bufferWrite(*config.tabMidChar);
                    terminal::bufferWrite(*config.tabEndChar);
                } else {
                    for (size_t i = 0; i < config.tabWidth; ++i)
                        terminal::bufferWrite(' ');
                }

                if (moveCursor)
                    drawCursor.x += config.tabWidth;
            } else if (std::iscntrl(ch)) {
                setFaint(true);
                const auto str = getControlString(ch);
                terminal::bufferWrite(str);

                if (moveCursor)
                    drawCursor.x += str.size();
            } else {
                setFaint(false);
                terminal::bufferWrite(ch);

                // Somewhat hacky, but we just don't count utf8 continuation bytes
                if (moveCursor && (ch & 0b11000000) != 0b10000000)
                    drawCursor.x++;
            }
        }

        setFaint(false);
        terminal::bufferWrite(control::clearLine);
        if (l - firstLine < size.y - 1)
            terminal::bufferWrite("\r\n");
    }

    for (size_t y = lastLine + 1; y < size.y; ++y) {
        if (pos.x > 0)
            terminal::bufferWrite(control::moveCursorForward(pos.x));
        terminal::bufferWrite("~");
        terminal::bufferWrite(control::clearLine);
        if (y - (lastLine + 1) < size.y - 1)
            terminal::bufferWrite("\r\n");
    }

    return drawCursor;
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
