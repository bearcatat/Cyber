#if !defined(CYBER_POLLER_CPP)
#define CYBER_POLLER_CPP
#include <sys/unistd.h>
#include "sys/epoll.h"
#include <map>

#include "poller.h"
#include "time_ticker.h"

#define EPOLL_SIZE 1024
#define SOCKET_DEFAULT_BUF_SIZE 2048

#define ToEpoll(event) (((event)&EVENT_READ) ? EPOLLIN : 0) | (((event)&EVENT_WRITE) ? EPOLLOUT : 0) | (((event)&EVENT_ERROR) ? (EPOLLHUP | EPOLLERR) : 0) | (((event)&EVENT_LT) ? 0 : EPOLLET)

#define ToPoller(event) (((event)&EPOLLIN) ? EVENT_READ : 0) | (((event)&EPOLLOUT) ? EVENT_WRITE : 0) | (((event)&EPOLLHUP) ? EVENT_ERROR : 0) | (((event)&EPOLLERR) ? EVENT_ERROR : 0)

namespace cyberweb
{
    EventPoller *EventPoller::Instance()
    {
        return *(EventPollerPool::Instance().GetFirstPoller());
    }

    EventPoller::EventPoller(ThreadPool::Priority priority = ThreadPool::PRIORITY_HIGHEST)
    {
        priority_ = priority;
        SockUtil::SetNoBlocked(pipe_.ReadFD());
        SockUtil::SetNoBlocked(pipe_.WriteFD());

        epoll_fd_ = epoll_create(EPOLL_SIZE);
        if (epoll_fd_ == -1)
        {
            throw "create epoll fd fail!";
        }

        loop_thread_id_ = std::this_thread::get_id();
        if (-1 == AddEvent(pipe_.ReadFD(), EVENT_READ, [this](int event)
                           { OnPipeEvent(); }))
        {
            throw std::runtime_error("poller adding pipe event failed");
        }
    }

    void EventPoller::Shutdown()
    {
        AsyncL([]()
               { throw ExitException(); },
               false, true);
        if (loop_thread_)
        {
            try
            {
                loop_thread_->join();
            }
            catch (...)
            {
            }
            delete loop_thread_;
            loop_thread_ = nullptr;
        }
    }

    EventPoller::~EventPoller()
    {
        Shutdown();
        Wait();
        if (epoll_fd_ != -1)
        {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
        loop_thread_id_ = std::this_thread::get_id();
        OnPipeEvent();
    }

    int EventPoller::AddEvent(int fd, int event, PollEventCB cb)
    {
        Ticker();
        if (!cb)
        {
            std::cerr << "poller callback is null"
                      << "\n";
            return -1;
        }
        if (IsCurrentThread())
        {
            epoll_event ev = {0};
            ev.events = ToEpoll(event);
            ev.data.fd = fd;
            int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
            if (ret == 0)
            {
                event_map_.emplace(fd, std::make_shared<PollEventCB>(std::move(cb)));
            }
            return ret;
        }
        Async([this, fd, event, cb]()
              { AddEvent(fd, event, std::move(const_cast<PollEventCB &>(cb))); });

        return 0;
    }

    int EventPoller::DelEvent(int fd, PollDelCB cb = nullptr)
    {
        Ticker();
        if (!cb)
        {
            cb = [](bool success) {};
        }

        if (IsCurrentThread())
        {
            bool success = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL) == 0 && event_map_.erase(fd) > 0;
            cb(success);
            return success ? 0 : -1;
        }

        Async([this, fd, cb]()
              { DelEvent(fd, std::move(const_cast<PollDelCB &>(cb))); });

        return 0;
    }

