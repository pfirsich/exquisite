#include "buffer.hpp"

#include <cassert>
#include <tuple>

#include "debug.hpp"
#include "utf8.hpp"

TextBuffer::TextBuffer()
{
    lineOffsets_.push_back(0);
}

TextBuffer::TextBuffer(std::string_view str)
{
    set(str);
}

size_t TextBuffer::getSize() const
{
    return data_.size();
}

char TextBuffer::operator[](size_t offset) const
{
    return data_[offset];
}

std::string TextBuffer::getString() const
{
    return std::string(data_.data(), data_.size());
}

// TODO: Custom iterator
// (Pointer, length) tuples
std::vector<std::string_view> TextBuffer::getStrings(const Range& range)
{
    assert(range.offset < getSize());
    std::vector<std::string_view> strs;
    const char* ptr = data_.data() + range.offset;
    const size_t length = std::min(range.length, data_.size() - range.offset);
    strs.push_back(std::string_view(ptr, length));
    return strs;
}

void TextBuffer::set(std::string_view str)
{
    data_ = std::vector<char>(str.data(), str.data() + str.size());
    updateLineOffsets();
}

void TextBuffer::updateLineOffsets()
{
    lineOffsets_.clear();
    lineOffsets_.push_back(0);
    for (size_t i = 0; i < data_.size(); ++i) {
        if (data_[i] == '\n')
            lineOffsets_.push_back(i + 1);
    }
    assert(checkLineOffsets());
}

bool TextBuffer::checkLineOffsets() const
{
    if (lineOffsets_[0] != 0) {
        return false;
    }
    size_t offsetIndex = 1;
    for (size_t i = 0; i < getSize(); ++i) {
        const auto ch = operator[](i);
        if (ch == '\n') {
            if (lineOffsets_[offsetIndex] != i + 1) {
                return false;
            }
            offsetIndex++;
        }
    }
    return offsetIndex == lineOffsets_.size();
}

void TextBuffer::insert(size_t offset, std::string_view str)
{
    data_.insert(data_.begin() + offset, str.data(), str.data() + str.size());

    auto line = getLineIndex(offset) + 1;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n') {
            lineOffsets_.insert(lineOffsets_.begin() + line, offset + i + 1);
            line++;
        }
    }

    // Move all line offsets after the new ones str.size() forward
    while (line < lineOffsets_.size()) {
        lineOffsets_[line] += str.size();
        line++;
    }
    assert(checkLineOffsets());
}

void TextBuffer::remove(const Range& range)
{
    const auto first = data_.begin() + range.offset;
    data_.erase(first, first + range.length);

    // For all lines after the one that contains range.offset, move their offsets back.
    // If any of them moved in front of range.offset, they have been removed
    // This function looks like shit with iterators, so I will use indices
    const auto line = getLineIndex(range.offset);
    for (size_t l = getLineCount() - 1; l > line; --l) {
        lineOffsets_[l] -= range.length;
        if (lineOffsets_[l] <= range.offset) {
            lineOffsets_.erase(lineOffsets_.begin() + l);
        }
    }
    assert(checkLineOffsets());
}

size_t TextBuffer::getLineCount() const
{
    return lineOffsets_.size();
}

Range TextBuffer::getLine(LineIndex idx) const
{
    assert(idx < lineOffsets_.size());
    const auto offset = lineOffsets_[idx];
    const auto length = idx == lineOffsets_.size() - 1
        ? data_.size() - offset
        : lineOffsets_[idx + 1] - offset - 1; // -1 so we don't count \n
    return Range { offset, length };
}

TextBuffer::LineIndex TextBuffer::getLineIndex(size_t offset) const
{
    assert(offset < getSize());
    for (LineIndex i = 0; i < lineOffsets_.size() - 1; ++i)
        if (offset < lineOffsets_[i + 1])
            return i;
    return lineOffsets_.size() - 1;
}

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

void Cursor::set(const End& pos)
{
    start = pos;
    end = pos;
}

///////////////////////////////////////////// Buffer

void Buffer::insert(std::string_view str)
{
    if (!cursor.emptySelection())
        deleteSelection();
    assert(cursor.emptySelection());
    const auto lineCount = text.getLineCount();
    text.insert(getOffset(cursor.start), str);
    const auto newLines = text.getLineCount() - lineCount;
    cursor.setY(cursor.start.y + newLines, false);
    if (newLines > 0) {
        const auto nl = str.rfind('\n');
        assert(nl != std::string_view::npos);
        cursor.setX(str.size() - nl - 1, false);
    } else {
        cursor.setX(getX(cursor.start) + str.size(), false);
    }
}

