#include "editor.hpp"

#include "terminal.hpp"

namespace editor {
Coord cursor { 0, 0 };
Buffer logBuffer;
Buffer buffer;

void redraw()
{
    terminal::bufferWrite(control::hideCursor());
    terminal::bufferWrite(control::resetCursor());

    const auto size = terminal::getSize();
    const auto firstLine = 0;
    const auto lastLine = std::min(static_cast<size_t>(size.y), buffer.getLineCount());
    for (size_t l = firstLine; l < lastLine; ++l) {
        const auto line = buffer.getLine(l);
        for (size_t i = line.offset; i < line.offset + line.length; ++i) {
            const auto ch = buffer[i];
            if (!std::iscntrl(ch))
                terminal::bufferWrite(std::string_view(&ch, 1));
        }
        if (l < size.y - 1)
            terminal::bufferWrite("\r\n");
    }
    for (size_t y = lastLine + 1; y < size.y; ++y) {
        terminal::bufferWrite("~");
        terminal::bufferWrite(control::clearLine());
        if (y < size.y - 1)
            terminal::bufferWrite("\r\n");
    }

    terminal::bufferWrite(control::moveCursor(cursor));
    terminal::bufferWrite(control::showCursor());

    terminal::flushWrite();
}
}
