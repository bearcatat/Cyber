#include <unistd.h>
#include <signal.h>

#include "../Cyber/Util/logger.h"
#include "../Cyber/Socket/tcp_server.h"
#include "../Cyber/Socket/sock.h"
#include "../Cyber/Util/time_ticker.h"

class EchoSession : public cyber::TCPSession
{
public:
    EchoSession(const cyber::Socket::Ptr &sock) : cyber::TCPSession(sock) {}
    ~EchoSession()
    {
        DebugL;
    }
    virtual void OnRecv(const cyber::Buffer::Ptr &buf) override
    {
        TraceL << "receive: " << buf->Data();
        Send(buf);
        if (strcmp(buf->Data(), "close") == 0)
        {
            // Shutdown();
            is_closed = true;
        }
    }
    virtual void OnError(const cyber::SockException &err) override
    {
        WarnL << err.what();
    }
    virtual void OnManager() override
    {
        DebugL;
        if (is_closed)
        {
            try
            {
                SafeShutdown();
            }
            catch (const std::exception &e)
            {
                ErrorL << e.what() << '\n';
            }
        }
    }

private:
    cyber::Ticker ticker_;
    bool is_closed = false;
};

int main(int argc, char const *argv[])
{
    cyber::Logger::Instance().Add(std::make_shared<cyber::ConsoleChannel>());
    cyber::Logger::Instance().SetWriter(std::make_shared<cyber::AsyncLogWriter>());

    TraceL << "server start";
    cyber::TCPServer::Ptr server(new cyber::TCPServer());
    server->Start<EchoSession>(16555);

    static cyber::Semaphore sem;
    signal(SIGINT, [](int)
           { sem.Post(); });
    sem.Wait();

    return 0;
}
