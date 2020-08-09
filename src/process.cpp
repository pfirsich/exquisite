#include "process.hpp"

Process::Process(pid_t pid, Fd stdinFd, Fd stdoutFd, Fd stderrFd)
    : pid_(pid)
    , stdin_(std::move(stdinFd))
    , stdout_(std::move(stdoutFd))
    , stderr_(std::move(stderrFd))
{
}

Process::~Process()
{
    // wait();
}

// This return value needs to be better (use all the other macros to get more info out)
std::optional<int> Process::wait()
{
    closeStdin();
    int status = 0;
    assert(pid_ > 0);
    const auto ret = ::waitpid(pid_, &status, 0);
    if (ret != pid_) {
        return std::nullopt;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return std::nullopt;
}

bool Process::kill(int signal) const
{
    assert(pid_ > 0);
    return ::kill(pid_, signal) == 0;
}

// Don't close this yourself!
int Process::stdin() const
{
    return stdin_;
}

void Process::closeStdin()
{
    stdin_.close();
}

int Process::write(std::string_view str) const
{
    return ::write(stdin_, str.data(), str.size());
}

// Don't close this yourself!
int Process::stdout() const
{
    return stdout_;
}

std::string Process::read() const
{
    return readAll(stdout_);
}

// Don't close this yourself!
int Process::stderr() const
{
    return stderr_;
}

std::string Process::readError() const
{
    return readAll(stderr_);
}

pid_t Process::pid() const
{
    return pid_;
}

std::optional<Process> Process::start(
    std::vector<std::string> args, const std::unordered_map<std::string, std::string>& env)
{
    Pipe in, out, err, status;

    pid_t pid_ = ::fork();
    if (pid_ == -1) {
        return std::nullopt;
    }

    if (pid_ == 0) { // this is the child process
        if (::dup2(in.read, STDIN_FILENO) == -1)
            std::exit(errno);
        if (::dup2(out.write, STDOUT_FILENO) == -1)
            std::exit(errno);
        if (::dup2(err.write, STDERR_FILENO) == -1)
            std::exit(errno);

        // Destructors will not be called from execvp, so we have to close manually
        in.close();
        out.close();
        err.close();
        status.read.close();
        // if execvp succeeds, the pipe will be closed and the read in the parent process will
        // fail
        ::fcntl(status.write, F_SETFD, FD_CLOEXEC);

        assert(!args.empty());

        const auto file = args[0];

        std::vector<char*> execArgv;
        execArgv.reserve(args.size() + 1); // +1 for nullptr
        for (size_t i = 0; i < args.size(); ++i)
            execArgv.push_back(&args[i][0]);
        execArgv.push_back(nullptr);

        // execvpe would overwrite the environment completely and I think it would be surprising
        // if the child process would not inherit the parents environment, if something is
        // passed. So I set the extra variable here instead.
        for (const auto& [k, v] : env) {
            ::setenv(k.c_str(), v.c_str(), 1);
        }

        ::execvp(file.c_str(), &execArgv[0]);
        // If this function returned, an error occured
        // Use the status pipe to tell the parent process what happened
        int err = errno;
        ::write(status.write, &err, sizeof(err));
        std::exit(errno);
    } else { // this is the parent process
        // We have to close this end, because we want to read from it now
        // If the child closed it, but we kept it open, the read would block forever
        status.write.close();
        int errn;
        const auto nread = ::read(status.read, &errn, sizeof(errn));
        if (nread > 0) {
            return std::nullopt;
        }

        return Process(pid_, std::move(in.write), std::move(out.read), std::move(err.read));
    }
}

std::optional<Process::Result> Process::run(
    const std::vector<std::string>& args, std::string_view stdinStr)
{
    auto proc = Process::start(args);
    if (!proc) {
        return std::nullopt;
    }
    if (!stdinStr.empty())
        proc->write(stdinStr);
    const auto status = proc->wait();
    if (!status) {
        return std::nullopt;
    }
    return Result { *status, proc->read(), proc->readError() };
}

std::string Process::readAll(int fd)
{
    constexpr int bufSize = 64;
    char buf[bufSize];
    std::string str;
    int n = 0;
    while ((n = ::read(fd, &buf[0], sizeof(buf))) > 0) {
        str.append(&buf[0], n);
    }
    return str;
}

std::optional<std::string> which(std::string_view cmd)
{
    const auto res = Process::run({ "which", std::string(cmd) });
    if (!res || res->status != 0)
        return std::nullopt;
    return res->out;
}
