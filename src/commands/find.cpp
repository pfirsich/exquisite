#include "commands.hpp"

#include <sstream>
#include <string_view>

#include "debug.hpp"
#include "editor.hpp"

namespace {
// This function does not check the bounds of text
bool match(const TextBuffer& text, size_t offset, std::string_view input)
{
    for (size_t i = 0; i < input.size(); ++i) {
        if (text[offset + i] != input[i])
            return false;
    }
    return true;
}

struct FindResult {
    Range find;
    size_t matchIndex = 0;
    size_t occurences = 0;
};

enum class FindMode { Normal, Next, Prev };

FindResult editorFind(std::string_view input, FindMode mode = FindMode::Normal)
{
    if (input.empty())
        return FindResult {};

    const auto& text = editor::getBuffer().getText();
    const auto cursorPos
        = editor::getBuffer().getCursorOffset(editor::getBuffer().getCursor().start);
    // This function was so complicated before, so I just do it this dumb, probably slow way
    std::vector<size_t> matches;
    matches.reserve(64);
    size_t cursorMatch = MaxSizeT; // the index of the first match right after the cursor
    for (size_t i = 0; i < text.getSize() - input.size(); ++i) {
        if (match(text, i, input)) {
            matches.push_back(i);
            if (i >= cursorPos && cursorMatch == MaxSizeT)
                cursorMatch = matches.size() - 1;
            i += input.size();
        }
    }

    if (matches.empty())
        return FindResult {};

    FindResult res;
    res.occurences = matches.size();

    if (cursorMatch == MaxSizeT) // the next after the cursor is at the start of the file
        cursorMatch = 0;

    if (mode == FindMode::Normal) {
        res.matchIndex = cursorMatch;
    } else if (mode == FindMode::Prev) {
        res.matchIndex = (cursorMatch - 1) % matches.size();
    } else if (mode == FindMode::Next) {
        if (cursorPos == matches[cursorMatch])
            res.matchIndex = (cursorMatch + 1) % matches.size();
        else // Just like Normal
            res.matchIndex = cursorMatch;
    }

    res.find = Range { matches[res.matchIndex], input.size() };

    if (res.find.length > 0) {
        editor::getBuffer().select(res.find);
    }

    return res;
}

std::string& getLastFind()
{
    static std::string find;
    return find;
}

std::string resultString(const FindResult& res, std::string_view input)
{
    auto ret = fmt::format("{}/{} matches", res.matchIndex + 1, res.occurences);
    if (!hasNewlines(input))
        ret += fmt::format(" for {}", input);
    return ret;
}

std::pair<FindResult, editor::StatusMessage> getFindStatus(std::string_view input, FindMode mode)
{
    if (input.empty())
        return std::pair(FindResult {}, editor::StatusMessage {});
    const auto res = editorFind(input, mode);
    if (res.occurences == 0)
        return std::pair(
            res, editor::StatusMessage { "No matches", editor::StatusMessage::Type::Error });
    return std::pair(res, editor::StatusMessage { resultString(res, input) });
}

editor::StatusMessage confirmCallback(std::string_view input)
{
    const auto f = getFindStatus(input, FindMode::Normal);
    // if (f.first.occurences > 0)
    getLastFind() = input;
    return f.second;
}

std::string updateCallback(editor::Prompt* prompt)
{
    const auto text = prompt->input.getText().getString();
    if (text.empty())
        return "No matches";

    const auto f = getFindStatus(text, FindMode::Normal);
    return f.second.message;
}
}

namespace commands {
Command find()
{
    return []() {
        auto prompt = editor::Prompt { "Find> ", confirmCallback, updateCallback };
        prompt.input.setText(getLastFind());
        prompt.input.moveCursorEol(true);
        editor::setPrompt(std::move(prompt));
    };
}

Command findPrevSelection()
{
    return []() {
        const auto str = editor::getBuffer().getSelectionString();
        if (str.empty()) {
            editor::setStatusMessage("No last search");
            return;
        }

        const auto f = getFindStatus(str, FindMode::Prev);
        editor::setStatusMessage(f.second.message, f.second.type);
    };
}

Command findNextSelection()
{
    return []() {
        const auto str = editor::getBuffer().getSelectionString();
        if (str.empty()) {
            editor::setStatusMessage("No selection");
            return;
        }

        const auto f = getFindStatus(str, FindMode::Next);
        editor::setStatusMessage(f.second.message, f.second.type);
    };
}
}
