#include <signal.h>
#include "../Cyber/logger.h"
#include "../Cyber/time_ticker.h"
#include "../Cyber/onceToken.h"
#include "../Cyber/poller.h"

int main(int argc, char const *argv[])
{
    cyber::Logger::Instance().Add(std::make_shared<cyber::ConsoleChannel>());
    cyber::Logger::Instance().SetWriter(std::make_shared<cyber::AsyncLogWriter>());

    cyber::Ticker ticker0;
    int nextDelay0 = 50;
    std::shared_ptr<cyber::OnceToken> token0 = std::make_shared<cyber::OnceToken>(nullptr, []()
                                                                                  { TraceL << "task 0 cancel"; });
    auto tag0 = cyber::EventPollerPool::Instance().GetPoller()->DoDelayTask(
        nextDelay0, [&, token0]()
        {
            TraceL << "task 0, except time: " << nextDelay0 << "acutal time: " << ticker0.EleapsedTime();
            ticker0.ResetTime();
            return nextDelay0;
        });
    token0 = nullptr;

    cyber::Ticker ticker1;
    int nextDelay1 = 50;
    auto tag1 = cyber::EventPollerPool::Instance().GetPoller()->DoDelayTask(
        nextDelay1, [&]()
        {
            DebugL << "task 1, expect time: " << nextDelay1 << " acutal time: " << ticker1.EleapsedTime();
            ticker1.ResetTime();
            nextDelay1 += 1;
            return nextDelay1;
        });

    cyber::Ticker ticker2;
    int nextDelay2 = 3000;
    auto tag2 = cyber::EventPollerPool::Instance().GetPoller()->DoDelayTask(
        nextDelay2, [&]()
        {
            InfoL << "task2, expect time:" << nextDelay2 << "actual time: " << ticker2.EleapsedTime();
            ticker2.ResetTime();
            return 0;
        });

    cyber::Ticker ticker3;
    int nextDelay3 = 50;
    auto tag3 = cyber::EventPollerPool::Instance().GetPoller()->DoDelayTask(
        nextDelay3, [&]() -> uint64_t
        { throw std::runtime_error("task 2 throw error."); });

    sleep(2);
    tag0->Cancel();
    tag1->Cancel();
    WarnL << "cancel task 0,1";

    static cyber::Semaphore sem;
    signal(SIGINT, [](int)
           { sem.Post(); });
    sem.Wait();
    return 0;
}
