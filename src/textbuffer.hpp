#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "util.hpp"

// TODO: Use a Rope: https://en.wikipedia.org/wiki/Rope_(data_structure)
class TextBuffer {
public:
    using LineIndex = size_t;

    TextBuffer();
    TextBuffer(std::string_view str);

    size_t getSize() const;

    char operator[](size_t offset) const;
    std::string getString(const Range& range) const;
    std::string getString() const;

    size_t getLineCount() const;
    Range getLine(LineIndex idx) const;
    LineIndex getLineIndex(size_t offset) const;

    void set(std::string_view str);
    void insert(size_t offset, std::string_view str);
    void remove(const Range& range);

private:
    void updateLineOffsets();
    bool checkLineOffsets() const;

    std::vector<char> data_;
    std::vector<size_t> lineOffsets_;
};
