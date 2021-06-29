#if !defined(CYBER_THREAD_POOL_H)
#define CYBER_THREAD_POOL_H

#include <thread>
#include <memory>

#include "task_executor.h"
#include "thread_group.h"
#include "task_queue.h"

namespace cyber
{
    class ThreadPool : public TaskExecutor
    {
    public:
        enum Priority
        {
            PRIORITY_LOWEST = 0,
            PRIORITY_LOW,
            PRIORITY_NORMAL,
            PRIORITY_HIGH,
            PRIORITY_HIGHEST
        };
        ThreadPool(int num = 1, Priority priority = PRIORITY_HIGHEST, bool auto_run = true)
            : thread_num_(num), priority_(priority)
        {
            if (auto_run)
            {
                Start();
            }
        }

        ~ThreadPool()
        {
            ShutDown();
            Wait();
        }

        Task::Ptr Async(TaskIn &&task, bool may_sync) override
        {
            if (may_sync && thread_group_.IsThisThreadIn())
            {
                task();
                return nullptr;
            }
            auto ret = std::make_shared<Task>(task);
            queue_.PushBack(ret);
            return ret;
        }

        Task::Ptr AsyncFirst(TaskIn &&task, bool may_sync) override
        {
            if (may_sync && thread_group_.IsThisThreadIn())
            {
                task();
                return nullptr;
            }
            auto ret = std::make_shared<Task>(task);
            queue_.PushBackFirst(ret);
            return ret;
        }

        size_t Size()
        {
            return queue_.Size();
        }

        static bool SetPriority(Priority priority = PRIORITY_NORMAL, std::thread::native_handle_type thread_id = 0)
        {
            static int Min = sched_get_priority_min(SCHED_OTHER);
            if (Min == -1)
            {
                return false;
            }
            static int Max = sched_get_priority_max(SCHED_OTHER);
            if (Max == -1)
            {
                return false;
            }
            static int Priorities[] = {
                Min, Min + (Max - Min) / 4,
                Min + (Max - Min) / 2, Min + (Max - Min) * 3 / 4, Max};

            if (thread_id == 0)
            {
                thread_id = pthread_self();
            }

            sched_param params;

            params.sched_priority = Priorities[priority];

            return pthread_setschedparam(thread_id, SCHED_OTHER, &params) == 0;
        }

        void Start()
        {
            if (thread_num_ <= 0)
            {
                return;
            }
            size_t total = thread_num_ - thread_group_.Size();
            for (int i = 0; i < total; i++)
            {
                thread_group_.CreateThread(std::bind(&ThreadPool::Run, this));
            }
        }

    private:
        void Run()
        {
            ThreadPool::SetPriority(priority_);
            Task::Ptr task;
            while (true)
            {
                StartSleep();
                if (!queue_.GetTask(task))
                {
                    break;
                }
                SleepWakeUp();
                try
                {
                    (*task)();
                    task = nullptr;
                }
                catch (const std::exception &e)
                {
                    std::cerr << "ThreadPoll error" << e.what() << '\n';
                }
            }
        }
        void Wait()
        {
            thread_group_.JoinAll();
        }
        void ShutDown()
        {
            queue_.PushExit();
        }

    private:
        size_t thread_num_;
        Priority priority_;
        TaskQueue<Task::Ptr> queue_;
        ThreadGroup thread_group_;
    };

} // namespace cyberweb

#endif // CYBER_THREAD_POOL_H
