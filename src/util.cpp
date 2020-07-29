#include "util.hpp"

#include <cstdio>
#include <cstdlib>

void die(std::string_view msg)
{
    perror(msg.data());
    exit(1);
}
