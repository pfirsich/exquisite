#include "buffer.hpp"

#include <cassert>
#include <tuple>

#include <unistd.h>

#include "debug.hpp"
#include "editor.hpp"
#include "utf8.hpp"
#include "util.hpp"

using namespace std::literals;

///////////////////////////////////////////// Cursor

bool Cursor::emptySelection() const
{
    return start == end;
}

bool Cursor::isOrdered() const
{
    // I use this function to know whether to stop start and end and I don't
    // need to swap if they are equal.
    if (start == end)
        return true;

    if (start.y < end.y)
        return true;
    if (end.y < start.y)
        return false;

    // start.y == end.y, start.x != end.x
    return start.x < end.x;
}

Cursor::End Cursor::min() const
{
    if (isOrdered())
        return start;
    return end;
}

Cursor::End Cursor::max() const
{
    if (isOrdered())
        return end;
    return start;
}

void Cursor::setX(size_t x, bool select)
{
    start.x = x;
    if (!select)
        end.x = x;
}

void Cursor::setY(size_t y, bool select)
{
    start.y = y;
    if (!select)
        end.y = y;
}

void Cursor::set(const End& pos, bool select)
{
    start = pos;
    if (!select) {
        end = pos;
    }
}

///////////////////////////////////////////// Buffer

Buffer::Buffer()
    : tabWidth(Config::get().tabWidth)
    // Consider an uninitialized buffer not modified
    , savedVersionId_(actions_.getCurrentVersionId())
{
}

const TextBuffer& Buffer::getText() const
{
    return text_;
}

const Cursor& Buffer::getCursor() const
{
    return cursor_;
}

size_t Buffer::getScroll() const
{
    return scroll_;
}

void Buffer::setPath(const fs::path& p)
{
    debug("set path");
    path = p;
    name = p.filename();
    if (fs::exists(p)) {
        watchFileModifications();
    }
}

void Buffer::setText(std::string_view str)
{
    text_.set(str);
    actions_.clear();
    cursor_ = Cursor {};
    scroll_ = 0;
    indentation = detectIndentation(str);
    savedVersionId_ = std::numeric_limits<size_t>::max();
}

bool Buffer::readFromFile(const fs::path& p)
{
    const auto data = readFile(p.c_str());
    if (!data)
        return false;
    setText(*data);
    setPath(p);
    savedVersionId_ = actions_.getCurrentVersionId();
    const auto extStr = std::string(p.extension());
    auto ext = std::string_view(extStr);
    if (ext[0] == '.')
        ext = ext.substr(1);
    setLanguage(languages::getFromExt(ext));
    lastModTime_ = fs::last_write_time(path);
    return true;
}

void Buffer::readFromStdin()
{
    const auto data = readAll(STDIN_FILENO);
    setText(data);
    name = "STDIN";
    path = "";
}

void Buffer::watchFileModifications()
{
    debug("watch file modifications");
    fileModHandler_.reset(&getEventHandler(), getEventHandler().addFilesystemHandler(path, [this] {
        debug("modified");
        if (!isModified()) {
            reload();
            editor::triggerRedraw();
        }
    }));
}

void Buffer::setTextUndoable(std::string text)
{
    const auto lines = countNewlines(text);
    auto cursorAfter = cursor_;
    cursorAfter.start.y = std::min(lines - 1, cursorAfter.start.y);
    cursorAfter.end.y = std::min(lines - 1, cursorAfter.end.y);
    // It's okay if x is large
    auto action = TextAction { this, 0, text_.getString(), std::move(text), cursor_, cursorAfter };
    actions_.perform(std::move(action));
}

bool Buffer::reload()
{
    assert(!path.empty());
    const auto data = readFile(path.c_str());
    if (!data)
        return false;
    setTextUndoable(std::move(*data));
    savedVersionId_ = actions_.getCurrentVersionId();
    lastModTime_ = fs::last_write_time(path);
    return true;
}

bool Buffer::canSave() const
{
    assert(!path.empty());
    if (!fs::exists(path))
        return true;

    return lastModTime_ >= fs::last_write_time(path);
}

