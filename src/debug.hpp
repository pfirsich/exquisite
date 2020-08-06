#pragma once

#include <cassert>
#include <cstdio>
#include <memory>

#include <fmt/format.h>

template <typename String, typename... Args>
void debug([[maybe_unused]] String&& format, [[maybe_unused]] Args&&... args)
{
#ifndef NDEBUG
    // I don't use the function from util.hpp here, so this file is self-contained
    auto f = std::unique_ptr<FILE, decltype(&fclose)>(fopen("debug.log", "a"), &fclose);
    assert(f);
    fmt::print(f.get(), format, std::forward<Args>(args)...);
    fmt::print(f.get(), "\n");
#endif
}
