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
    std::string getString() const;

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

// Cursor::x will be considered to be at the end of the line if it exceeds the line's length.
// It is not clamped to the line length, so the x position is retained when moving up/down.
// It is in bytes from the start of the line, but always at the start of a code point.
// It would be nicer if it was at the start of a glyph/grapheme, but that is hard (maybe font
// dependent = impossible?).
struct Cursor {
    static constexpr auto EndOfLine = std::numeric_limits<size_t>::max();
    using End = Vec;

    // start/end contain the start/end of a selection respectively
    // a cursor without (empty) a selection will have start == end.
    // start may be after end (if you select forwards).
    // start is the end you are moving with your arrow keys.
    End start;
    End end;

    bool emptySelection() const;
    std::pair<End, End> sorted() const;
    void setX(size_t x);
    void setY(size_t y);
};

struct Buffer {
    std::string name;

    TextBuffer text;

    Cursor cursor;

    size_t scrollY = 0; // in lines

    // This will return the cursorX position clamped to the line length
    size_t getX(const Cursor::End& cursorEnd) const;
    size_t getOffset(const Cursor::End& cursorEnd) const;
    Range getCursorRange() const;

    void insert(std::string_view str);
    void deleteBackwards();
    void deleteForwards();

    void moveCursorHome();
    void moveCursorEnd();
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorY(int dy);
    void scroll(size_t terminalHeight);
};