void Buffer::deleteSelection()
{
    assert(!cursor.emptySelection());
    text.remove(getSelection());
    cursor.set(cursor.min());
}

void Buffer::deleteBackwards()
{
    if (cursor.emptySelection())
        moveCursorLeft(true);
    deleteSelection();
}

void Buffer::deleteForwards()
{
    if (cursor.emptySelection())
        moveCursorRight(true);
    deleteSelection();
}

size_t Buffer::getX(const Cursor::End& cursorEnd) const
{
    // Treat cursor past the end of the line as positioned at the end of the line
    return std::min(text.getLine(cursorEnd.y).length, cursorEnd.x);
}

size_t Buffer::getOffset(const Cursor::End& cursorEnd) const
{
    return text.getLine(cursorEnd.y).offset + getX(cursorEnd);
}

Range Buffer::getSelection() const
{
    auto startOffset = getOffset(cursor.start);
    auto endOffset = getOffset(cursor.end);
    if (endOffset < startOffset)
        std::swap(startOffset, endOffset);
    return Range { startOffset, endOffset - startOffset };
}

void Buffer::moveCursorHome(bool select)
{
    cursor.setX(0, select);
}

void Buffer::moveCursorEnd(bool select)
{
    cursor.setX(Cursor::EndOfLine, select);
}

void Buffer::moveCursorRight(bool select)
{
    debug("move cursor right");

    // This is the only combination of emptySelection and select that needs special handling
    if (!cursor.emptySelection() && !select) {
        cursor.set(cursor.max());
        return;
    }

    const auto line = text.getLine(cursor.start.y);

    // end of document
    if (cursor.start.y == text.getLineCount() - 1 && cursor.start.x == line.length - 1)
        return;

    // Skip one newline if it's there
    if (text[line.offset + getX(cursor.start)] == '\n') {
        moveCursorY(1, select);
        cursor.setX(0, select);
        debug("skip newline");
        return;
    }
    // If cursorX > line.length the condition above should have been true
    assert(cursor.start.x <= line.length);

    // multi-byte code point
    const auto ch = text[line.offset + cursor.start.x];
    if (!utf8::isAscii(ch)) {
        const auto cpLen
            = utf8::getCodePointLength(text, text.getSize(), line.offset + cursor.start.x);
        cursor.setX(cursor.start.x + cpLen, select);
        debug("skipped utf8: cursorX = ", cursor.start.x);
        return;
    }

    // single code unit of utf8 or ascii
    if (cursor.start.x < line.length) {
        cursor.setX(cursor.start.x + 1, select);
        debug("skipped ascii: cursorX = ", cursor.start.x);
    }
}

void Buffer::moveCursorLeft(bool select)
{
    if (!cursor.emptySelection() && !select) {
        cursor.set(cursor.min());
        return;
    }

    const auto line = text.getLine(cursor.start.y);

    if (cursor.start.x > line.length) {
        cursor.setX(line.length, select);
    }

    if (cursor.start.x == 0) {
        if (cursor.start.y > 0) {
            moveCursorY(-1, select);
            cursor.setX(text.getLine(cursor.start.y).length, select);
        }
        // do nothing
        return;
    }

    // Skip all utf8 continuation bytes (0xb10XXXXXX)
    auto isContinuation = [this](size_t idx) {
        return (static_cast<uint8_t>(text[idx]) & 0b11000000) == 0b10000000;
    };
    while (cursor.start.x > 0 && isContinuation(line.offset + cursor.start.x - 1)) {
        cursor.setX(cursor.start.x - 1, select);
    }

    // First byte of a code point is either 0XXXXXXX, 110XXXXX, 1110XXXX or 11110XXX
    if (cursor.start.x > 0)
        cursor.setX(cursor.start.x - 1, select);
}

void Buffer::moveCursorY(int dy, bool select)
{
    debug("move cursor y ", dy);
    if (dy > 0) {
        cursor.setY(std::min(text.getLineCount() - 1, cursor.start.y + dy), select);
    } else if (dy < 0) {
        if (cursor.start.y >= static_cast<size_t>(-dy))
            cursor.setY(cursor.start.y + dy, select);
        else
            cursor.setY(0, select);
    }
    debug("cursor: ", cursor.start.x, ", ", cursor.start.y);
}

void Buffer::scroll(size_t terminalHeight)
{
    if (cursor.start.y < scrollY)
        scrollY = cursor.start.y;
    else if (cursor.start.y - scrollY > terminalHeight)
        scrollY = std::min(text.getLineCount() - 1, cursor.start.y - terminalHeight);
}
