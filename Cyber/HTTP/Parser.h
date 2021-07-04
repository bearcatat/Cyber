#if !defined(CYBER_PASER_H)
#define CYBER_PASER_H

#include <string>
#include <string.h>
#include <map>

namespace cyber
{
    std::string FindField(const char *buf, const char *start, const char *end, size_t buf_size = 0);

    struct StrCaseCompare
    {
        bool operator()(const std::string &a, const std::string &b) const
        {
            return strcasecmp(a.data(), b.data()) < 0;
        }
    };

    class StrCaseMap : public std::multimap<std::string, std::string, StrCaseCompare>
    {
    public:
        typedef std::multimap<std::string, std::string, StrCaseCompare> Super;
        StrCaseMap() = default;
        ~StrCaseMap() = default;

        std::string &operator[](const std::string &k)
        {
            auto it = find(k);
            if (it == end())
            {
                it = Super::emplace(k, "");
            }
            return it->second;
        }

        template <typename V>
        void emplace(const std::string &s, V &&v)
        {
            auto it = find(s);
            if (it != end())
            {
                return;
            }
            Super::emplace(s, std::forward<V>(v));
        }

        template <typename V>
        void emplace_force(const std::string k, V &v)
        {
            Super::emplace(k, v);
        }
    };

    class Parser
    {
    public:
        Parser(/* args */);
        ~Parser();
        void Parse(const char *buf);
        const std::string &Method() const;
        const std::string &Url() const;
        const std::string &FullUrl() const;
        const std::string &Tail() const;
        const std::string operator[](const char *name) const;
        const std::string &Content() const;
        void Clear();
        const std::string &Params() const;
        void SetUrl(const std::string url);
        void SetContent(const std::string content);
        StrCaseMap &GetHeader() const;
        StrCaseMap &GetUrlArgs() const;
        static StrCaseMap ParseArgs(const std::string &str, const char *pair_delim = "&", const char *key_delim = "=");

    private:
        /* data */
        std::string str_method_;
        std::string str_url_;
        std::string str_tail_;
        std::string str_content_;
        std::string str_null_;
        std::string str_full_url_;
        std::string params_;
        mutable StrCaseMap map_headers_;
        mutable StrCaseMap map_url_args_;
    };

} // namespace cyber

#endif // CYBER_PASER_H
