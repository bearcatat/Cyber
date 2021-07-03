#if !defined(CYBER_SOCK_H)
#define CYBER_SOCK_H

#include <exception>
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <sys/unistd.h>

#include "util.h"
#include "poller.h"
#include "buffer.h"
#include "logger.h"

#define SOCKET_DEFAULE_FLAGS (MSG_NOSIGNAL | MSG_DONTWAIT)
#define SEND_TIME_OUT_SEC 10

namespace cyber
{
    typedef enum
    {
        ERR_SUCCESS = 0,
        ERR_EOF,
        ERR_TIMEOUT,
        ERR_REFUSE,
        ERR_DNS,
        ERR_SHUTDOWN,
        ERR_OTHER = 0xFF
    } ErrCode;

    class SockException : public std::exception
    {
    public:
        SockException(ErrCode err_code = ERR_SUCCESS, const std::string &err_msg = "", int custom_code = 0)
        {
            err_msg_ = err_msg;
            err_code_ = err_code;
            custom_code_ = custom_code;
        }
        void Reset(ErrCode err_code, const std::string &err_msg)
        {
            err_code_ = err_code;
            err_msg_ = err_msg;
        }
        const char *what() const noexcept override
        {
            return err_msg_.c_str();
        }

        ErrCode GetErrCode() const
        {
            return err_code_;
        }

        operator bool() const
        {
            return err_code_ == ERR_SUCCESS;
        }

        int GetCustomCode() const
        {
            return custom_code_;
        }
        void SetCustomCode(int code)
        {
            custom_code_ = code;
        }

    private:
        ErrCode err_code_;
        std::string err_msg_;
        int custom_code_;
    };

    class SockNum
    {
    public:
        typedef std::shared_ptr<SockNum> Ptr;
        SockNum(int fd) : fd_(fd){};
        ~SockNum()
        {
            DebugL << "Close: " << fd_;
            close(fd_);
        }
        int GetFD() const { return fd_; }

    private:
        int fd_;
    };

    class SockFD : public noncopyable
    {
    public:
        typedef std::shared_ptr<SockFD> Ptr;
        SockFD(int fd, EventPoller::Ptr poller)
        {
            fd_ = std::make_shared<SockNum>(fd);
            poller_ = poller;
        }
        SockFD(SockFD::Ptr other, EventPoller::Ptr poller)
        {
            fd_ = other->fd_;
            poller_ = poller;
        }
        ~SockFD()
        {
            try
            {
                poller_->DelEvent(fd_->GetFD());
            }
            catch (const std::exception &e)
            {
                ErrorL << e.what() << '\n';
            }
        }
        int GetFd() const
        {
            return fd_->GetFD();
        }

    private:
        SockNum::Ptr fd_;
        EventPoller::Ptr poller_;
    };

    class Socket : public std::enable_shared_from_this<Socket>, public noncopyable
    {
    public:
        typedef std::shared_ptr<Socket> Ptr;

        typedef std::function<void(const Buffer::Ptr &buf, sockaddr *addr, int addr_len)> OnReadCB;
        typedef std::function<void(const SockException &err)> OnErrorCB;
        typedef std::function<void(Socket::Ptr &sock)> OnAcceptCB;
        typedef std::function<bool()> OnFlushCB;
        typedef std::function<Ptr(const EventPoller::Ptr &poller)> OnCreateSocketCB;

        static Ptr CreateSocket(const EventPoller::Ptr &poller, bool enable_recursive_mutex = true);
        Socket(const EventPoller::Ptr &poller, bool enable_recursive_mutex = true);

        ~Socket();

        virtual void Connect(const std::string &ip, sa_family_t family, uint16_t port, OnErrorCB con_cb, float timeout_sec = 5);

        virtual bool Listen(uint16_t port, const std::string &local_ip, int backlog = 1024);

        virtual void SetOnRead(OnReadCB cb);
        virtual void SetOnError(OnErrorCB cb);
        virtual void SetOnAccept(OnAcceptCB cb);
        virtual void SetOnFlush(OnFlushCB cb);
        virtual void SetOnBeforeAccept(OnCreateSocketCB cb);

        ssize_t Send(const char *buf, size_t size = 0, sockaddr *addr = nullptr, socklen_t addr_len = 0, bool try_flush = true);
        ssize_t Send(const std::string &buf, sockaddr *addr = nullptr, socklen_t addr_len = 0, bool try_flush = true);
        ssize_t Send(const Buffer::Ptr &buf, sockaddr *addr = nullptr, socklen_t addr_len = 0, bool try_flush = true);
        virtual ssize_t Send(const BufferSock::Ptr &buf, bool try_flush = true);

