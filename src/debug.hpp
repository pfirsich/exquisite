#pragma once

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include "util.hpp"

template <typename... Args>
std::string stringify(Args&&... args)
{
    std::stringstream ss;
    (ss << ... << args);
    return ss.str();
}

template <typename... Args>
void debug(Args&&... args)
{
    const auto str = stringify(std::forward<Args>(args)..., "\n");
    auto f = uniqueFopen("debug.log", "a");
    fwrite(str.data(), 1, str.size(), f.get());
}
