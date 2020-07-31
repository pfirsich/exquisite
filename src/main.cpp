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

bool processBufferInput(Buffer& buffer, const Key& key)
{
    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        // debug("special: ", toString(*special));
        const auto size = terminal::getSize();
        switch (*special) {
        case SpecialKey::Left:
            buffer.moveCursorLeft();
            buffer.scroll(size.y);
            return true;
        case SpecialKey::Right:
            buffer.moveCursorRight();
            buffer.scroll(size.y);
            return true;
        case SpecialKey::Up:
            buffer.moveCursorY(-1);
            buffer.scroll(size.y);
            return true;
        case SpecialKey::Down:
            buffer.moveCursorY(1);
            buffer.scroll(size.y);
            return true;
        case SpecialKey::PageUp:
            buffer.moveCursorY(-size.y - 1);
            buffer.scroll(size.y);
            return true;
        case SpecialKey::PageDown:
            buffer.moveCursorY(size.y - 1);
            buffer.scroll(size.y);
            return true;
        case SpecialKey::Home:
            buffer.cursorX = 0;
            buffer.scroll(size.y);
            return true;
        case SpecialKey::End:
            buffer.cursorX = Buffer::EndOfLine;
            buffer.scroll(size.y);
            return true;
        case SpecialKey::Backspace:
            buffer.deleteBackwards(1);
            return true;
        case SpecialKey::Delete:
            buffer.deleteForwards(1);
            return true;
        default:
            return false;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        // debug("utf8seq (", seq->length, "): ", std::string_view(&seq->bytes[0], seq->length));
        if (!key.ctrl) {
            buffer.insert(std::string_view(&seq->bytes[0], seq->length));
            return true;
        }
    } else {
        die("Invalid Key variant");
    }
    return false;
}

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

editor::StatusMessage insertTextCallback(std::string_view input)
{
    if (input.empty())
        return editor::StatusMessage { "Empty", editor::StatusMessage::Type::Error };

    editor::buffer.insert(input);
    return editor::StatusMessage { "" };
}

void processInput(const Key& key)
{
    if (processBufferInput(editor::buffer, key))
        return;

    debug("key hex: ", hexString(key.bytes.data(), key.bytes.size()));
    debug("ctrl: ", key.ctrl, ", alt: ", key.alt);

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        // debug("special: ", toString(*special));
        switch (*special) {
        case SpecialKey::Return:
            editor::buffer.insert("\n" + getCurrentIndent());
            break;
        default:
            break;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        debug("utf8seq (", seq->length, "): ", std::string_view(&seq->bytes[0], seq->length));
        if (key.ctrl) {
            if (*seq == 'q') {
                terminal::write(control::clear);
                terminal::write(control::resetCursor);
                exit(0);
            }

            if (*seq == 'p') {
                debug("set prompt");
                editor::currentPrompt = std::make_unique<editor::Prompt>(
                    editor::Prompt { "Insert Text> ", insertTextCallback });
            }
        }
    } else {
        die("Invalid Key variant");
    }
}

void processPromptInput(const Key& key)
{
    // debug("key hex: ", hexString(key.bytes.data(), key.bytes.size()));
    if (processBufferInput(editor::currentPrompt->input, key))
        return;

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        // debug("special: ", toString(*special));
        switch (*special) {
        case SpecialKey::Return:
            editor::confirmPrompt();
            break;
        case SpecialKey::Escape:
            editor::currentPrompt.reset();
            break;
        default:
            break;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
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
        editor::setStatusMessage(args[0]);
    } else if (!isatty(STDIN_FILENO)) {
        readStdin();
        editor::setStatusMessage("Read STDIN");
        // const int fd = dup(STDIN_FILENO);
        const int tty = open("/dev/tty", O_RDONLY);
        dup2(tty, STDIN_FILENO);
        close(tty);
    } else {
        printUsage();
        exit(1);
    }

    terminal::init();

    editor::redraw();

    while (true) {
        const auto key = terminal::readKey();
        if (key) {
            if (editor::currentPrompt)
                processPromptInput(*key);
            else
                processInput(*key);
        }

        editor::redraw();
    }

    return 0;
}
