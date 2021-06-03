#if !defined(CYBER_CONFIG)
#define CYBER_CONFIG
#include <map>

namespace cyberweb
{
    typedef std::map<std::string, std::string> Header;
    typedef std::map<std::string, std::string> ContentTypeMap;

    std::map<std::string, std::string> CONTENTTYPE = {
        {"html", "text/html"},
        {"ico", "image/x-icon"}
    };

} // namespace cyberweb

#endif // CYBER_CONFIG
