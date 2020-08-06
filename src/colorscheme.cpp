#include "colorscheme.hpp"

#include <algorithm>
#include <cassert>

#include "control.hpp"

// https://en.wikipedia.org/wiki/ANSI_escape_code
ColorScheme colorScheme = {
    // editor
    { "error.prompt", 1 },
    { "highlight.currentline", 238 },
    { "highlight.match.prompt", 243 },
    { "highlight.selection", 240 },

    // code
    { "identifier", 39 },
    { "identifier.namespace", 255 },
    { "identifier.type", 222 },
    { "identifier.type.primitive", 208 },
    { "identifier.type.auto", 228 },
    { "identifier.field", 51 },
    { "keyword", 204 },
    { "function", 69 },
    { "literal.string", 85 },
    { "literal.string.raw", 85 },
    { "literal.string.systemlib", 36 },
    { "literal.char", 83 },
    { "literal.number", 215 },
    { "literal.boolean.true", 216 },
    { "literal.boolean.false", 216 },
    { "comment", 240 },
    { "include", 204 },
    { "bracket.round.open", 1 },
    { "bracket.round.close", 1 },
    { "bracket.square.open", 2 },
    { "bracket.square.close", 2 },
    { "bracket.curly.open", 3 },
    { "bracket.curly.close", 3 },
    { "bracket.angle.open", 4 },
    { "bracket.angle.close", 4 },
};

ColorScheme::ColorScheme(std::initializer_list<std::pair<std::string, Color>> colors)
{
    for (const auto& entry : colors) {
        colors_.push_back(Entry { entry.first, control::sgr::color(entry.second) });
    }
    std::sort(colors_.begin(), colors_.end(),
        [](const Entry& a, const Entry& b) { return a.name > b.name; });
}

const std::vector<ColorScheme::Entry>& ColorScheme::getColors() const
{
    return colors_;
}

std::optional<size_t> ColorScheme::getEntryId(std::string_view name) const
{
    // TODO: Be '.'-separator-aware
    for (size_t i = 0; i < colors_.size(); ++i) {
        const auto& entry = colors_[i];
        if (name.substr(0, entry.name.size()) == entry.name)
            return i;
    }
    return std::nullopt;
}

const std::string& ColorScheme::getColor(size_t entryId) const
{
    return colors_[entryId].color;
}

const std::string& ColorScheme::getColor(std::string_view name) const
{
    return colors_[getEntryId(name).value()].color;
}

const std::string& ColorScheme::operator[](size_t entryId) const
{
    return getColor(entryId);
}
const std::string& ColorScheme::operator[](std::string_view name) const
{
    return getColor(name);
}
