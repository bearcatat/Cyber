#if !defined(CYBER_HTTPD_UTILS)
#define CYBER_HTTPD_UTILS

#include <cstring>
#include <map>
#include <string>
#include <iostream>
#include "config.hpp"

using std::string;

namespace cyberweb
{
    void AnalyzeRequestLine(char *buff, string &method, string &url, string &version)
    {
        int buff_len = strlen(buff);
        int i = 0, j = 0;
        while (i + j < buff_len && buff[i + j] != ' ')
        {
            j++;
        }
        method.assign(buff, j);
        i += (j + 1);
        j = 0;
        while (i + j < buff_len && buff[i + j] != ' ')
        {
            j++;
        }
        url.assign(buff + i, j);

        i += (j + 1);
        j = 0;
        while (i + j < buff_len && buff[i + j] != '\0')
        {
            j++;
        }
        version.assign(buff + i, j);
    }

    std::pair<string, string> AnalyzeHeaderLines(char *buff)
    {
        string field, value;
        int buff_len = strlen(buff);
        int i = 0, j = 0;
        while (i + j < buff_len && buff[i + j] != ':')
        {
            j++;
        }
        field.assign(buff, j);
        i += (j + 1);
        j = 0;
        while (i + j < buff_len && buff[i + j] != '\0')
        {
            j++;
        }
        value.assign(buff + i, j);
        std::pair<string, string> p;
        p.first = std::move(field);
        p.second = std::move(value);
        return std::move(p);
    }
}

#endif // CYBER_HTTPD_UTILS
