#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <queue>

#include "bitmask.hpp"

namespace fs = std::filesystem;

class CustomEventImpl;
class EventHandlerImpl;

class CustomEvent {
public:
    CustomEvent(std::unique_ptr<CustomEventImpl> impl);
    ~CustomEvent();
    CustomEvent(CustomEvent&&) = default;

    void emit() const;

private:
    std::unique_ptr<CustomEventImpl> impl_;
};

class EventHandler {
public:
    using HandlerId = size_t;

    EventHandler();

    HandlerId addSignalHandler(uint32_t signal, std::function<void()> callback);

    HandlerId addTimer(uint64_t interval, uint64_t expiration, std::function<void()> callback);

    // Currently only notifies if a file was modified
    HandlerId addFilesystemHandler(const fs::path& path, std::function<void()> callback);

    // Currently only notifies if an fd is readable, because that's all I need
    HandlerId addFdHandler(int fd, std::function<void()> callback);

    std::pair<HandlerId, CustomEvent> addCustomHandler(std::function<void()> callback);
    void removeHandler(HandlerId handle);

    void run(); // blocks until terminate is called

    void terminate();

private:
    void processEvents();

    std::unique_ptr<EventHandlerImpl> impl_;
    std::atomic<bool> running { true };
};

extern EventHandler eventHandler;