Result<std::monostate> Buffer::save()
{
    assert(!path.empty());

    if (Config::get().trimTrailingWhitespaceOnSave)
        setTextUndoable(trimTrailingWhitespace(text_.getString()));

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        debug("Could not open file");
        return errnoError();
    }
    const auto data = text_.getString();
    if (std::fwrite(data.c_str(), 1, data.size(), f) != data.size()) {
        debug("Could not write to file");
        return errnoError();
    }
    if (std::fclose(f) != 0) {
        debug("Could not close file");
        return errnoError();
    }
    savedVersionId_ = actions_.getCurrentVersionId();
    lastModTime_ = fs::last_write_time(path);

    if (!fileModHandler_.isValid())
        watchFileModifications();

    return success();
}

bool Buffer::rename(const fs::path& newPath)
{
    if (::rename(path.c_str(), newPath.c_str()) != 0) {
        return false;
    }
    setPath(newPath);
    return true;
}

bool Buffer::isModified() const
{
    return actions_.getCurrentVersionId() != savedVersionId_;
}

std::string Buffer::getTitle() const
{
    std::string title;
    const auto pathStr = std::string(path);
    title.reserve(name.size() + pathStr.size() + 16);

    if (isModified())
        title.push_back('*');

    if (readOnly_)
        title.append("[ro] ");

    if (path.empty())
        title.append(name);
    else
        title.append(fmt::format("{} ({})", name, pathStr));

    return title;
}

void Buffer::setLanguage(const Language* lang)
{
    language_ = lang ? lang : &languages::plainText;
    if (language_->highlighter)
        highlighting_ = std::make_unique<Highlighting>(*language_->highlighter);
    else
        highlighting_ = nullptr;
}

const Language* Buffer::getLanguage() const
{
    return language_;
}

void Buffer::setReadOnly(bool readOnly)
{
    readOnly_ = readOnly;
    savedVersionId_ = actions_.getCurrentVersionId();
}

bool Buffer::getReadOnly() const
{
    return readOnly_;
}

void Buffer::updateHighlighting()
{
    if (highlighting_)
        highlighting_->update(text_);
}

const Highlighting* Buffer::getHighlighting() const
{
    return highlighting_.get();
}

void Buffer::TextAction::perform() const
{
    buffer->text_.remove(Range { offset, textBefore.size() });
    buffer->text_.insert(offset, textAfter);
    buffer->cursor_ = cursorAfter;
}

void Buffer::TextAction::undo() const
{
    buffer->text_.remove(Range { offset, textAfter.size() });
    buffer->text_.insert(offset, textBefore);
    buffer->cursor_ = cursorBefore;
}

bool Buffer::shouldMerge(const TextAction& action) const
{
    if (actions_.getSize() == 0)
        return false;

    const auto& top = actions_.getTop();

    const bool isInsertion = action.textAfter.size() > 0;
    if (isInsertion) {
        // insert single char right after last insert and it's not
        // whitespace (we want a separate undo after words or lines)
        return action.textAfter.size() == 1 && !std::isspace(action.textAfter[0])
            && action.offset == top.offset + 1;
    } else { // deletion
        // same as insertion, but not just after, also before
        // also don't merge with deletions that are bigger than 1 char
        return action.textBefore.size() == 1 && !std::isspace(action.textBefore[0])
            && (action.offset == top.offset || action.offset + 1 == top.offset)
            && top.textBefore.size() == 1;
    }
}

void Buffer::performAction(std::string_view text, const Cursor& cursorAfter)
{
    auto action = TextAction { this, getCursorOffset(cursor_.min()), getSelectionString(),
        std::string(text), cursor_, cursorAfter };
    actions_.perform(std::move(action), shouldMerge(action));
}

void Buffer::insert(std::string_view str)
{
    if (readOnly_)
        return;

    // If you select text and start typing, I want a separate undo action for deleting the selection
    if (!cursor_.emptySelection() && str.size() == 1)
        deleteSelection();

    auto cursorAfter = cursor_;
    const auto newLines = std::count(str.cbegin(), str.cend(), '\n');
    cursorAfter.setY(cursorAfter.start.y + newLines, false);
    if (newLines > 0) {
        const auto nl = str.rfind('\n');
        assert(nl != std::string_view::npos);
        cursorAfter.setX(str.size() - nl - 1, false);
    } else {
        cursorAfter.setX(getCursorX(cursorAfter.start) + str.size(), false);
    }
    performAction(str, cursorAfter);
}

