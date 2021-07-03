#if !defined(CYBER_PIPE_WRAP)
#define CYBER_PIPE_WRAP

#include <stdexcept>
#include <unistd.h>

#include "uv_errno.h"
#include "sockutil.h"

#define CloseFD(fd) \
    if (fd != -1)   \
    {               \
        close(fd);  \
        fd = -1;    \
    }

namespace cyber
{
    class PipeWrap
    {
    public:
        PipeWrap(/* args */)
        {
            if (-1 == pipe(pipe_fd_))
            {
                throw std::runtime_error(get_uv_errmsg());
            }
            SockUtil::SetNoBlocked(pipe_fd_[0], true);
            SockUtil::SetNoBlocked(pipe_fd_[1], false);
        }
        ~PipeWrap()
        {
            ClearFD();
        }
        int Write(const void *buf, int n)
        {
            int ret;
            do
            {
                /* code */
                ret = write(pipe_fd_[1], (char *)buf, n);
            } while (-1 == ret && UV_EINTR == get_uv_error(true));
            return ret;
        }
        int Read(void *buf, int n)
        {
            int ret;
            do
            {
                /* code */
                ret = read(pipe_fd_[0], (char *)buf, n);
            } while (-1 == ret && UV_EINTR == get_uv_error(true));
            return ret;
        }
        int ReadFD() const
        {
            return pipe_fd_[0];
        }
        int WriteFD() const
        {
            return pipe_fd_[1];
        }

    private:
        int pipe_fd_[2] = {-1, -1};
        void ClearFD()
        {
            CloseFD(pipe_fd_[0]);
            CloseFD(pipe_fd_[1]);
        }
    };

} // namespace cyberweb

#endif // CYBER_PIPE_WRAP
