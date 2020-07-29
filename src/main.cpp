#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>

#include "terminal.hpp"
#include "util.hpp"

// char code for ctrl + letter
constexpr char ctrl(char c)
{
    return c & 0b00011111;
}

int main()
{
    terminal::init();

    while (true) {
        const auto keyOpt = terminal::readKey();

        if (keyOpt) {
            const auto& key = keyOpt.value();

            for (const auto c : key.bytes)
                printf("%X", static_cast<uint8_t>(c));
            printf(" ");
            if (key.ctrl)
                printf("ctrl+");
            if (key.alt)
                printf("alt+");

            if (const auto special = std::get_if<SpecialKey>(&key.key)) {
                printf("%s\r\n", toString(*special).c_str());
            } else if (const auto seq = std::get_if<Utf8Sequence>(&key.key)) {
                printf("%.*s\r\n", static_cast<int>(seq->length), seq->bytes);

                if (key.ctrl && *seq == 'q') {
                    printf("Quit.\r\n");
                    break;
                }
            } else {
                die("Invalid Key variant");
            }
        }
    }
    return 0;
}
