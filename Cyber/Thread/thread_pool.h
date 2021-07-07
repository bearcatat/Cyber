#if !defined(CYBER_THREAD_POOL_H)
#define CYBER_THREAD_POOL_H

#include <thread>
#include <memory>

#include "task_executor.h"
#include "../Util/logger.h"

namespace cyber
{
    class ThreadPool : public TaskExecutor
    {
    public:
        enum Priority
        {
            PRIORITY_LOWEST = 0,
            PRIORITY_LOW,
            PRIORITY_NORMAL,
            PRIORITY_HIGH,
            PRIORITY_HIGHEST
        };
        static bool SetPriority(Priority priority = PRIORITY_NORMAL, std::thread::native_handle_type thread_id = 0)
        {
            static int Min = sched_get_priority_min(SCHED_OTHER);
            if (Min == -1)
            {
                return false;
            }
            static int Max = sched_get_priority_max(SCHED_OTHER);
            if (Max == -1)
            {
                return false;
            }
            static int Priorities[] = {
                Min, Min + (Max - Min) / 4,
                Min + (Max - Min) / 2, Min + (Max - Min) * 3 / 4, Max};

            if (thread_id == 0)
            {
                thread_id = pthread_self();
            }

            sched_param params;

            params.sched_priority = Priorities[priority];

            return pthread_setschedparam(thread_id, SCHED_OTHER, &params) == 0;
        }
    };

} // namespace cyberweb

#endif // CYBER_THREAD_POOL_H
