#if !defined(CYBER_TIME_TIKER_H)
#define CYBER_TIME_TIKER_H

#include <inttypes.h>

#include "util.h"
#include "logger.h"

namespace cyber
{
    class Ticker
    {
    public:
        Ticker(uint64_t min_ms = 0, LogContextCapturer ctx = LogContextCapturer(Logger::Instance(), LWARN, __FILE__, "", __LINE__), bool print_log = false) : ctx_(std::move(ctx))
        {
            if (!print_log)
            {
                ctx_.Clear();
            }
            created_ = begin_ = GetCurrentMillisecond();
            min_ms_ = min_ms;
        }
        ~Ticker()
        {
            uint64_t tm = CreatedTime();
            if (tm > min_ms_)
            {
                ctx_ << "take time:" << tm << "ms"
                     << ",thread may be overloaded";
            }
            else
            {
                ctx_.Clear();
            }
        };

        uint64_t EleapsedTime() const
        {
            return GetCurrentMillisecond() - begin_;
        }

        uint64_t CreatedTime() const
        {
            return GetCurrentMillisecond() - created_;
        }

        void ResetTime()
        {
            begin_ = GetCurrentMillisecond();
        }

    private:
        uint64_t min_ms_;
        uint64_t begin_;
        uint64_t created_;
        LogContextCapturer ctx_;
    };

    class SmoothTicker
    {
    public:
        SmoothTicker(uint64_t reset_ms = 10000)
        {
            reset_ms_ = reset_ms;
            ticker_.ResetTime();
        }
        ~SmoothTicker() {}
        uint64_t ElapsedTime()
        {
            auto now_time = ticker_.EleapsedTime();
            if (first_time_ == 0)
            {
                if (now_time < last_time_)
                {
                    auto last_time = last_time_ - time_inc_;
                    double elapsed_time = (now_time - last_time);
                    time_inc_ += (elapsed_time / ++pkt_count_) / 3;
                    auto ret_time = last_time + time_inc_;
                    last_time = (uint64_t)ret_time;
                    return (uint64_t)ret_time;
                }
                first_time_ = now_time;
                last_time_ = now_time;
                pkt_count_ = 0;
                time_inc_ = 0;
                return now_time;
            }

            auto elapse_time = now_time - first_time_;
            time_inc_ += (elapse_time / ++pkt_count_);
            auto ret = first_time_ + time_inc_;
            if (elapse_time > reset_ms_)
            {
                first_time_ = 0;
            }
            last_time_ = (uint64_t)ret;
            return (uint64_t)ret;
        }

        void ResetTime()
        {
            first_time_ = 0;
            pkt_count_ = 0;
            ticker_.ResetTime();
        }

    private:
        double time_inc_ = 0;
        uint64_t first_time_ = 0;
        uint64_t last_time_ = 0;
        uint64_t pkt_count_ = 0;
        uint64_t reset_ms_;
        Ticker ticker_;
    };
} // namespace cyberweb

#endif // CYBER_TIME_TIKER_H
