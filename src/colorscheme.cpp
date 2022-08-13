#include "colorscheme.hpp"

#include <algorithm>
#include <cassert>

#include "control.hpp"
#include "util.hpp"

// https://en.wikipedia.org/wiki/ANSI_escape_code
ColorScheme colorScheme({
    // editor
    { "error.prompt", 1_uc },
    { "whitespace", 238_uc },
    { "background", RgbColor { 41, 42, 43 } },
    { "highlight.currentline", 238_uc },
    { "highlight.match.prompt", 243_uc },
    { "highlight.selection", 240_uc },

    // code
    { "identifier", 39_uc },
    { "identifier.namespace", 255_uc },
    { "identifier.type", 222_uc },
    { "identifier.type.primitive", 208_uc },
    { "identifier.type.auto", 228_uc },
    { "identifier.field", 51_uc },
    { "keyword", 204_uc },
    { "function", 69_uc },
    { "literal.string", 85_uc },
    { "literal.string.raw", 85_uc },
    { "literal.string.systemlib", 36_uc },
    { "literal.char", 83_uc },
    { "literal.number", 215_uc },
    { "literal.boolean.true", 216_uc },
    { "literal.boolean.false", 216_uc },
    { "comment", 245_uc },
    { "include", 204_uc },
    { "bracket.round.open", 1_uc },
    { "bracket.round.close", 1_uc },
    { "bracket.square.open", 2_uc },
    { "bracket.square.close", 2_uc },
    { "bracket.curly.open", 3_uc },
    { "bracket.curly.close", 3_uc },
    { "bracket.angle.open", 4_uc },
    { "bracket.angle.close", 4_uc },
});

ColorScheme::ColorScheme(std::vector<std::pair<std::string, Color>> colors)
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
