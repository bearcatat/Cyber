#if !defined(CYBER_SOCK_H)
#define CYBER_SOCK_H

#include <exception>
#include <string>
#include <sys/unistd.h>
#include <memory>

#include "util.h"
#include "poller.h"

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
    class SockFd : public noncopyable
    {
    public:
        typedef std::shared_ptr<SockFd> Ptr;
        SockFd(int fd, EventPoller::Ptr poller)
        {
            fd_ = fd;
            poller_ = poller;
        }
        ~SockFd()
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

} // namespace cyberweb

#endif // CYBER_SOCK_H
