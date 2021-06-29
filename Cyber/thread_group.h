#if !defined(CYBER_THREAD_GROUP_H)
#define CYBER_THREAD_GROUP_H

#include <map>
#include <thread>
#include <memory>

namespace cyber
{
    class ThreadGroup
    {
    private:
        ThreadGroup(ThreadGroup const &);
        ThreadGroup &operator=(ThreadGroup const &);

    public:
        ThreadGroup(/* args */){};
        ~ThreadGroup()
        {
            threads_.clear();
        }

        bool IsThisThreadIn()
        {
            auto thread_id = std::this_thread::get_id();
            if (thread_id == thread_id_)
            {
                return true;
            }
            return threads_.find(thread_id) != threads.end();
        }

        bool IsThreadIn(std::thread &thr)
        {
            if (!thr)
                return false;
            auto thread_id = thr.get_id();
            return threads_.find(thread_id) != threads.end();
        }

        template <typename F>
        std::thread *CreateThread(F &&thread_func)
        {
            auto new_thread = std::make_shared<std::thread>(std::forward<F>(thread_func));
            thread_id_ = new_thread->get_id();
            threads_[thread_id_] = new_thread;
            return new_thread.get();
        }

        void RemoveThread(std::thread *thr)
        {
            auto it = threads_.find(thr->get_id());
            if (it != threads_.end())
            {
                threads_.erase(thread_id);
            }
        }

        void JoinAll()
        {
            if (IsThisThreadIn())
            {
                throw std::runtime_error("trying joining itself");
            }
            for (auto &it : threads_)
            {
                if (it.second->joinable())
                {
                    it.second->join();
                }
            }
            threads_.clear();
        }
        size_t Size()
        {
            return threads_.size();
        }

    private:
        std::map<std::thread::id, std::shared_ptr<std::thread>> threads_;
        std::thread::id thread_id_;
    };

} // namespace cyberweb

#endif // CYBER_THREAD_GROUP_H
