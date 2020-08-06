#pragma once

#include <vector>

#include "commands.hpp"
#include "key.hpp"

struct Shortcut {
    Key key;
    Command command;
    std::string help;
};

inline std::vector<Shortcut>& getShortcuts()
{
    static std::vector<Shortcut> shortcuts {
        { Key(Modifiers::Ctrl, 'q'), commands::quit(), "Quit" },
        { Key(Modifiers::Ctrl, 'l'), commands::clearStatusLine(), "Clear status line" },
        { Key(Modifiers::Ctrl, 'o'), commands::openFile(), "Open file" },
        { Key(Modifiers::Ctrl, 's'), commands::saveFile(), "Close file" },
        { Key(Modifiers::Ctrl, 'z'), commands::undo(), "Undo" },
        { Key(Modifiers::Ctrl | Modifiers::Alt, 'z'), commands::redo(), "Redo" },
        { Key(Modifiers::Ctrl, 'c'), commands::copy(), "Copy" },
        { Key(Modifiers::Ctrl, 'v'), commands::paste(), "Paste" },
        { Key(Modifiers::Ctrl, 'p'), commands::gotoFile(), "Goto file" },
        { Key(Modifiers::Ctrl | Modifiers::Alt, 'p'), commands::showCommandPalette(),
            "Show command palette" },
        { Key(Modifiers::Ctrl, 'f'), commands::find(), "Find" },
        { Key(Modifiers::Ctrl, 'n'), commands::findNextSelection(),
            "Find next occurence of current selection" },
        { Key(Modifiers::Ctrl | Modifiers::Alt, 'n'), commands::findPrevSelection(),
            "Find previous occurence of current selection" },
        { Key(Modifiers::Ctrl, 'b'), commands::showBufferList(), "Show buffer list" },
    };
    return shortcuts;
}
