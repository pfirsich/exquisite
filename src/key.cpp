#include "key.hpp"

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

bool Utf8Sequence::operator==(char c) const
{
    return length == 1 && bytes[0] == c;
}
