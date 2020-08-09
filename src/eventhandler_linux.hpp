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

    HandlerId addSignalHandler(int signum, std::function<void()> callback);

    HandlerId addTimer(uint64_t interval, uint64_t expiration, std::function<void()> callback);

    HandlerId addFilesystemHandler(const fs::path& path, std::function<void()> callback);

    HandlerId addFdHandler(int fd, std::function<void()> callback);

    std::pair<HandlerId, CustomEvent> addCustomHandler(std::function<void()> callback);

    void removeHandler(HandlerId handle);

    void processEvents();

private:
    struct SignalHandler {
        Fd fd;
        std::function<void()> callback;
    };

    struct FdHandler {
        std::function<void()> callback;
    };

    struct CustomHandler {
        Fd fd;
        std::function<void()> callback;
    };

    using Handler = std::variant<SignalHandler, FdHandler, CustomHandler>;

    HandlerId addHandler(Handler&& handler, int fd);

    size_t handlerIdCounter_ = 0;
    std::vector<Handler> handlers_;
    std::unordered_map<HandlerId, size_t> handlerIdMap_;
    std::unordered_map<int, size_t> fdMap_;
    std::vector<pollfd> pollFds;
};
