#include <sys/socket.h>
#include <thread>
#include <arpa/inet.h>
#include <string.h>
#include "../Cyber/Util/util.h"
#include <iostream>
#include <memory>
using namespace std;

void send_one_request(const char *ip, uint16_t port)
{
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(addr.sin_family, ip, &((&addr)->sin_addr));
    addr.sin_port = htons(port);
    char buf[2048];
    bzero(buf, sizeof(buf));
    int sock_fd = (int)socket(addr.sin_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd < 0)
    {
        throw "create socket failed";
        return;
    }
    if (connect(sock_fd, (sockaddr *)&addr, sizeof(sockaddr)) == 0)
    {
        cyber::StrPrinter strprt;
        strprt << "GET / HTTP/1.0\r\n"
               << "Connection: close\r\n"
               << "Host: localhost:16557\r\n"
               << "User-Agent: ApacheBench/2.3\r\n"
               << "thread: " << this_thread::get_id() << "\r\n"
               << "Accept: */*\r\n\r\n";
        // cout << strprt;
        cout << this_thread::get_id() << " " << strprt.size() << endl;
        ssize_t nsend = send(sock_fd, strprt.data(), strprt.size(), 0);
        cout << this_thread::get_id() << " "
             << "send: " << nsend << endl;
        ssize_t nread = recv(sock_fd, buf, 2048, 0);
        cout << this_thread::get_id() << " " << nread << endl;
        // cout << buf;
    }
    close(sock_fd);
}

void send_requests(const char *ip, uint16_t port, int n)
{
    while (n--)
    {
        send_one_request(ip, port);
    }
}

int main(int argh, char *argv[])
{

    const char *ip = "127.0.0.1";
    uint16_t port = 16557;
    int client_num = atoi(argv[1]), request_num = atoi(argv[2]);
    // cout << argv[0] << " " << argv[1];
    vector<shared_ptr<thread>> threads(client_num);
    for (int i = 0; i < client_num; i++)
    {
        threads[i] = make_shared<thread>(send_requests, ip, port, request_num);
    }

    for (auto &thr : threads)
    {
        thr->join();
    }

    return 0;
}
