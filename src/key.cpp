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

Key::Key(const char* b, size_t l)
    : bytes(b, b + l)
    , ctrl(false)
    , alt(false)
    , key(Utf8Sequence(b, l))
{
}

Key::Key(const char* b, size_t l, SpecialKey k)
    : bytes(b, b + l)
    , ctrl(false)
    , alt(false)
    , key(k)
{
}

Key::Key(char b, bool c, bool a, char k)
    : bytes { b }
    , ctrl(c)
    , alt(a)
    , key(Utf8Sequence(k))
{
}

Key::Key(const char* b, size_t l, bool c, bool a, char k)
    : bytes(b, b + l)
    , ctrl(c)
    , alt(a)
    , key(Utf8Sequence(k))
{
}

Key::Key(const char* b, size_t l, bool c, bool a, SpecialKey k)
    : bytes(b, b + l)
    , ctrl(c)
    , alt(a)
    , key(k)
{
}
