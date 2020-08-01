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
        const bool select = key.modifiers.test(Modifiers::Shift);
        switch (*special) {
        case SpecialKey::Left:
            buffer.moveCursorLeft(select);
            return true;
        case SpecialKey::Right:
            buffer.moveCursorRight(select);
            return true;
        case SpecialKey::Up:
            buffer.moveCursorY(-1, select);
            return true;
        case SpecialKey::Down:
            buffer.moveCursorY(1, select);
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

bool readFile(const std::string& path)
{
    auto f = uniqueFopen(path.c_str(), "rb");
    if (!f)
        return false;
    fseek(f.get(), 0, SEEK_END);
    const auto size = ftell(f.get());
    fseek(f.get(), 0, SEEK_SET);
    std::vector<char> buf(size, '\0');
    fread(buf.data(), 1, size, f.get());
    const auto sv = std::string_view(buf.data(), buf.size());
    editor::buffer.setText(sv);
    editor::buffer.name = path;
    return true;
}

void readStdin()
{
    std::vector<char> buf;
    char c;
    while (read(STDIN_FILENO, &c, 1) > 0) {
        buf.push_back(c);
    }
    const auto sv = std::string_view(buf.data(), buf.size());
    editor::buffer.setText(sv);
    editor::buffer.name = "STDIN";
}

editor::StatusMessage insertTextPromptCallback(std::string_view input)
{
    if (input.empty())
        return editor::StatusMessage { "Empty", editor::StatusMessage::Type::Error };

    editor::buffer.insert(input);
    return editor::StatusMessage { "" };
}

editor::StatusMessage openFilePromptCallback(std::string_view input)
{
    const auto path = std::string(input);
    const auto read = readFile(path);
    if (!read) {
        return editor::StatusMessage { "Could not open file '" + path + "'",
            editor::StatusMessage::Type::Error };
    }
    return editor::getStatusMessage();
}

editor::StatusMessage saveFilePromptCallback(std::string_view input)
{
    const auto path = std::string(input);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
        return editor::StatusMessage { "Could not open file", editor::StatusMessage::Type::Error };
    const auto data = editor::buffer.getText().getString();
    if (fwrite(data.c_str(), 1, data.size(), f) != data.size())
        return editor::StatusMessage { "Could not write file", editor::StatusMessage::Type::Error };
    if (fclose(f) != 0)
        return editor::StatusMessage { "Could not close file", editor::StatusMessage::Type::Error };
    return editor::getStatusMessage();
}

std::unique_ptr<editor::Prompt> saveFilePrompt()
{
    auto ptr = std::make_unique<editor::Prompt>(
        editor::Prompt { "Save File> ", saveFilePromptCallback });
    ptr->input.setText(editor::buffer.name);
    ptr->input.moveCursorEnd(false);
    return ptr;
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
        switch (*special) {
        case SpecialKey::Return:
            editor::buffer.insert("\n" + getCurrentIndent());
            break;
        default:
            break;
        }
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
        if (key.modifiers.test(Modifiers::Ctrl) && !key.modifiers.test(Modifiers::Alt)) {
            switch (seq->bytes[0]) {
            case 'q':
                terminal::write(control::clear);
                terminal::write(control::resetCursor);
                exit(0);
            case 'l':
                editor::setStatusMessage("");
                break;
            case 'o':
                editor::currentPrompt = std::make_unique<editor::Prompt>(
                    editor::Prompt { "Open File> ", openFilePromptCallback });
                break;
            case 's':
                editor::currentPrompt = saveFilePrompt();
                break;
            case 'p':
                editor::currentPrompt = std::make_unique<editor::Prompt>(
                    editor::Prompt { "Insert Text> ", insertTextPromptCallback });
                break;
            }
        }
    } else {
        die("Invalid Key variant");
    }
}

void processPromptInput(const Key& key)
{
    if (processBufferInput(editor::currentPrompt->input, key))
        return;

    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
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

void printUsage()
{
    printf("Usage: `exquisite <file>` or `<cmd> | exquisite`\n");
}

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() > 0) {
        if (!readFile(args[0])) {
            fprintf(stderr, "Could not open file '%s'\n", args[0].c_str());
            exit(1);
        }
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

    debug(">>>>>>>>>>>>>>>>>>>>>> INIT <<<<<<<<<<<<<<<<<<<<<<");

    terminal::init();

    editor::redraw();

    while (true) {
        const auto key = terminal::readKey();
        if (key) {
            debugKey(*key);
            if (editor::currentPrompt)
                processPromptInput(*key);
            else
                processInput(*key);
        }

        editor::redraw();
    }

    return 0;
}
