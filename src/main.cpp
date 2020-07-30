#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>

#include "debug.hpp"
#include "editor.hpp"
#include "terminal.hpp"
#include "util.hpp"

// char code for ctrl + letter
constexpr char ctrl(char c)
{
    return c & 0b00011111;
}

bool processInput(const Key& key)
{
    /*for (const auto c : key.bytes)
        printf("%X", static_cast<uint8_t>(c));
    printf(" ");
    if (key.ctrl)
        printf("ctrl+");
    if (key.alt)
        printf("alt+");
    */

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        // printf("%s\r\n", toString(*special).c_str());

        const auto size = terminal::getSize();
        switch (*special) {
        case SpecialKey::Left:
            if (editor::cursor.x > 0)
                editor::cursor.x--;
            debug("Cursor: ", editor::cursor.x, ", ", editor::cursor.y);
            break;
        case SpecialKey::Right:
            editor::cursor.x = std::min(size.x - 1, editor::cursor.x + 1);
            debug("Cursor: ", editor::cursor.x, ", ", editor::cursor.y);
            break;
        case SpecialKey::Up:
            if (editor::cursor.y > 0)
                editor::cursor.y--;
            debug("Cursor: ", editor::cursor.x, ", ", editor::cursor.y);
            break;
        case SpecialKey::Down:
            editor::cursor.y = std::min(size.y - 1, editor::cursor.y + 1);
            debug("Cursor: ", editor::cursor.x, ", ", editor::cursor.y);
            break;
        case SpecialKey::PageUp:
            editor::cursor.y = std::max(0ul, editor::cursor.y - size.y);
            break;
        case SpecialKey::PageDown:
            editor::cursor.y = std::min(size.y - 1, editor::cursor.y + size.y);
            break;
        case SpecialKey::Home:
            editor::cursor.x = 0;
            break;
        case SpecialKey::End:
            editor::cursor.x = size.x - 1;
            break;
        default:
            break;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        // printf("%.*s\r\n", static_cast<int>(seq->length), seq->bytes);

        if (key.ctrl && *seq == 'q') {
            terminal::write(control::clear());
            terminal::write(control::resetCursor());
            printf("Quit.\r\n");
            return false;
        }
    } else {
        die("Invalid Key variant");
    }

    return true;
}

int main()
{
    terminal::init();

    editor::buffer.set("Hello World\nIt is me..\n\nJoel!");
    for (size_t l = 0; l < editor::buffer.getLineCount(); ++l) {
        const auto line = editor::buffer.getLine(l);
        debug("line ", l, ": offset = ", line.offset, ", length = ", line.length);
    }

    while (true) {
        editor::redraw();

        const auto key = terminal::readKey();

        if (key) {
            if (!processInput(*key))
                break;
        }
    }

    return 0;
}
