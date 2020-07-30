#include "utf8.hpp"

namespace utf8 {
size_t getByteSequenceLength(uint8_t firstCodeUnit)
{
    if ((firstCodeUnit & 0b11110000) == 0b11110000)
        return 4;
    if ((firstCodeUnit & 0b11100000) == 0b11100000)
        return 3;
    if ((firstCodeUnit & 0b11000000) == 0b11000000)
        return 2;
    else
        return 1;
}
}
