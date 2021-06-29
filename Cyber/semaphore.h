#if !defined(CYBER_SEMAPHORE_H)
#define CYBER_SEMAPHORE_H

#include <atomic>
#include <mutex>
#include <condition_variable>

namespace cyber
{
    class Semaphore
    {
    public:
        Semaphore()
        {
            count_ = 0;
        }
        ~Semaphore() {}

        void Post(size_t n = 1)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            count_ += n;
            if (n == 1)
            {
                condition_.notify_one();
            }
            else
            {
                condition_.notify_all();
            }
        }

        void Wait()
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (count_ == 0)
            {
                condition_.wait(lock);
            }
            count_--;
        }

    private:
        size_t count_;
        std::mutex mutex_;
        std::condition_variable_any condition_;
    };

} // namespace cyberweb

#endif // CYBER_SEMAPHORE_H
