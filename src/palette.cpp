#include "palette.hpp"

std::vector<PaletteEntry>& getPalette()
{
    static std::vector<PaletteEntry> palette {
        { "Quit", commands::quit() },
        { "Clear Status Line", commands::clearStatusLine() },
        { "Open File", commands::openFile() },
        { "Save File", commands::saveFile() },
        { "Save File As", commands::saveFileAs() },
        { "Undo", commands::undo() },
        { "Redo", commands::redo() },
        { "Goto File", commands::gotoFile() },
        { "Copy", commands::copy() },
        { "Paste", commands::paste() },
        { "Set Language", commands::setLanguage() },
        { "New Buffer", commands::newBuffer() },
        { "Close Buffer", commands::closeBuffer() },
        { "Rename Buffer", commands::renameBuffer() },
        { "Show Shortcut Help", commands::showShortcutHelp() },
        { "Toggle Buffer Read-Only", commands::toggleBufferReadOnly() },
    };
    std::sort(palette.begin(), palette.end(),
        [](const PaletteEntry& a, const PaletteEntry& b) { return a.title < b.title; });
    return palette;
}
