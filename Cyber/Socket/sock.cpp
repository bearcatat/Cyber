#if !defined(CYBER_SOCK_CPP)
#define CYBER_SOCK_CPP
#include <mutex>
#include <memory>
#include <iostream>

#include "sock.h"
#include "sockutil.h"
#include "../Util/uv_errno.h"
#include "../Thread/work_thread_pool.h"

namespace cyber
{

    static SockException ToSockException(int error)
    {
        switch (error)
        {
        case 0:
        case UV_EAGAIN:
            return SockException(ERR_SUCCESS, "success");
        case UV_ECONNREFUSED:
            return SockException(ERR_REFUSE, uv_strerror(error), error);
        case UV_ETIMEDOUT:
            return SockException(ERR_TIMEOUT, uv_strerror(error), error);
        default:
            return SockException(ERR_OTHER, uv_strerror(error), error);
        }
    }

    static SockException GetSockErr(const SockFD::Ptr &sock, bool try_errno)
    {
        int error = 0, len = sizeof(int);
        getsockopt(sock->GetFd(), SOL_SOCKET, SO_ERROR, (char *)&error, (socklen_t *)&len);
        if (error == 0)
        {
            if (try_errno)
            {
                error = get_uv_error(true);
            }
        }
        else
        {
            error = uv_translate_posix_error(error);
        }
        return ToSockException(error);
    }

    Socket::Ptr Socket::CreateSocket(const EventPoller::Ptr &poller, bool enable_recursive_mutex)
    {
        return Socket::Ptr(new Socket(poller, enable_recursive_mutex));
    }
    Socket::Socket(const EventPoller::Ptr &poller, bool enable_recursive_mutex)
    {
        poller_ = poller;
        SetOnRead(nullptr);
        SetOnError(nullptr);
        SetOnAccept(nullptr);
        SetOnFlush(nullptr);
        SetOnBeforeAccept(nullptr);
    }
    Socket::~Socket()
    {
        CLoseSock();
    }

    void Socket::SetOnRead(OnReadCB cb)
    {
        std::lock_guard<std::recursive_mutex> guard(event_lock_);
        if (cb)
        {
            on_read_ = std::move(cb);
        }
        else
        {
            on_read_ = [](const Buffer::Ptr &buf, sockaddr *addr, int addr_len)
            {
                WarnL << "Socket not set readCB";
            };
        }
    }
    void Socket::SetOnError(OnErrorCB cb)
    {
        std::lock_guard<std::recursive_mutex> guard(event_lock_);
        if (cb)
        {
            on_err_ = std::move(cb);
        }
        else
        {
            on_err_ = [](const SockException &err)
            {
                WarnL << "Socket not set errCB";
            };
        }
    }
    void Socket::SetOnAccept(OnAcceptCB cb)
    {
        std::lock_guard<std::recursive_mutex> guard(event_lock_);
        if (cb)
        {
            on_accept_ = std::move(cb);
        }
        else
        {
            on_accept_ = [](Socket::Ptr &sock)
            {
                WarnL << "Socket not set acceptCB";
            };
        }
    }
    void Socket::SetOnFlush(OnFlushCB cb)
    {
        std::lock_guard<std::recursive_mutex> guard(event_lock_);
        if (cb)
        {
            on_flush_ = std::move(cb);
        }
        else
        {
            on_flush_ = []()
            { return true; };
        }
    }
    void Socket::SetOnBeforeAccept(OnCreateSocketCB cb)
    {
        std::lock_guard<std::recursive_mutex> guard(event_lock_);
        if (cb)
        {
            on_before_accept_ = std::move(cb);
        }
        else
        {
            on_before_accept_ = [](const EventPoller::Ptr &poller)
            { return nullptr; };
        }
    }

#define CLOSE_SOCK(fd) \
    if (fd != -1)      \
    {                  \
        close(fd);     \
    }

