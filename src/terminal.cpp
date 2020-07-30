#include "terminal.hpp"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "debug.hpp"
#include "util.hpp"

using namespace std::literals;

namespace {
termios termiosBackup;
std::vector<char> writeBuffer;

void switchToAlternateScreen()
{
    // https://stackoverflow.com/questions/52988525/restore-terminal-output-after-exit-program
    // https://unix.stackexchange.com/questions/288962/what-does-1049h-and-1h-ansi-escape-sequences-do/289055#289055
    // https://invisible-island.net/xterm/xterm.faq.html#xterm_tite
    // https://www.gnu.org/software/screen/manual/html_node/Control-Sequences.html
    terminal::write("\x1b[?1049h");
}

void switchFromAlternateScreen()
{
    terminal::write("\x1b[?1049l");
}

void deinit()
{
    switchFromAlternateScreen();
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosBackup))
        die("tcsetattr");
}
}

namespace terminal {
void init()
{
    switchToAlternateScreen();
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

Vec getSize()
{
    winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // TODO:
        // https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html#window-size-the-hard-way
        die("Invalid terminal size");
    } else {
        assert(ws.ws_col > 0 && ws.ws_row > 0);
        return Vec { ws.ws_col, ws.ws_row };
    }
}

Vec getCursorPosition()
{
    if (::write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        die("write 6n");

    char buf[32];
    size_t i = 0;
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        die("invalid cursor pos");

    int x = 0, y = 0;
    debug("get cursor buf (", i, "): '", std::string_view(&buf[1], i - 1), "'");
    if (sscanf(&buf[2], "%d;%d", &y, &x) != 2)
        die("malformed cursor pos");
    debug("pos: ", x, ", ", y);

    assert(x > 0 && y > 0);
    return Vec { static_cast<size_t>(x - 1), static_cast<size_t>(y - 1) };
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

void write(std::string_view str)
{
    if (::write(STDOUT_FILENO, str.data(), str.size()) != static_cast<ssize_t>(str.size()))
        die("write");
}

void bufferWrite(std::string_view str)
{
    writeBuffer.insert(writeBuffer.end(), str.begin(), str.end());
}

void flushWrite()
{
    write(std::string_view(writeBuffer.data(), writeBuffer.size()));
    writeBuffer.clear();
}
}
