#if !defined(CYBER_ONCETOKEN_H)
#define CYBER_ONCETOKEN_H

#include <functional>
#include "util.h"

namespace cyberweb
{
    class OnceToken : public noncopyable
    {
    public:
        typedef std::function<void(void)> task;
        template <typename FUNC>
        OnceToken(const FUNC &on_constructed, task on_destroyed = nullptr)
        {
            on_constructed();
            on_destroyed_ = std::move(on_destroyed);
        }
        OnceToken(nullptr_t, task on_destroyed = nullptr)
        {
            on_destroyed_ = std::move(on_destroyed);
        }
        ~OnceToken()
        {
            if (on_destroyed_)
            {
                on_destroyed_();
            }
        }

    private:
        task on_destroyed_;
        OnceToken() = delete;
    };

} // namespace cyberweb

#endif // CYBER_ONCETOKEN_H
