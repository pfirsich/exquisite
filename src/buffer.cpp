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

    lineOffsets_.clear();
    lineOffsets_.push_back(0);
    for (size_t i = 0; i < data_.size(); ++i) {
        if (data_[i] == '\n')
            lineOffsets_.push_back(i + 1);
    }
}

void TextBuffer::insert(size_t offset, std::string_view str)
{
    data_.insert(data_.begin() + offset, str.data(), str.data() + str.size());

    std::vector<size_t> newLines;
    newLines.reserve(32); // Allocate once
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n')
            newLines.push_back(i);
    }
    if (!newLines.empty()) {
        // Insert new line offset for each newline found
        auto line = lineOffsets_.begin() + getLineIndex(offset) + 1;
        for (size_t i = 0; i < newLines.size(); ++i) {
            lineOffsets_.insert(line, offset + newLines[i] + 1);
            line++;
        }

        // Move all line offsets after the new ones str.size() forward
        while (line != lineOffsets_.end()) {
            *line += str.size();
            line++;
        }
    }
}

void TextBuffer::append(std::string_view str)
{
    const auto size = data_.size();
    data_.insert(data_.end(), str.data(), str.data() + str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n')
            lineOffsets_.push_back(size + i + 1);
    }
}

void TextBuffer::remove(const Range& range)
{
    const auto first = data_.begin() + range.offset;
    data_.erase(first, first + (range.length - 1));

    // TODO: LINES!!!!
}

size_t TextBuffer::getLineCount() const
{
    return lineOffsets_.size();
}

TextBuffer::Range TextBuffer::getLine(LineIndex idx) const
{
    assert(idx < lineOffsets_.size());
    const auto offset = lineOffsets_[idx];
    const auto nextOffset = idx == lineOffsets_.size() - 1 ? data_.size() : lineOffsets_[idx + 1];
    return Range { offset, nextOffset - offset };
}

TextBuffer::LineIndex TextBuffer::getLineIndex(size_t offset) const
{
    assert(offset < getSize());
    for (LineIndex i = 0; i < lineOffsets_.size() - 1; ++i)
        if (offset < lineOffsets_[i + 1])
            return i;
    return lineOffsets_.size() - 1;
}

size_t Buffer::getCursorOffset() const
{
    assert(cursorY < text.getLineCount());
    const auto line = text.getLine(cursorY);
    return line.offset + std::min(line.length, cursorX);
}

void Buffer::moveCursorX(int dx)
{
    debug("move cursor x ", dx);
    assert(cursorY < text.getLineCount());
    const auto lineLength = text.getLine(cursorY).length;

    if (dx > 0) {
        if (cursorX + dx <= lineLength) {
            cursorX += dx;
        } else {
            // Don't move multiple lines because dx is large
            moveCursorY(1);
            cursorX = std::min(cursorX, lineLength) - lineLength + dx - 1;
        }
    } else if (dx < 0) {
        if (cursorX >= static_cast<size_t>(-dx)) {
            // If you move left and cursorX is > lineLength, pretend it was at the end
            cursorX = std::min(cursorX, lineLength) + dx;
        } else {
            if (cursorY > 0) {
                moveCursorY(-1);
                const auto length = text.getLine(cursorY).length;
                cursorX = std::max(0ul, length + dx + 1);
            }
        }
    }
    debug("cursor: ", cursorX, ", ", cursorY);
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
