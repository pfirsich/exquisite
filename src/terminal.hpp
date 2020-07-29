#pragma once

#include <optional>

#include "key.hpp"

namespace terminal {
void init();
std::optional<Key> readKey();
}
