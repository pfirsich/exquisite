#include "buffer.hpp"

#include <cassert>
#include <tuple>

#include "debug.hpp"

TextBuffer::TextBuffer()
{
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
    debug("offsets:");
    for (const auto off : lineOffsets_)
        debug(off);

    if (lineOffsets_[0] != 0) {
        debug("first offset wrong");
        return false;
    }
    size_t offsetIndex = 1;
    for (size_t i = 0; i < getSize(); ++i) {
        const auto ch = operator[](i);
        if (ch == '\n') {
            if (lineOffsets_[offsetIndex] != i + 1) {
                debug("Expected offset ", offsetIndex, " to be ", i, ", but it's ",
                    lineOffsets_[offsetIndex]);
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

    auto line = lineOffsets_.begin() + getLineIndex(offset) + 1;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n')
            lineOffsets_.insert(line, offset + i);
    }

    // Move all line offsets after the new ones str.size() forward
    while (line != lineOffsets_.end()) {
        *line += str.size();
        line++;
    }
    assert(checkLineOffsets());
}

void TextBuffer::remove(const Range& range)
{
    debug("remove: offset = ", range.offset, ", length = ", range.length);
    const auto first = data_.begin() + range.offset;
    data_.erase(first, first + range.length);

    // For all lines after the one that contains range.offset, move their offsets back.
    // If any of them moved in front of range.offset, they have been removed
    // This function looks like shit with iterators, so I will use indices
    const auto line = getLineIndex(range.offset);
    for (size_t l = getLineCount() - 1; l > line; --l) {
        debug("move back: ", l);
        lineOffsets_[l] -= range.length;
        if (lineOffsets_[l] <= range.offset) {
            debug("delete: ", l);
            lineOffsets_.erase(lineOffsets_.begin() + l);
        }
    }
    assert(checkLineOffsets());
}

size_t TextBuffer::getLineCount() const
{
    return lineOffsets_.size();
}

TextBuffer::Range TextBuffer::getLine(LineIndex idx) const
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

void Buffer::insert(std::string_view str)
{
    const auto lineCount = text.getLineCount();
    text.insert(getCursorOffset(), str);
    const auto newLines = text.getLineCount() - lineCount;
    cursorY += newLines;
    if (newLines > 0) {
        const auto nl = str.rfind('\n');
        assert(nl != std::string_view::npos);
        cursorX = str.size() - nl - 1;
    } else {
        cursorX += str.size();
    }
}

void Buffer::deleteBackwards(size_t num)
{
    // This seems like an easy solution, but really fucking dumb
    for (size_t i = 0; i < num; ++i)
        moveCursorLeft();
    text.remove(TextBuffer::Range { getCursorOffset(), num });
}

void Buffer::deleteForwards(size_t num)
{
    // The cursor can stay where it is
    text.remove(TextBuffer::Range { getCursorOffset(), num });
}

size_t Buffer::getCursorOffset() const
{
    assert(cursorY < text.getLineCount());
    const auto line = text.getLine(cursorY);
    return line.offset + std::min(line.length, cursorX);
}

void Buffer::moveCursorRight()
{
    debug("move cursor right");
    assert(cursorY < text.getLineCount());
    const auto line = text.getLine(cursorY);

    // Skip one newline if it's there
    if (text[line.offset + cursorX] == '\n') {
        moveCursorY(1);
        cursorX = 0;
        debug("skip newline: cursorX = ", cursorX);
        return;
    }

    // multi-code unit code point
    if (text[line.offset + cursorX] > 0x7f) {
        while (cursorX < line.length && text[line.offset + cursorX] > 0x7f)
            cursorX++;
        debug("skipped utf8: cursorX = ", cursorX);
        return;
    }

    // single code unit of utf8 or ascii
    if (cursorX < line.length)
        cursorX++;
    debug("skipped ascii: cursorX = ", cursorX);
}

void Buffer::moveCursorLeft()
{
    assert(cursorY < text.getLineCount());

    if (cursorX == 0) {
        if (cursorY > 0) {
            moveCursorY(-1);
            cursorX = text.getLine(cursorY).length;
        }
        // do nothing
        return;
    }

    const auto line = text.getLine(cursorY);

    // Skip all utf8 continuation bytes (0xb10XXXXXX)
    while (cursorX > 0 && (text[line.offset + cursorX] & 0b11000000) == 0b10000000)
        cursorX--;

    // First byte of a code point is either 0XXXXXXX, 110XXXXX, 1110XXXX or 11110XXX
    if (cursorX > 0)
        cursorX--;
}

void Buffer::moveCursorY(int dy)
{
    debug("move cursor y ", dy);
    if (dy > 0) {
        cursorY = std::min(text.getLineCount() - 1, cursorY + dy);
    } else if (dy < 0) {
        if (cursorY >= static_cast<size_t>(-dy))
            cursorY += dy;
        else
            cursorY = 0;
    }
    debug("cursor: ", cursorX, ", ", cursorY);
}

void Buffer::scroll(size_t terminalHeight)
{
    if (cursorY < scrollY)
        scrollY = cursorY;
    else if (cursorY - scrollY > terminalHeight)
        scrollY = std::min(text.getLineCount() - 1, cursorY - terminalHeight);
    debug("new scroll: ", scrollY);
}
