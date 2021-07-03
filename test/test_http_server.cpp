#include "../Cyber/session.h"
#include "../Cyber/sock.h"
#include "../Cyber/logger.h"

class HTTPSession : cyber::TCPSession
{
private:
    /* data */
public:
    HTTPSession(cyber::Socket::Ptr &sock) : cyber::TCPSession(sock) {}
    ~HTTPSession()
    {
        DebugL;
    }
    void OnRecv(const cyber::Buffer::Ptr &buf) override
    {
        
    }
};
