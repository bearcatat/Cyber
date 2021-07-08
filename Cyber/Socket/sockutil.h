#if !defined(CYBER_SOCKUTIL_CPP)
#define CYBER_SOCKUTIL_CPP

#include <sys/ioctl.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/unistd.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#define SOCKET_DEFAULT_BUF_SIZE 256 * 1024

namespace cyber
{
    class SockUtil
    {
    public:
        static int SetNoBlocked(int sock, bool noblock = true)
        {
            // int block_flag = noblock ? O_NONBLOCK : ~O_NONBLOCK;
            // int flags = fcntl(sock, F_GETFL, 0);
            // int ret = fcntl(sock, F_SETFL, flags & block_flag);
            int ul = noblock;
            int ret = ioctl(sock, FIONBIO, &ul);
            if (ret == -1)
            {
                throw "non blocked failed";
            }
            return ret;
        }

        static int setNoDelay(int sock, bool on = true)
        {
            int opt = on ? 1 : 0;
            int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt, static_cast<socklen_t>(sizeof(opt)));
            if (ret == -1)
            {
                TraceL << "设置 NoDelay 失败!";
            }
            return ret;
        }
        static int setRecvBuf(int sock, int size = SOCKET_DEFAULT_BUF_SIZE)
        {
            int ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
            if (ret == -1)
            {
                TraceL << "设置接收缓冲区失败!";
            }
            return ret;
        }
        static int setSendBuf(int sock, int size = SOCKET_DEFAULT_BUF_SIZE)
        {
            int ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
            if (ret == -1)
            {
                TraceL << "设置发送缓冲区失败!";
            }
            return ret;
        }
        static int setCloseWait(int sock, int second = 0)
        {
            linger m_sLinger;
            //在调用closesocket()时还有数据未发送完，允许等待
            // 若m_sLinger.l_onoff=0;则调用closesocket()后强制关闭
            m_sLinger.l_onoff = (second > 0);
            m_sLinger.l_linger = second; //设置等待时间为x秒
            int ret = setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&m_sLinger, sizeof(linger));
            return ret;
        }

        static int setCloExec(int fd, bool on = true)
        {
            int flags = fcntl(fd, F_GETFD);
            if (flags == -1)
            {
                TraceL << "设置 FD_CLOEXEC 失败!";
                return -1;
            }
            if (on)
            {
                flags |= FD_CLOEXEC;
            }
            else
            {
                int cloexec = FD_CLOEXEC;
                flags &= ~cloexec;
            }
            int ret = fcntl(fd, F_SETFD, flags);
            if (ret == -1)
            {
                TraceL << "设置 FD_CLOEXEC 失败!";
                return -1;
            }
            return ret;
        }

        static int setReuseable(int sockFd, bool on = true)
        {
            int opt = on ? 1 : 0;
            int ret = setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, static_cast<socklen_t>(sizeof(opt)));
            if (ret == -1)
            {
                TraceL << "SO_REUSEADDR fail";
                return ret;
            }
            ret = setsockopt(sockFd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, static_cast<socklen_t>(sizeof(opt)));
            if (ret == -1)
            {
                TraceL << "SO_REUSEPORT fail";
            }
            return ret;
        }

        static int Connect(const char *ip, sa_family_t family, uint16_t port, bool bAsync = true)
        {
            sockaddr addr;
            bzero(&addr, sizeof(addr));
            addr.sa_family = family;
            inet_pton(family, ip, &(((sockaddr_in *)&addr)->sin_addr));
            ((sockaddr_in *)&addr)->sin_port = htons(port);

            int sock_fd = (int)socket(addr.sa_family, SOCK_STREAM, IPPROTO_TCP);
            if (sock_fd < 0)
            {
                throw "create socket failed";
                return -1;
            }
            setReuseable(sock_fd);
            SetNoBlocked(sock_fd, bAsync);
            setSendBuf(sock_fd);
            setRecvBuf(sock_fd);
            setCloseWait(sock_fd);
            setCloExec(sock_fd);

            if (connect(sock_fd, &addr, sizeof(sockaddr)) == 0)
            {
                return sock_fd;
            }
            if (bAsync)
            {
                return sock_fd;
            }
            close(sock_fd);
            return -1;
        }

        static int Listen(const uint16_t port, const char *local_ip = "0.0.0.0", int back_log = 1024)
        {
            int sockfd = -1;
            if (-1 == (sockfd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)))
            {
                throw "create socket failed when listend";
            }
            setReuseable(sockfd);
            SetNoBlocked(sockfd);
            setNoDelay(sockfd);
            setSendBuf(sockfd);
            setRecvBuf(sockfd);
            setCloseWait(sockfd);
            setCloExec(sockfd);

            if (-1 == BindSock(sockfd, local_ip, port))
            {
                close(sockfd);
                return -1;
            }

            if (-1 == listen(sockfd, back_log))
            {
                throw "listen failed";
                close(sockfd);
                return -1;
            }
            return sockfd;
        }

        static int BindSock(int sockfd, const char *ip, uint16_t port)
        {
            sockaddr_in servaddr;
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(port);
            servaddr.sin_addr.s_addr = inet_addr(ip);
            bzero(&servaddr.sin_zero, sizeof(servaddr.sin_zero));

            if (-1 == bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)))
            {
                throw "bind socket failed";
                return -1;
            }
            return 0;
        }
    };

} // namespace cyberweb

#endif // CYBER_SOCKUTIL_CPP