    void Socket::Connect(const std::string &ip, sa_family_t family, uint16_t port, OnErrorCB con_cb_in, float timeout_sec)
    {
        CLoseSock();
        std::weak_ptr<Socket> weak_self = shared_from_this();
        auto con_cb = [con_cb_in, weak_self](const SockException &err)
        {
            auto strong_self = weak_self.lock();
            if (!strong_self)
            {
                return;
            }
            strong_self->async_con_cb_ = nullptr;
            if (err)
            {
                std::lock_guard<std::recursive_mutex> guard(strong_self->sock_fd_lock_);
                strong_self->sock_fd_ = nullptr;
            }
            con_cb_in(err);
        };

        auto async_con_cb = std::make_shared<std::function<void(int)>>(
            [weak_self, con_cb](int sock)
            {
                auto strong_self = weak_self.lock();
                if (sock == -1 || !strong_self)
                {
                    if (!strong_self)
                    {
                        CLOSE_SOCK(sock);
                    }
                    else
                    {
                        con_cb(SockException(ERR_DNS));
                    }
                    return;
                }
                auto sock_fd = strong_self->MakeSock(sock);
                std::weak_ptr<SockFD> weak_sock_fd = sock_fd;
                int result = strong_self->poller_->AddEvent(
                    sock, EVENT_WRITE, [weak_self, weak_sock_fd, con_cb](int event)
                    {
                        auto strong_self = weak_self.lock();
                        auto strong_sock_fd = weak_sock_fd.lock();
                        if (strong_self && strong_sock_fd)
                        {
                            strong_self->OnConnected(strong_sock_fd, con_cb);
                        }
                    });

                if (result == -1)
                {
                    con_cb(SockException(ERR_OTHER, "add event to poller failed when start connect"));
                    return;
                }

                std::lock_guard<std::recursive_mutex> guard(strong_self->sock_fd_lock_);
                strong_self->sock_fd_ = sock_fd;
            });

        auto poller = poller_;
        std::weak_ptr<std::function<void(int)>> weak_task = async_con_cb;

        WorkThreadPool::Instance().GetExecutor()->Async(
            [ip, family, port, weak_task, poller]()
            {
                int sock = SockUtil::Connect(ip.c_str(), family, port, true);
                poller->Async(
                    [sock, weak_task]()
                    {
                        auto strong_task = weak_task.lock();
                        if (strong_task)
                        {
                            (*strong_task)(sock);
                        }
                        else
                        {
                            CLOSE_SOCK(sock);
                        }
                    });
            });

        async_con_cb_ = async_con_cb;
    }

    void Socket::OnConnected(const SockFD::Ptr &sock, const OnErrorCB &cb)
    {
        poller_->DelEvent(sock->GetFd());
        if (!AttachEvent(sock))
        {
            cb(SockException(ERR_OTHER, "add event to poller failed when connected"));
            return;
        }
    }

    bool Socket::AttachEvent(const SockFD::Ptr &sock)
    {
        std::weak_ptr<Socket> weak_self = shared_from_this();
        std::weak_ptr<SockFD> weak_sock = sock;
        enable_recv_ = true;
        read_buffer_ = poller_->GetSharedBuffer();
        int result = poller_->AddEvent(
            sock->GetFd(), EVENT_READ | EVENT_WRITE | EVENT_ERROR, [weak_self, weak_sock](int event)
            {
                auto strong_self = weak_self.lock();
                auto strong_sock = weak_sock.lock();
                if (!strong_self || !strong_sock)
                {
                    return;
                }
                if (event & EVENT_READ)
                {
                    strong_self->OnRead(strong_sock);
                }
                if (event & EVENT_WRITE)
                {
                    strong_self->OnWriteAble(strong_sock);
                }
                if (event & EVENT_ERROR)
                {
                    strong_self->EmitError(SockException(ERR_OTHER, "cb happened"));
                }
            });

        return -1 != result;
    }
    ssize_t Socket::OnRead(const SockFD::Ptr &sock) noexcept
    {
        ssize_t ret = 0, nread = 0;
        auto sock_fd = sock->GetFd();

        auto data = read_buffer_->Data();
        auto capacity = read_buffer_->GetCapacity() - 1;

        sockaddr addr;
        socklen_t addr_len = sizeof(sockaddr);

        while (enable_recv_)
        {
            do
            {
                nread = recvfrom(sock_fd, data, capacity, 0, &addr, &addr_len);
            } while (-1 == nread && UV_EINTR == get_uv_error(true));

            if (nread == 0)
            {
                EmitError(SockException(ERR_EOF, "end of file"));
                return ret;
            }

            if (nread == -1)
            {
                auto err = get_uv_error(true);
                if (err != UV_EAGAIN)
                {
                    EmitError(ToSockException(err));
                }
                return ret;
            }

            ret += nread;
            data[nread] = '\0';
            read_buffer_->SetSize(nread);

            std::lock_guard<std::recursive_mutex> guard(event_lock_);
            try
            {
                on_read_(read_buffer_, &addr, sizeof(addr));
            }
            catch (std::exception &ex)
            {
                ErrorL << "on read error: " << ex.what();
            }
        }
        return 0;
    }

