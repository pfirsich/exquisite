#include "key.hpp"

#include <cassert>
#include <cstring>

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
