#pragma once

#include <string>
#include <string_view>
#include <vector>

size_t fuzzyMatchScore(
    std::string_view input, std::string_view str, std::vector<size_t>* matchedCharacters = nullptr);
