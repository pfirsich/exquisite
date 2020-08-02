#include "fuzzy.hpp"

#include "debug.hpp"

size_t fuzzyMatchScore(
    std::string_view input, std::string_view str, std::vector<size_t>* matchedCharacters)
{
    if (matchedCharacters)
        matchedCharacters->clear();

    size_t s = 0;
    const auto max = std::numeric_limits<size_t>::max();
    size_t score = max;
    for (size_t i = 0; i < input.size(); ++i) {
        size_t lastS = 0;
        while (s < str.size() && std::tolower(str[s]) != std::tolower(input[i]))
            s++;
        if (s == str.size())
            return 0;

        if (matchedCharacters)
            matchedCharacters->push_back(s);

        score -= s; // reduce the score for late matches
        score -= s - lastS - 1; // reduce the score if this match is far apart from the last
        lastS = s;
    }
    return score;
}
