#if !defined(CYBER_UTIL_H)
#define CYBER_UTIL_H

#include <list>
#include <chrono>
#include <atomic>
#include <thread>
#include <iostream>
#include <unistd.h>

#include "onceToken.h"

namespace cyberweb
{
    class noncopyable
    {
    protected:
        noncopyable() {}
        ~noncopyable() {}

    private:
        noncopyable(const noncopyable &that) = delete;
        noncopyable(noncopyable &&that) = delete;
        noncopyable &operator=(const noncopyable &that) = delete;
        noncopyable &operator=(noncopyable &&that) = delete;
    };

    template <typename T>
    class List
    {
    public:
        List() {}
        List(List &&that)
        {
            list_.swap(that->list_);
        }
        ~List()
        {
            list_.clear();
        }

        template <typename FUN>
        void ForEach(FUN &&fun)
        {
            for (auto ele : list_)
            {
                fun(ele);
            }
        }
        size_t Size() const
        {
            return list_.size();
        }
        bool Empty() const
        {
            return list_.empty();
        }
        template <class... Args>
        void EmplaceFront(Args &&...args)
        {
            list_.emplace_front(std::forward(args)...);
        }
        template <class... Args>
        void EmplaceBack(Args &&...args)
        {
            list_.emplace_back(std::forward(args)...);
        }
        T &Front() const
        {
            return list_.front();
        }
        T &Back() const
        {
            return list_.back();
        }
        T &operator[](size_t pos)
        {
            for (auto it = list_.begin(); it != list_.end(); it++)
            {
                if ((pos--) == 0)
                {
                    return *it;
                }
            }
        }
        void PopFront()
        {
            list_.pop_front();
        }
        void PopBack()
        {
            list_.pop_back();
        }
        void Swap(List &other)
        {
            list_.swap(other->list_);
        }
        void Append(List &other)
        {
            list_.splice(list_.back(), other.list_);
        }

    private:
        std::list<T> list_;
    };

    static inline uint64_t GetCurrentMicrosecondOrigin()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static std::atomic<uint64_t> s_current_microsecond(0);
    static std::atomic<uint64_t> s_current_millisecond(0);
    static std::atomic<uint64_t> s_current_microsecond_system(GetCurrentMicrosecondOrigin());
    static std::atomic<uint64_t> s_current_millisecond_system(GetCurrentMicrosecondOrigin() / 1000);

    static inline bool InitMillisecondThread()
    {
        static std::thread s_thread(
            []()
            {
                uint64_t last = GetCurrentMicrosecondOrigin();
                uint64_t now;
                uint64_t microsecond = 0;
                while (true)
                {
                    now = GetCurrentMicrosecondOrigin();

                    s_current_microsecond_system.store(now, std::memory_order_release);
                    s_current_millisecond_system.store(now / 1000, std::memory_order_release);

                    int64_t expired = now - last;
                    last = now;
                    if (expired > 0 && expired < 1000 * 1000)
                    {
                        microsecond += expired;
                        s_current_microsecond.store(microsecond, std::memory_order_release);
                        s_current_millisecond.store(microsecond / 1000, std::memory_order_release);
                    }
                    else if (expired != 0)
                    {
                        std::cerr << "Stamp expired is not abnormal" << expired;
                    }
                    usleep(500);
                }
            });
        static OnceToken s_token([]()
                                 { s_thread.detach(); });
        return true;
    }

    uint64_t GetCurrentMillisecond(bool system_time)
    {
        static bool flag = InitMillisecondThread();
        if (system_time)
        {
            return s_current_millisecond_system.load(std::memory_order_acquire);
        }
        return s_current_millisecond.load(std::memory_order_acquire);
    }

    uint64_t GetCurrentMicrosecond(bool system_time)
    {
        static bool flag = InitMillisecondThread();
        if (system_time)
        {
            return s_current_microsecond_system.load(std::memory_order_acquire);
        }
        return s_current_microsecond.load(std::memory_order_acquire);
    }

} // namespace cyberweb

#endif // CYBER_UTIL_H
