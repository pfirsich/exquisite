#pragma once

#include <vector>

#include "commands.hpp"
#include "key.hpp"

struct Shortcut {
    Key key;
    Command command;
};

inline std::vector<Shortcut>& getShortcuts()
{
    static std::vector<Shortcut> shortcuts {
        { Key(Modifiers::Ctrl, 'q'), commands::quit() },
        { Key(Modifiers::Ctrl, 'l'), commands::clearStatusLine() },
        { Key(Modifiers::Ctrl, 'o'), commands::openFile() },
        { Key(Modifiers::Ctrl, 's'), commands::saveFile() },
        { Key(Modifiers::Ctrl, 'z'), commands::undo() },
        { Key(Modifiers::Ctrl | Modifiers::Alt, 'z'), commands::redo() },
        { Key(Modifiers::Ctrl, 'c'), commands::copy() },
        { Key(Modifiers::Ctrl, 'v'), commands::paste() },
        { Key(Modifiers::Ctrl, 'p'), commands::gotoFile() },
        { Key(Modifiers::Ctrl | Modifiers::Alt, 'p'), commands::showCommandPalette() },
    };
    return shortcuts;
}
