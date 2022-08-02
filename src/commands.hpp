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
Command renameFile();
Command undo();
Command redo();
Command gotoFile();
Command showCommandPalette();
Command cut();
Command copy();
Command paste();
Command find();
Command findPrevSelection();
Command findNextSelection();
Command setLanguage();
Command newBuffer();
Command renameBuffer();
Command toggleBufferReadOnly();
Command closeBuffer();
Command showBufferList();
Command showShortcutHelp();
Command indentUsingSpaces();
Command indentUsingTabs();
Command setTabWidth();
Command moveCursorY(int offset, bool select);
Command insertNewLine(bool insertAtEol);
Command promptSelectUp();
Command promptSelectDown();
Command promptConfirm();
Command promptAbort();
Command promptClear();
Command duplicateSelection();
Command moveCursorBol(bool select);
Command moveCursorEol(bool select);
Command moveCursorBof(bool select);
Command moveCursorEof(bool select);
}
