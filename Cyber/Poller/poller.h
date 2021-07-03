#if !defined(CYBER_POLLER_H)
#define CYBER_POLLER_H

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "buffer.h"
#include "task_executor.h"
#include "thread_pool.h"
#include "pipe_wrap.h"
#include "logger.h"

namespace cyber
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
    typedef TaskCancelableImp<uint64_t(void)> DelayTask;

    class EventPoller : public TaskExecutor, public std::enable_shared_from_this<EventPoller>
    {
    public:
        typedef std::shared_ptr<EventPoller> Ptr;

        friend class EventPollerPool;
        friend class WorkThreadPool;

        ~EventPoller();

        static EventPoller &Instance();

        int AddEvent(int fd, int event, PollEventCB cb);

        int DelEvent(int fd, PollDelCB cb = nullptr);

        int ModifyEvent(int fd, int event);

        Task::Ptr Async(TaskIn task, bool may_sync = true) override;

        Task::Ptr AsyncFirst(TaskIn task, bool may_sync = true) override;

        bool IsCurrentThread();

        DelayTask::Ptr DoDelayTask(uint64_t delay_ms, std::function<uint64_t()> task);

        static EventPoller::Ptr GetCurrentPoller();

        BufferRaw::Ptr GetSharedBuffer();

    private:
        EventPoller(ThreadPool::Priority priority = ThreadPool::PRIORITY_HIGHEST);

        void RunLoop(bool blocked, bool regist_self);

        void OnPipeEvent();

        Task::Ptr AsyncL(TaskIn task, bool may_sync = true, bool first = false);

        void Wait();

        void Shutdown();

        uint64_t FlushDelayTask(uint64_t now_time);

        uint64_t GetMinDelay();

    private:
        class ExitException : public std::exception
        {
        public:
            ExitException(/* args */) {}
            ~ExitException() {}
        };

    private:
        std::weak_ptr<BufferRaw> shared_buffer_;
        int epoll_fd_;
        std::mutex lock_;
        std::unordered_map<int, std::shared_ptr<PollEventCB>> event_map_;
        bool exit_flag_;

        ThreadPool::Priority priority_;
        std::mutex mtx_runing_;
        std::thread *loop_thread_ = nullptr;
        std::thread::id loop_thread_id_;
        Semaphore sem_run_started_;

        PipeWrap pipe_;
        std::mutex mtx_task_;
        List<Task::Ptr> list_task_;

        Logger::Ptr logger_;

        std::multimap<uint64_t, DelayTask::Ptr> delay_task_map_;
    };

    class EventPollerPool : public std::enable_shared_from_this<EventPollerPool>, public TaskExecutorGetterImp
    {
    public:
        typedef std::shared_ptr<EventPollerPool> Ptr;
        ~EventPollerPool(){};

        static EventPollerPool &Instance();

        static void SetPoolSize(int size = 0);

        EventPoller::Ptr GetFirstPoller();

        EventPoller::Ptr GetPoller();

        void PreferCurrentThread(bool flag = true);

    private:
        EventPollerPool(/* args */);

    private:
        bool prefer_current_thread_ = true;
    };

} // namespace cyberweb

#endif // CYBER_POLLER_H
