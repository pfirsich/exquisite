#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <queue>

// We don't actually need this header here, but I want the signals to be included with this header
#include <signal.h>

#include "bitmask.hpp"

namespace fs = std::filesystem;

class CustomEventImpl;
class EventHandlerImpl;

class CustomEvent {
public:
    CustomEvent(std::unique_ptr<CustomEventImpl> impl);
    ~CustomEvent();
    CustomEvent(CustomEvent&&);

    void emit() const;

private:
    std::unique_ptr<CustomEventImpl> impl_;
};

class EventHandler {
public:
    using HandlerId = size_t;
    static constexpr auto InvalidHandlerId = std::numeric_limits<HandlerId>::max();

    EventHandler();

    HandlerId addSignalHandler(int signum, std::function<void()> callback);

    HandlerId addTimer(uint64_t interval, uint64_t expiration, std::function<void()> callback);

    // Currently only notifies if a file was modified
    HandlerId addFilesystemHandler(const fs::path& path, std::function<void()> callback);

    // Currently only notifies if an fd is readable, because that's all I need
    HandlerId addFdHandler(int fd, std::function<void()> callback);

    // When you call emit on the CustomEvent returned, the callback will be called in the next
    // processEvents. If emit is called multiple times before that, the callback is only called
    // once.
    std::pair<HandlerId, CustomEvent> addCustomHandler(std::function<void()> callback);

    void removeHandler(HandlerId handle);

    void run(); // blocks until terminate is called

    void terminate();

private:
    void processEvents();

    std::unique_ptr<EventHandlerImpl> impl_;
    std::atomic<bool> running { true };
};

class ScopedHandlerHandle {
public:
    ScopedHandlerHandle();
    ScopedHandlerHandle(EventHandler& eventHandler, EventHandler::HandlerId id);
    ScopedHandlerHandle(const ScopedHandlerHandle&) = delete;
    ScopedHandlerHandle& operator=(const ScopedHandlerHandle&) = delete;
    ScopedHandlerHandle(ScopedHandlerHandle&& other);
    ScopedHandlerHandle& operator=(ScopedHandlerHandle&& other);
    ~ScopedHandlerHandle();

    EventHandler& getEventHandler() const;
    EventHandler::HandlerId getHandlerId() const;
    bool isValid() const;

    void reset(EventHandler* eventHandler, EventHandler::HandlerId id);
    void reset();
    EventHandler::HandlerId release();

private:
    EventHandler* eventHandler_ = nullptr;
    EventHandler::HandlerId id_ = EventHandler::InvalidHandlerId;
};

EventHandler& getEventHandler();
