#include "terminal.hpp"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <termios.h>
#include <unistd.h>

#include "util.hpp"

namespace {
termios termiosBackup;

void deinit()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosBackup))
        die("tcsetattr");
}
}

namespace terminal {
void init()
{
    if (tcgetattr(STDIN_FILENO, &termiosBackup))
        die("tcgetattr");
    atexit(deinit);

    termios ios = termiosBackup;

    ios.c_iflag &= ~(BRKINT); // No SIGINT when for break condition
    ios.c_iflag &= ~(ICRNL); // Don't map 13 to 10
    ios.c_iflag &= ~(INPCK); // No input parity check
    ios.c_iflag &= ~(ISTRIP); // Strip the eight
    ios.c_iflag &= ~(IXON); // Disable Software Flow Control (Ctrl-S and Ctrl-Q)

    ios.c_oflag &= ~(OPOST); // No output processing (move cursor for newlines, etc.)

    ios.c_cflag |= (CS8); // Set character size to 8 bits

    ios.c_lflag &= ~(ECHO); // Don't echo input characters
    ios.c_lflag &= ~(ICANON); // Disable canonical mode (get each char, not lines)
    ios.c_lflag &= ~(IEXTEN); // Disable Ctrl-V
    ios.c_lflag &= ~(ISIG); // Don't get SIGINT on Ctrl-C or SIGTSTP on Ctrl-Z

    ios.c_cc[VMIN] = 0; // minimum numbers to be read
    ios.c_cc[VTIME] = 1; // read() timeout

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ios))
        die("tcsetattr");
}

namespace {
    size_t getUtf8ByteSequenceLength(uint8_t firstCodeUnit)
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

std::optional<Key> readKey()
{
    int nread;
    char c = 0;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    const auto seqLength = getUtf8ByteSequenceLength(static_cast<uint8_t>(c));

    if (seqLength > 1) {
        char seq[4] = { c };
        if (read(STDIN_FILENO, &seq[0], seqLength - 1) != static_cast<int>(seqLength) - 1)
            die("Read incomplete utf8 code point from terminal");
        return Key(&seq[0], seqLength);
    }

    // TODO: Be smarter about Ctrl. Ctrl is pressed if the first two bits are 00

    if (c == 9)
        return Key(&c, 1, SpecialKey::Tab);

    if (c == 13)
        return Key(&c, 1, SpecialKey::Return);

    if (c == 27) {
        char seq[4] = { c };

        // From now on if we can't understand the sequence, just return Escape

        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return Key(&seq[0], 1, SpecialKey::Escape);

        if (seq[1] == '[') {
            if (read(STDIN_FILENO, &seq[2], 1) != 1)
                return Key(&seq[0], 2, SpecialKey::Escape);

            if (seq[2] >= '0' && seq[2] <= '9') {
                if (read(STDIN_FILENO, &seq[3], 1) != 1)
                    return Key(&seq[0], 3, SpecialKey::Escape);
                if (seq[3] == '~') {
                    switch (seq[2]) {
                    case '1':
                        return Key(&seq[0], 4, SpecialKey::Home);
                    case '3':
                        return Key(&seq[0], 4, SpecialKey::Delete);
                    case '4':
                        return Key(&seq[0], 4, SpecialKey::End);
                    case '5':
                        return Key(&seq[0], 4, SpecialKey::PageUp);
                    case '6':
                        return Key(&seq[0], 4, SpecialKey::PageDown);
                    case '7':
                        return Key(&seq[0], 4, SpecialKey::Home);
                    case '8':
                        return Key(&seq[0], 4, SpecialKey::End);
                    default:
                        return Key(&seq[0], 4, SpecialKey::Escape);
                    }
                }
            } else {
                switch (seq[2]) {
                case 'A':
                    return Key(&seq[0], 3, SpecialKey::Up);
                case 'B':
                    return Key(&seq[0], 3, SpecialKey::Down);
                case 'C':
                    return Key(&seq[0], 3, SpecialKey::Right);
                case 'D':
                    return Key(&seq[0], 3, SpecialKey::Left);
                case 'H':
                    return Key(&seq[0], 3, SpecialKey::Home);
                case 'F':
                    return Key(&seq[0], 3, SpecialKey::End);
                default:
                    return Key(&seq[0], 3, SpecialKey::Escape);
                }
            }
        } else if (seq[1] == 'O') {
            if (read(STDIN_FILENO, &seq[2], 1) != 1)
                return Key(&seq[0], 2, false, true, 'O');

            switch (seq[2]) {
            case 'H':
                return Key(&seq[0], 3, SpecialKey::Home);
            case 'F':
                return Key(&seq[0], 3, SpecialKey::End);
            default:
                return Key(&seq[0], 3, SpecialKey::Escape);
            }
        } else if (seq[1] == 13) {
            return Key(&seq[0], 2, false, true, SpecialKey::Return);
        } else if (seq[1] > 0 && seq[1] < 27) {
            return Key(&seq[0], 2, true, true, seq[1] - 1 + 'a');
        } else if (seq[1] == 127) {
            return Key(&seq[0], 2, false, true, SpecialKey::Backspace);
        } else {
            return Key(&seq[0], 2, false, true, seq[1]);
        }
    }

    if (c == 127)
        return Key(&c, 1, SpecialKey::Backspace);

    if (c > 0 && c < 27)
        return Key(c, true, false, c - 1 + 'a');

    return Key(c, false, false, c);
}
}
