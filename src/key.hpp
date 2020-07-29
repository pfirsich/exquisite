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
    Utf8Sequence(const char* bytes, size_t length);

    char32_t getCodePoint() const;

    bool operator==(char c) const;
};

struct Key {
    std::vector<char> bytes;
    bool ctrl;
    bool alt;
    std::variant<Utf8Sequence, SpecialKey> key;

    Key(const char* bytes, size_t len); // utf8 sequence
    Key(const char* bytes, size_t len, SpecialKey key); // special key
    Key(char byte, bool ctrl, bool alt, char key); // other chars
    Key(const char* bytes, size_t len, bool ctrl, bool alt, char key);
    Key(const char* bytes, size_t len, bool ctrl, bool alt, SpecialKey key);
};
