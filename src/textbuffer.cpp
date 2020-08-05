#include "textbuffer.hpp"

#include <cassert>

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

std::string TextBuffer::getString(const Range& range) const
{
    assert(range.offset + range.length <= data_.size());
    return std::string(data_.data() + range.offset, range.length);
}

std::string TextBuffer::getString() const
{
    return std::string(data_.data(), data_.size());
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
        // We need the second condition, because if you delete a line right until it's end,
        // you delete the newline that makes the next line a line!
        // So you need to delete the next line as well.
        if (range.contains(lineOffsets_[l]) || lineOffsets_[l] == range.offset + range.length)
            lineOffsets_.erase(lineOffsets_.begin() + l);
        else
            lineOffsets_[l] -= range.length;
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
    assert(offset <= getSize());
    for (LineIndex i = 0; i < lineOffsets_.size() - 1; ++i)
        if (offset < lineOffsets_[i + 1])
            return i;
    return lineOffsets_.size() - 1;
}
