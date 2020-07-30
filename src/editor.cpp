#include "editor.hpp"

#include <cassert>

#include "debug.hpp"
#include "terminal.hpp"

namespace editor {
Buffer buffer;

void drawBuffer(const Buffer& buf)
{
    const auto lineCount = buf.text.getLineCount();
    const auto size = terminal::getSize();
    assert(buf.scrollY <= lineCount - 1);
    const auto firstLine = buf.scrollY;
    const auto lastLine = firstLine + std::min(static_cast<size_t>(size.y), lineCount);

    std::string dbg;
    const auto fl = buf.text.getLine(firstLine);
    dbg.reserve(fl.length);
    for (size_t i = fl.offset; i < fl.offset + fl.length; ++i) {
        const auto ch = buffer.text[i];
        dbg.append(&ch, 1);
    }
    debug("firstLine: ", dbg);

    for (size_t l = firstLine; l < lastLine; ++l) {
        const auto line = buf.text.getLine(l);
        for (size_t i = line.offset; i < line.offset + line.length; ++i) {
            const auto ch = buf.text[i];
            if (!std::iscntrl(ch))
                terminal::bufferWrite(std::string_view(&ch, 1));
        }
        terminal::bufferWrite(control::clearLine());
        if (l - firstLine < size.y - 1)
            terminal::bufferWrite("\r\n");
    }
    for (size_t y = lastLine + 1; y < size.y; ++y) {
        terminal::bufferWrite("~");
        terminal::bufferWrite(control::clearLine());
        if (y < size.y - 1)
            terminal::bufferWrite("\r\n");
    }
}

void redraw()
{
    terminal::bufferWrite(control::hideCursor());
    terminal::bufferWrite(control::resetCursor());

    drawBuffer(buffer);

    const auto lineLength = buffer.text.getLine(buffer.cursorY).length;
    const auto cursorX = std::min(lineLength - 1, buffer.cursorX);
    const auto cursorY = buffer.cursorY - buffer.scrollY;
    terminal::bufferWrite(control::moveCursor(Coord { cursorX, cursorY }));
    terminal::bufferWrite(control::showCursor());

    terminal::flushWrite();
}
}
