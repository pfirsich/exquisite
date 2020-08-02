#pragma once

#include <optional>
#include <string_view>

using namespace std::literals;

struct Config {
    size_t tabWidth = 4;

    bool renderWhitespace = true;
    std::optional<std::string_view> spaceChar = "·"sv;
    std::optional<std::string_view> newlineChar = "↵"sv;
    std::optional<std::string_view> tabStartChar = "-"sv;
    std::optional<std::string_view> tabMidChar = "-"sv;
    std::optional<std::string_view> tabEndChar = ">"sv;

    bool showLineNumbers = true;

    size_t highlightCurrentLine = true;

    size_t numPromptOptions = 7;
};

extern Config config;
