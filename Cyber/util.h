#if !defined(CYBER_UTIL_H)
#define CYBER_UTIL_H

#include <list>

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
} // namespace cyberweb

#endif // CYBER_UTIL_H