    bool Socket::EmitError(const SockException &err) noexcept
    {
        {
            std::lock_guard<std::recursive_mutex> guard(sock_fd_lock_);
            if (!sock_fd_)
            {
                return false;
            }
        }
        CLoseSock();
        std::lock_guard<std::recursive_mutex> gurad(event_lock_);
        try
        {
            on_err_(err);
        }
        catch (std::exception &ex)
        {
            ErrorL << "on err error: " << ex.what();
        }
        return true;
    }

    ssize_t Socket::Send(const char *buf, size_t size, sockaddr *addr, socklen_t addr_len, bool try_flush)
    {
        if (size <= 0)
        {
            size = strlen(buf);
            if (!size)
            {
                return 0;
            }
        }
        auto buffer = BufferRaw::Create();
        buffer->Assign(buf, size);
        return Send(std::move(buffer), addr, addr_len, try_flush);
    }

    ssize_t Socket::Send(const std::string &buf, sockaddr *addr, socklen_t addr_len, bool try_flush)
    {
        if (!buf.size())
        {
            return 0;
        }
        auto buffer = BufferRaw::Create();
        buffer->Assign(buf.c_str(), buf.size());
        return Send(std::move(buffer), addr, addr_len, try_flush);
    }

    ssize_t Socket::Send(const Buffer::Ptr &buf, sockaddr *addr, socklen_t addr_len, bool try_flush)
    {
        return Send(std::make_shared<BufferSock>(std::move(buf), addr, addr_len), try_flush);
    }

    ssize_t Socket::Send(const BufferSock::Ptr &buf, bool try_flush)
    {
        ssize_t size = buf ? buf->Size() : 0;
        if (!size)
        {
            return 0;
        }
        SockFD::Ptr sock;
        {
            std::lock_guard<std::recursive_mutex> guard(sock_fd_lock_);
            sock = sock_fd_;
        }
        if (!sock)
        {
            return -1;
        }
        {
            std::lock_guard<std::recursive_mutex> guard(send_buf_waiting_lock_);
            send_buf_waiting_.emplace_back(std::move(buf));
        }
        if (try_flush)
        {
            if (sendable_)
            {
                return FlushData(sock, false) ? size : -1;
            }
            //TODO timeout
        }
        return size;
    }

    void Socket::OnFlushed(const SockFD::Ptr &p_sock)
    {
        std::lock_guard<std::recursive_mutex> guard(event_lock_);
        bool flag = on_flush_();
        if (!flag)
        {
            SetOnFlush(nullptr);
        }
    }

    void Socket::CLoseSock()
    {
        async_con_cb_ = nullptr;
        std::lock_guard<std::recursive_mutex> guard(sock_fd_lock_);
        sock_fd_ = nullptr;
        // epoll fd delete in deconstroctor of sock_fd;
    }

