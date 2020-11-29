#pragma once

#include <string>
#include <variant>
#include <vector>

#include "bitmask.hpp"

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
    bool operator==(const Utf8Sequence& other) const;
    explicit operator std::string_view() const;
};

enum class Modifiers { Ctrl, Alt, Shift };

struct Key {
    std::vector<char> bytes;
    Bitmask<Modifiers> modifiers;
    std::variant<Utf8Sequence, SpecialKey> key;

    Key(const std::vector<char>& bytes); // utf8 sequence
    Key(const std::vector<char>& bytes, SpecialKey key); // special key
    Key(const std::vector<char>& bytes, Bitmask<Modifiers> modifiers, SpecialKey key);
    Key(const std::vector<char>& bytes, char key); // other chars
    Key(const std::vector<char>& bytes, Bitmask<Modifiers> modifiers, char key);

    // These are just for shortcuts
    Key(SpecialKey key);
    Key(Bitmask<Modifiers> modifiers, SpecialKey key);
    Key(Bitmask<Modifiers>, char key);

    bool operator==(const Key& other) const;
    std::string getAsString() const;
};

template <>
struct BitmaskEnabled<Modifiers> : std::true_type {
};
