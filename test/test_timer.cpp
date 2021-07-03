#include <signal.h>

#include "../Cyber/util.h"
#include "../Cyber/logger.h"
#include "../Cyber/time_ticker.h"
#include "../Cyber/Timer.h"
#include "../Cyber/semaphore.h"

int main(int argc, char const *argv[])
{
    /* code */
    cyber::Logger::Instance().Add(std::make_shared<cyber::ConsoleChannel>());
    cyber::Logger::Instance().SetWriter(std::make_shared<cyber::AsyncLogWriter>());
    TraceL << "begin:";

    cyber::Ticker ticker0;
    cyber::Timer::Ptr time0 = std::make_shared<cyber::Timer>(
        0.5f, [&]()
        {
            TraceL << "time0 repeat:" << ticker0.EleapsedTime();
            ticker0.ResetTime();
            return true;
        },
        nullptr);
    cyber::Timer::Ptr time1 = std::make_shared<cyber::Timer>(
        1.0f, []()
        {
            DebugL << "timer1 non repeatable";
            return false;
        },
        nullptr);

    cyber::Ticker ticker1;
    cyber::Timer::Ptr time2 = std::make_shared<cyber::Timer>(
        2.0f, [&ticker1]()->bool /*& is refernce*/
        {
            InfoL << "timer2 repeatable but throw error: " << ticker1.EleapsedTime();
            ticker1.ResetTime();
            throw std::runtime_error("timer2");
        },
        nullptr);

    static cyber::Semaphore sema;
    signal(SIGINT, [](int)
           { sema.Post(); });
    sema.Wait();

    return 0;
}
