#if !defined(CYBER_TCP_SERVER_H)
#define CYBER_TCP_SERVER_H

#include <assert.h>

#include "sock.h"
#include "../Poller/Timer.h"
#include "server.h"

namespace cyber
{
    class TCPServer : public Server
    {
    public:
        typedef std::shared_ptr<TCPServer> Ptr;
        TCPServer(const EventPoller::Ptr &poller = nullptr) : Server(poller)
        {
            SetOnCreateSocket(nullptr);
            socket_ = CreateSocket();
            socket_->SetOnAccept(std::bind(&TCPServer::OnAcceptConnectionL, this, std::placeholders::_1));
            socket_->SetOnBeforeAccept(std::bind(&TCPServer::OnBeforeAcceptConnectionL, this, std::placeholders::_1));
        }
        ~TCPServer()
        {
            if (!cloned_ && socket_->GetFD() != -1)
            {
                std::cout << "close tcp server" << std::endl;
            }
            timer_.reset();
            socket_.reset();
            session_map_.clear();
            cloned_server_.clear();
        }

        template <typename SessionType>
        void Start(uint16_t port, const std::string &host = "0.0.0.0", uint32_t backlog = 1024)
        {
            StartL<SessionType>(port, host, backlog);
            EventPollerPool::Instance().ForEach(
                [&](const TaskExecutor::Ptr &executor)
                {
                    EventPoller::Ptr poller = std::dynamic_pointer_cast<EventPoller>(executor);
                    if (poller == poller_ || !poller)
                    {
                        return;
                    }
                    auto &serverRef = cloned_server_[poller.get()];
                    if (!serverRef)
                    {
                        serverRef = OnCreateServer(poller);
                    }
                    if (serverRef)
                    {
                        serverRef->CloneFrom(*this);
                    }
                });
        }

        void SetOnCreateSocket(Socket::OnCreateSocketCB cb)
        {
            if (cb)
            {
                on_create_socket_ = std::move(cb);
            }
            else
            {
                on_create_socket_ = [](const EventPoller::Ptr &poller)
                {
                    return Socket::CreateSocket(poller, false);
                };
            }
            for (auto &pr : cloned_server_)
            {
                pr.second->SetOnCreateSocket(cb);
            }
        }

    protected:
        virtual TCPServer::Ptr OnCreateServer(const EventPoller::Ptr &poller)
        {
            return std::make_shared<TCPServer>(poller);
        }

        virtual Socket::Ptr OnBeforeAcceptConnection(const EventPoller::Ptr &poller)
        {
            assert(poller_->IsCurrentThread());
            return CreateSocket();
        }

        virtual void CloneFrom(const TCPServer &that)
        {
            if (!that.socket_)
            {
                throw std::invalid_argument("TcpServer::cloneFrom other with null socket!");
            }
            on_create_socket_ = that.on_create_socket_;
            session_alloc_ = that.session_alloc_;
            socket_->CloneFromListenSocket(that.socket_);
            std::weak_ptr<TCPServer> weak_self = std::dynamic_pointer_cast<TCPServer>(shared_from_this());
            timer_ = std::make_shared<Timer>(
                2.0f, [weak_self]() -> bool
                {
                    auto strong_self = weak_self.lock();
                    if (!strong_self)
                    {
                        return false;
                    }
                    strong_self->OnManagerSession();
                    return true;
                },
                poller_);
            cloned_ = true;
        }

