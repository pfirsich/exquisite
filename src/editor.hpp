#pragma once

#include "buffer.hpp"
#include "terminal.hpp"

namespace editor {
extern Coord cursor;
extern Buffer buffer;

void redraw();
}
