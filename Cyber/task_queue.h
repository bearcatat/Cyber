#if !defined(CYBER_TASK_QUEUE_H)
#define CYBER_TASK_QUEUE_H

#include <mutex>
#include "util.h"
#include "semaphore.h"

namespace cyberweb
{
    template <typename T>
    class TaskQueue
    {

    public:
        template <typename C>
        void PushBack(C &&task_func)
        {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                queue_.EmplaceBack(std::forward<C>(task_func));
            }
            sem_.Post();
        }

        template <typename C>
        void PushBackFirst(C &&task_func)
        {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                queue_.EmplaceFront(std::forward<C>(task_func));
            }
            sem_.Post();
        }

        void PushExit(size_t n)
        {
            sem_.Post(n);
        }

        bool GetTask(T &task)
        {
            sem_.Wait();
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.Size() == 0)
            {
                return false;
            }
            task = std::move(queue_.Front());
            queue_.PopFront();
            return true;
        }

        size_t Size()
        {
            return queue_.Size();
        }

    private:
        List<T> queue_;
        mutable std::mutex mutex_;
        Semaphore sem_;
    };

} // namespace cyberweb

#endif // CYBER_TASK_QUEUE_H
