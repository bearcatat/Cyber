#if !defined(CYBER_UTIL_H)
#define CYBER_UTIL_H

#include <list>
#include <chrono>
#include <atomic>
#include <thread>
#include <iostream>
#include <unistd.h>
#include <string>
#include <sstream>
#include <linux/limits.h>
#include <vector>

namespace cyber
{
    class StrPrinter : public std::string
    {

    public:
        StrPrinter() {}
        template <typename T>
        StrPrinter &operator<<(T &&data)
        {
            stream_ << std::forward<T>(data);
            this->std::string::operator=(stream_.str());
            return *this;
        }
        std::string operator<<(std::ostream &(*f)(std::ostream &)) const
        {
            return *this;
        }

    private:
        std::stringstream stream_;
    };

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
        List(List<T> &&that)
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
        size_t size() const
        {
            return list_.size();
        }
        bool empty() const
        {
            return list_.empty();
        }
        template <class... Args>
        void emplace_front(Args &&...args)
        {
            list_.emplace_front(std::forward<Args>(args)...);
        }
        template <class... Args>
        void emplace_back(Args &&...args)
        {
            list_.emplace_back(std::forward<Args>(args)...);
        }
        T &front()
        {
            // T *f = &(list_.front());
            // return *f;
            return list_.front();
        }
        T &back()
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
        void pop_front()
        {
            list_.pop_front();
        }
        void pop_back()
        {
            list_.pop_back();
        }
        void swap(List<T> &other)
        {
            list_.swap(other.list_);
        }
        void Append(List<T> &other)
        {
            list_.splice(list_.end(), (other.list_));
        }

    private:
        std::list<T> list_;
    };

    uint64_t GetCurrentMillisecond(bool system_time = false);

    uint64_t GetCurrentMicrosecond(bool system_time = false);

    std::string ExePath();

    std::string ExeName();

    struct tm GetLocalTime(time_t sec);

    std::vector<std::string> split(const std::string &s, const char *delim);
    std::string &trim(std::string &s, const std::string &chars = " \t\n\r");
    std::string trim(std::string &&s, const std::string &chars = " \t\n\r");
} // namespace cyberweb

#endif // CYBER_UTIL_H