    size_t Socket::GetSendBufferCount()
    {
        int ret = 0;
        {
            std::lock_guard<std::recursive_mutex> guard(send_buf_waiting_lock_);
            ret += send_buf_waiting_.size();
        }
        {
            std::lock_guard<std::recursive_mutex> guard(send_buf_sending_lock_);
            send_buf_sending_.ForEach(
                [&](BufferList::Ptr &buf)
                {
                    ret += buf->Count();
                });
        }
        return ret;
    }

    uint64_t Socket::ElapsedTimeAfterFlushed()
    {
        return 0;
    }

    bool Socket::Listen(const SockFD::Ptr &sock)
    {
        CLoseSock();
        std::weak_ptr<SockFD> weak_sock = sock;
        std::weak_ptr<Socket> weak_self = shared_from_this();
        enable_recv_ = true;
        int result = poller_->AddEvent(
            sock->GetFd(), EVENT_READ | EVENT_ERROR, [weak_sock, weak_self](int event)
            {
                auto strong_sock = weak_sock.lock();
                auto strong_self = weak_self.lock();
                if (!strong_sock || !strong_self)
                {
                    return;
                }
                strong_self->OnAccept(strong_sock, event);
            });

        if (result == -1)
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> guard(sock_fd_lock_);
        sock_fd_ = sock;
        return true;
    }

    bool Socket::Listen(uint16_t port, const std::string &local_ip, int backlog)
    {
        int sock = SockUtil::Listen(port, local_ip.c_str(), backlog);
        if (-1 == sock)
        {
            return false;
        }
        return Listen(MakeSock(sock));
    }

    int Socket::OnAccept(const SockFD::Ptr &sock, int event) noexcept
    {
        int fd;
        while (true)
        {
            if (event & EVENT_READ)
            {
                do
                {
                    fd = (int)accept(sock->GetFd(), NULL, NULL);
                } while (-1 == fd && UV_EINTR == get_uv_error(true));

                if (fd == -1)
                {
                    int err = get_uv_error(true);
                    if (err == UV_EAGAIN)
                    {
                        return 0;
                    }
                    auto ex = ToSockException(err);
                    EmitError(ex);
                    ErrorL << "tcp listen failed!" << ex.what();
                    return -1;
                }

                SockUtil::SetNoBlocked(fd);
                Socket::Ptr peer_sock;
                {
                    std::lock_guard<std::recursive_mutex> guard(event_lock_);
                    try
                    {
                        peer_sock = on_before_accept_(poller_);
                    }
                    catch (const std::exception &e)
                    {
                        ErrorL << "socket before accept failed when on accept" << e.what() << '\n';
                        close(fd);
                        continue;
                    }
                }

                if (!peer_sock)
                {
                    peer_sock = Socket::CreateSocket(poller_, false);
                }

                auto peer_sock_fd = peer_sock->SetPeerSock(fd);

                {
                    std::lock_guard<std::recursive_mutex> guard(event_lock_);
                    try
                    {
                        on_accept_(peer_sock);
                    }
                    catch (const std::exception &e)
                    {
                        ErrorL << "socket accpet failed when on accept" << e.what() << '\n';
                        continue;
                    }
                }

                if (!peer_sock->AttachEvent(peer_sock_fd))
                {
                    peer_sock->EmitError(SockException(ERR_EOF, "add event to poller failed when accept a socket"));
                }
            }

            if (event & EVENT_ERROR)
            {
                EmitError(SockException(ERR_OTHER, "listen error"));
                ErrorL << "tcp server listen failed"
                       << "\n";
                return -1;
            }
        }
    }

    SockFD::Ptr Socket::SetPeerSock(int fd)
    {
        CLoseSock();
        auto sock = MakeSock(fd);
        std::lock_guard<std::recursive_mutex> guard(sock_fd_lock_);
        sock_fd_ = sock;
        return sock;
    }

