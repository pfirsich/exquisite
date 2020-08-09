#include "eventhandler.hpp"

#include <cassert>

#ifdef __linux__
#include "eventhandler_linux.hpp"
#else
#error Unsupported platform
#endif

CustomEvent::CustomEvent(std::unique_ptr<CustomEventImpl> impl)
    : impl_(std::move(impl))
{
}

CustomEvent::~CustomEvent()
{
}

CustomEvent::CustomEvent(CustomEvent&& other)
    : impl_(std::move(other.impl_))
{
}

void CustomEvent::emit() const
{
    impl_->emit();
}

EventHandler::EventHandler()
    : impl_(std::make_unique<EventHandlerImpl>())
{
}

EventHandler::HandlerId EventHandler::addSignalHandler(int signum, std::function<void()> callback)
{
    return impl_->addSignalHandler(signum, callback);
}

EventHandler::HandlerId EventHandler::addTimer(
    uint64_t interval, uint64_t expiration, std::function<void()> callback)
{
    return impl_->addTimer(interval, expiration, callback);
}

EventHandler::HandlerId EventHandler::addFilesystemHandler(
    const fs::path& path, std::function<void()> callback)
{
    return impl_->addFilesystemHandler(path, callback);
}

EventHandler::HandlerId EventHandler::addFdHandler(int fd, std::function<void()> callback)
{
    return impl_->addFdHandler(fd, callback);
}

std::pair<EventHandler::HandlerId, CustomEvent> EventHandler::addCustomHandler(
    std::function<void()> callback)
{
    return impl_->addCustomHandler(callback);
}

void EventHandler::removeHandler(HandlerId handle)
{
    impl_->removeHandler(handle);
}

void EventHandler::run()
{
    while (running.load()) {
        processEvents();
    }
}

void EventHandler::terminate()
{
    running.store(false);
}

void EventHandler::processEvents()
{
    impl_->processEvents();
}

ScopedHandlerHandle::ScopedHandlerHandle()
{
}

ScopedHandlerHandle::ScopedHandlerHandle(EventHandler& eventHandler, EventHandler::HandlerId id)
    : eventHandler_(&eventHandler)
    , id_(id)
{
}

ScopedHandlerHandle::ScopedHandlerHandle(ScopedHandlerHandle&& other)
    : eventHandler_(other.eventHandler_)
    , id_(other.release())
{
}

ScopedHandlerHandle& ScopedHandlerHandle::operator=(ScopedHandlerHandle&& other)
{
    eventHandler_ = other.eventHandler_;
    id_ = other.release();
    return *this;
}

ScopedHandlerHandle::~ScopedHandlerHandle()
{
    reset();
}

EventHandler& ScopedHandlerHandle::getEventHandler() const
{
    return *eventHandler_;
}

EventHandler::HandlerId ScopedHandlerHandle::getHandlerId() const
{
    return id_;
}

bool ScopedHandlerHandle::isValid() const
{
    return eventHandler_ && id_ != EventHandler::InvalidHandlerId;
}

void ScopedHandlerHandle::reset(EventHandler* eventHandler, EventHandler::HandlerId id)
{
    if (isValid())
        eventHandler_->removeHandler(id_);

    assert(id == EventHandler::InvalidHandlerId || eventHandler);
    if (id == EventHandler::InvalidHandlerId)
        eventHandler = nullptr;

    eventHandler_ = eventHandler;
    id_ = id;
}

void ScopedHandlerHandle::reset()
{
    reset(nullptr, EventHandler::InvalidHandlerId);
}

EventHandler::HandlerId ScopedHandlerHandle::release()
{
    const auto id = id_;
    eventHandler_ = nullptr;
    id_ = EventHandler::InvalidHandlerId;
    return id;
}

EventHandler& getEventHandler()
{
    static EventHandler eventHandler;
    return eventHandler;
}
