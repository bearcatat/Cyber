#if !defined(CYBER_SESSION_H)
#define CYBER_SESSION_H

#include <memory>

#include "sock.h"

namespace cyber
{
    class Server;
    class Session : public std::enable_shared_from_this<Session>, public SocketHelper
    {
    public:
        typedef std::shared_ptr<Session> Ptr;
        Session(const Socket::Ptr &sock);
        ~Session() = default;

        virtual void OnRecv(const Buffer::Ptr &buf) = 0;
        virtual void OnError(const SockException &ex) = 0;

        virtual void OnManager() = 0;

        virtual void AttachServer(const Server &) {}

        std::string GetIdentifier() const;

        void SafeShutdown(const SockException &ex = SockException(ERR_SHUTDOWN, "Self shutdown"));
    };

    class TCPSession : public Session
    {
    public:
        TCPSession(const Socket::Ptr &sock) : Session(sock){};
        ~TCPSession() override = default;
        Ptr shared_from_this()
        {
            return std::static_pointer_cast<TCPSession>(Session::shared_from_this());
        }
    };

} // namespace cyberweb

#endif // CYBER_SESSION_H
