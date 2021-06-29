#if !defined(CYBER_SOCKUTIL_CPP)
#define CYBER_SOCKUTIL_CPP

#include <sys/ioctl.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/unistd.h>

namespace cyber
{
    class SockUtil
    {
    public:
        static int SetNoBlocked(int sock, bool noblock = true)
        {
            unsigned long ul = noblock;
            int ret = ioctl(sock, FIONBIO, &ul);
            if (ret == -1)
            {
                throw "non blocked failed";
            }
            return ret;
        }

        static int Connect(const char *ip, sa_family_t family, uint16_t port, bool bAsync = true)
        {
            sockaddr addr;
            bzero(&addr, sizeof(addr));
            addr.sa_family = family;
            inet_pton(family, ip, ((sockaddr_in *)&addr)->sin_addr);
            ((sockaddr_in *)&addr)->sin_port = htons(port);

            int sock_fd = (int)socket(addr.sa_family, SOCK_STREAM, IPPROTO_TCP);
            if (sock_fd < 0)
            {
                throw "create socket failed";
                return -1;
            }
            SetNoBlocked(sock_fd, bAsync);
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
            SetNoBlocked(sockfd);

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

            if (-1 == bind(sockfd, servaddr, size(servaddr)))
            {
                throw "bind socket failed";
                return -1;
            }
            return 0;
        }
    };

} // namespace cyberweb

#endif // CYBER_SOCKUTIL_CPP
