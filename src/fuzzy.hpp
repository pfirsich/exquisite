#pragma once

#include <string>
#include <string_view>
#include <vector>

struct FuzzyMatchCompare {
    std::string_view input;

    bool operator()(const std::string& a, const std::string& b);
};

size_t fuzzyMatchScore(
    std::string_view input, std::string_view str, std::vector<size_t>* matchedCharacters = nullptr);
