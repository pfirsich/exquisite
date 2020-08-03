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
    size_t matchNumber = 1;
    size_t occurences = 0;
};

enum class FindMode { Normal, Next, Prev };

FindResult editorFind(std::string_view input, FindMode mode = FindMode::Normal)
{
    if (input.empty())
        return FindResult {};

    const auto& text = editor::buffer.getText();
    const auto cursorPos = editor::buffer.getCursorOffset(editor::buffer.getCursor().start)
        + (mode == FindMode::Next ? 1 : 0); // hack?
    FindResult res;
    Range first, last, prev, current; // current is starting from cursor
    for (size_t i = 0; i < text.getSize(); ++i) {
        if (match(text, i, input)) {
            res.occurences++;
            const auto find = Range { i, input.size() };

            if (first.length == 0)
                first = find;

            if (i >= cursorPos) {
                if (current.length == 0)
                    current = find;
            } else {
                prev = find;
                res.matchNumber++;
            }

            last = find;

            i += input.size();
        }
    }

    if (res.occurences == 0)
        return res;

    if (mode == FindMode::Normal || mode == FindMode::Next)
        res.find = current.length > 0 ? current : first;
    else if (mode == FindMode::Prev)
        res.find = prev.length > 0 ? prev : last;

    if (res.find.length > 0) {
        debug("select: ", res.find.offset, ", ", res.find.length);
        editor::buffer.select(res.find);
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
    std::stringstream ss;
    ss << res.matchNumber << "/" << res.occurences << " matches for '" << input << "'";
    return ss.str();
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
        prompt.input.moveCursorEnd(true);
        editor::setPrompt(std::move(prompt));
    };
}

Command findPrev()
{
    return []() {
        const auto& lastFind = getLastFind();
        if (lastFind.empty()) {
            editor::setStatusMessage("No last search");
            return;
        }

        const auto f = getFindStatus(lastFind, FindMode::Prev);
        editor::setStatusMessage(f.second.message, f.second.type);
    };
}

Command findNext()
{
    return []() {
        const auto& lastFind = getLastFind();
        if (lastFind.empty()) {
            editor::setStatusMessage("No last search");
            return;
        }

        const auto f = getFindStatus(lastFind, FindMode::Next);
        editor::setStatusMessage(f.second.message, f.second.type);
    };
}
}
