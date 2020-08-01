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

std::pair<Cursor::End, Cursor::End> Cursor::sorted() const
{
    if (start == end)
        return std::pair(start, end);
    if (start.y < end.y)
        return std::pair(start, end);
    if (end.y < start.y)
        return std::pair(end, start);
    // start.y == end.y, start.x != start.x
    if (start.x < end.x)
        return std::pair(start, end);
    return std::pair(end, start);
}

void Cursor::setX(size_t x)
{
    start.x = x;
    end.x = x;
}

void Cursor::setY(size_t y)
{
    start.y = y;
    end.y = y;
}

///////////////////////////////////////////// Buffer

void Buffer::insert(std::string_view str)
{
    assert(cursor.emptySelection());
    const auto lineCount = text.getLineCount();
    text.insert(getOffset(cursor.start), str);
    const auto newLines = text.getLineCount() - lineCount;
    cursor.setY(cursor.start.y + newLines);
    if (newLines > 0) {
        const auto nl = str.rfind('\n');
        assert(nl != std::string_view::npos);
        cursor.setX(str.size() - nl - 1);
    } else {
        cursor.setX(getX(cursor.start) + str.size());
    }
}

void Buffer::deleteBackwards()
{
    assert(cursor.emptySelection());
    // This seems like an easy solution, but really fucking dumb
    moveCursorLeft();
    deleteForwards();
}

void Buffer::deleteForwards()
{
    assert(cursor.emptySelection());
    // The cursor can stay where it is
    text.remove(Range { getOffset(cursor.start), 1 });
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

Range Buffer::getCursorRange() const
{
    auto startOffset = getOffset(cursor.start);
    auto endOffset = getOffset(cursor.end);
    if (endOffset < startOffset)
        std::swap(startOffset, endOffset);
    return Range { startOffset, endOffset - startOffset };
}

void Buffer::moveCursorHome()
{
    assert(cursor.emptySelection());
    cursor.setX(0);
}

void Buffer::moveCursorEnd()
{
    assert(cursor.emptySelection());
    cursor.setX(Cursor::EndOfLine);
}

void Buffer::moveCursorRight()
{
    assert(cursor.emptySelection());
    debug("move cursor right");
    const auto line = text.getLine(cursor.start.y);

    // end of document
    if (cursor.start.y == text.getLineCount() - 1 && cursor.start.x == line.length - 1)
        return;

    // Skip one newline if it's there
    if (text[line.offset + getX(cursor.start)] == '\n') {
        moveCursorY(1);
        cursor.setX(0);
        debug("skip newline");
        return;
    }
    // If cursorX > line.length the condition above should have been true
    assert(cursor.start.x <= line.length);

    // multi-code unit code point
    const auto ch = text[line.offset + cursor.start.x];
    if (ch < 0) {
        cursor.setX(cursor.start.x + utf8::getByteSequenceLength(static_cast<uint8_t>(ch)));
        debug("skipped utf8: cursorX = ", cursor.start.x);
        return;
    }

    // single code unit of utf8 or ascii
    if (cursor.start.x < line.length) {
        cursor.setX(cursor.start.x + 1);
        debug("skipped ascii: cursorX = ", cursor.start.x);
    }
}

void Buffer::moveCursorLeft()
{
    assert(cursor.emptySelection());
    const auto line = text.getLine(cursor.start.y);

    if (cursor.start.x > line.length) {
        cursor.setX(line.length);
    }

    if (cursor.start.x == 0) {
        if (cursor.start.y > 0) {
            moveCursorY(-1);
            cursor.setX(text.getLine(cursor.start.y).length);
        }
        // do nothing
        return;
    }

    // Skip all utf8 continuation bytes (0xb10XXXXXX)
    auto isContinuation = [this](size_t idx) {
        return (static_cast<uint8_t>(text[idx]) & 0b11000000) == 0b10000000;
    };
    while (cursor.start.x > 0 && isContinuation(line.offset + cursor.start.x - 1)) {
        cursor.setX(cursor.start.x - 1);
    }

    // First byte of a code point is either 0XXXXXXX, 110XXXXX, 1110XXXX or 11110XXX
    if (cursor.start.x > 0)
        cursor.setX(cursor.start.x - 1);
}

void Buffer::moveCursorY(int dy)
{
    assert(cursor.emptySelection());
    debug("move cursor y ", dy);
    if (dy > 0) {
        cursor.setY(std::min(text.getLineCount() - 1, cursor.start.y + dy));
    } else if (dy < 0) {
        if (cursor.start.y >= static_cast<size_t>(-dy))
            cursor.setY(cursor.start.y + dy);
        else
            cursor.setY(0);
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
