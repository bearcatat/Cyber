#include <signal.h>
#include <iostream>
#include "../Cyber/logger.h"
#include "../Cyber/time_ticker.h"
#include "../Cyber/onceToken.h"
#include "../Cyber/poller.h"
#include "../Cyber/util.h"

int main(int argc, char const *argv[])
{
    /* code */
    static bool exit_flag = false;
    signal(SIGINT, [](int)
           { exit_flag = true; });

    cyber::Logger::Instance().Add(std::make_shared<cyber::ConsoleChannel>());
    cyber::Logger::Instance().SetWriter(std::make_shared<cyber::AsyncLogWriter>());
    cyber::Ticker ticker;
    while (!exit_flag)
    {
        if (ticker.EleapsedTime() > 1000)
        {
            auto vec = cyber::EventPollerPool::Instance().GetExecutorLoad();
            cyber::StrPrinter printer;
            for (auto load : vec)
            {
                printer << load << "-";
            }
            DebugL << "cpu load:" << printer;

            cyber::EventPollerPool::Instance().GetExecutorDelay(
                [](const std::vector<int> &vec)
                {
                    cyber::StrPrinter printer;
                    for (auto load : vec)
                    {
                        printer << load << "-";
                    }
                    DebugL << "cpu delay load:" << printer;
                });
            ticker.ResetTime();
        }
        cyber::EventPollerPool::Instance().GetExecutor()->Async(
            []()
            {
                auto usec = rand() % 4000;
                usleep(usec);
            });
        usleep(2000);
    }
    return 0;
}
