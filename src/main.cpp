#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <clipp.hpp>

#include "commands.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "editor.hpp"
#include "eventhandler.hpp"
#include "shortcuts.hpp"
#include "terminal.hpp"
#include "util.hpp"

// Most of this should probably be in shortcuts
bool processBufferInput(Buffer& buffer, const Key& key)
{
    if (const auto special = std::get_if<SpecialKey>(&key.key)) {
        // debug("special: ", toString(*special));
        const auto size = terminal::getSize();
        const bool ctrl = key.modifiers.test(Modifiers::Ctrl);
        const bool shift = key.modifiers.test(Modifiers::Shift);
        switch (*special) {
        case SpecialKey::Left:
            if (ctrl)
                buffer.moveCursorWordLeft(shift);
            else
                buffer.moveCursorLeft(shift);
            return true;
        case SpecialKey::Right:
            if (ctrl)
                buffer.moveCursorWordRight(shift);
            else
                buffer.moveCursorRight(shift);
            return true;
        case SpecialKey::PageUp:
            // I actually don't really know how much a page is here
            buffer.moveCursorY(-(size.y - 3), shift);
            return true;
        case SpecialKey::PageDown:
            buffer.moveCursorY(size.y - 3, shift);
            return true;
        case SpecialKey::Backspace:
            buffer.deleteBackwards();
            return true;
        case SpecialKey::Delete:
            buffer.deleteForwards();
            return true;
        case SpecialKey::Tab:
            if (shift)
                buffer.dedent();
            else
                buffer.indent();
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

void executeShortcuts(Bitmask<Context> contexts, const Key& key)
{
    for (const auto& s : getShortcuts()) {
        if (s.contexts.test(contexts) && s.key == key) {
            s.command();
            break;
        }
    }
}

void processInput(Buffer& buffer, const Key& key)
{
    if (processBufferInput(buffer, key))
        return;

    executeShortcuts(Context::Buffer, key);
}

void processPromptInput(const Key& key)
{
    auto prompt = editor::getPrompt();
    const auto text = prompt->input.getText().getString();
    if (processBufferInput(prompt->input, key)) {
        // not very clever of checking whether the input changed something
        if (text != prompt->input.getText().getString())
            prompt->update();
        return;
    }

    executeShortcuts(Context::Prompt, key);
}

struct Args : clipp::ArgsBase {
    bool readOnly = false;
    bool debug = false;
    std::vector<std::string> files;

    void args()
    {
        flag(readOnly, "read-only", 'R').help("Start editor in read-only mode");
        flag(debug, "debug", 'D').help("Write log output to debug.out");
        positional(files, "files")
            .optional()
            .help("Files to open. May be a single directory to be used as the working "
                  "directory.");
    }

    std::string description() const override
    {
        return "You may also pass text via STDIN.";
    }
};

int main(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<Args>(argc, argv).value();

    if (args.readOnly || std::string_view(argv[0]) == "rexq")
        editor::setReadOnly();

    if (args.debug)
        logDebugToFile = true;

    // HACK: We force the event handler to be instantiated first, so it's destructed after all the
    // buffers. This is to avoid a segfault on exit.
    getEventHandler();

    debug(">>>>>>>>>>>>>>>>>>>>>> INIT <<<<<<<<<<<<<<<<<<<<<<");

    loadConfig();

    if (!args.files.empty()) {
        if (args.files.size() == 1 && fs::is_directory(args.files.front())) {
            const auto res = ::chdir(args.files.front().c_str());
            if (res != 0) {
                fprintf(stderr, "Could not change working directory");
                exit(1);
            }
            editor::openBuffer();
        } else {
            for (const auto& file : args.files) {
                fs::path path = file;
                if (!fs::exists(path)) {
                    editor::openBuffer().setPath(path);
                } else {
                    if (!editor::openBuffer().readFromFile(path)) {
                        fprintf(stderr, "Could not open file '%s'\n", file.c_str());
                        exit(1);
                    }
                }
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

    terminal::init();

    editor::redraw();

    getEventHandler().addSignalHandler(SIGWINCH, [] {
        debug("sigwinch handler");
        // If the terminal resized, we just redraw, because we get the size there anyway
        editor::triggerRedraw();
    });

    getEventHandler().addFdHandler(STDIN_FILENO, [] {
        debug("read stdin");
        const auto key = terminal::readKey();
        if (key) {
            // debugKey(*key);
            if (editor::getPrompt())
                processPromptInput(*key);
            else
                processInput(editor::getBuffer(), *key);
            editor::triggerRedraw();
        }
    });

    getEventHandler().run();

    return 0;
}
