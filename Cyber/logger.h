#if !defined(CYBER_LOGGER_H)
#define CYBER_LOGGER_H

#include <memory>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <mutex>

#include "util.h"
#include "semaphore.h"

namespace cyber
{
    class LogContext;
    class LogChannel;
    class LogWriter;
    class Logger;
    class LogContextCapturer;

    typedef std::shared_ptr<LogContext> LogContextPtr;
    typedef enum
    {
        LTRACE,
        LDEBUG,
        LINFO,
        LWARN,
        LERROR
    } LogLevel;

    Logger &GetLogger();
    void SetLogger(Logger *logger);

    class Logger : public std::enable_shared_from_this<Logger>, public noncopyable
    {
    public:
        friend class AsyncLogWriter;
        typedef std::shared_ptr<Logger> Ptr;

        static Logger &Instance();
        Logger(const std::string &logger_name);
        ~Logger();

        void Add(const std::shared_ptr<LogChannel> &channel);

        void Del(const std::string &name);

        std::shared_ptr<LogChannel> Get(const std::string &name);

        void SetWriter(const std::shared_ptr<LogWriter> &writer);

        void SetLevel(LogLevel level);

        const std::string &GetName() const;

        void Write(const LogContextPtr &ctx);

    private:
        void WriteChannels(const LogContextPtr &ctx);

    private:
        /* data */
        std::map<std::string, std::shared_ptr<LogChannel>> channels_;
        std::shared_ptr<LogWriter> writer_;
        std::string logger_name_;
    };

    class LogContext : public std::ostringstream
    {
    public:
        LogContext(LogLevel level, const char *file, const char *function, int line, std::thread::id thread_id);
        ~LogContext() = default;

        LogLevel level_;
        int line_;
        std::string file_;
        std::string function_;
        std::thread::id thread_id_;
        timeval tv_;
    };

    class LogContextCapturer
    {
    public:
        typedef std::shared_ptr<LogContextCapturer> Ptr;
        LogContextCapturer(Logger &logger, LogLevel level, const char *file, const char *function, int line, std::thread::id thread_id = std::thread::id());
        LogContextCapturer(const LogContextCapturer &that);
        ~LogContextCapturer();

        LogContextCapturer &operator<<(std::ostream &(*f)(std::ostream &));

        template <typename T>
        LogContextCapturer &operator<<(T &&data)
        {
            if (!ctx_)
            {
                return *this;
            }
            (*ctx_) << std::forward<T>(data);
            return *this;
        }

        void Clear();

    private:
        LogContextPtr ctx_;
        Logger &logger_;
    };

    class LogWriter : public noncopyable
    {
    public:
        LogWriter() {}
        virtual ~LogWriter(){};
        virtual void Write(const LogContextPtr &ctx, Logger &logger) = 0;
    };

    class AsyncLogWriter : public LogWriter
    {
    public:
        AsyncLogWriter();
        ~AsyncLogWriter();

    private:
        void Run();
        void FlushAll();
        void Write(const LogContextPtr &ctx, Logger &logger) override;

    private:
        bool exit_flag_;
        std::mutex mutex_;
        Semaphore sem_;
        std::shared_ptr<std::thread> thread_;
        List<std::pair<LogContextPtr, Logger *>> pending_;
    };

    class LogChannel : public noncopyable
    {
    public:
        LogChannel(const std::string &name, LogLevel level = LTRACE);
        virtual ~LogChannel(){};

        virtual void Write(const Logger &logger, const LogContextPtr &ctx) = 0;
        const std::string &Name() const { return name_; };
        void SetLevel(LogLevel level) { level_ = level; };
        static std::string PrintTime(const timeval &tv);

    protected:
        virtual void format(const Logger &logger, std::ostream &ost, const LogContextPtr &ctx, bool enable_color = true, ool enable_detail = true);

    protected:
        std::string name_;
        LogLevel level_;
    };

    class ConsoleChannel : public LogChannel
    {
    public:
        ConsoleChannel(const std::string &name = "ConsoleChannel", LogLevel level = LTRACE);
        ~ConsoleChannel(){};
        void Write(const Logger &logger, const LogContextPtr &log_context) override;
    };

    extern Logger *g_defaultLogger;
#define TraceL LogContextCapturer(GetLogger(), LTRACE, __FILE__, __FUNCTION__, __LINE__, std::this_thread::get_id())
#define DebugL LogContextCapturer(GetLogger(), LDEBUG, __FILE__, __FUNCTION__, __LINE__, std::this_thread::get_id())
#define InfoL LogContextCapturer(GetLogger(), LINFO, __FILE__, __FUNCTION__, __LINE__, std::this_thread::get_id())
#define WarnL LogContextCapturer(GetLogger(), LWARN, __FILE__, __FUNCTION__, __LINE__, std::this_thread::get_id())
#define ErrorL LogContextCapturer(GetLogger(), LERROR, __FILE__, __FUNCTION__, __LINE__, std::this_thread::get_id())
#define WriteL(level) LogContextCapturer(GetLogger(), level, __FILE__, __FUNCTION__, __LINE__, std::this_thread::get_id())

} // namespace cyberweb

#endif // CYBER_LOGGER_H
