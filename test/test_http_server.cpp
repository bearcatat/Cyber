#include <unistd.h>
#include <signal.h>

#include "../Cyber/Util/logger.h"
#include "../Cyber/Socket/tcp_server.h"
#include "../Cyber/Socket/sock.h"
#include "../Cyber/Util/time_ticker.h"
#include "../Cyber/HTTP/http_session.h"

int main(int argc, char const *argv[])
{

    cyber::Logger::Instance().Add(std::make_shared<cyber::ConsoleChannel>());
    cyber::Logger::Instance().SetWriter(std::make_shared<cyber::AsyncLogWriter>());

    TraceL << "server start";
    cyber::EventPollerPool::Instance().PreferCurrentThread(false);
    cyber::TCPServer::Ptr server(new cyber::TCPServer());
    server->Start<cyber::HTTPSession>(16557);

    static cyber::Semaphore sem;
    signal(SIGINT, [](int)
           { sem.Post(); });
    sem.Wait();

    return 0;
}
