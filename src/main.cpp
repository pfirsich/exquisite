#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <CLI/CLI.hpp>

#include "commands.hpp"
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
        case SpecialKey::Home:
            buffer.moveCursorHome(shift);
            return true;
        case SpecialKey::End:
            buffer.moveCursorEnd(shift);
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

void printUsage()
{
    printf("Usage: `exquisite <file>` or `<cmd> | exquisite`\n");
}

struct CliOptions {
    bool readOnly = false;
    bool debug = false;
    const Language* language = &languages::plainText;
    std::vector<std::string> files;
};

struct OptParserLanguage {
    CliOptions& opts;
    void operator()(const std::string& opt)
    {
        opts.language = languages::getFromExt(opt);
        if (opts.language == &languages::plainText && !opt.empty())
            throw CLI::ValidationError { "--lang: Unrecognized extension" };
    }
};

CliOptions parseOpts(int argc, char** argv)
{
    CLI::App app { "Joel's text editor" };
    CliOptions opts;
    app.add_flag("-R,--read-only", opts.readOnly, "Start editor in read-only mode");
    app.add_flag("-D,--debug", opts.debug, "Write log output to debug.out")->envname("EXQ_DEBUG");
    app.add_option_function<std::string>("-L,--lang", OptParserLanguage { opts },
        "Use this as the extension of the file to determine the language mode\n"
        "Pass empty to force plain text");
    app.add_option("files", opts.files,
        "Files to open. May be a single directory, which will be used as working directory.");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(app.exit(e));
    }

    if (std::string_view(argv[0]) == "rexq")
        opts.readOnly = true;

    return opts;
}

int main(int argc, char** argv)
{
    const auto opts = parseOpts(argc, argv);

    if (opts.readOnly)
        editor::setReadOnly();

    if (opts.debug)
        logDebugToFile = true;

    // HACK: We force the event handler to be instantiated first, so it's destructed after all the
    // buffers. This is to avoid a segfault on exit.
    getEventHandler();

    debug(">>>>>>>>>>>>>>>>>>>>>> INIT <<<<<<<<<<<<<<<<<<<<<<");

    if (!opts.files.empty()) {
        if (opts.files.size() == 1 && fs::is_directory(opts.files.front())) {
            const auto res = ::chdir(opts.files.front().c_str());
            if (res != 0) {
                fprintf(stderr, "Could not change working directory");
                exit(1);
            }
            editor::openBuffer();
        } else {
            for (const auto& file : opts.files) {
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
            debugKey(*key);
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
