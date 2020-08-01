#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace utf8 {
bool isAscii(char ch); // i.e. is single-byte code point
bool isContinuationByte(char ch);
size_t getCodePointLength(char firstCodeUnit);

// This function reports a smaller length, if the code point is malformed.
// Possible cases:
// * The buffer is too small to fit the whole code point
// * The code units after the first are not continuation bytes
// It's templated, so I can use it with TextBuffer
// size is the full size of the buffer (not the remaining size after offset).
template <typename Buffer>
size_t getCodePointLength(const Buffer& buffer, size_t size, size_t offset)
{
    if (offset >= size)
        return 0;

    const auto ch = buffer[offset];
    const auto cpLen = std::min(getCodePointLength(ch), size - offset);
    if (cpLen == 1) {
        assert(isAscii(ch));
        return 1;
    }

    for (size_t i = 1; i < cpLen; ++i) {
        if (!isContinuationByte(buffer[offset + i])) {
            // Pretend the code point ended and the next byte is another code point.
            return i;
        }
    }
    return cpLen;
}
}
