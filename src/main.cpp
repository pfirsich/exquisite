#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "control.hpp"
#include "debug.hpp"
#include "editor.hpp"
#include "terminal.hpp"
#include "util.hpp"

std::string getCurrentIndent()
{
    std::string indent;
    indent.reserve(32);
    const auto line = editor::buffer.getCurrentLine();
    for (size_t i = line.offset; i < line.offset + line.length; ++i) {
        const auto ch = editor::buffer.text[i];
        if (ch == ' ' || ch == '\t')
            indent.push_back(ch);
        else
            break;
    }
    return indent;
}

void processInput(const Key& key)
{
    // debug("key hex: ", hexString(key.bytes.data(), key.bytes.size()));

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        // debug("special: ", toString(*special));
        const auto size = terminal::getSize();
        switch (*special) {
        case SpecialKey::Left:
            editor::buffer.moveCursorLeft();
            editor::buffer.scroll(size.y);
            break;
        case SpecialKey::Right:
            editor::buffer.moveCursorRight();
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
        case SpecialKey::Return:
            editor::buffer.insert("\n" + getCurrentIndent());
            break;
        case SpecialKey::Backspace:
            editor::buffer.deleteBackwards(1);
            break;
        case SpecialKey::Delete:
            editor::buffer.deleteForwards(1);
            break;
        default:
            break;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        // debug("utf8seq (", seq->length, "): ", std::string_view(&seq->bytes[0], seq->length));
        if (key.ctrl) {
            if (*seq == 'q') {
                terminal::write(control::clear);
                terminal::write(control::resetCursor);
                exit(0);
            }
        } else {
            editor::buffer.insert(std::string_view(&seq->bytes[0], seq->length));
        }
    } else {
        die("Invalid Key variant");
    }
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

    editor::redraw();

    while (true) {
        const auto key = terminal::readKey();
        if (key) {
            processInput(*key);
        }

        editor::redraw();
    }

    return 0;
}
