#include "key.hpp"

#include <cassert>
#include <cstring>

#include "util.hpp"

std::string toString(SpecialKey key)
{
    switch (key) {
    case SpecialKey::Tab:
        return "Tab";
    case SpecialKey::Return:
        return "Return";
    case SpecialKey::Escape:
        return "Escape";
    case SpecialKey::Backspace:
        return "Backspace";
    case SpecialKey::Home:
        return "Home";
    case SpecialKey::Delete:
        return "Delete";
    case SpecialKey::End:
        return "End";
    case SpecialKey::PageUp:
        return "PageUp";
    case SpecialKey::PageDown:
        return "PageDown";
    case SpecialKey::Up:
        return "Up";
    case SpecialKey::Down:
        return "Down";
    case SpecialKey::Right:
        return "Right";
    case SpecialKey::Left:
        return "Left";
    default:
        return "Unknown";
    }
}

Utf8Sequence::Utf8Sequence()
    : bytes { 0 }
    , length(0)
{
}

Utf8Sequence::Utf8Sequence(char c)
    : bytes { c }
    , length(1)
{
}

Utf8Sequence::Utf8Sequence(const char* b, size_t l)
    : length(l)
{
    assert(l <= 4);
    std::memcpy(bytes, b, l);
}

bool Utf8Sequence::operator==(char c) const
{
    return length == 1 && bytes[0] == c;
}

bool Utf8Sequence::operator==(const Utf8Sequence& other) const
{
    return length == other.length && std::memcmp(&bytes[0], &other.bytes[0], length) == 0;
}

Utf8Sequence::operator std::string_view() const
{
    return std::string_view(&bytes[0], length);
}

Key::Key(const std::vector<char>& b)
    : bytes(b)
    , modifiers()
    , key(Utf8Sequence(&b[0], b.size()))
{
}

Key::Key(const std::vector<char>& b, SpecialKey k)
    : bytes(b)
    , modifiers()
    , key(k)
{
}

Key::Key(const std::vector<char>& b, Bitmask<Modifiers> m, SpecialKey k)
    : bytes(b)
    , modifiers(m)
    , key(k)
{
}

Key::Key(const std::vector<char>& b, char k)
    : bytes(b)
    , modifiers()
    , key(Utf8Sequence(k))
{
}

Key::Key(const std::vector<char>& b, Bitmask<Modifiers> m, char k)
    : bytes(b)
    , modifiers(m)
    , key(Utf8Sequence(k))
{
}

Key::Key(Bitmask<Modifiers> m, char k)
    : bytes()
    , modifiers(m)
    , key(Utf8Sequence(k))
{
}

bool Key::operator==(const Key& other) const
{
    if (modifiers != other.modifiers)
        return false;

    if (const auto special = std::get_if<SpecialKey>(&key)) {
        if (const auto oSpecial = std::get_if<SpecialKey>(&other.key)) {
            return *special == *oSpecial;
        }
        return false;
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key)) {
        if (const auto oSeq = std::get_if<Utf8Sequence>(&other.key)) {
            return *seq == *oSeq;
        }
        return false;
    }
    die("Invalid variant state");
}

std::string Key::getAsString() const
{
    std::string str;
    str.reserve(64);
    if (modifiers.test(Modifiers::Ctrl))
        str.append("Ctrl-");
    if (modifiers.test(Modifiers::Alt))
        str.append("Alt-");
    if (const auto special = std::get_if<SpecialKey>(&key)) {
        if (modifiers.test(Modifiers::Shift))
            str.append("Shift-");
        str.append(toString(*special));
    } else if (const auto seq = std::get_if<Utf8Sequence>(&key)) {
        str.append(static_cast<std::string_view>(*seq));
    } else {
        die("Invalid variant state");
    }
    return str;
}
