#if !defined(CYBER_WORK_THREAD_POOL_H)
#define CYBER_WORK_THREAD_POOL_H
#include <memory>
#include "thread_pool.h"
#include "../Poller/poller.h"

namespace cyber
{
    class WorkThreadPool : public std::enable_shared_from_this<WorkThreadPool>, public TaskExecutorGetterImp
    {

    public:
        typedef std::shared_ptr<WorkThreadPool> Ptr;
        ~WorkThreadPool(){};

        static WorkThreadPool &Instance();

        static void SetPoolSize(int size = 0);
        EventPoller::Ptr GetFirstPoller();
        EventPoller::Ptr GetPoller();

    private:
        WorkThreadPool();

    private:
        static int s_pool_size;
    };

} // namespace cyber

#endif // CYBER_WORK_THREAD_POOL_H
