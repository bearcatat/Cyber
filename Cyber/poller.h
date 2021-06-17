#if !defined(CYBER_POLLER_H)
#define CYBER_POLLER_H
#include <functional>
#include <memory>
#include <mutex>
#include <map>

namespace cyberweb
{
    typedef enum
    {
        EVENT_READ = 1 << 0,
        EVENT_WRITE = 1 << 1,
        EVENT_ERROR = 1 << 2,
        EVENT_LT = 1 << 3
    } PollEvent;
    typedef std::function<void(int event)> PollEventCB;
    typedef std::function<void(bool success)> PollDelCB;

    class EventPoller
    {
    public:
        typedef std::shared_ptr<EventPoller> Ptr;
        EventPoller();
        ~EventPoller();

        static EventPoller *Instance();

        int AddEvent(int fd, int event, PollEventCB cb);

        int DelEvent(int fd, PollDelCB cb = nullptr);

        int ModifyEvent(int fd, int event);

        void RunLoop();

    private:
        static EventPoller *poller_;
        int epoll_fd_;
        std::mutex lock_;
        std::map<int, std::shared_ptr<PollEventCB>> event_map_;
        bool exit_flag_;
    };
    EventPoller *EventPoller::poller_ = nullptr;
} // namespace cyberweb

#endif // CYBER_POLLER_H
