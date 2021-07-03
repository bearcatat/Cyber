#if !defined(CYBER_TASK_EXECUTOR_H)
#define CYBER_TASK_EXECUTOR_H

#include <memory>
#include <functional>
#include <mutex>
#include <vector>
#include <thread>

#include "../Util/util.h"
#include "../Util/time_ticker.h"
#include "../Util/onceToken.h"

#define LOCKGUARD(mtx) std::lock_guard<decltype(mtx)> Guard(mtx)

namespace cyber
{
    class ThreadLoadCounter
    {
    public:
        ThreadLoadCounter(uint64_t max_size, uint64_t max_usec)
            : max_size_(max_size), max_usec_(max_usec)
        {
            last_sleep_time_ = last_wake_time_ = GetCurrentMicrosecond();
        }
        ~ThreadLoadCounter() {}

        void StartSleep()
        {
            LOCKGUARD(mtx_);

            sleeping_ = true;

            auto current_time = GetCurrentMicrosecond();
            auto run_time = current_time - last_wake_time_;
            last_wake_time_ = current_time;

            time_list_.emplace_back(run_time, false);

            if (time_list_.size() > max_size_)
            {
                time_list_.pop_front();
            }
        }

        void SleepWakeUp()
        {
            LOCKGUARD(mtx_);

            sleeping_ = false;

            auto current_time = GetCurrentMicrosecond();
            auto sleep_time = current_time - last_sleep_time_;
            last_sleep_time_ = current_time;

            time_list_.emplace_back(sleep_time, true);

            if (time_list_.size() > max_size_)
            {
                time_list_.pop_front();
            }
        }

        int Load()
        {
            LOCKGUARD(mtx_);
            uint64_t total_sleep_time = 0, total_run_time = 0;

            time_list_.ForEach(
                [&](const TimeRecord &tr)
                {
                    if (tr.sleep_)
                    {
                        total_sleep_time += tr.time_;
                    }
                    else
                    {
                        total_run_time += tr.time_;
                    }
                });

            if (sleeping_)
            {
                total_sleep_time += (GetCurrentMicrosecond() - last_sleep_time_);
            }
            else
            {
                total_run_time += (GetCurrentMicrosecond() - last_wake_time_);
            }

            uint64_t total_time = total_run_time + total_sleep_time;
            while (time_list_.size() != 0 && (total_time > max_usec_ || time_list_.size() > max_size_))
            {
                TimeRecord &tr = time_list_.front();
                if (tr.sleep_)
                {
                    total_sleep_time -= tr.time_;
                }
                else
                {
                    total_run_time -= tr.time_;
                }
                total_time -= tr.time_;
                time_list_.pop_front();
            }
            if (total_time == 0)
            {
                return 0;
            }
            return (int)(total_run_time * 100 / total_time);
        }

    private:
        class TimeRecord
        {
        public:
            TimeRecord(uint64_t tm, bool slp)
            {
                time_ = tm;
                sleep_ = slp;
            }

        public:
            uint64_t time_;
            bool sleep_;
        };

    private:
        uint64_t last_sleep_time_;
        uint64_t last_wake_time_;
        List<TimeRecord> time_list_;
        bool sleeping_ = true;
        uint64_t max_size_;
        uint64_t max_usec_;
        std::mutex mtx_;
    };

    class TaskCancelable : public noncopyable
    {
    public:
        TaskCancelable() = default;
        virtual ~TaskCancelable() = default;
        virtual void Cancel() = 0;
    };

    template <class R, class... ArgTypes>
    class TaskCancelableImp;

    template <class R, class... ArgTypes>
    class TaskCancelableImp<R(ArgTypes...)> : public TaskCancelable
    {
    public:
        typedef std::shared_ptr<TaskCancelableImp> Ptr;
        typedef std::function<R(ArgTypes...)> FuncType;

        ~TaskCancelableImp() = default;

        template <typename FUNC>
        TaskCancelableImp(FUNC &&task)
        {
            strong_task_ = std::make_shared<FuncType>(std::forward<FUNC>(task));
            weak_task_ = strong_task_;
        }

        void Cancel() override
        {
            strong_task_ = nullptr;
        }

        operator bool()
        {
            return strong_task_ && *strong_task_;
        }

        void operator=(nullptr_t)
        {
            strong_task_ = nullptr;
        }

        R operator()(ArgTypes... args) const
        {
            auto strong_task = weak_task_.lock();
            if (strong_task && *strong_task)
            {
                return (*strong_task)(std::forward<ArgTypes>(args)...);
            }
            return DefaultValue<R>();
        }

        template <typename T>
        static typename std::enable_if<std::is_void<T>::value, void>::type
        DefaultValue() {}

