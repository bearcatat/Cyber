#if !defined(CYBER_HTTPD)
#define CYBER_HTTPD
#include <sys/socket.h>
#include "socket.hpp"
#include "httpd_utils.hpp"
#include <string>
#include <map>
#include "config.hpp"
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>
#include <fstream>
using std::cout;
using std::endl;
using std::string;

#define DEFAULT_HTTP_PORT 80
#define DEFAULT_MAXLINK 2048
#define BUFFSIZE 2048

namespace cyberweb
{
    class HTTPD
    {
    private:
        Socket server_;
        char buff_[BUFFSIZE];
        string static_path_;

    public:
        HTTPD(int port = DEFAULT_HTTP_PORT, int maxlink = DEFAULT_MAXLINK, string static_path = "./static") : static_path_(static_path)
        {
            server_.SetIPAndPort(AF_INET, INADDR_ANY, port);
            server_.Bind();
            server_.Listen(maxlink);
        };

        void AcceptRequest()
        {
            int n;
            string method, url, version;
            Connection conn = server_.Accept();
            // std::cout << "Accept One" << std::endl;
            n = conn.ReadLine(buff_, BUFFSIZE - 1);
            AnalyzeRequestLine(buff_, method, url, version);
            std::cout << method << " " << url << " " << version << std::endl;
            Header req_header;
            while ((n = conn.ReadLine(buff_, BUFFSIZE - 1)) > 0)
            {
                if (n == 0)
                {
                    break;
                }
                std::pair<string, string> p = AnalyzeHeaderLines(buff_);
                req_header.insert(p);
            }
            ResponseRequest(conn, method, url, req_header);
        }

        void ResponseRequest(Connection &conn, string &method, string &url, Header &header)
        {
            if (method == "GET")
            {
                Get(conn, url, header);
            }
        }

        void Get(Connection &conn, string &url, Header &header)
        {
            string local_path;
            int query_start = url.find('?');
            if (query_start != string::npos)
            {
                local_path = url.substr(0, query_start);
            }
            else
            {
                local_path = url;
            }
            if (url == "/")
            {
                url = "/index.html";
            }
            cout << url << endl;
            string access_path = static_path_ + url;
            if (access(access_path.c_str(), F_OK) != -1)
            {
                ServeFile(conn, access_path);
            }
            else
            {
                NotFound(conn);
            }
        }
        void ServeFile(Connection &conn, string &path)
        {
            string suffix = path.substr(path.find_last_of('.') + 1);
            std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
            // compute the size of file.
            file.seekg(0, std::ios::end);
            int file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            // send header
            conn << "HTTP/1.1 200 OK\r\n"
                 << "connection: close\r\n"
                 //  << "content-type: " << CONTENTTYPE[suffix] << "\r\n"
                 << "content-length: " << file_size << "\r\n"
                 << "\r\n";
            // Since the file the httpd send may not be text file,
            // we regard all file as binary file.
            // Thus, we use ifstream::read/Connection::Write instead of <<, >> and getline.
            while ((file.rdstate() & std::ifstream::eofbit) == 0)
            {
                file.read(buff_, BUFFSIZE);
                conn.Write(buff_, file.gcount());
            }
            file.close();
        }
        void NotFound(Connection &conn)
        {
            conn << "HTTP/1.0 404 NOT FOUND\r\n"
                 << "Content-Type: text/html\r\n"
                 << "<HTML><TITLE>Not Found</TITLE>\r\n"
                 << "<BODY><P>The server could not fulfill\r\n"
                 << "your request because the resource specified\r\n"
                 << "is unavailable or nonexistent.\r\n"
                 << "</BODY></HTML>\r\n";
        }

        ~HTTPD()
        {
            server_.Close();
        }
    };
} // namespace cyberweb

#endif // CYBER_HTTPD
