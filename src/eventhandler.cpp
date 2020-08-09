#include "eventhandler.hpp"

#ifdef __linux__
#include "eventhandler_linux.hpp"
#else
#error Unsupported platform
#endif

EventHandler eventHandler;

CustomEvent::CustomEvent(std::unique_ptr<CustomEventImpl> impl)
    : impl_(std::move(impl))
{
}

CustomEvent::~CustomEvent()
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

EventHandler::HandlerId EventHandler::addSignalHandler(
    uint32_t signal, std::function<void()> callback)
{
    return impl_->addSignalHandler(signal, callback);
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
