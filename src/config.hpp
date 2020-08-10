#pragma once

#include <optional>
#include <string>
#include <string_view>

using namespace std::literals;

// I don't think this class should be here, but I don't want config to have dependencies
struct Indentation {
    enum class Type { Unknown, Spaces, Tabs };

    Type type = Type::Unknown;
    size_t width = 0; // unused for Tabs

    const std::string getString() const;
};

struct Config {
    size_t defaultTabWidth = 8;
    Indentation defaultIndentation { Indentation::Type::Spaces, 4 };

    bool renderWhitespace = true;
    std::optional<std::string_view> spaceChar = "·"sv;
    std::optional<std::string_view> newlineChar = "$"sv;
    std::optional<std::string_view> tabStartChar = "-"sv;
    std::optional<std::string_view> tabMidChar = "-"sv;
    std::optional<std::string_view> tabEndChar = ">"sv;

    bool trimTrailingWhitespaceOnSave = true;

    bool showLineNumbers = true;

    size_t highlightCurrentLine = true;

    size_t numPromptOptions = 7;
};

extern Config config;
