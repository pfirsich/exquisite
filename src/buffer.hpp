#pragma once

#include <filesystem>

#include "actionstack.hpp"
#include "languages.hpp"
#include "textbuffer.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

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

// I kinda want to split this class (into an EditBuffer that is just a TextBuffer + Cursor)
// but a bunch of stuff gets weird then and I don't really know how to do it nicely.
class Buffer {
public:
    std::string name;
    fs::path path;

    Buffer();

    void setPath(const fs::path& path);
    void setText(std::string_view str);
    bool readFromFile(const fs::path& path);
    void readFromStdin();
    bool save();
    bool isModified() const;
    void setLanguage(const Language* language);
    const Language* getLanguage() const;

    void updateHighlighting();
    const Highlighting* getHighlighting() const;

    const TextBuffer& getText() const;
    void insert(std::string_view str);
    void deleteSelection();
    void deleteBackwards();
    void deleteForwards();

    const Cursor& getCursor() const;
    // This will return the cursorX position clamped to the line length
    size_t getCursorX(const Cursor::End& cursorEnd) const;
    size_t getCursorOffset(const Cursor::End& cursorEnd) const;
    Range getSelection() const;
    std::string getSelectionString() const;

    void select(const Range& range); // can use range.length = 0 to set cursor pos
    void moveCursorHome(bool select);
    void moveCursorEnd(bool select);
    void moveCursorLeft(bool select);
    void moveCursorRight(bool select);
    void moveCursorY(int dy, bool select);

    size_t getScroll() const;
    void scroll(size_t terminalHeight);

    void startUndoTransaction();
    void endUndoTransaction();
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

        void perform() const;
        void undo() const;
    };

    bool shouldMerge(const TextAction& action) const;
    void performAction(std::string_view text, const Cursor& cursorAfter);

    TextBuffer text_;
    ActionStack<TextAction> actions_;
    size_t savedVersionId_ = std::numeric_limits<size_t>::max();
    Cursor cursor_;
    size_t scroll_ = 0; // in lines
    const Language* language_ = &languages::plainText;
    std::unique_ptr<Highlighting> highlighting_;
};
