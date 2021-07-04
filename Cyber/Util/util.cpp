#if !defined(CYBER_UTIL_CPP)
#define CYBER_UTIL_CPP

#include "util.h"
#include "onceToken.h"
#include "string.h"

namespace cyber
{
    static inline uint64_t GetCurrentMicrosecondOrigin()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static std::atomic<uint64_t> s_current_microsecond(0);
    static std::atomic<uint64_t> s_current_millisecond(0);
    static std::atomic<uint64_t> s_current_microsecond_system(GetCurrentMicrosecondOrigin());
    static std::atomic<uint64_t> s_current_millisecond_system(GetCurrentMicrosecondOrigin() / 1000);
    static inline bool InitMillisecondThread()
    {
        static std::thread s_thread(
            []()
            {
                uint64_t last = GetCurrentMicrosecondOrigin();
                uint64_t now;
                uint64_t microsecond = 0;
                while (true)
                {
                    now = GetCurrentMicrosecondOrigin();

                    s_current_microsecond_system.store(now, std::memory_order_release);
                    s_current_millisecond_system.store(now / 1000, std::memory_order_release);

                    int64_t expired = now - last;
                    last = now;
                    if (expired > 0 && expired < 1000 * 1000)
                    {
                        microsecond += expired;
                        s_current_microsecond.store(microsecond, std::memory_order_release);
                        s_current_millisecond.store(microsecond / 1000, std::memory_order_release);
                    }
                    else if (expired != 0)
                    {
                        std::cerr << "Stamp expired is not abnormal" << expired;
                    }
                    usleep(500);
                }
            });
        static OnceToken s_token([]()
                                 { s_thread.detach(); });
        return true;
    }

    uint64_t GetCurrentMillisecond(bool system_time)
    {
        static bool flag = InitMillisecondThread();
        if (system_time)
        {
            return s_current_millisecond_system.load(std::memory_order_acquire);
        }
        return s_current_millisecond.load(std::memory_order_acquire);
    }

    uint64_t GetCurrentMicrosecond(bool system_time)
    {
        static bool flag = InitMillisecondThread();
        if (system_time)
        {
            return s_current_microsecond_system.load(std::memory_order_acquire);
        }
        return s_current_microsecond.load(std::memory_order_acquire);
    }

    struct tm GetLocalTime(time_t sec)
    {
        struct tm tm;
        localtime_r(&sec, &tm);
        return tm;
    }

    std::string ExeName()
    {
        auto path = ExePath();
        return path.substr(path.rfind('/') + 1);
    }

    std::string ExePath()
    {
        char buffer[PATH_MAX * 2 + 1] = {0};
        int n = -1;
        n = readlink("/proc/self/exe", buffer, sizeof(buffer));

        std::string file_path;
        if (n <= 0)
        {
            file_path = "./";
        }
        else
        {
            file_path = buffer;
        }
        return file_path;
    }
    std::vector<std::string> split(const std::string &s, const char *delim)
    {
        std::vector<std::string> ret;
        size_t last = 0;
        auto index = s.find(delim, last);
        while (index != std::string::npos)
        {
            if (index - last > 0)
            {
                ret.push_back(s.substr(last, index - last));
            }
            last = index + strlen(delim);
            index = s.find(delim, last);
        }
        if (!s.size() || s.size() - last > 0)
        {
            ret.push_back(s.substr(last));
        }
        return ret;
    }

#define TRIM(s, chars)                                         \
    do                                                         \
    {                                                          \
        std::string map(0xFF, '\0');                           \
        for (auto &ch : chars)                                 \
        {                                                      \
            map[(unsigned char &)ch] = '\1';                   \
        }                                                      \
        while (s.size() && map.at((unsigned char &)s.back()))  \
            s.pop_back();                                      \
        while (s.size() && map.at((unsigned char &)s.front())) \
            s.erase(0, 1);                                     \
    } while (0);

    //去除前后的空格、回车符、制表符
    std::string &trim(std::string &s, const std::string &chars)
    {
        TRIM(s, chars);
        return s;
    }
    std::string trim(std::string &&s, const std::string &chars)
    {
        TRIM(s, chars);
        return std::move(s);
    }
} // namespace cyber

#endif // CYBER_UTIL_CPP
