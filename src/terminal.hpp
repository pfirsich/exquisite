#pragma once

#include <optional>

#include "key.hpp"

struct Coord {
    // top left is 0, 0
    size_t x, y;
};

namespace terminal {
void init();

// query
Coord getSize();

// input
std::optional<Key> readKey();

// output
void write(std::string_view str);
void bufferWrite(std::string_view str);
void flushWrite();
}

namespace control {
std::string_view clear();
std::string_view clearLine();
std::string_view hideCursor();
std::string_view showCursor();
std::string_view resetCursor(); // set cursor to 0, 0
std::string moveCursor(const Coord& position);
}
