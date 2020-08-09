#include "eventhandler_linux.hpp"

#include <cassert>

#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "debug.hpp"

CustomEventImpl::CustomEventImpl(int fd)
    : fd_(fd)
{
}

void CustomEventImpl::emit() const
{
    const uint64_t value = 1;
    ::write(fd_, &value, sizeof(value));
}

EventHandlerImpl::HandlerId EventHandlerImpl::addSignalHandler(
    int signum, std::function<void()> callback)
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, signum);

    // We have to block the signal for the process, if we want to receive it via signalfd.
    // This mask will be inherited by forked processes!
    // As the man page suggests, we should unblock the blocked signals between fork and exec in
    // process.cpp.
    // If any library we use ever forks somewhere, this might go super wrong though.
    // https://news.ycombinator.com/item?id=9564975 - Maybe only set the mask before poll?
    const auto err = sigprocmask(SIG_BLOCK, &sigset, nullptr);
    if (err != 0) {
        std::perror("sigprocmask");
        std::exit(1);
    }

    const auto fd = ::signalfd(-1, &sigset, 0);
    if (fd < 0) {
        std::perror("signalfd");
        std::exit(1);
    }
    return addHandler(SignalHandler { Fd(fd), callback }, fd);
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
    const auto fd = ::eventfd(0, 0);
    if (fd < 0) {
        std::perror("eventfd");
        std::exit(1);
    }
    const auto id = addHandler(CustomHandler { Fd(fd), callback }, fd);
    return std::pair<HandlerId, CustomEvent>(
        id, CustomEvent(std::make_unique<CustomEventImpl>(fd)));
}

void EventHandlerImpl::removeHandler(HandlerId /*handle*/)
{
}

void EventHandlerImpl::processEvents()
{
    const auto ret = poll(&pollFds[0], pollFds.size(), -1);
    if (ret < 0) {
        std::perror("poll");
        std::exit(1);
    }
    if (ret == 0)
        return;

    // We need to first collect all the handlers we want to call and then call them, so
    // it's possible for them to remove handlers.
    // We save the ids so if a handler is removed, it's not called during this call of
    // processEvents. Future you, you wanted to collect the callbacks itself and if you did, they
    // would still be called.
    // Example: If a callback (i.e. input - Ctrl-W) closes a buffer and you
    // remove the filesystem watch, you want the filesystem watch to not be called anymore
    // (potentially with a dangling pointer).
    std::vector<HandlerId> handlers;
    handlers.reserve(pollFds.size());
    for (const auto& pfd : pollFds) {
        if (pfd.revents & POLLIN) {
            handlers.push_back(fdMap_.at(pfd.fd));
        }
    }

    for (const auto handlerId : handlers) {
        const auto& handler = handlers_[handlerId];
        if (const auto sh = std::get_if<SignalHandler>(&handler)) {
            signalfd_siginfo info;
            ::read(sh->fd, &info, sizeof(signalfd_siginfo));
            sh->callback();
        } else if (const auto fdh = std::get_if<FdHandler>(&handler)) {
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

EventHandlerImpl::HandlerId EventHandlerImpl::addHandler(Handler&& handler, int fd)
{
    const auto id = handlerIdCounter_++;
    handlers_.push_back(std::forward<Handler>(handler));
    handlerIdMap_[id] = handlers_.size() - 1;
    fdMap_[fd] = handlers_.size() - 1;
    pollFds.push_back(pollfd { fd, POLLIN, 0 });
    return id;
}
