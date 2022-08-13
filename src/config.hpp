#pragma once

#include <optional>
#include <string>
#include <string_view>

using namespace std::literals;

struct Config {
    size_t tabWidth = 8;
    bool indentUsingSpaces = true;
    size_t indentWidth = 4;
    uint8_t cursor = 5;

    std::string colorscheme = "default";

    bool renderWhitespace = true;
    struct Whitespace {
        std::string space = "·";
        std::string newline = "$";
        std::string tabStart = "-";
        std::string tabMid = "-";
        std::string tabEnd = ">";
    } whitespace;

    bool trimTrailingWhitespaceOnSave = true;
    bool showLineNumbers = true;
    size_t highlightCurrentLine = true;
    size_t numPromptOptions = 7;

    static Config& get();

    void load();
};

void executeHook(std::string_view hookName);
void loadConfig();
