#include "util.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <queue>

#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <unistd.h>

#include "debug.hpp"

Indentation::Indentation()
    : Indentation(Config::get().indentUsingSpaces, Config::get().indentWidth)
{
}

Indentation::Indentation(bool indentUsingSpaces, size_t indentWith)
    : type(indentUsingSpaces ? Indentation::Type::Spaces : Indentation::Type::Tabs)
    , width(indentUsingSpaces ? indentWith : 1)
{
}

Indentation::Indentation(Type type, size_t width)
    : type(type)
    , width(width)
{
}

std::string Indentation::getString() const
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

void die(std::string_view msg)
{
    write(STDOUT_FILENO, "\x1b[?1049l", 8);
    perror(msg.data());
    std::abort();
}

std::string hexString(const void* data, size_t size)
{
    static const char hexDigits[] = "0123456789ABCDEF";

    std::string out;
    out.reserve(size * 3);
    for (size_t i = 0; i < size; ++i) {
        const auto c = reinterpret_cast<const uint8_t*>(data)[i];
        out.push_back(hexDigits[c >> 4]);
        out.push_back(hexDigits[c & 15]);
    }
    return out;
}

std::unique_ptr<FILE, decltype(&fclose)> uniqueFopen(const char* path, const char* modes)
{
    return std::unique_ptr<FILE, decltype(&fclose)>(fopen(path, modes), &fclose);
}

std::optional<std::string> readFile(const fs::path& path)
{
    auto f = uniqueFopen(path.c_str(), "rb");
    if (!f)
        return std::nullopt;
    if (fseek(f.get(), 0, SEEK_END) != 0)
        return std::nullopt;
    const auto size = ftell(f.get());
    if (size < 0)
        return std::nullopt;
    if (fseek(f.get(), 0, SEEK_SET) != 0)
        return std::nullopt;
    std::string buf(size, '\0');
    if (fread(buf.data(), 1, size, f.get()) != static_cast<size_t>(size))
        return std::nullopt;
    return buf;
}

std::string readAll(int fd)
{
    static constexpr int bufSize = 64;
    char buf[bufSize];
    std::string str;
    int n = 0;
    while ((n = ::read(fd, &buf[0], bufSize)) > 0) {
        str.append(&buf[0], n);
    }
    return str;
}

size_t countNewlines(std::string_view str)
{
    return std::count(str.begin(), str.end(), '\n');
}

bool hasNewlines(std::string_view str)
{
    return str.find('\n') != std::string_view::npos;
}

std::pair<size_t, size_t> getIndentWidth(std::string_view line, size_t tabWidth)
{
    size_t width = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == ' ') {
            width++;
        } else if (line[i] == '\t') {
            width = width + tabWidth - (width % tabWidth);
        } else {
            return std::pair(i, width);
        }
    }
    return std::pair(line.size(), width);
}

Indentation detectLineIndent(std::string_view line)
{
    const auto indentChar = line[0];
    Indentation indent;
    if (indentChar == ' ')
        indent.type = Indentation::Type::Spaces;
    else if (indentChar == '\t')
        indent.type = Indentation::Type::Tabs;
    else
        return indent;

    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == indentChar)
            indent.width++;
        else
            break;
    }
    return indent;
}

size_t getNextLineOffset(std::string_view text, size_t offset)
{
    for (size_t i = offset; i < text.size(); ++i) {
        if (text[i] == '\n')
            return i + 1;
    }
    return text.size();
}

Indentation detectIndentation(std::string_view text)
{
    std::vector<size_t> spacesHist(16, 0);
    size_t tabsCount = 0;

    size_t lineOffset = getNextLineOffset(text, 0);
    Indentation lastIndent;
    while (lineOffset < text.size()) {
        const auto endOffset = getNextLineOffset(text, lineOffset);
        const auto indent = detectLineIndent(text.substr(lineOffset, endOffset - lineOffset));

        if (indent.type != Indentation::Type::Unknown
            && lastIndent.type != Indentation::Type::Unknown) {
            if (indent.type == Indentation::Type::Tabs) {
                tabsCount++;
            } else {
                assert(indent.type == Indentation::Type::Spaces);
                const auto delta = indent.width > lastIndent.width
                    ? indent.width - lastIndent.width
                    : lastIndent.width - indent.width;
                if (delta >= spacesHist.size())
                    spacesHist.resize(delta + 1, 0);
                spacesHist[delta]++;
            }
        }
        lastIndent = indent;

        lineOffset = endOffset;
    }

    const auto maxIt = std::max_element(spacesHist.begin(), spacesHist.end());
    const auto max = static_cast<size_t>(std::distance(spacesHist.begin(), maxIt));

    if (max == 0 && tabsCount == 0)
        return Indentation();
    if (max >= tabsCount) // = because spaces are superior
        return Indentation(Indentation::Type::Spaces, max);
    else
        // width will be 0, because it's just display
        return Indentation(Indentation::Type::Tabs, 1);
}

std::optional<int> toInt(const std::string& str, int base)
{
    try {
        size_t pos = 0;
        const auto value = std::stoi(str, &pos, base);
        if (pos < str.size())
            return std::nullopt;
        return value;
    } catch (const std::invalid_argument& exc) {
        return std::nullopt;
    } catch (const std::out_of_range& exc) {
        return std::nullopt;
    }
}

std::string trimTrailingWhitespace(std::string_view str)
{
    // Essentially we write all characters, except whitespace and remember when that whitespace
    // sequence started. When we encounter a newline, we drop our buffer of whitespace characters
    // and when we encounter a non-whitespace character, we flush that whitespace buffer.
    size_t lastWsSeqStart = 0; // last whitespace sequence start
    std::string out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n') {
            out.push_back(str[i]);
            lastWsSeqStart = i + 1;
        } else if (std::isspace(str[i])) {
            // don't move lastWsSeqStart, because the whitespace sequence is still going
        } else {
            for (size_t j = lastWsSeqStart; j <= i; ++j) {
                out.push_back(str[j]);
            }
            lastWsSeqStart = i + 1;
        }
    }
    return out;
}

std::optional<std::vector<std::string>> walkDirectory(
    const fs::path& dirPath, size_t maxDepth, size_t maxItems)
{
    struct Dir {
        std::string path;
        size_t depth;
    };

    std::queue<Dir> dirs;
    dirs.push(Dir { dirPath.u8string(), 0 });

    std::vector<std::string> files;
    while (!dirs.empty()) {
        auto cur = dirs.front();
        dirs.pop();

        DIR* dir = ::opendir(cur.path.c_str());
        if (!dir) {
            return std::nullopt;
        }

        dirent* ent;
        while ((ent = ::readdir(dir))) {
            // Skip ".", ".." and everything hidden
            const std::string name = ent->d_name;
            std::string path = cur.path;
            path.reserve(path.size() + 1 + name.size());
            path.push_back('/');
            path.append(name);

            if (ent->d_type == DT_DIR && cur.depth < maxDepth && ent->d_name[0] != '.') {
                dirs.push(Dir { path, cur.depth + 1 });
            } else if (ent->d_type == DT_REG) {
                files.push_back(std::move(path));
                if (files.size() >= maxItems) {
                    break;
                }
            }
        }
        ::closedir(dir);
    }
    return files;
}
