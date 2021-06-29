#if !defined(CYBER_LOGGER_CP)
#define CYBER_LOGGER_CP
#include "logger.h"
#include "onceToken.h"
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

namespace cyber
{
#define CLEAR_COLOR "\033[0m"
    static const char *LOG_CONST_TABLE[][3] = {
        {"\033[44;37m", "\033[34m", "T"},
        {"\033[42;37m", "\033[32m", "D"},
        {"\033[46;37m", "\033[36m", "I"},
        {"\033[43;37m", "\033[33m", "W"},
        {"\033[41;37m", "\033[31m", "E"}};

    Logger *g_defaultLogger = nullptr;

    Logger &GetLogger()
    {
        if (!g_defaultLogger)
        {
            g_defaultLogger = &Logger::Instance();
        }
        return *g_defaultLogger;
    }

    void SetLogger(Logger *logger)
    {
        g_defaultLogger = logger;
    }

    Logger &Logger::Instance()
    {
        static std::shared_ptr<Logger> s_instance(new Logger(ExeName()));
        static Logger &s_instance_ref = *s_instance;
        return s_instance_ref;
    }

    Logger::Logger(const std::string &logger_name)
    {
        logger_name_ = logger_name;
    }
    Logger::~Logger()
    {
        writer_.reset();
        {
            LogContextCapturer(*this, LINFO, __FILE__, __FUNCTION__, __LINE__);
        }
        channels_.clear();
    }

    void Logger::Add(const std::shared_ptr<LogChannel> &channel)
    {
        channels_[channel->Name()] = channel;
    }

    void Logger::Del(const std::string name)
    {
        channels_.erase(name);
    }

    std::shared_ptr<LogChannel> Logger::Get(const std::string name)
    {
        auto it = channels_.find(name);
        if (it == channels_.end())
        {
            return nullptr;
        }
        return it->second;
    }

    void Logger::SetWriter(const std::shared_ptr<LogWriter> &writer)
    {
        writer_ = writer;
    }

    void Logger::SetLevel(LogLevel level)
    {
        for (auto &chn : channels_)
        {
            chn.second->SetLevel(level);
        }
    }
    const std::string &Logger::GetName() const
    {
        return logger_name_;
    }

    void Logger::Write(const LogContextPtr &ctx)
    {
        if (writer_)
        {
            writer_->Write(ctx, *this);
        }
        else
        {
            WriteChannels(ctx);
        }
    }

    void Logger::WriteChannels(const LogContextPtr &ctx)
    {
        for (auto &chn : channels_)
        {
            chn.second->Write(*this, ctx);
        }
    }

    static inline const char *GetFileName(const char *file)
    {
        auto pos = strrchr(file, '/');
        return pos ? pos + 1 : file;
    }

    static inline const char *GetFunctionName(const char *func)
    {
        auto pos = strrchr(func, ':');
        return pos ? pos + 1 : func;
    }

    LogContext::LogContext(LogLevel level, const char *file, const char *function, int line, std::thread::id thread_id) : level_(level), line_(line), file_(GetFileName(file)), function_(GetFunctionName(function)), thread_id_(thread_id)
    {
        gettimeofday(tv_, NULL);
    };

    LogContextCapturer::LogContextCapturer(Logger &logger, LogLevel level, const char *file, const char *function, int line, std::thread::id thread_id = std::thread::id()) : ctx_(new LogContext(level, file, function, line, std::move(thread_id))), logger_(logger)
    {
    }

    LogContextCapturer::LogContextCapturer(const LogContextCapturer &that)
    {
        const_cast<LogContextPtr &>(that.ctx_).reset();
    }

    LogContextCapturer::~LogContextCapturer()
    {
        *this << std::endl;
    }

    LogContextCapturer &LogContextCapturer::operator<<(std::ostream &(*f)(std::ostream &))
    {
        if (!ctx_)
        {
            return *this;
        }
        logger_.Write(ctx_);
        ctx_.reset();
        return *this;
    }

    void LogContextCapturer::Clear()
    {
        ctx_.reset();
    }

    AsyncLogWriter::AsyncLogWriter() : exit_flag_(false)
    {
        thread_ = std::make_shared<std::thread>([this]()
                                                { this->Run(); });
    }
    AsyncLogWriter::~AsyncLogWriter()
    {
        exit_flag_ = true;
        sem_.Post();
        thread_->join();
        FlushAll();
    }

    void AsyncLogWriter::Write(const LogContextPtr &ctx, Logger &logger)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_.EmplaceBack(std::make_pair(ctx, &logger));
        }
        sem_.Post();
    }
    void AsyncLogWriter::Run()
    {
        while (exit_flag_)
        {
            sem_.Wait();
            FlushAll();
        }
    }
    void AsyncLogWriter::FlushAll()
    {
        decltype(pending_) tmp;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tmp.Swap(pending_);
        }
        tmp.ForEach([&](std::pair<LogContext, Logger *> &pr)
                    { pr.second->WriteChannels(pr.first); });
    }

    LogChannel::LogChannel(const std::string &name, LogLevel level = LTRACE) : name_(name), level_(level) {}

    std::string LogChannel::PrintTime(const timeval &tv)
    {
        auto tm = GetLocalTime(tv.tv_sec);
        char buf[128];
        snprintf(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d.%03d",
                 1900 + tm.tm_year,
                 1 + tm.tm_mon,
                 tm.tm_mday,
                 tm.tm_hour,
                 tm.tm_min,
                 tm.tm_sec,
                 (int)(tv.tv_usec / 1000));
        return buf;
    }

    void LogChannel::format(const Logger &logger, std::ostream &ost, const LogContextPtr &ctx, bool enable_color = true, bool enable_detail = true)
    {
        if (!enable_detail && ctx->str().empty())
        {
            return;
        }
        if (enable_color)
        {
            ost << LOG_CONST_TABLE[ctx->level_][1];
        }
        ost << PrintTime(ctx->tv_) << " " << LOG_CONST_TABLE[ctx->level_][2] << " ";
        if (enable_detail)
        {
            ost << logger.GetName() << "[" << getpid() << "-" << ctx->thread_id_;
            ost << "]" << ctx->file_ << ":" << ctx->line_ << " " << ctx->function_ << "|";
        }
        ost << ctx->str();
        if (enable_color)
        {
            ost << CLEAR_COLOR;
        }
        ost << std::endl;
    }

    ConsoleChannel::ConsoleChannel(const std::string &name = "ConsoleChannel", LogLevel level = LTRACE) : LogChannel(name, level)
    {
    }
    void ConsoleChannel::Write(const Logger &logger, const LogContextPtr &log_context)
    {
        if (level_ > log_context->level_)
        {
            return;
        }
        format(logger, std::cout, ctx);
    }
} // namespace cyberweb

#endif // CYBER_LOGGER_CP
