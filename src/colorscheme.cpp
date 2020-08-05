#include "colorscheme.hpp"

#include <algorithm>
#include <cassert>

#include "control.hpp"

// https://en.wikipedia.org/wiki/ANSI_escape_code
ColorScheme colorScheme = {
    { "error.prompt", 1 },
    { "highlight.currentline", 238 },
    { "highlight.match.prompt", 243 },
    { "highlight.match.find", 240 },
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