    bool Socket::FlushData(const SockFD::Ptr &sock, bool poller_thread)
    {
        decltype(send_buf_sending_) send_buf_sending_tmp;
        {
            std::lock_guard<std::recursive_mutex> guard(send_buf_sending_lock_);
            if (!send_buf_sending_.empty())
            {
                send_buf_sending_tmp.swap(send_buf_sending_);
            }
        }

        if (send_buf_sending_tmp.empty())
        {
            do
            {
                {
                    std::lock_guard<std::recursive_mutex> guard(send_buf_waiting_lock_);
                    if (!send_buf_waiting_.empty())
                    {
                        send_buf_sending_tmp.emplace_back(std::make_shared<BufferList>(send_buf_waiting_));
                        break;
                    }
                }
                if (poller_thread)
                {
                    StopWriteAbleEvent(sock);
                    OnFlushed(sock);
                }
                return true;
            } while (0);
        }
        int fd = sock->GetFd();
        while (!send_buf_sending_tmp.empty())
        {
            auto &subbuffer = send_buf_sending_tmp.front();
            int n = subbuffer->Send(fd, sock_flags_);
            if (n > 0)
            {
                if (subbuffer->Empty())
                {
                    send_buf_sending_tmp.pop_front();
                    continue;
                }
                if (!poller_thread)
                {
                    StartWriteAbleEvent(sock);
                }
                break;
            }
            int err = get_uv_error(true);
            if (UV_EAGAIN == err)
            {
                if (!poller_thread)
                {
                    StartWriteAbleEvent(sock);
                }
                break;
            }

            EmitError(ToSockException(err));
            return false;
        }
        if (!send_buf_sending_tmp.empty())
        {
            std::lock_guard<std::recursive_mutex> guard(send_buf_sending_lock_);
            send_buf_sending_tmp.swap(send_buf_sending_);
            send_buf_sending_.Append(send_buf_sending_tmp);
            return true;
        }
        return poller_thread ? FlushData(sock, poller_thread) : true;
    }

    void Socket::OnWriteAble(const SockFD::Ptr &sock)
    {
        bool empty_waiting, empty_sending;
        {
            std::lock_guard<std::recursive_mutex> guard(send_buf_sending_lock_);
            empty_sending = send_buf_sending_.empty();
        }
        {
            std::lock_guard<std::recursive_mutex> guard(send_buf_waiting_lock_);
            empty_waiting = send_buf_waiting_.empty();
        }
        if (empty_sending && empty_waiting)
        {
            StopWriteAbleEvent(sock);
        }
        else
        {
            FlushData(sock, true);
        }
    }

    void Socket::StartWriteAbleEvent(const SockFD::Ptr &sock)
    {
        sendable_ = false;
        int recv_flag = enable_recv_ ? EVENT_READ : 0;
        poller_->ModifyEvent(sock->GetFd(), recv_flag | EVENT_WRITE | EVENT_ERROR);
    }

    void Socket::StopWriteAbleEvent(const SockFD::Ptr &sock)
    {
        sendable_ = true;
        int recv_flag = enable_recv_ ? EVENT_READ : 0;
        poller_->ModifyEvent(sock->GetFd(), recv_flag | EVENT_ERROR);
    }

    void Socket::EnableRecv(bool recv)
    {
        if (enable_recv_ == recv)
        {
            return;
        }
        enable_recv_ = recv;
        int recv_flag = enable_recv_ ? EVENT_READ : 0;
        int send_flag = sendable_ ? 0 : EVENT_WRITE;
        poller_->ModifyEvent(GetFD(), recv_flag | send_flag | EVENT_ERROR);
    }

    SockFD::Ptr Socket::MakeSock(int sock)
    {
        return std::make_shared<SockFD>(sock, poller_);
    }

    int Socket::GetFD()
    {
        std::lock_guard<std::recursive_mutex> guard(sock_fd_lock_);
        if (!sock_fd_)
        {
            return -1;
        }
        return sock_fd_->GetFd();
    }

    void Socket::SetTimeOutSecond(uint32_t second)
    {
        max_send_buffer_ms_ = second;
    }

