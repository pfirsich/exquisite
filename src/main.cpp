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
#include "shortcuts.hpp"
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
            // I actually don't really know how much a page is here
            buffer.moveCursorY(-(size.y - 3), select);
            return true;
        case SpecialKey::PageDown:
            buffer.moveCursorY(size.y - 3, select);
            return true;
        case SpecialKey::Home:
            buffer.moveCursorHome(select);
            return true;
        case SpecialKey::End:
            buffer.moveCursorEnd(select);
            return true;
        case SpecialKey::Backspace:
            if (!buffer.getReadOnly())
                buffer.deleteBackwards();
            return true;
        case SpecialKey::Delete:
            if (!buffer.getReadOnly())
                buffer.deleteForwards();
            return true;
        default:
            return false;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        // debug("utf8seq (", seq->length, "): ", std::string_view(&seq->bytes[0], seq->length));
        if (!key.modifiers.test(Modifiers::Ctrl)) {
            if (!buffer.getReadOnly())
                buffer.insert(std::string_view(&seq->bytes[0], seq->length));
            return true;
        }
    } else {
        die("Invalid Key variant");
    }
    return false;
}

std::string getCurrentIndent(const Buffer& buffer)
{
    std::string indent;
    indent.reserve(32);
    const auto line = buffer.getText().getLine(buffer.getCursor().min().y);
    for (size_t i = line.offset; i < line.offset + line.length; ++i) {
        const auto ch = buffer.getText()[i];
        if (ch == ' ' || ch == '\t')
            indent.push_back(ch);
        else
            break;
    }
    return indent;
}

void debugKey(const Key& key)
{
    debug("key hex ({}): {}", key.bytes.size(), hexString(key.bytes.data(), key.bytes.size()));
    if (key.bytes[0] == '\x1b') {
        debug("escape: {}", std::string_view(key.bytes.data() + 1, key.bytes.size() - 1));
    }
    debug("ctrl: {}, alt: {}, shift: {}", key.modifiers.test(Modifiers::Ctrl),
        key.modifiers.test(Modifiers::Alt), key.modifiers.test(Modifiers::Shift));

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        debug("special: {}", toString(*special));
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        debug("utf8seq ({}): {}", seq->length, std::string_view(&seq->bytes[0], seq->length));
    }
}

void processInput(Buffer& buffer, const Key& key)
{
    if (processBufferInput(buffer, key))
        return;

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        const bool select = key.modifiers.test(Modifiers::Shift);
        switch (*special) {
        case SpecialKey::Up:
            buffer.moveCursorY(-1, select);
            break;
        case SpecialKey::Down:
            buffer.moveCursorY(1, select);
            break;
        case SpecialKey::Return:
            if (!buffer.getReadOnly())
                buffer.insert("\n" + getCurrentIndent(buffer));
            break;
        default:
            break;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        for (const auto& shortcut : getShortcuts()) {
            if (shortcut.key == key) {
                shortcut.command();
                break;
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
        if (key == Key(Modifiers::Ctrl, 'c')) {
            prompt->input.setText("");
            prompt->update();
        }
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
            editor::openBuffer().setPath(path);
        } else {
            if (!editor::openBuffer().readFromFile(path)) {
                fprintf(stderr, "Could not open file '%s'\n", args[0].c_str());
                exit(1);
            }
        }
    } else if (!isatty(STDIN_FILENO)) {
        editor::openBuffer().readFromStdin();
        // const int fd = dup(STDIN_FILENO);
        const int tty = open("/dev/tty", O_RDONLY);
        dup2(tty, STDIN_FILENO);
        close(tty);
    } else {
        editor::openBuffer();
    }

    for (const auto lang : languages::getAll()) {
        if (lang->highlighter)
            lang->highlighter->setColorScheme(colorScheme);
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
                processInput(editor::getBuffer(), *key);
        }

        editor::redraw();
    }

    return 0;
}
