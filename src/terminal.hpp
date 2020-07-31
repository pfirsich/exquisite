#pragma once

#include <optional>

#include "key.hpp"
#include "util.hpp"

namespace terminal {
void init();

// query
Vec getSize();
Vec getCursorPosition();

// input
std::optional<Key> readKey();

// output
void write(std::string_view str);
void bufferWrite(char ch);
void bufferWrite(std::string_view str);
void flushWrite();
}
