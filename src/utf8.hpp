#pragma once

#include <cstdint>
#include <cstring>

namespace utf8 {
size_t getByteSequenceLength(uint8_t firstCodeUnit);
}
