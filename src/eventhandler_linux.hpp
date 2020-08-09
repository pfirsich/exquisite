#pragma once

#include <variant>

#include <poll.h>

#include "eventhandler.hpp"
#include "fd.hpp"

class CustomEventImpl {
public:
    CustomEventImpl(int fd);

    void emit() const;

private:
    int fd_;
};

class EventHandlerImpl {
public:
    using HandlerId = EventHandler::HandlerId;

    EventHandlerImpl();

    HandlerId addSignalHandler(int signum, std::function<void()> callback);

    HandlerId addTimer(uint64_t interval, uint64_t expiration, std::function<void()> callback);

    HandlerId addFilesystemHandler(const fs::path& path, std::function<void()> callback);

    HandlerId addFdHandler(int fd, std::function<void()> callback);

    std::pair<HandlerId, CustomEvent> addCustomHandler(std::function<void()> callback);

    void removeHandler(HandlerId handle);

    void processEvents();

private:
    struct SignalHandler {
        std::function<void()> callback;
        Fd fd;
    };

    struct FilesystemHandler {
        std::function<void()> callback;
        fs::path path; // I need to save it, so I can re-add when I get IN_IGNORED
        int wd;
    };

    struct FdHandler {
        std::function<void()> callback;
    };

    struct CustomHandler {
        std::function<void()> callback;
        Fd fd;
    };

    using Handler = std::variant<SignalHandler, FilesystemHandler, FdHandler, CustomHandler>;

    HandlerId addHandler(Handler&& handler);
    HandlerId addHandlerFd(Handler&& handler, int fd);
    HandlerId addHandlerWd(Handler&& handler, int wd);

    size_t handlerIdCounter_ = 0;
    std::vector<Handler> handlers_;
    std::unordered_map<HandlerId, size_t> handlerIdMap_;
    std::unordered_map<int, size_t> fdMap_;
    std::unordered_map<int, size_t> wdMap_;
    std::vector<pollfd> pollFds;
    Fd inotifyFd;
};
