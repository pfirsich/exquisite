#pragma once

#include "tree-sitter.hpp"

#include "colorscheme.hpp"
#include "textbuffer.hpp"

class Highlighter {
public:
    ts::Query query;

    Highlighter(const TSLanguage* language, std::string_view querySource);

    void setColorScheme(const ColorScheme& colors);

    const TSLanguage* getLanguage() const;

    size_t getNumHighlights() const;

    std::string_view getHighlightName(size_t highlightId) const;

    std::optional<size_t> getHighlightId(std::string_view name) const;

    const std::string& getColor(size_t highlightId) const;

private:
    struct Highlight {
        std::string name;
        std::string color;
    };

    const TSLanguage* language_;
    std::vector<Highlight> highlights_;
};

struct Highlight {
    size_t id;
    size_t start;
    size_t end;
};

class Highlighting {
public:
    Highlighting(const Highlighter& highlighter);

    const Highlighter& getHighlighter() const;

    // edit

    void reset();

    void update(const TextBuffer& text);

    std::vector<Highlight> getHighlights(size_t start, size_t end) const;

    const std::string& getColor(size_t highlightId) const;

    const ts::Tree* getTree() const;

private:
    const Highlighter& highlighter_;
    ts::Parser parser_;
    std::unique_ptr<ts::Tree> tree_;
};
