#include "eventhandler_linux.hpp"

#include <cassert>

#include <limits.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
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

EventHandlerImpl::EventHandlerImpl()
    : inotifyFd(::inotify_init())
{
    if (inotifyFd < 0) {
        std::perror("inotify_init");
        std::exit(1);
    }
    pollFds.push_back(pollfd { inotifyFd, POLLIN, 0 });
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
    return addHandlerFd(SignalHandler { callback, Fd(fd) }, fd);
}

EventHandlerImpl::HandlerId EventHandlerImpl::addTimer(
    uint64_t /*interval*/, uint64_t /*expiration*/, std::function<void()> /*callback*/)
{
    return 0;
}

EventHandlerImpl::HandlerId EventHandlerImpl::addFilesystemHandler(
    const fs::path& path, std::function<void()> callback)
{
    // re IN_CLOSE_WRITE: http://inotify.aiken.cz/?section=inotify&page=faq&lang=en
    const auto wd = ::inotify_add_watch(inotifyFd, path.c_str(), IN_ALL_EVENTS);
    debug("add watch: {} ({})", path.c_str(), wd);
    if (wd < 0) {
        perror("inotify_add_watch");
        return 1;
    }
    return addHandlerWd(FilesystemHandler { callback, path, wd }, wd);
}

EventHandlerImpl::HandlerId EventHandlerImpl::addFdHandler(int fd, std::function<void()> callback)
{
    return addHandlerFd(FdHandler { callback }, fd);
}

std::pair<EventHandlerImpl::HandlerId, CustomEvent> EventHandlerImpl::addCustomHandler(
    std::function<void()> callback)
{
    const auto fd = ::eventfd(0, 0);
    if (fd < 0) {
        std::perror("eventfd");
        std::exit(1);
    }
    const auto id = addHandlerFd(CustomHandler { callback, Fd(fd) }, fd);
    return std::pair<HandlerId, CustomEvent>(
        id, CustomEvent(std::make_unique<CustomEventImpl>(fd)));
}

