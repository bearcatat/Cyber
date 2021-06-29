#include "../Cyber/logger.h"
#include "../Cyber/time_ticker.h"

int main(int argc, char const *argv[])
{
    /* code */
    cyber::Logger::Instance().Add(std::make_shared<cyber::ConsoleChannel>());
    cyber::Logger::Instance().SetWriter(std::make_shared<cyber::AsyncLogWriter>());

    cyber::Ticker ticker0;

    
    

    return 0;
}
