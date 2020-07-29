#pragma once

#include <string>
#include <variant>
#include <vector>

enum class SpecialKey {
    Tab,
    Return,
    Escape,
    Backspace,
    Home,
    Delete,
    End,
    PageUp,
    PageDown,
    Up,
    Down,
    Right,
    Left,
};

std::string toString(SpecialKey key);

struct Utf8Sequence {
    char bytes[4];
    size_t length;

    Utf8Sequence();
    Utf8Sequence(char c);

    char32_t getCodePoint() const;

    bool operator==(char c) const;
};

struct Key {
    std::vector<char> bytes;
    bool ctrl;
    bool alt;
    std::variant<Utf8Sequence, SpecialKey> key;
};
