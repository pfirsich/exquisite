#pragma once

#include <string_view>
#include <vector>

// TODO: Use a Rope: https://en.wikipedia.org/wiki/Rope_(data_structure)
class TextBuffer {
public:
    using LineIndex = size_t;

    struct Range {
        size_t offset;
        size_t length;
    };

    TextBuffer();
    TextBuffer(std::string_view str);

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

struct Buffer {
    static constexpr auto EndOfLine = std::numeric_limits<size_t>::max();

    TextBuffer text;
    // cursorX may be any positive value (including EndOfLine) and will be considered to be at the
    // end of the line if it exceeds the line's length.
    // It is not clamped to the line length, so the x position is retained when moving up/down.
    size_t cursorX = 0;
    size_t cursorY = 0;
    size_t scrollY = 0;

    size_t getCursorOffset() const;

    void moveCursorX(int dx);
    void moveCursorY(int dy);
    void scroll(size_t terminalHeight);
};
