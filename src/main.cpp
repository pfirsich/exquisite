#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
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
            editor::buffer.moveCursorX(-1);
            editor::buffer.scroll(size.y);
            break;
        case SpecialKey::Right:
            editor::buffer.moveCursorX(1);
            editor::buffer.scroll(size.y);
            break;
        case SpecialKey::Up:
            editor::buffer.moveCursorY(-1);
            editor::buffer.scroll(size.y);
            break;
        case SpecialKey::Down:
            editor::buffer.moveCursorY(1);
            editor::buffer.scroll(size.y);
            break;
        case SpecialKey::PageUp:
            editor::buffer.moveCursorY(-size.y - 1);
            editor::buffer.scroll(size.y);
            break;
        case SpecialKey::PageDown:
            editor::buffer.moveCursorY(size.y - 1);
            editor::buffer.scroll(size.y);
            break;
        case SpecialKey::Home:
            editor::buffer.cursorX = 0;
            editor::buffer.scroll(size.y);
            break;
        case SpecialKey::End:
            editor::buffer.cursorX = Buffer::EndOfLine;
            editor::buffer.scroll(size.y);
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

void readFile(const std::string& path)
{
    auto f = std::unique_ptr<FILE, decltype(&fclose)>(fopen(path.c_str(), "rb"), &fclose);
    fseek(f.get(), 0, SEEK_END);
    const auto size = ftell(f.get());
    fseek(f.get(), 0, SEEK_SET);
    std::vector<char> buf(size, '\0');
    fread(buf.data(), 1, size, f.get());
    const auto sv = std::string_view(buf.data(), buf.size());
    editor::buffer.text.set(sv);
    debug("Read from file:\n", sv);
}

void readStdin()
{
    std::vector<char> buf;
    char c;
    while (read(STDIN_FILENO, &c, 1) > 0) {
        buf.push_back(c);
    }
    const auto sv = std::string_view(buf.data(), buf.size());
    editor::buffer.text.set(sv);
    debug("Read from tty:\n", sv);
}

void printUsage()
{
    printf("Usage: `exquisite <file>` or `<cmd> | exquisite`\n");
}

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() > 0) {
        readFile(args[0]);
    } else if (!isatty(STDIN_FILENO)) {
        readStdin();
        // const int fd = dup(STDIN_FILENO);
        const int tty = open("/dev/tty", O_RDONLY);
        dup2(tty, STDIN_FILENO);
        close(tty);
    } else {
        printUsage();
        exit(1);
    }

    terminal::init();

    for (size_t l = 0; l < editor::buffer.text.getLineCount(); ++l) {
        const auto line = editor::buffer.text.getLine(l);
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
