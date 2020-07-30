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
    void remove(const Range& range);

private:
    void updateLineOffsets();
    bool checkLineOffsets() const;

    std::vector<char> data_;
    std::vector<size_t> lineOffsets_;
};

struct Buffer {
    static constexpr auto EndOfLine = std::numeric_limits<size_t>::max();

    TextBuffer text;

    // cursorX will be considered to be at the end of the line if it exceeds the line's length.
    // It is not clamped to the line length, so the x position is retained when moving up/down.
    // It is in bytes from the start of the line, but always at the start of a code point.
    // It would be nicer if it was at the start of a glyph/grapheme, but that is hard (maybe font
    // dependent = impossible?).
    size_t cursorX = 0;
    size_t cursorY = 0; // in lines
    size_t scrollY = 0; // in lines

    size_t getCursorOffset() const;

    void insert(std::string_view str);
    void deleteBackwards(size_t num);
    void deleteForwards(size_t num);

    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorY(int dy);
    void scroll(size_t terminalHeight);
};
