#if !defined(CYBER_PARSER_CPP)
#define CYBER_PARSER_CPP
#include "Parser.h"
#include "../Util/util.h"
#include "../Util/logger.h"

namespace cyber
{
    std::string FindField(const char *buf, const char *start, const char *end, size_t buf_size)
    {
        if (buf_size <= 0)
        {
            buf_size = strlen(buf);
        }
        const char *msg_start = buf, *msg_end = buf + buf_size;
        size_t len = 0;
        if (start != NULL)
        {
            len = strlen(start);
            msg_start = strstr(buf, start);
        }
        if (msg_start == NULL)
        {
            return "";
        }
        msg_start += len;
        if (end != NULL)
        {
            msg_end = strstr(msg_start, end);
            if (msg_end == NULL)
            {
                return "";
            }
        }
        return std::string(msg_start, msg_end);
    }
    Parser::Parser(/* args */) {}
    Parser::~Parser(/* args */) {}

    void Parser::Parse(const char *buf)
    {
        const char *start = buf;
        Clear();
        while (true)
        {
            /* code */
            auto line = FindField(start, NULL, "\r\n");
            if (line.size() == 0)
            {
                break;
            }
            if (start == buf)
            {
                str_method_ = FindField(line.data(), NULL, " ");
                str_full_url_ = FindField(line.data(), " ", " ");
                auto args_pos = str_full_url_.find('?');
                if (args_pos != std::string::npos)
                {
                    str_url_ = str_full_url_.substr(0, args_pos);
                    params_ = str_full_url_.substr(args_pos + 1);
                    map_url_args_ = ParseArgs(params_);
                }
                else
                {
                    str_url_ = str_full_url_;
                }
                str_tail_ = FindField(line.data(), (str_full_url_ + " ").data(), NULL);
            }
            else
            {
                auto field = FindField(line.data(), NULL, ": ");
                auto value = FindField(line.data(), ": ", NULL);
                if (field.size() != 0)
                {
                    map_headers_.emplace_force(field, value);
                }
            }
            start = start + line.size() + 2;
            if (strncmp(start, "\r\n", 2) == 0)
            {
                str_content_ = FindField(start, "\r\n", NULL);
                break;
            }
        }
    }

    const std::string &Parser::Method() const
    {
        return str_method_;
    }

    const std::string &Parser::Url() const
    {
        return str_url_;
    }

    const std::string &Parser::FullUrl() const
    {
        return str_full_url_;
    }

    const std::string &Parser::Tail() const
    {
        return str_tail_;
    }
    const std::string Parser::operator[](const char *name) const
    {
        auto it = map_headers_.find(name);
        return it == map_headers_.end() ? "" : it->second;
    }
    const std::string &Parser::Content() const
    {
        return str_content_;
    }

    void Parser::Clear()
    {
        str_method_.clear();
        str_url_.clear();
        str_full_url_.clear();
        str_tail_.clear();
        str_content_.clear();
        params_.clear();
        map_headers_.clear();
        map_url_args_.clear();
    }

    const std::string &Parser::Params() const
    {
        return params_;
    }

    void Parser::SetUrl(const std::string url)
    {
        str_url_ = url;
    }

    void Parser::SetContent(const std::string content)
    {
        str_content_ = content;
    }

    StrCaseMap &Parser::GetHeader() const
    {
        return map_headers_;
    }

    StrCaseMap &Parser::GetUrlArgs() const
    {
        return map_url_args_;
    }

    StrCaseMap Parser::ParseArgs(const std::string &str, const char *pair_delim, const char *key_delim)
    {
        StrCaseMap ret;
        auto arg_vec = split(str, pair_delim);
        for (std::string &key_val : arg_vec)
        {
            auto key = trim(FindField(key_val.data(), NULL, key_delim));
            if (!key.empty())
            {
                auto val = trim(FindField(key_val.data(), key_delim, NULL));
                ret.emplace_force(key, val);
            }
            else
            {
                ret.emplace_force(key_val, "");
            }
        }
        return ret;
    }

} // namespace cyber

#endif // CYBER_PARSER_CPP
