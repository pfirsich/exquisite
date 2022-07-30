#include "config.hpp"

#include <filesystem>

#include "util.hpp"

namespace fs = std::filesystem;

Config& Config::get()
{
    static Config config;
    return config;
}