    bool Socket::IsSocketBusy() const
    {
        return !sendable_.load();
    }

    const EventPoller::Ptr Socket::GetPoller() const
    {
        return poller_;
    }

    void Socket::SetSendFlags(int flags)
    {
        sendable_ = flags;
    }

    bool Socket::CloneFromListenSocket(const Socket::Ptr &other)
    {
        SockFD::Ptr sock;
        {
            std::lock_guard<std::recursive_mutex> guard(other->sock_fd_lock_);
            if (!other->sock_fd_)
            {
                WarnL << "sockfd of src socket is null";
                return false;
            }
            sock = std::make_shared<SockFD>(other->sock_fd_, poller_);
        }
        return Listen(sock);
    }

    SockSender &SockSender::operator<<(const char *buf)
    {
        Send(buf, strlen(buf));
        return *this;
    }

    SockSender &SockSender::operator<<(std::string buf)
    {
        Send(buf);
        return *this;
    }

    SockSender &SockSender::operator<<(Buffer::Ptr buf)
    {
        Send(buf);
        return *this;
    }

    ssize_t SockSender::Send(std::string buf)
    {
        auto buffer = BufferRaw::Create();
        buffer->Assign(buf.c_str(), buf.size());
        return Send(std::move(buffer));
    }

    ssize_t SockSender::Send(const char *buf, size_t size)
    {
        auto buffer = BufferRaw::Create();
        buffer->Assign(buf, size);
        return Send(std::move(buffer));
    }

    SocketHelper::SocketHelper(const Socket::Ptr &sock)
    {
        SetSocket(sock);
        SetOnCreateSocket(nullptr);
    }
    SocketHelper::~SocketHelper()
    {
    }

    const EventPoller::Ptr &SocketHelper::GetPoller() const
    {
        return poller_;
    }
    void SocketHelper::SetSendFlushFlag(bool try_flush)
    {
        try_flush_ = try_flush;
    }
    void SocketHelper::SetSendFlags(int flags)
    {
        if (!sock_)
        {
            return;
        }
        sock_->SetSendFlags(flags);
    }
    bool SocketHelper::IsSocketBusy() const
    {
        if (!sock_)
        {
            return true;
        }
        return sock_->IsSocketBusy();
    }
    void SocketHelper::SetOnCreateSocket(Socket::OnCreateSocketCB cb)
    {
        if (cb)
        {
            on_create_socket_ = std::move(cb);
        }
        else
        {
            on_create_socket_ = [](const EventPoller::Ptr &poller)
            { return Socket::CreateSocket(poller, false); };
        }
    }

    Socket::Ptr SocketHelper::CreateSocket()
    {
        return on_create_socket_(poller_);
    }

    Task::Ptr SocketHelper::Async(TaskIn task, bool may_sync)
    {
        return poller_->Async(std::move(task), may_sync);
    }

    Task::Ptr SocketHelper::AsyncFirst(TaskIn task, bool may_sync)
    {
        return poller_->AsyncFirst(std::move(task), may_sync);
    }

    ssize_t SocketHelper::Send(Buffer::Ptr buf)
    {
        if (!sock_)
        {
            return -1;
        }
        return sock_->Send(std::move(buf), nullptr, 0, try_flush_);
    }

    void SocketHelper::Shutdown(const SockException &ex)
    {
        sock_->EmitError(ex);
    }

    void SocketHelper::SetPoller(const EventPoller::Ptr &poller)
    {
        poller_ = poller;
    }

    void SocketHelper::SetSocket(const Socket::Ptr &sock)
    {
        peer_port_ = 0;
        local_port_ = 0;
        peer_ip_.clear();
        local_ip_.clear();
        sock_ = sock;
        if (sock_)
        {
            poller_ = sock_->GetPoller();
        }
    }
    const Socket::Ptr SocketHelper::GetSocket() const
    {
        return sock_;
    }

} // namespace cyberweb

#endif // CYBER_SOCK_CPP