    int EventPoller::ModifyEvent(int fd, int event)
    {
        Ticker();
        epoll_event ev = {0};
        ev.events = ToEpoll(event);
        ev.data.fd = fd;
        std::lock_guard<std::mutex> > guard(lock_);
        return epoll_data(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }

    Task::Ptr EventPoller::Async(TaskIn task, bool may_sync = true) override
    {
        return AsyncL(std::move(task), may_sync, false);
    }

    Task::Ptr EventPoller::AsyncFirst(TaskIn task, bool may_sync = true) override
    {
        return AsyncL(std::move(task), may_sync, true);
    }

    Task::Ptr EventPoller::AsyncL(TaskIn task, bool may_sync = true, bool first = false)
    {
        Ticker();
        if (may_sync && IsCurrentThread())
        {
            task();
            return nullptr;
        }

        auto ret = std::shared_ptr<Task>(std::move(task));
        {
            std::lock_guard<std::mutex> guard(mtx_task_);
            if (first)
            {
                list_task_.EmplaceFront(ret);
            }
            else
            {
                list_task_.EmplaceBack(ret);
            }
        }

        pipe_.Write("", 1);
        return ret;
    }

    bool EventPoller::IsCurrentThread()
    {
        return loop_thread_id_ == std::this_thread::get_id();
    }

    inline void EventPoller::OnPipeEvent()
    {
        Ticker();
        char buf[1024];
        int err = 0;
        do
        {
            if (pipe_.Read(buf, sizeof(buf)) > 0)
            {
                continue;
            }
            err = get_uv_error();
        } while (get_uv_error() != UV_EAGAIN);

        decltype(list_task_) list_swap;
        {
            LOCKGUARD(mtx_task_);
            list_swap.Swap(list_task_);
        }

        list_swap.ForEach(
            [&](const Task::Ptr &task)
            {
                try
                {
                    (*task)();
                }
                catch (ExitException &)
                {
                    exit_flag_ = true;
                }
                catch (const std::exception &e)
                {
                    std::cerr << e.what() << '\n';
                }
            });
    }

    void EventPoller::Wait()
    {
        LOCKGUARD(mtx_runing_);
    }

    BufferRaw::Ptr EventPoller::GetSharedBuffer()
    {
        BufferRaw::Ptr buffer = shared_buffer_.lock();
        if (!buffer)
        {
            buffer = BufferRaw::Create();
            buffer->SetCapacity(SOCKET_DEFAULT_BUF_SIZE + 1);
            shared_buffer_ = buffer;
        }
        return buffer;
    }

    static std::mutex s_all_poller_mtx;
    static std::map<std::thread::id, std::weak_ptr<EventPoller>> s_all_poller;

    EventPoller::Ptr EventPoller::GetCurrentPoller()
    {
        LOCKGUARD(s_all_poller_mtx);
        auto it = s_all_poller.find(std::this_thread::get_id());
        if (it == s_all_poller.end())
        {
            return nullptr;
        }
        return it->second.lock();
    }

    void EventPoller::RunLoop(bool blocked, bool regist_self)
    {
        if (blocked)
        {
            ThreadPool::SetPriority(priority_);
            LOCKGUARD(mtx_runing_);
            loop_thread_id_ = std::this_thread::get_id();
            if (regist_self)
            {
                LOCKGUARD(s_all_poller_mtx);
                s_all_poller[loop_thread_id_] = shared_from_this();
            }
            sem_run_started_.Post();
            exit_flag_ = false;
            uint64_t min_delay;
            epoll_event events[EPOLL_SIZE];
            while (!exit_flag_)
            {
                min_delay = GetMinDelay();
                StartSleep();
                int ret = epoll_wait(epoll_fd_, events, EPOLL_SIZE, min_delay ? min_delay : -1);
                SleepWakeUp();
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
                        std::cerr << "EventPoller callback fail" << ex.what();
                    }
                }
            }
        }
        else
        {
            loop_thread_ = new std::thread(&EventPoller::RunLoop, this, true, regist_self);
            sem_run_started_.Wait();
        }
    }

    uint64_t EventPoller::FlushDelayTask(uint64_t now_time)
    {
        decltype(delay_task_map_) task_copy;
        task_copy.swap(delay_task_map_);

        for (auto it = task_copy.begin(); it != task_copy.end() && it->first <= now_time; it = task_copy.erase())
        {
            try
            {
                /* code */
                auto next_delay = (*(it->second))();
                if (next_delay)
                {
                    delay_task_map_.emplace(next_delay, it->second);
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << '\n';
            }
        }

        task_copy.insert(delay_task_map_.begin(), delay_task_map_.end());
        task_copy.swap(delay_task_map_);
        auto it = delay_task_map_.begin();
        if (it == delay_task_map_.end())
        {
            return 0;
        }
        return it->first;
    }

    uint64_t EventPoller::GetMinDelay()
    {
        auto it = delay_task_map_.begin();
        if (it == delay_task_map_.end())
        {
            return 0;
        }
        auto now = GetCurrentMillisecond();
        if (it->first > now)
        {
            return it->first;
        }
        return FlushDelayTask(now);
    }

    DelayTask::Ptr EventPoller::DoDelayTask(uint64_t delay_ms, std::function<uint64_t()> task)
    {
        DelayTask::Ptr ret = std::shared_ptr<DelayTask>(std::move(task));
        auto time_line = delay_ms + GetCurrentMillisecond();
        AsyncFirst([this, time_line, ret]()
                   { delay_task_map_.emplace(time_line, ret); });
        return ret;
    }

    int s_pool_size = 0;

    EventPollerPool &EventPollerPool::Instance()
    {
        static std::shared_ptr<EventPollerPool> s_instance(new EventPollerPool());
        static EventPoller &s_ref = *s_instance;
        return s_ref;
    }

    EventPoller::Ptr EventPollerPool::GetFirstPoller()
    {
        return std::dynamic_pointer_cast<EventPoller>(threads_.front());
    }

    EventPoller::Ptr EventPollerPool::GetPoller()
    {
        auto poller = EventPoller::GetCurrentPoller();
        if (prefer_current_thread_ && poller)
        {
            return poller;
        }

        return std::dynamic_pointer_cast<EventPoller>(GetExecutor());
    }

    void EventPollerPool::PreferCurrentThread(bool flag = true)
    {
        prefer_current_thread_ = flag;
    }

    EventPollerPool::EventPollerPool()
    {
        auto size = s_pool_size > 0 ? s_pool_size : std::thread::hardware_concurrency;
        CreateThreads([]()
                      {
                          EventPoller::Ptr ret(new EventPoller());
                          ret->RunLoop();
                          return ret;
                      },
                      size);
    }

    void EventPollerPool::SetPoolSize(int size)
    {
        s_pool_size = size;
    }

    

} // namespace cyberweb

#endif // CYBER_POLLER_CPP
