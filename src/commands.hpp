#pragma once

#include <functional>
#include <string>
#include <vector>

struct Command {
    std::string name;
    std::function<void()> func;

    Command(std::string name, std::function<void()> func);
    void operator()() const;
};

namespace commands {
std::vector<const Command*>& all();

extern Command quit;
extern Command clearStatusLine;
extern Command open;
extern Command save;
extern Command undo;
extern Command redo;
extern Command gotoFile;
extern Command showCommandPalette;
extern Command copy;
extern Command paste;
}
