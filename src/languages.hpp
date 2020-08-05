#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "highlighting.hpp"

struct Language {
    std::string name;
    std::vector<std::string_view> extensions;
    // It's so ugly that I have to make this a pointer, just because of "Plain Text"
    std::unique_ptr<Highlighter> highlighter;
};

namespace languages {
extern Language plainText;
extern Language cpp;

const std::vector<const Language*>& getAll();
const Language* getFromExt(std::string_view ext);
}