        template <typename T>
        static typename std::enable_if<std::is_pointer<T>::value, T>::type
        DefaultValue() { return nullptr; }

        template <typename T>
        static typename std::enable_if<std::is_integral<T>::value, T>::type
        DefaultValue() { return 0; }

    protected:
        std::shared_ptr<FuncType> strong_task_;
        std::weak_ptr<FuncType> weak_task_;
    };

    typedef std::function<void()> TaskIn;
    typedef TaskCancelableImp<void()> Task;

    class TaskExecutorInterface
    {
    public:
        TaskExecutorInterface(/* args */) = default;
        virtual ~TaskExecutorInterface() = default;

        virtual Task::Ptr Async(TaskIn task, bool may_sync = true) = 0;

        virtual Task::Ptr AsyncFirst(TaskIn task, bool may_sync = true)
        {
            return Async(std::move(task), may_sync);
        }

        virtual void Sync(TaskIn &task)
        {
            std::mutex lock;
            auto ret = Async(
                [&]()
                {
                    OnceToken token(nullptr, [&]()
                                    { lock.unlock(); });
                    task();
                });
            if (ret && *ret)
            {
                lock.lock();
            }
        }

        virtual void SyncFirst(TaskIn &task)
        {
            std::mutex lock;
            auto ret = AsyncFirst(
                [&]()
                {
                    OnceToken token(nullptr, [&]()
                                    { lock.unlock(); });
                    task();
                });
            if (ret && *ret)
            {
                lock.lock();
            }
        }
    };

    class TaskExecutor : public ThreadLoadCounter, public TaskExecutorInterface
    {
    public:
        typedef std::shared_ptr<TaskExecutor> Ptr;
        TaskExecutor(uint64_t max_size = 32, uint64_t max_usec = 2 * 1000 * 1000) : ThreadLoadCounter(max_size, max_usec) {}
        ~TaskExecutor() {}
    };

    class TaskExecutorGetter
    {
    public:
        typedef std::shared_ptr<TaskExecutorGetter> Ptr;
        virtual ~TaskExecutorGetter(){};

        virtual TaskExecutor::Ptr GetExecutor() = 0;
    };

    class TaskExecutorGetterImp : public TaskExecutorGetter
    {
    public:
        TaskExecutorGetterImp() {}
        ~TaskExecutorGetterImp() {}

        TaskExecutor::Ptr GetExecutor() override
        {
            auto thread_pos = thread_pos_;
            if (thread_pos >= threads_.size())
            {
                thread_pos = 0;
            }
            TaskExecutor::Ptr executor_min_load = threads_[thread_pos];
            auto min_load = executor_min_load->Load();

            for (size_t i = 0; i < threads_.size(); ++i, ++thread_pos)
            {
                if (thread_pos >= threads_.size())
                {
                    thread_pos = 0;
                }
                auto executor = threads_[thread_pos];
                auto load = executor->Load();
                if (load < min_load)
                {
                    min_load = load;
                    executor_min_load = executor;
                }
                if (min_load == 0)
                {
                    break;
                }
            }
            thread_pos_ = thread_pos;
            return executor_min_load;
        }

        std::vector<int> GetExecutorLoad()
        {
            std::vector<int> vec(threads_.size());
            for (int i = 0; i < threads_.size(); i++)
            {
                vec[i] = threads_[i]->Load();
            }
            return vec;
        }

        void GetExecutorDelay(const std::function<void(const std::vector<int> &)> &callback)
        {
            std::shared_ptr<std::vector<int>> delay_vec = std::make_shared<std::vector<int>>(threads_.size());
            std::shared_ptr<void> finished(nullptr, [callback, delay_vec](void *)
                                           { callback((*delay_vec)); });

            int index = 0;
            for (auto &th : threads_)
            {
                std::shared_ptr<Ticker> delay_tiker = std::make_shared<Ticker>();
                th->Async([finished, delay_vec, index, delay_tiker]()
                          { (*delay_vec)[index] = (int)delay_tiker->EleapsedTime(); },
                          false);
                index++;
            }
        }

        template <typename FUN>
        void ForEach(FUN &&fun)
        {
            for (auto &th : threads_)
            {
                fun(th);
            }
        }

    protected:
        template <typename FUN>
        void CreateThreads(FUN &&fun, int threadnum = std::thread::hardware_concurrency)
        {
            for (int i = 0; i < threadnum; i++)
            {
                threads_.emplace_back(fun());
            }
        }

    protected:
        size_t thread_pos_ = 0;
        std::vector<TaskExecutor::Ptr> threads_;
    };

} // namespace cyberweb

#endif // CYBER_TASK_EXECUTOR_H