void EventHandlerImpl::removeHandler(HandlerId id)
{
    const auto index = handlerIdMap_.at(id);
    if (const auto fh = std::get_if<FilesystemHandler>(&handlers_[index])) {
        debug("delete watch: {}", fh->wd);
        wdMap_.erase(fh->wd);
        ::inotify_rm_watch(inotifyFd, fh->wd);
    } else {
        const auto fdIt = std::find_if(fdMap_.begin(), fdMap_.end(),
            [index](const auto& elem) { return elem.second == index; });
        assert(fdIt != fdMap_.end());
        const auto fd = fdIt->first;
        const auto pfdIt = std::find_if(
            pollFds.begin(), pollFds.end(), [fd](const auto& pfd) { return pfd.fd == fd; });
        assert(pfdIt != pollFds.end());
        pollFds.erase(pfdIt);
    }
    handlers_.erase(handlers_.begin() + index);
    handlerIdMap_.erase(id);
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
    // it's possible for them to remove handlers (so pollFds doesn't change during iteration).
    // We save the ids so if a handler is removed, it's not called during this call of
    // processEvents. Future you, you wanted to collect the callbacks itself and if you did, they
    // would still be called.
    // Example: If a callback (i.e. input - Ctrl-W) closes a buffer and you
    // remove the filesystem watch, you want the filesystem watch to not be called anymore
    // (potentially with a dangling pointer).
    std::vector<int> fds;
    fds.reserve(pollFds.size());
    for (const auto& pfd : pollFds) {
        if (pfd.revents & POLLIN)
            fds.push_back(pfd.fd);
    }

    for (const auto fd : fds) {
        if (fd == inotifyFd) {
            // If there are even more events, we'll get 'em later
            static constexpr auto eventBufLen = 16 * (sizeof(inotify_event) + NAME_MAX + 1);
            static char eventBuffer[eventBufLen];

            std::vector<int> wdsIgnored;

            const auto len = ::read(inotifyFd, eventBuffer, eventBufLen);
            if (len < 0 && errno != EAGAIN) {
                perror("read");
                std::exit(1);
            }
            debug("read {} from inotify fd", len);

            long i = 0;
            while (i < len) {
                const auto event = reinterpret_cast<inotify_event*>(&eventBuffer[i]);
                debug("wd: {}, mask: {}", event->wd, event->mask);
                if (event->mask & IN_ACCESS)
                    debug("IN_ACCESS");
                if (event->mask & IN_MODIFY)
                    debug("IN_MODIFY");
                if (event->mask & IN_ATTRIB)
                    debug("IN_ATTRIB");
                if (event->mask & IN_CLOSE_WRITE)
                    debug("IN_CLOSE_WRITE");
                if (event->mask & IN_CLOSE_NOWRITE)
                    debug("IN_CLOSE_NOWRITE");
                if (event->mask & IN_OPEN)
                    debug("IN_OPEN");
                if (event->mask & IN_MOVED_FROM)
                    debug("IN_MOVED_FROM");
                if (event->mask & IN_MOVED_TO)
                    debug("IN_MOVED_TO");
                if (event->mask & IN_CREATE)
                    debug("IN_CREATE");
                if (event->mask & IN_DELETE)
                    debug("IN_DELETE");
                if (event->mask & IN_DELETE_SELF)
                    debug("IN_DELETE_SELF");
                if (event->mask & IN_MOVE_SELF)
                    debug("IN_MOVE_SELF");
                if (event->mask & IN_UNMOUNT)
                    debug("IN_UNMOUNT");
                if (event->mask & IN_Q_OVERFLOW)
                    debug("IN_Q_OVERFLOW");
                if (event->mask & IN_IGNORED)
                    debug("IN_IGNORED");
                if (event->len > 0)
                    debug("name: {}", std::string_view(event->name, event->len));
                // len = 0, name empty, cookie unused
                const auto it = wdMap_.find(event->wd);
                if (it != wdMap_.end()) { // == end => handler was probably removed
                    // vim `:w` generates: IN_MOVE_SELF, IN_ATTRIB, IN_DELETE_SELF, IN_IGNORED
                    // so I'll just interpret IN_ATTRIB as a modification too.
                    // I kind of want to reload on `touch` anyway.
                    if (event->mask & (IN_CLOSE_WRITE | IN_ATTRIB)) {
                        const auto fh = std::get_if<FilesystemHandler>(&handlers_[it->second]);
                        assert(fh);
                        fh->callback();
                    }
                    if (event->mask & IN_IGNORED) {
                        wdsIgnored.push_back(event->wd);
                    }
                }
                i += sizeof(inotify_event) + event->len;
            }

            // Some programs write to a different file and rename, which deletes the old file,
            // so we need to re-watch them.
            for (const auto wd : wdsIgnored) {
                const auto idx = wdMap_.at(wd);
                const auto fh = std::get_if<FilesystemHandler>(&handlers_[idx]);
                assert(fh);
                ::inotify_rm_watch(inotifyFd, wd);
                wdMap_.erase(wd);
                fh->wd = ::inotify_add_watch(inotifyFd, fh->path.c_str(), IN_ALL_EVENTS);
                wdMap_[fh->wd] = idx;
            }
        } else {
            const auto it = fdMap_.find(fd);
            if (it == fdMap_.end()) // handler was probably removed
                continue;
            const auto& handler = handlers_[it->second];
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
}

EventHandlerImpl::HandlerId EventHandlerImpl::addHandler(Handler&& handler)
{
    const auto id = handlerIdCounter_++;
    handlers_.push_back(std::forward<Handler>(handler));
    handlerIdMap_[id] = handlers_.size() - 1;
    return id;
}

EventHandlerImpl::HandlerId EventHandlerImpl::addHandlerFd(Handler&& handler, int fd)
{
    const auto id = addHandler(std::move(handler));
    assert(fdMap_.count(fd) == 0);
    fdMap_[fd] = handlers_.size() - 1;
    pollFds.push_back(pollfd { fd, POLLIN, 0 });
    return id;
}

EventHandlerImpl::HandlerId EventHandlerImpl::addHandlerWd(Handler&& handler, int wd)
{
    const auto id = addHandler(std::move(handler));
    assert(wdMap_.count(wd) == 0);
    wdMap_[wd] = handlers_.size() - 1;
    return id;
}
