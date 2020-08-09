#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fd.hpp"

class Process {
public:
    Process(pid_t pid, Fd stdinFd, Fd stdoutFd, Fd stderrFd);

    Process(const Process& other) = delete;
    Process& operator=(const Process& other) = delete;

    Process(Process&& other) = default;

    ~Process();

    // This return value needs to be better (use all the other macros to get more info out)
    std::optional<int> wait();

    bool kill(int signal = SIGTERM) const;

    // Don't close this yourself!
    int stdin() const;
    void closeStdin();
    int write(std::string_view str) const;

    // Don't close this yourself!
    int stdout() const;
    std::string read() const;

    // Don't close this yourself!
    int stderr() const;
    std::string readError() const;

    pid_t pid() const;

    static std::optional<Process> start(std::vector<std::string> args,
        const std::unordered_map<std::string, std::string>& env = {});

    struct Result {
        int status;
        std::string out;
        std::string err;
    };

    static std::optional<Result> run(
        const std::vector<std::string>& args, std::string_view stdinStr = "");

private:
    static std::string readAll(int fd);

    pid_t pid_ = 0;
    Fd stdin_ = -1;
    Fd stdout_ = -1;
    Fd stderr_ = -1;
};

std::optional<std::string> which(std::string_view cmd);
