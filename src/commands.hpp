#pragma once

#include <functional>
#include <string>
#include <vector>

using CommandFunction = void();
using Command = std::function<CommandFunction>;

namespace commands {
Command quit();
Command clearStatusLine();
Command openFile(std::string_view path = "");
Command saveFile(std::string_view path = "");
Command saveFileAs();
Command undo();
Command redo();
Command gotoFile();
Command showCommandPalette();
Command copy();
Command paste();
Command find();
Command findPrev();
Command findNext();
Command setLanguage();
}
