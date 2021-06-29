#if !defined(CYBER_SOCKET_H)
#define CYBER_SOCKET_H

#include <exception>
#include <string>
#include <memory>

// #include "util.h"
// #include "poll.h"

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
        ERR_OTHER = 0xff,
    } ErrCode;
    class SockException:public 
    {
    private:
        /* data */
    public:
        SockException(/* args */);
        ~SockException();
    };
    

    // class SockException : public std::exception
    // {
    // public:
    //     SockException(ErrCode err_code = ERR_SUCCESS, const std::string &err_msg = "", int custom_code = 0)
    //         : err_code_(err_code), err_msg_(std::move(err_msg)), custom_code_(custom_code)
    //     {
    //     }

    //     void Reset(ErrCode err_code, const std::string &err_msg)
    //     {
    //         err_code_ = err_code;
    //         err_msg_ = err_msg;
    //     }

    //     const char *What() const noexcept override
    //     {
    //         return err_msg_.c_str();
    //     }

    //     operator bool() const
    //     {
    //         return err_code_ == ERR_SUCCESS;
    //     }

    //     int GetCustomCode() const
    //     {
    //         return custom_code_;
    //     }

    //     int SetCustomCode(int custom_code)
    //     {
    //         custom_code_ = custom_code;
    //     }

    // private:
    //     std::string err_msg_;
    //     ErrCode err_code_;
    //     int custom_code_;
    // };

    // class SockFD : public noncopyable
    // {
    // public:
    //     typedef std::shared_ptr<SockFD> Ptr;
    //     SockFD(int fd, const EventPoller::Ptr &poller)
    //     {
    //         fd_ = new int(fd);
    //         poller_ = poller;
    //     }

    //     ~SockFD()
    //     {
    //     }

    // private:
    //     int *fd_;
    //     EventPoller::Ptr poller_;
    // };
} // namespace cyberweb

#endif // CYBER_SOCKET_H
