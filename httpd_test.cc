#include "Cyber/httpd.hpp"
#include <iostream>
#include <signal.h>

void StopServerRunning(int p)
{
    exit(0);
}

int main(int argc, char const *argv[])
{
    try
    {
        cyberweb::HTTPD http_server(15668);
        while (true)
        {
            signal(SIGINT, StopServerRunning);
            http_server.AcceptRequest();
        }
    }
    catch (const char *e)
    {
        std::cerr << e << std::endl;
        exit(-1);
    }

    return 0;
}
