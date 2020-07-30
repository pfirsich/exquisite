#include "util.hpp"

#include <cstdio>
#include <cstdlib>

#include <unistd.h>

void die(std::string_view msg)
{
    write(STDOUT_FILENO, "\x1b[?1049l", 8);
    perror(msg.data());
    exit(1);
}
