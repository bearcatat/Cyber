#if !defined(CYBER_SERVER_H)
#define CYBER_SERVER_H
#include <memory>
#include <functional>
#include <string>

#include "session.h"

namespace cyber
{
    class SessionMap : public std::enable_shared_from_this<SessionMap>
    {
    public:
        friend class SessionHelper;
        typedef std::shared_ptr<SessionMap> Ptr;

        static SessionMap &Instance()
        {
            static std::shared_ptr<SessionMap> s_instance(new SessionMap());
            static SessionMap &ret = *s_instance;
            return ret;
        }
        ~SessionMap(){};

        Session::Ptr get(const std::string &tag)
        {
            LOCKGUARD(mtx_session_);
            auto it = map_session_.find(tag);
            if (it == map_session_.end())
            {
                return nullptr;
            }
            return it->second.lock();
        }

        void ForEachSession(const std::function<void(const std::string &id, const Session::Ptr &session)> cb)
        {
            LOCKGUARD(mtx_session_);
            for (auto it = map_session_.begin(); it != map_session_.end();)
            {
                auto session = it->second.lock();
                if (!session)
                {
                    map_session_.erase(it);
                    continue;
                }
                cb(it->first, session);
                it++;
            }
        }

    private:
        /* data */
        SessionMap(){};
        bool Add(const std::string &tag, const Session::Ptr &session)
        {
            LOCKGUARD(mtx_session_);
            return map_session_.emplace(tag, session).second;
        }
        bool Del(const std::string &tag)
        {
            LOCKGUARD(mtx_session_);
            return map_session_.erase(tag);
        }

    private:
        std::mutex mtx_session_;
        std::unordered_map<std::string, std::weak_ptr<Session>> map_session_;
    };

    class Server;
    class SessionHelper
    {
    public:
        typedef std::shared_ptr<SessionHelper> Ptr;

        SessionHelper(const std::weak_ptr<Server> &server, Session::Ptr session)
        {
            server_ = server;
            session_ = session;
            session_map_ = SessionMap::Instance().shared_from_this();
            identifier_ = session_->GetIdentifier();
            session_map_->Add(identifier_, session);
        }
        ~SessionHelper()
        {
            if (!server_.lock())
            {
                session_->OnError(SockException(ERR_OTHER, "server shutdown"));
            }
            session_map_->Del(identifier_);
        };

        const Session::Ptr session() const { return session_; };

    private:
        std::string identifier_;
        Session::Ptr session_;
        SessionMap::Ptr session_map_;
        std::weak_ptr<Server> server_;
    };

    class Server : public std::enable_shared_from_this<Server>
    {
    protected:
        /* data */
        EventPoller::Ptr poller_;

    public:
        typedef std::shared_ptr<Server> Ptr;
        explicit Server(EventPoller::Ptr poller = nullptr)
        {
            poller_ = poller ? std::move(poller) : EventPollerPool::Instance().GetPoller();
        }
        virtual ~Server(){};
    };

} // namespace cyberweb

#endif // CYBER_SERVER_H
