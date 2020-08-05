#include "languages.hpp"

using namespace std::literals;

namespace languages {
Language plainText { "Plain Text", {}, nullptr };

const std::vector<const Language*>& getAll()
{
    static std::vector<const Language*> all {
        &plainText,
        &cpp,
    };
    return all;
}

const Language* getFromExt(std::string_view ext)
{
    for (const auto lang : getAll()) {
        for (const auto otherExt : lang->extensions) {
            if (ext == otherExt)
                return lang;
        }
    }
    return &languages::plainText;
}
}