        virtual void OnAcceptConnection(const Socket::Ptr &sock)
        {
            assert(poller_->IsCurrentThread());
            std::weak_ptr<TCPServer> weak_self = std::dynamic_pointer_cast<TCPServer>(shared_from_this());

            // alloc a session and create a session helper
            auto helper = session_alloc_(std::dynamic_pointer_cast<TCPServer>(shared_from_this()), sock);

            auto session = helper->session();

            session->AttachServer(*this);

            auto success = session_map_.emplace(helper.get(), helper).second;
            assert(success);

            std::weak_ptr<Session> weak_session = session;
            sock->SetOnRead(
                [weak_session](const Buffer::Ptr &buf, sockaddr *, int)
                {
                    auto strong_session = weak_session.lock();
                    if (strong_session)
                    {
                        strong_session->OnRecv(buf);
                    }
                });

            SessionHelper *ptr = helper.get();
            sock->SetOnError(
                [weak_self, weak_session, ptr](const SockException &err)
                {
                    OnceToken once(
                        nullptr, [weak_self, weak_session, ptr]()
                        {
                            auto strong_self = weak_self.lock();
                            if (!strong_self)
                            {
                                return;
                            }
                            assert(strong_self->poller_->IsCurrentThread());
                            if (!strong_self->is_on_manager)
                            {
                                strong_self->session_map_.erase(ptr);
                            }
                            else
                            {
                                strong_self->poller_->Async(
                                    [weak_self, ptr]()
                                    {
                                        /* code */
                                        auto strong_self = weak_self.lock();
                                        if (!strong_self)
                                        {
                                            return;
                                        }
                                        if (strong_self->is_on_manager)
                                        {
                                            WarnL << "is_on_manager";
                                            strong_self->on_manager_delete.push_back(ptr);
                                            return;
                                        }
                                        strong_self->session_map_.erase(ptr);
                                    },
                                    false);
                            }
                        });
                    auto strong_session = weak_session.lock();
                    if (strong_session)
                    {
                        strong_session->OnError(err);
                    }
                });
        }

    private:
        Socket::Ptr OnBeforeAcceptConnectionL(const EventPoller::Ptr &poller)
        {
            return OnBeforeAcceptConnection(poller);
        }

        void OnAcceptConnectionL(const Socket::Ptr &sock)
        {
            OnAcceptConnection(sock);
        }

        template <typename SessionType>
        void StartL(uint16_t port, const std::string &host = "0.0.0.0", uint32_t backlog = 1024)
        {
            session_alloc_ = [](const TCPServer::Ptr &server, const Socket::Ptr &sock)
            {
                auto session = std::make_shared<SessionType>(sock);
                session->SetOnCreateSocket(server->on_create_socket_);
                return std::make_shared<SessionHelper>(server, session);
            };

            if (!socket_->Listen(port, host, backlog))
            {
                throw std::runtime_error("tcp server listen error");
            }

            std::weak_ptr<TCPServer> weak_self = std::dynamic_pointer_cast<TCPServer>(shared_from_this());
            timer_ = std::make_shared<Timer>(
                2.0f, [weak_self]() -> bool
                {
                    auto strong_self = weak_self.lock();
                    if (!strong_self)
                    {
                        return false;
                    }
                    strong_self->OnManagerSession();
                    return true;
                },
                poller_);
        }

        void OnManagerSession()
        {
            assert(poller_->IsCurrentThread());

            OnceToken once(
                [&]()
                {
                    is_on_manager = true;
                },
                [&]()
                {
                    is_on_manager = false;
                    for (auto ptr : on_manager_delete)
                    {
                        session_map_.erase(ptr);
                    }
                    on_manager_delete.clear();
                });
            try
            {
                for (auto &pr : session_map_)
                {
                    pr.second->session()->OnManager();
                }
            }
            catch (const std::exception &e)
            {
                ErrorL << e.what() << '\n';
            }
        }

        Socket::Ptr CreateSocket()
        {
            return on_create_socket_(poller_);
        }

    private:
        /* data */
        bool cloned_ = false;
        bool is_on_manager = false;
        Socket::Ptr socket_;
        std::shared_ptr<Timer> timer_;
        Socket::OnCreateSocketCB on_create_socket_;
        std::unordered_map<SessionHelper *, SessionHelper::Ptr> session_map_;
        std::function<SessionHelper::Ptr(const TCPServer::Ptr &server, const Socket::Ptr &)> session_alloc_;
        std::unordered_map<EventPoller *, Ptr> cloned_server_;
        std::vector<SessionHelper *> on_manager_delete;
    };

} // namespace cyberweb

#endif // CYBER_TCP_SERVER_H
