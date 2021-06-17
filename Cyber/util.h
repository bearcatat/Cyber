#if !defined(CYBER_UTIL_H)
#define CYBER_UTIL_H

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
    }
} // namespace cyberweb

#endif // CYBER_UTIL_H
