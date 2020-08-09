#include "eventhandler_linux.hpp"

#include <cassert>

#include <sys/eventfd.h>
#include <unistd.h>

#include "debug.hpp"

CustomEventImpl::CustomEventImpl(int fd)
    : fd_(fd)
{
}

void CustomEventImpl::emit() const
{
    debug("emit to fd: {}", fd_);
    const uint64_t value = 1;
    ::write(fd_, &value, sizeof(value));
}

EventHandlerImpl::HandlerId EventHandlerImpl::addSignalHandler(
    uint32_t /*signal*/, std::function<void()> /*callback*/)
{
    return 0;
}

EventHandlerImpl::HandlerId EventHandlerImpl::addTimer(
    uint64_t /*interval*/, uint64_t /*expiration*/, std::function<void()> /*callback*/)
{
    return 0;
}

EventHandlerImpl::HandlerId EventHandlerImpl::addFilesystemHandler(
    const fs::path& /*path*/, std::function<void()> /*callback*/)
{
    return 0;
}

EventHandlerImpl::HandlerId EventHandlerImpl::addFdHandler(int fd, std::function<void()> callback)
{
    return addHandler(FdHandler { callback }, fd);
}

std::pair<EventHandlerImpl::HandlerId, CustomEvent> EventHandlerImpl::addCustomHandler(
    std::function<void()> callback)
{
    auto fd = ::eventfd(0, 0);
    debug("eventfd: {}", fd);
    const auto id = addHandler(CustomHandler { Fd(fd), callback }, fd);
    return std::pair<HandlerId, CustomEvent>(
        id, CustomEvent(std::make_unique<CustomEventImpl>(fd)));
}

void EventHandlerImpl::removeHandler(HandlerId /*handle*/)
{
}

void EventHandlerImpl::processEvents()
{
    debug("poll");
    const auto ret = poll(&pollFds[0], pollFds.size(), -1);
    if (ret < 0) {
        std::perror("poll");
        std::exit(1);
    }
    if (ret == 0)
        return;

    for (const auto& pfd : pollFds) {
        if (pfd.revents & POLLIN) {
            const auto& handler = handlers_[fdMap_.at(pfd.fd)];
            if (const auto fdh = std::get_if<FdHandler>(&handler)) {
                fdh->callback();
            } else if (const auto ch = std::get_if<CustomHandler>(&handler)) {
                uint64_t val = 0;
                ::read(ch->fd, &val, 8);
                ch->callback();
            } else {
                assert(false && "Invalid variant state");
            }
        }
    }
}

EventHandlerImpl::HandlerId EventHandlerImpl::addHandler(Handler&& handler, int fd)
{
    const auto id = handlerIdCounter_++;
    handlers_.push_back(std::forward<Handler>(handler));
    handlerIdMap_[id] = handlers_.size() - 1;
    fdMap_[fd] = handlers_.size() - 1;
    pollFds.push_back(pollfd { fd, POLLIN, 0 });
    return id;
}
