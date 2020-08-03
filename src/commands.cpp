#include "commands.hpp"

#include <cassert>
#include <filesystem>

#include "clipboard.hpp"
#include "debug.hpp"
#include "editor.hpp"
#include "palette.hpp"

namespace fs = std::filesystem;

namespace commands {
editor::StatusMessage quitPromptCallback(std::string_view input)
{
    if (std::tolower(input[0]) == 'y') { // works with "yeet" in particular
        exit(0);
    }
    return editor::StatusMessage { "" };
}

Command quit()
{
    return []() {
        if (!editor::buffer.isModified())
            exit(0);

        editor::setPrompt(
            editor::Prompt { "Unsaved Changes! Really quit [y/n]?> ", quitPromptCallback });
    };
}

Command clearStatusLine()
{
    return []() { editor::setStatusMessage(""); };
};

editor::StatusMessage openPromptCallback(std::string_view input)
{
    if (!editor::buffer.readFromFile(fs::path(input))) {
        return editor::StatusMessage { "Could not open file", editor::StatusMessage::Type::Error };
    }
    return editor::StatusMessage { "" };
}

Command openFile(std::string_view path)
{
    if (path.empty()) {
        return []() { editor::setPrompt(editor::Prompt { "Open File> ", openPromptCallback }); };
    } else {
        return [path = std::string(path)]() { editor::setStatusMessage(openPromptCallback(path)); };
    }
};

editor::StatusMessage savePromptCallback(std::string_view input)
{
    editor::buffer.setPath(fs::path(input));
    if (!editor::buffer.save()) {
        return editor::StatusMessage { "Error saving file", editor::StatusMessage::Type::Error };
    }
    return editor::StatusMessage { "Saved to '" + std::string(input) + "'" };
}

Command saveFile(std::string_view path)
{
    if (path.empty()) {
        return []() {
            if (editor::buffer.path.empty()) {
                editor::setPrompt(editor::Prompt { "Save File> ", savePromptCallback });
            } else {
                editor::buffer.save();
                editor::setStatusMessage("Saved");
            }
        };
    } else {
        return [path = std::string(path)]() { editor::setStatusMessage(savePromptCallback(path)); };
    }
}

Command saveFileAs()
{
    return []() { editor::setPrompt(editor::Prompt { "Save File> ", savePromptCallback }); };
}

Command undo()
{
    return []() {
        if (!editor::buffer.undo())
            editor::setStatusMessage("Nothing to undo");
    };
}

Command redo()
{
    return []() {
        if (!editor::buffer.redo())
            editor::setStatusMessage("Nothing to redo");
    };
}

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

Command gotoFile()
{
    return []() { editor::setPrompt(editor::Prompt { "> ", gotoFileCallback, walkDirectory() }); };
}

editor::StatusMessage commandPaletteCallback(std::string_view input)
{
    auto& palette = getPalette();
    const auto it = std::find_if(palette.begin(), palette.end(),
        [input](const PaletteEntry& entry) { return entry.title == input; });
    std::iter_swap(it, palette.end() - 1);
    debug("Command: ", palette.back().title);
    palette.back().command();
    return editor::StatusMessage {};
}

Command showCommandPalette()
{
    return []() {
        std::vector<std::string> options;
        for (const auto& cmd : getPalette()) {
            options.push_back(cmd.title);
        }
        editor::setPrompt(editor::Prompt { "> ", commandPaletteCallback, options });
    };
}

Command copy()
{
    return []() {
        if (!editor::buffer.getCursor().emptySelection()) {
            if (!setClipboardText(
                    editor::buffer.getText().getString(editor::buffer.getSelection()))) {
                editor::setStatusMessage(
                    "Could not set clipboard", editor::StatusMessage::Type::Error);
            }
        }
    };
}

Command paste()
{
    return []() {
        const auto clip = getClipboardText();
        if (clip) {
            editor::buffer.insert(*clip);
        } else {
            editor::setStatusMessage("Could not get clipboard", editor::StatusMessage::Type::Error);
        }
    };
}
}
