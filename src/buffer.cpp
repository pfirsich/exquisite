#include "buffer.hpp"

#include <cassert>
#include <tuple>

// TODO: Implement this using a Rope later

Buffer::Buffer()
{
}

Buffer::Buffer(std::string_view str)
{
    set(str);
}

size_t Buffer::getSize() const
{
    return data_.size();
}

char Buffer::operator[](size_t offset) const
{
    return data_[offset];
}

// TODO: Custom iterator
// (Pointer, length) tuples
std::vector<std::string_view> Buffer::getStrings(const Range& range)
{
    assert(range.offset < getSize());
    std::vector<std::string_view> strs;
    const char* ptr = data_.data() + range.offset;
    const size_t length = std::min(range.length, data_.size() - range.offset);
    strs.push_back(std::string_view(ptr, length));
    return strs;
}

void Buffer::set(std::string_view str)
{
    data_ = std::vector<char>(str.data(), str.data() + str.size());

    lineOffsets_.clear();
    lineOffsets_.push_back(0);
    for (size_t i = 0; i < data_.size(); ++i) {
        if (data_[i] == '\n')
            lineOffsets_.push_back(i + 1);
    }
}

void Buffer::insert(size_t offset, std::string_view str)
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

void Buffer::append(std::string_view str)
{
    const auto size = data_.size();
    data_.insert(data_.end(), str.data(), str.data() + str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n')
            lineOffsets_.push_back(size + i + 1);
    }
}

void Buffer::remove(const Range& range)
{
    const auto first = data_.begin() + range.offset;
    data_.erase(first, first + (range.length - 1));

    // TODO: LINES!!!!
}

size_t Buffer::getLineCount() const
{
    return lineOffsets_.size();
}

Buffer::Range Buffer::getLine(LineIndex idx) const
{
    assert(idx < lineOffsets_.size());
    const auto offset = lineOffsets_[idx];
    const auto nextOffset = idx == lineOffsets_.size() - 1 ? data_.size() : lineOffsets_[idx + 1];
    return Range { offset, nextOffset - offset };
}

Buffer::LineIndex Buffer::getLineIndex(size_t offset) const
{
    assert(offset < getSize());
    for (LineIndex i = 0; i < lineOffsets_.size() - 1; ++i)
        if (offset < lineOffsets_[i + 1])
            return i;
    return lineOffsets_.size() - 1;
}
