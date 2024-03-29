#include "highlighting.hpp"

#include <cassert>

#include "debug.hpp"

void printWithMarker(std::string_view str, size_t offset)
{
    const size_t errorLineOffset = [&] {
        for (size_t i = offset - 1; i < offset; --i) {
            if (str[i] == '\n') {
                return i + 1;
            }
        }
        return 0ul;
    }();
    const auto errorLineEnd
        = std::distance(str.begin(), std::find(str.begin() + errorLineOffset, str.end(), '\n'));
    debug(str.substr(0, errorLineEnd));
    debug(std::string(offset - errorLineOffset, ' '));
    debug("^\n");
    debug(str.substr(errorLineEnd));
}

Highlighter::Highlighter(const TSLanguage* language, std::string_view querySource)
    : query(language, querySource)
    , language_(language)
{
    const auto error = query.getError();
    if (error.second != ts::QueryError::None) {
        printWithMarker(querySource, error.first);
        debug("Error: {} at {}", static_cast<TSQueryError>(error.second), error.first);
        std::exit(1);
    }
}

void Highlighter::setColorScheme(const ColorScheme& colors)
{
    highlights_.clear();
    for (size_t i = 0; i < query.getCaptureCount(); ++i) {
        const auto name = std::string(query.getCaptureNameForId(i));
        const auto& color = colors.getColor(name);
        highlights_.emplace_back(Highlight { std::move(name), color });
    }
}

const TSLanguage* Highlighter::getLanguage() const
{
    return language_;
}

size_t Highlighter::getNumHighlights() const
{
    return highlights_.size();
}

std::string_view Highlighter::getHighlightName(size_t highlightId) const
{
    return query.getCaptureNameForId(highlightId);
}

std::optional<size_t> Highlighter::getHighlightId(std::string_view name) const
{
    for (size_t i = 0; i < highlights_.size(); ++i) {
        if (highlights_[i].name == name)
            return i;
    }
    return std::nullopt;
}

const std::string& Highlighter::getColor(size_t highlightId) const
{
    return highlights_.at(highlightId).color;
}

Highlighting::Highlighting(const Highlighter& highlighter)
    : highlighter_(highlighter)
{
    parser_.setLanguage(highlighter_.getLanguage());
}

const Highlighter& Highlighting::getHighlighter() const
{
    return highlighter_;
}

void Highlighting::reset()
{
    parser_.reset();
    tree_.reset();
}

void Highlighting::update(const TextBuffer& text)
{
    auto tree = parser_.parse(
        nullptr, // If we want to reuse the tree, we actually have to call edit
        [&text](size_t index, TSPoint) {
            const auto str = text.getString(index);
            return std::make_pair(str.data(), str.size());
        },
        ts::InputEncoding::Utf8);
    if (!tree)
        die("Could not parse file");
    tree_ = std::move(tree);
}

std::vector<Highlight> Highlighting::getHighlights(size_t start, size_t end) const
{
    assert(tree_);
    ts::QueryCursor cursor;
    cursor.setByteRange(start, end);
    cursor.exec(highlighter_.query, tree_->getRootNode());

    TSQueryMatch match;
    std::vector<Highlight> highlights;
    while (cursor.getNextMatch(&match)) {
        for (size_t i = 0; i < match.capture_count; ++i) {
            const auto& capture = match.captures[i];
            const auto start = ts_node_start_byte(capture.node);
            const auto end = ts_node_end_byte(capture.node);
            highlights.push_back(Highlight { capture.index, start, end });
        }
    }

    if (highlights.size() > 1) {
        for (size_t i = 0; i < highlights.size() - 1; ++i) {
            assert(highlights[i].start <= highlights[i + 1].start);
        }
    }

    return highlights;
}

const std::string& Highlighting::getColor(size_t highlightId) const
{
    return highlighter_.getColor(highlightId);
}

const ts::Tree* Highlighting::getTree() const
{
    return tree_.get();
}