void Buffer::deleteSelection()
{
    if (readOnly_)
        return;

    assert(!cursor_.emptySelection());
    auto cursorAfter = cursor_;
    cursorAfter.set(cursor_.min());
    performAction("", cursorAfter);
}

void Buffer::deleteBackwards()
{
    if (readOnly_)
        return;

    if (text_.getSize() == 0)
        return;

    const auto cursorBefore = cursor_;
    if (cursor_.emptySelection()) {
        const auto line = text_.getLine(cursor_.start.y);
        const auto cursorOffset = getCursorOffset(cursor_.start);
        const auto cursorLineOffset = cursorOffset - line.offset;

        if (cursorLineOffset == 0 || text_[cursorOffset - 1] == '\t') {
            moveCursorLeft(true);
        } else {
            const auto lineStr = text_.getString(line);
            const auto [numIndentBytes, indentWidth] = getIndentWidth(lineStr, tabWidth);
            const auto insideIndentation = cursorLineOffset <= numIndentBytes;
            if (insideIndentation) {
                assert(cursorLineOffset > 0);
                assert(text_[cursorOffset - 1] == ' ');
                const auto deleteNum = indentWidth % indentation.width > 0
                    ? indentWidth % indentation.width
                    : indentation.width;
                for (size_t i = 0; i < deleteNum; ++i)
                    moveCursorLeft(true);
            } else {
                moveCursorLeft(true);
            }
        }
    }
    deleteSelection();
    actions_.getTop().cursorBefore = cursorBefore;
}

void Buffer::deleteForwards()
{
    if (readOnly_)
        return;

    if (text_.getSize() == 0)
        return;

    const auto cursorBefore = cursor_;
    if (cursor_.emptySelection())
        moveCursorRight(true);
    deleteSelection();
    actions_.getTop().cursorBefore = cursorBefore;
}

void Buffer::insertNewline()
{
    if (readOnly_)
        return;

    const auto lineStr = text_.getString(text_.getLine(cursor_.min().y));
    const auto indentWidth = getIndentWidth(lineStr, tabWidth);
    insert("\n"s + lineStr.substr(0, indentWidth.first));
}

void Buffer::duplicateSelection()
{
    if (readOnly_)
        return;

    if (cursor_.emptySelection()) {
        const auto line = text_.getLine(cursor_.min().y);
        const auto lineStr = text_.getString(line);
        auto cursorAfter = cursor_;
        cursorAfter.setY(cursor_.min().y + 1);
        auto action = TextAction { this, line.offset + line.length + 1, "", lineStr + "\n", cursor_,
            cursorAfter };
        actions_.perform(std::move(action), false);
    } else {
        auto action = TextAction { this, getCursorOffset(cursor_.max()) + 1, "",
            getSelectionString(), cursor_, cursor_ };
        actions_.perform(std::move(action), false);
    }
}

void Buffer::indent()
{
    if (readOnly_)
        return;

    const auto indentStr = indentation.getString();
    // If multiple lines are selected, insert an indent string at the start of each
    if (!cursor_.emptySelection() && countNewlines(getSelectionString()) > 0) {
        const auto firstLine = cursor_.min().y;
        const auto lastLine = cursor_.max().y;
        auto cursorAfter = Cursor {
            { cursor_.start.x + indentStr.size(), cursor_.start.y },
            { cursor_.end.x + indentStr.size(), cursor_.end.y },
        };
        for (size_t l = firstLine; l < lastLine + 1; ++l) {
            // I'm really not sure how to handle the cursor, so I just change it with the last
            // action/insertion
            actions_.perform(TextAction { this, text_.getLine(l).offset, "", indentStr, cursor_,
                                 l == lastLine ? cursorAfter : cursor_ },
                l > firstLine);
        }
        return;
    }

    if (indentation.type == Indentation::Type::Tabs) {
        insert("\t");
        return;
    }

    assert(indentation.type == Indentation::Type::Spaces);
    const auto line = text_.getLine(cursor_.start.y);
    const auto [numIndentBytes, indentWidth] = getIndentWidth(text_.getString(line), tabWidth);

    const auto cursorOffset = getCursorOffset(cursor_.start);
    const auto cursorLineOffset = cursorOffset - line.offset;

    // If you are before any text (inside the indentation) indent to next tab stop
    const auto insideIndentation = cursorLineOffset <= numIndentBytes;
    if (insideIndentation) {
        const auto targetIndent
            = indentWidth + indentation.width - (indentWidth % indentation.width);
        insert(std::string(targetIndent - indentWidth, ' '));
    } else {
        // TODO: do this properly (count all tabs properly and also count code points)
        insert(indentStr);
    }
}

