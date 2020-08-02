#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "clipboard.hpp"
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

editor::StatusMessage openFilePromptCallback(std::string_view input)
{
    const auto path = std::string(input);
    const auto read = readFile(path);
    if (!read) {
        return editor::StatusMessage { "Could not open file '" + path + "'",
            editor::StatusMessage::Type::Error };
    }
    return editor::StatusMessage { "" };
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
    return editor::StatusMessage { "" };
}

editor::Prompt saveFilePrompt()
{
    editor::Prompt prompt { "Save File> ", saveFilePromptCallback };
    prompt.input.setText(editor::buffer.name);
    prompt.input.moveCursorEnd(false);
    return prompt;
}

std::vector<std::string> commands { "FOO", "BAR", "BLUB", "TEST", "KACKEN", "PISSEN", "ENTE",
    "BANANE", "WASSER" };

editor::StatusMessage commandPaletteCallback(std::string_view input)
{
    const auto it = std::find(commands.begin(), commands.end(), std::string(input));
    std::iter_swap(it, commands.end() - 1);
    return editor::StatusMessage { commands.back() };
}

editor::Prompt commandPalettePrompt()
{
    return editor::Prompt { "> ", commandPaletteCallback, commands };
}

editor::StatusMessage gotoFileCallback(std::string_view input)
{
    if (!readFile(std::string(input)))
        return editor::StatusMessage { "Could not close file", editor::StatusMessage::Type::Error };
    return editor::StatusMessage { "" };
}

std::vector<std::string> walkDirectory()
{
    std::vector<std::string> files;
    for (auto it = fs::recursive_directory_iterator(".",
             fs::directory_options::follow_directory_symlink
                 | fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
        if (it->path().filename().c_str()[0] == '.')
            it.disable_recursion_pending();
        if (it->is_regular_file())
            files.push_back(it->path());
    }
    return files;
}

editor::Prompt gotoFilePrompt()
{
    return editor::Prompt { "> ", gotoFileCallback, walkDirectory() };
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
                    terminal::write(control::clear);
                    terminal::write(control::resetCursor);
                    exit(0);
                case 'l':
                    editor::setStatusMessage("");
                    break;
                case 'o':
                    editor::setPrompt(editor::Prompt { "Open File> ", openFilePromptCallback });
                    break;
                case 's':
                    editor::setPrompt(saveFilePrompt());
                    break;
                case 'z':
                    if (!editor::buffer.undo())
                        editor::setStatusMessage("Nothing to undo");
                    break;
                case 'p':
                    editor::setPrompt(gotoFilePrompt());
                    break;
                case 'c':
                    if (!editor::buffer.getCursor().emptySelection()) {
                        if (!setClipboardText(editor::buffer.getText().getString(
                                editor::buffer.getSelection()))) {
                            editor::setStatusMessage(
                                "Could not set clipboard.", editor::StatusMessage::Type::Error);
                        }
                    }
                    break;
                case 'v': {
                    const auto clip = getClipboardText();
                    if (clip) {
                        editor::buffer.insert(*clip);
                    } else {
                        editor::setStatusMessage(
                            "Could not get clipboard.", editor::StatusMessage::Type::Error);
                    }
                } break;
                }
            } else {
                switch (seq->bytes[0]) {
                case 'z':
                    if (!editor::buffer.redo())
                        editor::setStatusMessage("Nothing to redo");
                    break;
                case 'p':
                    editor::setPrompt(commandPalettePrompt());
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
