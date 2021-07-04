#if !defined(CYBER_HTTP_FILE_MANAGER_H)
#define CYBER_HTTP_FILE_MANAGER_H

#include <functional>
#include "Parser.h"
#include "http_body.h"
#include "../Util/function_traits.h"
#include "../Socket/session.h"
#include "../Util/util.h"

namespace cyber
{
    class HTTPResponseInvokeImp
    {
    public:
        typedef std::function<void(int code, const StrCaseMap &header_out, const HTTPBody::Ptr &body)> HTTPResponseInvokeImpLambda;
        HTTPResponseInvokeImp() {}
        ~HTTPResponseInvokeImp() {}

        template <typename C>
        HTTPResponseInvokeImp(const C &c) : HTTPResponseInvokeImp(typename FunctionTraits<C>::stl_function_type(c)) {}
        HTTPResponseInvokeImp(const HTTPResponseInvokeImpLambda &lambda);
        void operator()(int code, const StrCaseMap &header_out, const HTTPBody::Ptr &body) const;
        void ResponseFile(const StrCaseMap &request_header, const StrCaseMap &response_header, const std::string &file_path) const;

    private:
        HTTPResponseInvokeImpLambda lambda_;
    };

    class HTTPFileManager
    {
    public:
        typedef std::function<void(int code, const std::string &content_type, const StrCaseMap &response_header, const HTTPBody::Ptr &body)> Invoker;

        static void OnAccessPath(TCPSession &sender, Parser &parser, const Invoker &cb);

        static const std::string &GetContentType(const char *name);

        static bool IsFile(std::string str_path);

    private:
        HTTPFileManager() = delete;
        ~HTTPFileManager() = delete;
        static std::string static_path_;
    };

} // namespace cyber

#endif // CYBER_HTTP_FILE_MANAGER_H
