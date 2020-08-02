#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "actionstack.hpp"
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
    End start = { 0, 0 };
    End end = { 0, 0 };

    bool emptySelection() const;
    bool isOrdered() const;
    End min() const;
    End max() const;

    void setX(size_t x, bool select);
    void setY(size_t y, bool select);
    void set(const End& pos);
};

class Buffer {
public:
    std::string name;

    const TextBuffer& getText() const;
    const Cursor& getCursor() const;
    size_t getScroll() const;

    // This will return the cursorX position clamped to the line length
    size_t getX(const Cursor::End& cursorEnd) const;
    size_t getOffset(const Cursor::End& cursorEnd) const;
    Range getSelection() const;

    void setText(std::string_view str);

    void insert(std::string_view str);
    void deleteSelection();
    void deleteBackwards();
    void deleteForwards();

    void moveCursorHome(bool select);
    void moveCursorEnd(bool select);
    void moveCursorLeft(bool select);
    void moveCursorRight(bool select);
    void moveCursorY(int dy, bool select);
    void scroll(size_t terminalHeight);

    bool undo();
    bool redo();

private:
    struct TextAction {
        Buffer* buffer;
        size_t offset;
        std::string textBefore;
        std::string textAfter;
        Cursor cursorBefore;
        Cursor cursorAfter;

        void perform();
        void undo();
        void merge(const TextAction& action);
    };

    void performAction(std::string_view text, const Cursor& cursorAfter);

    TextBuffer text_;
    ActionStack<TextAction> actions_;
    Cursor cursor_;
    size_t scroll_ = 0; // in lines
};
