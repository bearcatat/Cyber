#if !defined(CYBER_HTTP_CONST_H)
#define CYBER_HTTP_CONST_H
#include <string>

namespace cyber
{
    const char *getHttpStatusMessage(int status);
    const std::string &getHttpContentType(const char *name);
} // namespace cyber


#endif // CYBER_HTTP_CONST_H
