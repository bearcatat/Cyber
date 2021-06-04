#if !defined(CYBER_SOCKET)
#define CYBER_SOCKET

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

namespace cyberweb
{
    class Connection
    {
    private:
        int connfd_;

    public:
        Connection(int c) : connfd_(c)
        {
        }

        void Read(char *buff, size_t buff_len)
        {
            recv(connfd_, buff, buff_len, 0);
        }
        int ReadLine(char *buff, size_t buff_len)
        {
            int i = 0;
            char c = '\0';
            while (i < buff_len - 1 && c != '\n')
            {
                int n = recv(connfd_, &c, 1, 0);
                if (n > 0)
                {
                    if (c == '\r')
                    {
                        n = recv(connfd_, &c, 1, MSG_PEEK);
                        if (n > 0 && c == '\n')
                        {
                            recv(connfd_, &c, 1, 0);
                        }
                        else
                        {
                            c = '\n';
                        }
                    }
                    else
                    {
                        buff[i++] = c;
                    }
                }
                else
                {
                    c = '\n';
                }
            }
            buff[i] = '\0';

            return i;
        }

        void Write(const char *buff, const size_t buff_len)
        {
            send(connfd_, buff, buff_len, 0);
        }
        Connection &operator<<(std::string &data)
        {
            Write(data.c_str(), data.size());
            return *this;
        }

        Connection &operator<<(int &data)
        {
            std::string datastr = std::to_string(data);
            Write(datastr.c_str(), datastr.size());
            return *this;
        }

        Connection &operator<<(const char *data)
        {
            Write(data, strlen(data));
            return *this;
        }

        ~Connection()
        {
            close(connfd_);
            printf("Closing Connection.\n");
        }
    };

    //TODO: IPv6
    class Socket
    {
    private:
        int sockfd_;
        bool is_close;
        sockaddr_in servaddr_;

    public:
        Socket() : is_close(false)
        {
            printf("Starting....\n");
            sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
            if (-1 == sockfd_)
            {
                throw "Create socket error!";
            }
        }
        void SetIPAndPort(sa_family_t family, uint32_t ip, int port)
        {
            bzero(&servaddr_, sizeof(servaddr_));
            servaddr_.sin_family = family;
            servaddr_.sin_addr.s_addr = htonl(ip);
            servaddr_.sin_port = htons(port);
        }
        void SetIPAndPort(sa_family_t family, const char *ip, int port)
        {
            bzero(&servaddr_, sizeof(servaddr_));
            servaddr_.sin_family = family;
            inet_pton(family, ip, &servaddr_.sin_addr);
            servaddr_.sin_port = htons(port);
        }
        void Bind()
        {
            if (-1 == bind(sockfd_, (sockaddr *)(&servaddr_), sizeof(servaddr_)))
            {
                throw "Bind error!";
            }
        }
        void Listen(int maxlink)
        {

            if (-1 == listen(sockfd_, maxlink))
            {
                throw "Listen error!";
            }
        }
        Connection Accept()
        {
            int connfd = accept(sockfd_, NULL, NULL);
            if (-1 == connfd)
            {
                throw "Conn error!";
            }
            return Connection(connfd);
        }
        void Close()
        {
            close(sockfd_);
            is_close = true;
        }
        void Connect()
        {
            if (-1 == connect(sockfd_, (sockaddr *)(&servaddr_), sizeof(servaddr_)))
            {
                throw "Connection error!";
            }
        }
        void Read(char *buff, size_t buff_len)
        {
            recv(sockfd_, buff, buff_len, 0);
        }

        void Write(char *buff, size_t buff_len)
        {
            send(sockfd_, buff, buff_len, 0);
        }
        ~Socket()
        {
            if (is_close == false)
            {
                Close();
            }
            printf("Close Server\n");
        }
    };
}

#endif // CYBER_SOCKET
