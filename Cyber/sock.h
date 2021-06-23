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

#define SOCKET_DEFAULE_FLAGS (MSG_NOSIGNAL | MSG_DONTWAIT)
#define SEND_TIME_OUT_SEC 10

namespace cyberweb
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
        const char *What() const noexcept override
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
    class SockFD : public noncopyable
    {
    public:
        typedef std::shared_ptr<SockFD> Ptr;
        SockFD(int fd, EventPoller::Ptr poller)
        {
            fd_ = fd;
            poller_ = poller;
        }
        ~SockFD()
        {
            poller_->DelEvent(fd_);
            close(fd_);
        }
        int GetFd() const
        {
            return fd_;
        }

    private:
        int fd_;
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

        static Ptr CreateSocket(const EventPoller::Ptr &poller, bool enable_mutex = true);
        Socket(const EventPoller::Ptr &poller, bool enable_mutex = true);

        ~Socket() override;

        virtual void Connect(const string &ip, uint16_t port, OnErrorCB con_cb, float timeout_sec = 5);

        virtual bool Listen(uint16_t port, const string &local_ip, int backlog = 1024);

        virtual void SetOnRead(OnReadCB cb);
        virtual void SetOnError(OnErrorCB cb);
        virtual void SetOnAccept(OnAcceptCB cb);
        virtual void SetOnFlush(OnFlushCB cb);
        virtual void SetOnBeforeAccept(OnCreateSocketCB cb);

        ssize_t Send(const char *buf, size_t size = 0, sockaddr *addr = nullptr, socklen_t addr_len, bool try_flush = true);
        ssize_t Send(const std::string &buf, sockaddr *addr = nullptr, socklen_t addr_len = 0, bool try_flush = true);
        ssize_t Send(const Buffer::Ptr &buf, sockaddr *addr = nullptr, socklen_t addr_len = 0, bool try_flush = true);
        virtual ssize_t Send(const BufferSock::Ptr &buf, bool try_flush = true);

        virtual void EmitError(const SockException &err) noexcept;
        virtual void EnableRecv(bool enabled);
        virtual int GetFD() const;
        virtual void SetTimeOutSecond(uint32_t second);
        virtual bool IsSocketBusy() const;
        virtual const EventPoller::Ptr GetPoller() const;
        virtual void SetSendFlags(int flags = SOCKET_DEFAULE_FLAGS);

        virtual void CLoseSock();
        virtual void GetSendBufferCount();
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
        std::mutex sock_fd_lock_;

        OnErrorCB on_err_;
        OnReadCB on_read_;
        OnFlushCB on_flush_;
        OnAcceptCB on_accept_;
        OnCreateSocketCB on_before_accept_;
        std::mutex event_lock_;

        List<BufferSock::Ptr> send_buf_waiting_;
        std::mutex send_buf_waiting_lock_;
        List<BufferList::Ptr> send_buf_sending_;
        std::mutex send_buf_sending_lock_;
    };

} // namespace cyberweb

#endif // CYBER_SOCK_H
