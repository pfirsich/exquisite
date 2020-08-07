#include "config.hpp"

#include "util.hpp"

Config config;

const std::string Indentation::getString() const
{
    switch (type) {
    case Type::Spaces:
        return std::string(width, ' ');
    case Type::Tabs:
        return std::string(width, '\t');
    default:
        die("Invalid indentation type");
    }
}
