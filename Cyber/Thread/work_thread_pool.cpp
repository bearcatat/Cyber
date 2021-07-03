#if !defined(CYBER_WORK_THREAD_POOL_CPP)
#define CYBER_WORK_THREAD_POOL_CPP
#include "work_thread_pool.h"
namespace cyber
{
    int WorkThreadPool::s_pool_size = 0;

    WorkThreadPool &WorkThreadPool::Instance()
    {
        static std::shared_ptr<WorkThreadPool> s_instance(new WorkThreadPool());
        static WorkThreadPool &s_ref = *s_instance;
        return s_ref;
    }

    EventPoller::Ptr WorkThreadPool::GetFirstPoller()
    {
        return std::dynamic_pointer_cast<EventPoller>(threads_.front());
    }

    EventPoller::Ptr WorkThreadPool::GetPoller()
    {
        return std::dynamic_pointer_cast<EventPoller>(GetExecutor());
    }

    WorkThreadPool::WorkThreadPool()
    {
        auto size = s_pool_size > 0 ? s_pool_size : std::thread::hardware_concurrency();
        CreateThreads(
            []()
            {
                EventPoller::Ptr ret(new EventPoller(ThreadPool::PRIORITY_LOWEST));
                ret->RunLoop(false, false);
                return ret;
            },
            size);
    }
    void WorkThreadPool::SetPoolSize(int size)
    {
        s_pool_size = size;
    }
} // namespace cyber

#endif // CYBER_WORK_THREAD_POOL_CPP
