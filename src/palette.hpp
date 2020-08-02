#pragma once

#include <string>
#include <vector>

#include "commands.hpp"

struct PaletteEntry {
    std::string title;
    Command command;
};

std::vector<PaletteEntry>& getPalette();