std::string Buffer::getLineDedent(std::string_view line) const
{
    const auto indentChar = line[0];
    if (indentChar == '\t')
        return std::string(line.substr(0, 1));

    if (indentChar == ' ') {
        size_t leadingSpaces = 1;
        while (
            leadingSpaces < std::min(line.size(), indentation.width) && line[leadingSpaces] == ' ')
            leadingSpaces++;
        return std::string(line.substr(0, leadingSpaces));
    }

    // Don't remove anything
    return std::string("");
}

void Buffer::dedent()
{
    if (readOnly_)
        return;

    const auto firstLine = cursor_.min().y;
    const auto lastLine = cursor_.max().y;
    auto cursorAfter = cursor_;
    for (size_t l = firstLine; l < lastLine + 1; ++l) {
        const auto line = text_.getLine(l);
        const auto lineStr = text_.getString(line);
        auto textBefore = getLineDedent(lineStr);
        if (l == cursorAfter.start.y)
            cursorAfter.start.x = subClamp(cursorAfter.start.x, textBefore.size());
        if (l == cursorAfter.end.y)
            cursorAfter.end.x = subClamp(cursorAfter.end.x, textBefore.size());
        actions_.perform(TextAction { this, line.offset, std::move(textBefore), "", cursor_,
                             l == lastLine ? cursorAfter : cursor_ },
            l > firstLine);
    }
}

size_t Buffer::getCursorX(const Cursor::End& cursorEnd) const
{
    // Treat cursor past the end of the line as positioned at the end of the line
    return std::min(text_.getLine(cursorEnd.y).length, cursorEnd.x);
}

size_t Buffer::getCursorOffset(const Cursor::End& cursorEnd) const
{
    return text_.getLine(cursorEnd.y).offset + getCursorX(cursorEnd);
}

Range Buffer::getSelection() const
{
    auto startOffset = getCursorOffset(cursor_.start);
    auto endOffset = getCursorOffset(cursor_.end);
    if (endOffset < startOffset)
        std::swap(startOffset, endOffset);
    return Range { startOffset, endOffset - startOffset };
}

std::string Buffer::getSelectionString() const
{
    return text_.getString(getSelection());
}

void Buffer::select(const Range& range)
{
    const auto end = range.offset + range.length;
    const auto startLineIndex = text_.getLineIndex(range.offset);
    const auto startLine = text_.getLine(startLineIndex);
    const auto endLineIndex = text_.getLineIndex(end);
    const auto endLine = text_.getLine(text_.getLineIndex(end));
    assert(range.offset >= startLine.offset);
    cursor_.start = Vec { range.offset - startLine.offset, startLineIndex };
    cursor_.end = Vec { end - endLine.offset, endLineIndex };
}

void Buffer::moveCursorBol(bool select)
{
    cursor_.setX(0, select);
}

void Buffer::moveCursorEol(bool select)
{
    cursor_.setX(Cursor::EndOfLine, select);
}

void Buffer::moveCursorRight(bool select)
{
    debug("move cursor right");

    // This is the only combination of emptySelection and select that needs special handling
    if (!cursor_.emptySelection() && !select) {
        cursor_.set(cursor_.max());
        return;
    }

    const auto line = text_.getLine(cursor_.start.y);

    // end of document
    if (cursor_.start.y == text_.getLineCount() - 1 && cursor_.start.x == line.length - 1)
        return;

    // Skip one newline if it's there
    if (text_[line.offset + getCursorX(cursor_.start)] == '\n') {
        moveCursorY(1, select);
        cursor_.setX(0, select);
        debug("skip newline");
        return;
    }
    // If cursorX > line.length the condition above should have been true
    assert(cursor_.start.x <= line.length);

    // multi-byte code point
    const auto ch = text_[line.offset + cursor_.start.x];
    if (!utf8::isAscii(ch)) {
        const auto cpLen
            = utf8::getCodePointLength(text_, text_.getSize(), line.offset + cursor_.start.x);
        cursor_.setX(cursor_.start.x + cpLen, select);
        debug("skipped utf8: cursorX = {}", cursor_.start.x);
        return;
    }

    // single code unit of utf8 or ascii
    if (cursor_.start.x < line.length) {
        cursor_.setX(cursor_.start.x + 1, select);
        debug("skipped ascii: cursorX = {}", cursor_.start.x);
    }
}

