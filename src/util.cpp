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
        if (n < bufSize)
            break;
    }
    return str;
}
