#if !defined(CYBER_TIMER_H)
#define CYBER_TIMER_H

#include "poller.h"

namespace cyber
{
    class Timer
    {
    public:
        typedef std::shared_ptr<Timer> Ptr;
        Timer(float second, const std::function<bool()> &cb, const EventPoller::Ptr &poller, bool continue_when_excepton = true)
        {
            poller_ = poller;
            if (!poller_)
            {
                poller_ = EventPollerPool::Instance().GetPoller();
            }
            tag_ = poller_->DoDelayTask(
                (uint64_t)(second * 1000), [cb, second, continue_when_excepton]()
                {
                    try
                    {
                        if (cb())
                        {
                            return (uint64_t)(1000 * second);
                        }
                        return (uint64_t)0;
                    }
                    catch (const std::exception &e)
                    {
                        ErrorL << "delay error:" << e.what() << '\n';
                        return continue_when_excepton ? (uint64_t)(1000 * second) : 0;
                    }
                });
        }
        ~Timer()
        {
            auto tag = tag_.lock();
            if (tag)
            {
                tag->Cancel();
            }
        }

    private:
        /* data */
        std::weak_ptr<DelayTask> tag_;
        EventPoller::Ptr poller_;
    };

} // namespace cyberweb

#endif // CYBER_TIMER_H
