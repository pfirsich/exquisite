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
    };
    return palette;
}
