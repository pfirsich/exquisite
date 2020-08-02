#include "commands.hpp"

#include <filesystem>

#include "clipboard.hpp"
#include "debug.hpp"
#include "editor.hpp"

namespace fs = std::filesystem;

Command::Command(std::string n, std::function<void()> f)
    : name(std::move(n))
    , func(f)
{
    commands::all().push_back(this);
}

void Command::operator()() const
{
    func();
}

// clang-format off
namespace commands {
std::vector<const Command*>& all() {
    static std::vector<const Command*> cmd;
    return cmd;
}

Command quit {
    "Quit",
    // clang-format on
    []() { exit(0); }
    // clang-format off
};

Command clearStatusLine {
    "Clear Status Line",
    // clang-format on
    []() { editor::setStatusMessage(""); }
    // clang-format off
};

// clang-format on
editor::StatusMessage openPromptCallback(std::string_view input)
{
    if (!editor::buffer.readFromFile(fs::path(input))) {
        return editor::StatusMessage { "Could not open file", editor::StatusMessage::Type::Error };
    }
    return editor::StatusMessage { "" };
}
// clang-format off

Command open {
    "Open",
    // clang-format on
    []() {
        editor::setPrompt(editor::Prompt { "Open File> ", openPromptCallback });
    }
    // clang-format off
};

// clang-format on
editor::StatusMessage savePromptCallback(std::string_view input)
{
    editor::buffer.setPath(fs::path(input));
    if (!editor::buffer.save()) {
        return editor::StatusMessage { "Error saving file", editor::StatusMessage::Type::Error };
    }
    return editor::StatusMessage { "Saved to '" + std::string(input) + "'" };
}
// clang-format off

Command save {
    "Save",
    // clang-format on
    []() {
        if (editor::buffer.path.empty()) {
            editor::setPrompt(editor::Prompt { "Save File> ", savePromptCallback });
        } else {
            editor::buffer.save();
            editor::setStatusMessage("Saved");
        }
    }
    // clang-format off
};

Command undo {
    "Undo",
    // clang-format on
    []() {
        if (!editor::buffer.undo())
            editor::setStatusMessage("Nothing to undo");
    }
    // clang-format off
};

Command redo {
    "Redo",
    // clang-format on
    []() {
        if (!editor::buffer.redo())
            editor::setStatusMessage("Nothing to redo");
    }
    // clang-format off
};

// clang-format on
editor::StatusMessage gotoFileCallback(std::string_view input)
{
    if (!editor::buffer.readFromFile(fs::path(input))) {
        return editor::StatusMessage { "Could not open file", editor::StatusMessage::Type::Error };
    }
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
// clang-format off

Command gotoFile {
    "Goto File",
    // clang-format on
    []() {
        editor::setPrompt(editor::Prompt { "> ", gotoFileCallback, walkDirectory() });
    }
    // clang-format off
};

// clang-format on
editor::StatusMessage commandPaletteCallback(std::string_view input)
{
    const auto it = std::find_if(commands::all().begin(), commands::all().end(),
        [input](const Command* cmd) { return cmd->name == input; });
    std::iter_swap(it, commands::all().end() - 1);
    debug("Command: ", commands::all().back()->name);
    commands::all().back()->func();
    return editor::StatusMessage {};
}
// clang-format off

Command showCommandPalette {
    "Show Command Palette",
    // clang-format on
    []() {
        std::vector<std::string> options;
        for (const auto cmd : commands::all()) {
            options.push_back(cmd->name);
        }
        editor::setPrompt(editor::Prompt { "> ", commandPaletteCallback, options });
    }
    // clang-format off
};

Command copy {
    "Copy",
    // clang-format on
    []() {
        if (!editor::buffer.getCursor().emptySelection()) {
            if (!setClipboardText(
                    editor::buffer.getText().getString(editor::buffer.getSelection()))) {
                editor::setStatusMessage(
                    "Could not set clipboard.", editor::StatusMessage::Type::Error);
            }
        }
    }
    // clang-format off
};

Command paste {
    "Paste",
    // clang-format on
    []() {
        const auto clip = getClipboardText();
        if (clip) {
            editor::buffer.insert(*clip);
        } else {
            editor::setStatusMessage(
                "Could not get clipboard.", editor::StatusMessage::Type::Error);
        }
    }
    // clang-format off
};
}
// clang-format on
