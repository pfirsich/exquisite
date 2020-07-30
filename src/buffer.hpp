#pragma once

#include <string_view>
#include <vector>

class Buffer {
public:
    using LineIndex = size_t;

    struct Range {
        size_t offset;
        size_t length;
    };

    Buffer();
    Buffer(std::string_view str);

    size_t getSize() const;

    char operator[](size_t offset) const;

    size_t getLineCount() const;
    Range getLine(LineIndex idx) const;
    LineIndex getLineIndex(size_t offset) const;

    // TODO: Custom iterator
    // A buffer range may be split up in memory and going character by character may be inefficient
    std::vector<std::string_view> getStrings(const Range& range);

    void set(std::string_view str);
    void insert(size_t offset, std::string_view str);
    void append(std::string_view str);
    void remove(const Range& range);

private:
    std::vector<char> data_;
    std::vector<size_t> lineOffsets_;
};
