#pragma once

#include <vector>

#include "commands.hpp"
#include "key.hpp"

// I feel like this context system is pretty dumb and will prove insufficient *very* quickly

enum class Context {
    Buffer,
    Prompt,
};

template <>
struct BitmaskEnabled<Context> : std::true_type {
};

struct Shortcut {
    Bitmask<Context> contexts;
    Key key;
    Command command;
    std::string help;
};

inline std::vector<Shortcut>& getShortcuts()
{
    const auto all = Context::Buffer | Context::Prompt;
    static std::vector<Shortcut> shortcuts {
        // both contexts
        { all, Key(Modifiers::Ctrl, 'q'), commands::quit(), "Quit" },
        { all, Key(Modifiers::Ctrl, '.'), commands::clearStatusLine(), "Clear status line" },
        { all, Key(Modifiers::Ctrl, 'z'), commands::undo(), "Undo" },
        { all, Key(Modifiers::Ctrl | Modifiers::Alt, 'z'), commands::redo(), "Redo" },
        { all, Key(Modifiers::Ctrl, 'c'), commands::copy(), "Copy" },
        { all, Key(Modifiers::Ctrl, 'v'), commands::paste(), "Paste" },
        { all, Key(Modifiers::Ctrl, 'p'), commands::gotoFile(), "Goto file" },
        { all, Key(Modifiers::Ctrl | Modifiers::Alt, 'p'), commands::showCommandPalette(),
            "Show command palette" },
        { all, Key(Modifiers::Ctrl, 'j'), commands::showBufferList(), "Show buffer list" },

        // buffer only
        { Context::Buffer, Key(SpecialKey::Up), commands::moveCursorY(-1, false),
            "Move cursor up one line" },
        { Context::Buffer, Key(Modifiers::Shift, SpecialKey::Up), commands::moveCursorY(-1, true),
            "Move cursor up one line while selecting" },
        { Context::Buffer, Key(SpecialKey::Down), commands::moveCursorY(1, false),
            "Move cursor down one line" },
        { Context::Buffer, Key(Modifiers::Shift, SpecialKey::Down), commands::moveCursorY(1, true),
            "Move cursor down one line while selecting" },
        { Context::Buffer, Key(SpecialKey::Return), commands::insertNewLine(false),
            "Insert new line" },
        { Context::Buffer, Key(Modifiers::Alt, SpecialKey::Return), commands::insertNewLine(true),
            "Insert new line at end of line" },

        { Context::Buffer, Key(Modifiers::Ctrl, 'k'), commands::duplicateSelection(), "Duplicate selection or current line" },
        { Context::Buffer, Key(Modifiers::Ctrl, 'w'), commands::closeBuffer(), "Close buffer" },
        { Context::Buffer, Key(Modifiers::Ctrl, 'o'), commands::openFile(), "Open file" },
        { Context::Buffer, Key(Modifiers::Ctrl, 's'), commands::saveFile(), "Save file" },
        { Context::Buffer, Key(Modifiers::Ctrl, 'f'), commands::find(), "Find" },
        { Context::Buffer, Key(Modifiers::Ctrl, 'n'), commands::findNextSelection(),
            "Find next occurence of current selection" },
        { Context::Buffer, Key(Modifiers::Ctrl | Modifiers::Alt, 'n'),
            commands::findPrevSelection(), "Find previous occurence of current selection" },

        // prompt only
        { Context::Prompt, Key(SpecialKey::Up), commands::promptSelectUp(),
            "Select previous option" },
        { Context::Prompt, Key(SpecialKey::Down), commands::promptSelectDown(),
            "Select next option" },
        { Context::Prompt, Key(SpecialKey::Return), commands::promptConfirm(), "Confirm prompt" },
        { Context::Prompt, Key(SpecialKey::Escape), commands::promptAbort(), "Abort prompt" },
    };
    return shortcuts;
}