namespace {
enum class WordType { AlphaNum, Whitespace, SpecialChars };

WordType getWordType(char ch)
{
    if (std::isspace(ch))
        return WordType::Whitespace;
    if (ch == '_')
        return WordType::AlphaNum; // for snake_case identifiers
    if (ch > 0 && !(std::iscntrl(ch) || std::isalnum(ch)))
        return WordType::SpecialChars;
    return WordType::AlphaNum;
}
}

// We are doing things a little differently and go to the start of a word (in walking direction)
void Buffer::moveCursorWordLeft(bool select)
{
    auto off = getCursorOffset(cursor_.start);
    if (off == 0)
        return;

    const auto wordType = getWordType(text_[off - 1]);
    while (off > 0 && getWordType(text_[off - 1]) == wordType) {
        moveCursorLeft(select);
        off = getCursorOffset(cursor_.start);
    }
}

void Buffer::moveCursorWordRight(bool select)
{
    auto off = getCursorOffset(cursor_.start);
    if (off == text_.getSize())
        return;

    const auto wordType = getWordType(text_[off]);
    while (off < text_.getSize() && getWordType(text_[off]) == wordType) {
        moveCursorRight(select);
        off = getCursorOffset(cursor_.start);
    }
}

void Buffer::moveCursorLeft(bool select)
{
    if (!cursor_.emptySelection() && !select) {
        cursor_.set(cursor_.min());
        return;
    }

    const auto line = text_.getLine(cursor_.start.y);

    if (cursor_.start.x > line.length) {
        cursor_.setX(line.length, select);
    }

    if (cursor_.start.x == 0) {
        if (cursor_.start.y > 0) {
            moveCursorY(-1, select);
            cursor_.setX(text_.getLine(cursor_.start.y).length, select);
        }
        // do nothing
        return;
    }

    // Skip all utf8 continuation bytes (0xb10XXXXXX)
    auto isContinuation = [this](size_t idx) {
        return (static_cast<uint8_t>(text_[idx]) & 0b11000000) == 0b10000000;
    };
    while (cursor_.start.x > 0 && isContinuation(line.offset + cursor_.start.x - 1)) {
        cursor_.setX(cursor_.start.x - 1, select);
    }

    // First byte of a code point is either 0XXXXXXX, 110XXXXX, 1110XXXX or 11110XXX
    if (cursor_.start.x > 0)
        cursor_.setX(cursor_.start.x - 1, select);
}

void Buffer::moveCursorY(int dy, bool select)
{
    assert(dy != 0);
    if (!cursor_.emptySelection() && !select) {
        cursor_.set(dy > 0 ? cursor_.max() : cursor_.min());
    }

    if (dy > 0) {
        cursor_.setY(std::min(text_.getLineCount() - 1, cursor_.start.y + dy), select);
    } else if (dy < 0) {
        if (cursor_.start.y >= static_cast<size_t>(-dy))
            cursor_.setY(cursor_.start.y + dy, select);
        else
            cursor_.setY(0, select);
    }
}

void Buffer::moveCursorBof(bool select)
{
    cursor_.set({ 0, 0 }, select);
}

void Buffer::moveCursorEof(bool select)
{
    const auto lineIndex = text_.getLineCount() - 1;
    const auto line = text_.getLine(lineIndex);
    cursor_.set({ line.length - 1, lineIndex }, select);
}

void Buffer::scroll(size_t terminalHeight)
{
    if (cursor_.start.y < scroll_)
        scroll_ = cursor_.start.y;
    else if (cursor_.start.y - scroll_ >= terminalHeight)
        scroll_ = std::min(text_.getLineCount() - 1, cursor_.start.y - terminalHeight + 1);
}

bool Buffer::undo()
{
    return actions_.undo();
}

bool Buffer::redo()
{
    return actions_.redo();
}
