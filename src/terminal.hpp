#pragma once

#include <optional>

#include "key.hpp"
#include "util.hpp"

namespace terminal {
void init();

// query
Vec getSize();

// input
std::optional<Key> readKey();

// output
void write(std::string_view str);
void bufferWrite(std::string_view str);
void flushWrite();
}
