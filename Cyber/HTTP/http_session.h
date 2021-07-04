#if !defined(CYBER_HTTP_SESSION_H)
#define CYBER_HTTP_SESSION_H

#include <functional>
#include "../Socket/session.h"
#include "http_request_splitter.h"
#include "Parser.h"
#include "http_body.h"
#include "http_file_manager.h"

namespace cyber
{
    class HTTPSession : public TCPSession, public HTTPRequestSplitter
    {

    public:
        friend class AsyncSender;
        typedef StrCaseMap KeyValue;
        typedef HTTPResponseInvokeImp HttpResponseInvoker;
        HTTPSession(const Socket::Ptr &sock);
        ~HTTPSession() override;

        void OnRecv(const Buffer::Ptr &) override;
        void OnError(const SockException &err) override;
        void OnManager() override;

    protected:
        ssize_t OnRecvHeader(const char *data, size_t len) override;
        void OnRecvContent(const char *data, size_t len) override;

    private:
        void HandleReqGET(ssize_t &content_len);
        void HandleReqGETl(ssize_t &content_len, bool sendBody);
        // void HandleReqPOST(ssize_t &content_len);
        // void HandleReqHEAD(ssize_t &content_len);
        // void HandleReqOPTIONS(ssize_t &content_len);
        void sendNotFound(bool bClose);
        void sendResponse(int code, bool bClose, const char *pcContentType = nullptr,
                          const HTTPSession::KeyValue &header = HTTPSession::KeyValue(),
                          const HTTPBody::Ptr &body = nullptr, bool no_content_length = false);

    private:
        /* data */
        uint64_t total_bytes_usage = 0;
        std::string origin_;
        Ticker ticker_;
        std::function<bool(const char *data, size_t len)> content_callback_;
        Parser parser_;
    };

} // namespace cyber

#endif // CYBER_HTTP_SESSION_H