        virtual bool EmitError(const SockException &err) noexcept;
        virtual void EnableRecv(bool enabled);
        virtual int GetFD();
        virtual void SetTimeOutSecond(uint32_t second);
        virtual bool IsSocketBusy() const;
        virtual const EventPoller::Ptr GetPoller() const;
        virtual void SetSendFlags(int flags = SOCKET_DEFAULE_FLAGS);

        virtual bool CloneFromListenSocket(const Socket::Ptr &other);

        virtual void CLoseSock();
        virtual size_t GetSendBufferCount();
        virtual uint64_t ElapsedTimeAfterFlushed();

    private:
        SockFD::Ptr SetPeerSock(int fd);
        SockFD::Ptr MakeSock(int sock);
        int OnAccept(const SockFD::Ptr &sock, int event) noexcept;
        ssize_t OnRead(const SockFD::Ptr &sock) noexcept;
        void OnWriteAble(const SockFD::Ptr &sock);
        void OnConnected(const SockFD::Ptr &sock, const OnErrorCB &cb);
        void OnFlushed(const SockFD::Ptr &p_sock);
        void StartWriteAbleEvent(const SockFD::Ptr &sock);
        void StopWriteAbleEvent(const SockFD::Ptr &sock);
        bool Listen(const SockFD::Ptr &sock);
        bool FlushData(const SockFD::Ptr &sock, bool poller_thread);
        bool AttachEvent(const SockFD::Ptr &sock);

    private:
        int sock_flags_ = SOCKET_DEFAULE_FLAGS;
        uint32_t max_send_buffer_ms_ = SEND_TIME_OUT_SEC * 1000;
        std::atomic<bool> enable_recv_{true};
        std::atomic<bool> sendable_{true};

        std::shared_ptr<std::function<void(int)>> async_con_cb_;
        BufferRaw::Ptr read_buffer_;
        SockFD::Ptr sock_fd_;
        EventPoller::Ptr poller_;
        std::recursive_mutex sock_fd_lock_;

        OnErrorCB on_err_;
        OnReadCB on_read_;
        OnFlushCB on_flush_;
        OnAcceptCB on_accept_;
        OnCreateSocketCB on_before_accept_;
        std::recursive_mutex event_lock_;

        List<BufferSock::Ptr> send_buf_waiting_;
        std::recursive_mutex send_buf_waiting_lock_;
        List<BufferList::Ptr> send_buf_sending_;
        std::recursive_mutex send_buf_sending_lock_;
    };

    class SockSender
    {
    public:
        SockSender() = default;
        virtual ~SockSender() = default;
        virtual ssize_t Send(Buffer::Ptr buf) = 0;
        virtual void Shutdown(const SockException &ex = SockException(ERR_SHUTDOWN, "self shutdown")) = 0;

        SockSender &operator<<(const char *buf);
        SockSender &operator<<(std::string buf);
        SockSender &operator<<(Buffer::Ptr buf);

        template <typename T>
        SockSender &operator<<(T &&buf)
        {
            std::ostringstream ss;
            ss << std::forward<T>(buf);
            Send(ss.str());
            return *this;
        }
        ssize_t Send(std::string buf);
        ssize_t Send(const char *buf, size_t size = 0);
    };

    class SocketHelper : public SockSender, public TaskExecutorInterface
    {
    public:
        SocketHelper(const Socket::Ptr &sock);
        ~SocketHelper() override;

        const EventPoller::Ptr &GetPoller() const;

        void SetSendFlushFlag(bool try_flush);

        void SetSendFlags(int flags);

        bool IsSocketBusy() const;

        void SetOnCreateSocket(Socket::OnCreateSocketCB cb);

        Socket::Ptr CreateSocket();

        Task::Ptr Async(TaskIn task, bool may_sync = true) override;
        Task::Ptr AsyncFirst(TaskIn task, bool may_sync = true) override;

        ssize_t Send(Buffer::Ptr buf) override;
        void Shutdown(const SockException &ex = SockException(ERR_SHUTDOWN, "self shutdown")) override;

    protected:
        void SetPoller(const EventPoller::Ptr &poller);
        void SetSocket(const Socket::Ptr &sock);
        const Socket::Ptr GetSocket() const;

    private:
        /* data */
        bool try_flush_ = true;
        uint16_t peer_port_ = 0;
        uint16_t local_port_ = 0;
        std::string peer_ip_;
        std::string local_ip_;
        Socket::Ptr sock_;
        EventPoller::Ptr poller_;
        Socket::OnCreateSocketCB on_create_socket_;
    };

} // namespace cyberweb

#endif // CYBER_SOCK_H
