#if !defined(CYBER_SESSION_CPP)
#define CYBER_SESSION_CPP

#include "session.h"

namespace cyber
{
    Session::Session(const Socket::Ptr &sock) : SocketHelper(sock) {}

    std::string Session::GetIdentifier() const
    {
        return std::to_string(reinterpret_cast<uint64_t>(this));
    }

    void Session::SafeShutdown(const SockException &ex)
    {
        std::weak_ptr<Session> weak_self = shared_from_this();
        AsyncFirst(
            [weak_self, ex]()
            {
                auto strong_self = weak_self.lock();
                if (strong_self)
                {
                    strong_self->Shutdown(ex);
                }
            });
    }

} // namespace cyberweb

#endif // CYBER_SESSION_CPP
