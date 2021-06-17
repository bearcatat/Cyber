#if !defined(CYBER_POLLER_CPP)
#define CYBER_POLLER_CPP
#include "poller.h"
#include <sys/unistd.h>
#include "sys/epoll.h"

#define EPOLL_SIZE 1024

#define ToEpoll(event) (((event)&EVENT_READ) ? EPOLLIN : 0) | (((event)&EVENT_WRITE) ? EPOLLOUT : 0) | (((event)&EVENT_ERROR) ? (EPOLLHUP | EPOLLERR) : 0) | (((event)&EVENT_LT) ? 0 : EPOLLET)

#define ToPoller(event) (((event)&EPOLLIN) ? EVENT_READ : 0) | (((event)&EPOLLOUT) ? EVENT_WRITE : 0) | (((event)&EPOLLHUP) ? EVENT_ERROR : 0) | (((event)&EPOLLERR) ? EVENT_ERROR : 0)

namespace cyberweb
{
    EventPoller *EventPoller::Instance()
    {
        if (!poller_)
        {
            poller_ = new EventPoller();
        }
        return poller_;
    }

    EventPoller::EventPoller()
    {
        epoll_fd_ = epoll_create(EPOLL_SIZE);
        if (epoll_fd_ == -1)
        {
            throw "create epoll fd fail!";
        }
    }

    EventPoller::~EventPoller()
    {
        if (epoll_fd_ != -1)
        {
            close(epoll_fd_);
        }
    }

    int EventPoller::AddEvent(int fd, int event, PollEventCB cb)
    {
        epoll_event ev = {0};
        ev.events = ToEpoll(event);
        ev.data.fd = fd;
        std::lock_guard<std::mutex> > guard(lock_);
        int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
        if (ret == 0)
        {
            event_map_.emplace(fd, std::make_shared<PollEventCB>(std::move(cb)));
        }
        return ret;
    }

    int EventPoller::DelEvent(int fd, PollDelCB cb = nullptr)
    {
        if (!cb)
        {
            cb = [](bool success) {};
        }
        lock_.lock();
        bool success = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL) == 0 && event_map_.erase(fd) > 0;
        lock_.unlock();
        cb(success);
        return success ? 0 : -1;
    }

    int EventPoller::ModifyEvent(int fd, int event)
    {
        epoll_event ev = {0};
        ev.events = ToEpoll(event);
        ev.data.fd = fd;
        std::lock_guard<std::mutex> > guard(lock_);
        return epoll_data(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }

    void EventPoller::RunLoop()
    {
        exit_flag_ = false;
        epoll_event events[EPOLL_SIZE];
        while (!exit_flag_)
        {
            int ret = epoll_wait(epoll_fd_, events, EPOLL_SIZE, -1);
            if (ret <= 0)
            {
                continue;
            }
            for (int i = 0; i < ret; ++i)
            {
                epoll_event &ev = events[i];
                int fd = ev.data.fd;
                auto it = event_map_.find(fd);
                if (it == event_map_.end())
                {
                    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL);
                    continue;
                }
                auto cb = it->second;
                try
                {
                    (*cb)(ToPoller(ev.events));
                }
                catch (std::exception &ex)
                {
                    throw "EventPoller callback fail";
                }
            }
        }
    }

} // namespace cyberweb

#endif // CYBER_POLLER_CPP
