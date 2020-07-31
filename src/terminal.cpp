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
#include "utf8.hpp"
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
    if (sscanf(&buf[2], "%d;%d", &y, &x) != 2)
        die("malformed cursor pos");

    assert(x > 0 && y > 0);
    return Vec { static_cast<size_t>(x - 1), static_cast<size_t>(y - 1) };
}

void readAll(std::vector<char>& seq)
{
    char ch;
    while (read(STDIN_FILENO, &ch, 1) == 1)
        seq.push_back(ch);
}

std::optional<SpecialKey> getMovementKey(char ch)
{
    switch (ch) {
    case 'A':
        return SpecialKey::Up;
    case 'B':
        return SpecialKey::Down;
    case 'C':
        return SpecialKey::Right;
    case 'D':
        return SpecialKey::Left;
    case 'H':
        return SpecialKey::Home;
    case 'F':
        return SpecialKey::End;
    default:
        return std::nullopt;
    }
}

std::optional<Key> readKey()
{
    int nread;
    char ch = 0;
    // Loop until we get an error or a character
    while ((nread = read(STDIN_FILENO, &ch, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    std::vector<char> seq { ch };
    seq.reserve(32);

    // probably utf8 code unit
    if (ch < 0) {
        const auto seqLength = utf8::getByteSequenceLength(static_cast<uint8_t>(ch));
        seq.resize(seqLength, '\0');
        if (read(STDIN_FILENO, &seq[1], seqLength - 1) != static_cast<int>(seqLength) - 1)
            die("Read incomplete utf8 code point from terminal");
        return Key(seq);
    }

    // TODO: Be smarter about Ctrl. Ctrl is pressed if the first two bits are 00

    if (seq[0] == 9)
        return Key(seq, SpecialKey::Tab);

    if (seq[0] == 13)
        return Key(seq, SpecialKey::Return);

    if (seq[0] == 27) {
        // From now on if we can't understand the sequence, just return Escape

        if (read(STDIN_FILENO, &ch, 1) != 1)
            return Key(seq, SpecialKey::Escape);
        seq.push_back(ch);

        if (seq[1] == '[') {
            if (read(STDIN_FILENO, &ch, 1) != 1)
                return Key(seq, SpecialKey::Escape);
            seq.push_back(ch);

            if (seq[2] >= '0' && seq[2] <= '9') {
                if (read(STDIN_FILENO, &ch, 1) != 1)
                    return Key(seq, SpecialKey::Escape);
                seq.push_back(ch);

                if (seq[3] == '~') {
                    switch (seq[2]) {
                    case '1':
                        return Key(seq, SpecialKey::Home);
                    case '3':
                        return Key(seq, SpecialKey::Delete);
                    case '4':
                        return Key(seq, SpecialKey::End);
                    case '5':
                        return Key(seq, SpecialKey::PageUp);
                    case '6':
                        return Key(seq, SpecialKey::PageDown);
                    case '7':
                        return Key(seq, SpecialKey::Home);
                    case '8':
                        return Key(seq, SpecialKey::End);
                    default:
                        break;
                    }
                } else if (seq[2] == '1' && seq[3] == ';') {
                    for (size_t i = 0; i < 2; ++i) {
                        if (read(STDIN_FILENO, &ch, 1) != 1)
                            return Key(seq, SpecialKey::Escape);
                        seq.push_back(ch);
                    }

                    if (seq[4] >= '2' && seq[4] <= '6') {
                        const bool ctrl = seq[4] == '5' || seq[4] == '6';
                        const bool alt = seq[4] == '3' || seq[4] == '4';
                        const bool shift = seq[4] == '2' || seq[4] == '4' || seq[4] == '6';

                        const auto movementKey = getMovementKey(seq[5]);
                        if (movementKey)
                            return Key(seq, ctrl, alt, shift, *movementKey);
                    }
                }
            } else {
                const auto movementKey = getMovementKey(seq[2]);
                if (movementKey)
                    return Key(seq, *movementKey);
            }

            readAll(seq);
            return Key(seq, SpecialKey::Escape);
        } else if (seq[1] == 'O') {
            if (read(STDIN_FILENO, &ch, 1) != 1)
                return Key(seq, false, true, false, 'O');
            seq.push_back(ch);

            switch (seq[2]) {
            case 'H':
                return Key(seq, SpecialKey::Home);
            case 'F':
                return Key(seq, SpecialKey::End);
            default:
                break;
            }

            readAll(seq);
            return Key(seq, SpecialKey::Escape);
        } else if (seq[1] == 13) {
            return Key(seq, false, true, false, SpecialKey::Return);
        } else if (seq[1] > 0 && seq[1] < 27) {
            return Key(seq, true, true, false, seq[1] - 1 + 'a');
        } else if (seq[1] == 127) {
            return Key(seq, false, true, false, SpecialKey::Backspace);
        } else {
            return Key(seq, false, true, false, seq[1]);
        }
    }

    if (seq[0] == 127)
        return Key(seq, SpecialKey::Backspace);

    if (seq[0] > 0 && seq[0] < 27)
        return Key(seq, true, false, false, seq[0] - 1 + 'a');

    return Key(seq, false, false, false, seq[0]);
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
