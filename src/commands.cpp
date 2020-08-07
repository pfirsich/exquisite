#include "commands.hpp"

#include <cassert>
#include <filesystem>

#include "clipboard.hpp"
#include "debug.hpp"
#include "editor.hpp"
#include "palette.hpp"
#include "shortcuts.hpp"

using namespace std::literals;
namespace fs = std::filesystem;

namespace {
bool isYes(std::string_view input)
{
    return std::tolower(input[0]) == 'y'; // work with "yeet" in particular
}
}

namespace commands {
namespace {
    editor::StatusMessage quitPromptCallback(std::string_view input)
    {
        if (isYes(input)) { // works with "yeet" in particular
            exit(0);
        }
        return editor::StatusMessage { "" };
    }
}

Command quit()
{
    return []() {
        for (size_t i = 0; i < editor::getBufferCount(); ++i) {
            if (editor::getBuffer(i).isModified()) {
                editor::selectBuffer(i);
                editor::setPrompt(
                    editor::Prompt { "Unsaved Changes! Really quit [y/n]?> ", quitPromptCallback });
                return;
            }
        }
        exit(0);
    };
}

Command clearStatusLine()
{
    return []() { editor::setStatusMessage(""); };
};

namespace {
    editor::StatusMessage openPromptCallback(std::string_view input)
    {
        const auto path = fs::path(input);
        if (!editor::selectBuffer(path)) {
            if (!editor::openBuffer().readFromFile(path)) {
                editor::closeBuffer();
                return editor::StatusMessage { "Could not open file",
                    editor::StatusMessage::Type::Error };
            }
        }
        return editor::StatusMessage { "" };
    }
}

Command openFile(std::string_view path)
{
    if (path.empty()) {
        return []() { editor::setPrompt(editor::Prompt { "Open File> ", openPromptCallback }); };
    } else {
        return [path = std::string(path)]() { editor::setStatusMessage(openPromptCallback(path)); };
    }
};

namespace {
    editor::StatusMessage savePromptCallback(std::string_view input)
    {
        editor::getBuffer().setPath(fs::path(input));
        if (!editor::getBuffer().save()) {
            return editor::StatusMessage { "Error saving file",
                editor::StatusMessage::Type::Error };
        }
        return editor::StatusMessage { "Saved to '" + std::string(input) + "'" };
    }
}

Command saveFile(std::string_view path)
{
    if (path.empty()) {
        return []() {
            if (editor::getBuffer().path.empty()) {
                editor::setPrompt(editor::Prompt { "Save File> ", savePromptCallback });
            } else {
                if (!editor::getBuffer().save())
                    editor::setStatusMessage(
                        "Error saving file", editor::StatusMessage::Type::Error);
                else
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
        if (!editor::getBuffer().undo())
            editor::setStatusMessage("Nothing to undo");
    };
}

Command redo()
{
    return []() {
        if (!editor::getBuffer().redo())
            editor::setStatusMessage("Nothing to redo");
    };
}

namespace {
    std::vector<std::string> walkDirectory()
    {
        std::vector<std::string> files;
        for (auto it = fs::recursive_directory_iterator(".",
                 fs::directory_options::follow_directory_symlink
                     | fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it) {
            if (it->path().filename().c_str()[0] == '.')
                it.disable_recursion_pending();

            std::error_code ec;
            if (it->is_regular_file(ec))
                files.push_back(it->path().lexically_relative("./"));
            // If there was an error, we just ignore it
        }
        return files;
    }
}

Command gotoFile()
{
    return []() {
        try {
            editor::setPrompt(editor::Prompt { "> ", openPromptCallback, walkDirectory() });
        } catch (const fs::filesystem_error& exc) {
            editor::setStatusMessage(fmt::format("Error walking directory: {}", exc.what()),
                editor::StatusMessage::Type::Error);
        }
    };
}

namespace {
    editor::StatusMessage commandPaletteCallback(std::string_view input)
    {
        auto& palette = getPalette();
        const auto it = std::find_if(palette.begin(), palette.end(),
            [input](const PaletteEntry& entry) { return entry.title == input; });
        std::iter_swap(it, palette.end() - 1);
        debug("Command: {}", palette.back().title);
        palette.back().command();
        return editor::getStatusMessage(); // command might have changed it
    }
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
        const auto selection = editor::getBuffer().getSelectionString();
        if (!selection.empty()) {
            if (!setClipboardText(selection)) {
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
            editor::getBuffer().insert(*clip);
        } else {
            editor::setStatusMessage("Could not get clipboard", editor::StatusMessage::Type::Error);
        }
    };
}

namespace {
    editor::StatusMessage setLanguageCallback(std::string_view input)
    {
        for (const auto lang : languages::getAll()) {
            if (lang->name == input) {
                editor::getBuffer().setLanguage(lang);
                return editor::StatusMessage {};
            }
        }
        die("Unknown language");
    }
}

Command setLanguage()
{
    return []() {
        std::vector<std::string> options;
        for (const auto lang : languages::getAll())
            options.push_back(lang->name);
        std::sort(options.begin(), options.end());
        editor::setPrompt(editor::Prompt { "Set Language> ", setLanguageCallback, options });
    };
}

Command newBuffer()
{
    return []() { editor::openBuffer(); };
}

namespace {
    editor::StatusMessage renameBufferCallback(std::string_view input)
    {
        if (!input.empty())
            editor::getBuffer().name = input;
        return editor::StatusMessage {};
    }
}

Command renameBuffer()
{
    return []() { editor::setPrompt(editor::Prompt { "Rename Buffer> ", renameBufferCallback }); };
}

Command toggleBufferReadOnly()
{
    return []() {
        auto& buffer = editor::getBuffer();
        if (buffer.isModified()) {
            editor::setStatusMessage(
                "Cannot set modified buffer to read-only", editor::StatusMessage::Type::Error);
            return;
        } else if (editor::getReadOnly()) {
            editor::setStatusMessage("Cannot remove read-only, when editor is in read-only mode",
                editor::StatusMessage::Type::Error);
            return;
        }
        buffer.setReadOnly(!buffer.getReadOnly());
    };
}

namespace {
    editor::StatusMessage closeBufferCallback(std::string_view input)
    {
        if (isYes(input))
            editor::closeBuffer();
        return editor::StatusMessage {};
    }
}

Command closeBuffer()
{
    return []() {
        if (editor::getBuffer().isModified()) {
            editor::setPrompt(
                editor::Prompt { "Unsaved changes! Really close? [y/n]?> ", closeBufferCallback });
            return;
        }
        editor::closeBuffer();
    };
}

editor::StatusMessage showBufferListCallback(std::string_view input)
{
    for (size_t i = 0; i < editor::getBufferCount(); ++i) {
        const auto title = editor::getBuffer(i).getTitle();
        if (input.substr(0, title.size()) == title) {
            editor::selectBuffer(i);
            return editor::StatusMessage {};
        }
    }
    die("Invalid buffer");
}

Command showBufferList()
{
    return []() {
        std::vector<std::string> buffers;
        for (size_t i = 0; i < editor::getBufferCount(); ++i) {
            const auto& buffer = editor::getBuffer(i);
            buffers.push_back(buffer.getTitle());
        }
        std::reverse(buffers.begin(), buffers.end());
        editor::setPrompt(editor::Prompt { "> ", showBufferListCallback, buffers });
    };
}

Command showShortcutHelp()
{
    return []() {
        std::string text;
        for (const auto& sc : getShortcuts()) {
            text.append(fmt::format("{}: {}\n", sc.key.getAsString(), sc.help));
        }
        editor::openBuffer().setText(text);
        editor::getBuffer().setReadOnly();
    };
}

namespace {
    editor::StatusMessage indentUsingSpacesCallback(std::string_view input)
    {
        const auto num = toInt(std::string(input));
        if (!num || *num < 1)
            return editor::StatusMessage { "Invalid input", editor::StatusMessage::Type::Error };
        editor::getBuffer().indentation
            = Indentation { Indentation::Type::Spaces, static_cast<size_t>(*num) };
        return editor::getStatusMessage();
    }
}

Command indentUsingSpaces()
{
    return []() {
        editor::setPrompt(editor::Prompt { "Number Of Spaces> ", indentUsingSpacesCallback });
    };
}

Command indentUsingTabs()
{
    return []() { editor::getBuffer().indentation = Indentation { Indentation::Type::Tabs, 1 }; };
}

namespace {
    editor::StatusMessage setTabWidthCallback(std::string_view input)
    {
        const auto num = toInt(std::string(input));
        if (!num || *num < 1)
            return editor::StatusMessage { "Invalid input", editor::StatusMessage::Type::Error };
        editor::getBuffer().tabWidth = static_cast<size_t>(*num);
        return editor::getStatusMessage();
    }
}

Command setTabWidth()
{
    return []() { editor::setPrompt(editor::Prompt { "Tab Width> ", setTabWidthCallback }); };
}
}
