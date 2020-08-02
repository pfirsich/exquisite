#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "commands.hpp"
#include "debug.hpp"
#include "editor.hpp"
#include "terminal.hpp"
#include "util.hpp"

bool processBufferInput(Buffer& buffer, const Key& key)
{
    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        // debug("special: ", toString(*special));
        const auto size = terminal::getSize();
        const bool select = key.modifiers.test(Modifiers::Shift);
        switch (*special) {
        case SpecialKey::Left:
            buffer.moveCursorLeft(select);
            return true;
        case SpecialKey::Right:
            buffer.moveCursorRight(select);
            return true;
        case SpecialKey::PageUp:
            buffer.moveCursorY(-size.y - 1, select);
            return true;
        case SpecialKey::PageDown:
            buffer.moveCursorY(size.y - 1, select);
            return true;
        case SpecialKey::Home:
            buffer.moveCursorHome(select);
            return true;
        case SpecialKey::End:
            buffer.moveCursorEnd(select);
            return true;
        case SpecialKey::Backspace:
            buffer.deleteBackwards();
            return true;
        case SpecialKey::Delete:
            buffer.deleteForwards();
            return true;
        default:
            return false;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        // debug("utf8seq (", seq->length, "): ", std::string_view(&seq->bytes[0], seq->length));
        if (!key.modifiers.test(Modifiers::Ctrl)) {
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
    assert(editor::buffer.getCursor().emptySelection());
    const auto line = editor::buffer.getText().getLine(editor::buffer.getCursor().start.y);
    for (size_t i = line.offset; i < line.offset + line.length; ++i) {
        const auto ch = editor::buffer.getText()[i];
        if (ch == ' ' || ch == '\t')
            indent.push_back(ch);
        else
            break;
    }
    return indent;
}

void debugKey(const Key& key)
{
    debug("key hex (", key.bytes.size(), "): ", hexString(key.bytes.data(), key.bytes.size()));
    if (key.bytes[0] == '\x1b') {
        debug("escape: ", std::string_view(key.bytes.data() + 1, key.bytes.size() - 1));
    }
    debug("ctrl: ", key.modifiers.test(Modifiers::Ctrl),
        ", alt: ", key.modifiers.test(Modifiers::Alt),
        ", shift: ", key.modifiers.test(Modifiers::Shift));

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        debug("special: ", toString(*special));
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        debug("utf8seq (", seq->length, "): ", std::string_view(&seq->bytes[0], seq->length));
    }
}

void processInput(const Key& key)
{
    if (processBufferInput(editor::buffer, key))
        return;

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        const bool select = key.modifiers.test(Modifiers::Shift);
        switch (*special) {
        case SpecialKey::Up:
            editor::buffer.moveCursorY(-1, select);
            break;
        case SpecialKey::Down:
            editor::buffer.moveCursorY(1, select);
            break;
        case SpecialKey::Return:
            editor::buffer.insert("\n" + getCurrentIndent());
            break;
        default:
            break;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        if (key.modifiers.test(Modifiers::Ctrl)) {
            if (!key.modifiers.test(Modifiers::Alt)) {
                switch (seq->bytes[0]) {
                case 'q':
                    commands::quit();
                case 'l':
                    commands::clearStatusLine();
                    break;
                case 'o':
                    commands::open();
                    break;
                case 's':
                    commands::save();
                    break;
                case 'z':
                    commands::undo();
                    break;
                case 'p':
                    commands::gotoFile();
                    break;
                case 'c':
                    commands::copy();
                    break;
                case 'v': {
                    commands::paste();
                } break;
                }
            } else {
                switch (seq->bytes[0]) {
                case 'z':
                    commands::redo();
                    break;
                case 'p':
                    commands::showCommandPalette();
                    break;
                }
            }
        }
    } else {
        die("Invalid Key variant");
    }
}

void processPromptInput(const Key& key)
{
    const auto& prompt = editor::getPrompt();
    const auto text = prompt->input.getText().getString();
    if (processBufferInput(prompt->input, key)) {
        // not very clever
        if (text != prompt->input.getText().getString())
            prompt->update();
        return;
    }

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        switch (*special) {
        case SpecialKey::Up:
            prompt->selectUp();
            break;
        case SpecialKey::Down:
            prompt->selectDown();
            break;
        case SpecialKey::Return:
            editor::confirmPrompt();
            break;
        case SpecialKey::Escape:
            editor::abortPrompt();
            break;
        default:
            break;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
    } else {
        die("Invalid Key variant");
    }
}

void printUsage()
{
    printf("Usage: `exquisite <file>` or `<cmd> | exquisite`\n");
}

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() > 0) {
        fs::path path = args[0];
        if (!fs::exists(path)) {
            editor::setStatusMessage("New file");
            editor::buffer.setPath(path);
        } else {
            if (!editor::buffer.readFromFile(path)) {
                fprintf(stderr, "Could not open file '%s'\n", args[0].c_str());
                exit(1);
            }
        }
    } else if (!isatty(STDIN_FILENO)) {
        editor::buffer.readFromStdin();
        // const int fd = dup(STDIN_FILENO);
        const int tty = open("/dev/tty", O_RDONLY);
        dup2(tty, STDIN_FILENO);
        close(tty);
    }

    debug(">>>>>>>>>>>>>>>>>>>>>> INIT <<<<<<<<<<<<<<<<<<<<<<");

    terminal::init();

    editor::redraw();

    while (true) {
        const auto key = terminal::readKey();
        if (key) {
            debugKey(*key);
            if (editor::getPrompt())
                processPromptInput(*key);
            else
                processInput(*key);
        }

        editor::redraw();
    }

    return 0;
}
