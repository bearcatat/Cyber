#if !defined(CYBER_HTTP_SESSION_CPP)
#define CYBER_HTTP_SESSION_CPP

#include "http_session.h"
#include "../Util/logger.h"
#include <memory>
#include "http_const.h"

#define KEEPALIVESECOND 5
#define KSENDBUFSIZE 2048

namespace cyber
{
    HTTPSession::HTTPSession(const Socket::Ptr &sock) : TCPSession(sock)
    {
        TraceL;
        sock->SetTimeOutSecond(KEEPALIVESECOND);
    }
    HTTPSession::~HTTPSession() { TraceL; };

    void HTTPSession::OnRecv(const Buffer::Ptr &buf)
    {
        ticker_.ResetTime();
        DebugL << std::this_thread::get_id();
        Input(buf->Data(), buf->Size());
    }

    void HTTPSession::OnError(const SockException &err)
    {
        if (ticker_.CreatedTime() < 10 * 1000)
        {
            TraceL << err.what();
        }
        else
        {
            WarnL << err.what();
        }
    }

    void HTTPSession::OnManager()
    {
        if (ticker_.EleapsedTime() > KEEPALIVESECOND * 1000)
        {
            Shutdown(SockException(ERR_TIMEOUT, "session timeouted"));
        }
    }

    ssize_t HTTPSession::OnRecvHeader(const char *data, size_t len)
    {
        typedef void (HTTPSession::*HTTPCMDHandle)(ssize_t &);
        static std::unordered_map<std::string, HTTPCMDHandle> s_func_map;
        static OnceToken token(
            []()
            { s_func_map.emplace("GET", &HTTPSession::HandleReqGET); },
            nullptr);
        parser_.Parse(data);
        std::string cmd = parser_.Method();
        // DebugL << parser_.Url();
        auto it = s_func_map.find(cmd);
        if (it == s_func_map.end())
        {
            WarnL << "not support " << cmd;
            sendResponse(405, true);
            return 0;
        }
        origin_ = parser_["Origin"];
        ssize_t content_len = 0;
        auto &fun = it->second;
        try
        {
            (this->*fun)(content_len);
        }
        catch (const std::exception &e)
        {
            ErrorL << e.what() << '\n';
            Shutdown(SockException(ERR_SHUTDOWN, e.what()));
        }
        parser_.Clear();
        return content_len;
    }

    void HTTPSession::OnRecvContent(const char *data, size_t len)
    {
        if (content_callback_)
        {
            if (!content_callback_(data, len))
            {
                content_callback_ = nullptr;
            }
        }
    }

    void HTTPSession::HandleReqGET(ssize_t &content_len)
    {
        DebugL << "HandleReqGet";
        HandleReqGETl(content_len, true);
    }
    void HTTPSession::HandleReqGETl(ssize_t &content_len, bool sendBody)
    {
        bool b_close = !strcasecmp(parser_["Connection"].data(), "close");
        // GetSocket()->
        std::weak_ptr<HTTPSession> weak_self = std::dynamic_pointer_cast<HTTPSession>(shared_from_this());
        HTTPFileManager::OnAccessPath(
            *this, parser_, [weak_self, b_close](int code, const std::string &content_type, const StrCaseMap &response_header, const HTTPBody::Ptr &body)
            {
                DebugL << "SendBody";
                auto strong_self = weak_self.lock();
                if (!strong_self)
                {
                    return;
                }

                strong_self->sendResponse(code, b_close, content_type.data(), response_header, body);
                // strong_self->AsyncFirst(
                //     [weak_self, b_close, code, content_type, response_header, body]()
                //     {
                //         auto strong_self = weak_self.lock();
                //         if (!strong_self)
                //         {
                //             return;
                //         }
                //         strong_self->sendResponse(code, b_close, content_type.data(), response_header, body);
                //     });
            });
    }
    void HTTPSession::sendNotFound(bool bClose)
    {
        sendResponse(404, bClose, "text/html", KeyValue(), std::make_shared<HTTPStringBody>("Not Found"));
    }

    static std::string dateStr()
    {
        char buf[64];
        time_t tt = time(NULL);
        strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
        return buf;
    }

    class AsyncSenderData
    {
    public:
        friend class AsyncSender;
        typedef std::shared_ptr<AsyncSenderData> Ptr;
        AsyncSenderData(const TCPSession::Ptr &session, const HTTPBody::Ptr &body, bool close_when_complete)
        {
            session_ = std::dynamic_pointer_cast<HTTPSession>(session);
            body_ = body;
            close_when_complete_ = close_when_complete;
        }

    private:
        std::weak_ptr<HTTPSession> session_;
        HTTPBody::Ptr body_;
        bool close_when_complete_;
        bool read_complete_ = false;
    };

    class AsyncSender
    {
    public:
        typedef std::shared_ptr<AsyncSender> Ptr;
        static bool OnSocketFlushed(AsyncSenderData::Ptr data)
        {
            if (data->read_complete_)
            {
                if (data->close_when_complete_)
                {
                    ShutDown(data->session_.lock());
                }
                return false;
            }
            data->body_->ReadDataAsync(
                KSENDBUFSIZE, [data](const Buffer::Ptr &sendbuf)
                {
                    auto session = data->session_.lock();
                    if (!session)
                    {
                        return;
                    }
                    session->Async(
                        [data, sendbuf]()
                        {
                            auto session = data->session_.lock();
                            if (!session)
                            {
                                return;
                            }
                            AsyncSender::OnRequestData(data, session, sendbuf);
                        },
                        false);
                });
            return true;
        }

    private:
        static void OnRequestData(const AsyncSenderData::Ptr data, const std::shared_ptr<HTTPSession> &session, const Buffer::Ptr &sendbuf)
        {
            session->ticker_.ResetTime();
            if (sendbuf && session->Send(sendbuf) != -1)
            {
                if (!session->IsSocketBusy())
                {
                    OnSocketFlushed(data);
                }
                return;
            }
            data->read_complete_ = true;
            if (!session->IsSocketBusy() && data->close_when_complete_)
            {
                ShutDown(session);
            }
        }

        static void ShutDown(const std::shared_ptr<HTTPSession> &session)
        {
            if (session)
            {
                session->Shutdown(SockException(ERR_SHUTDOWN, StrPrinter() << "close connection after send http body completed."));
            }
        }
    };

    static const std::string kDate = "Date";
    static const std::string kServer = "Server";
    static const std::string kConnection = "Connection";
    static const std::string kKeepAlive = "Keep-Alive";
    static const std::string kContentType = "Content-Type";
    static const std::string kContentLength = "Content-Length";
    static const std::string kAccessControlAllowOrigin = "Access-Control-Allow-Origin";
    static const std::string kAccessControlAllowCredentials = "Access-Control-Allow-Credentials";

    void HTTPSession::sendResponse(int code, bool bClose, const char *pcContentType, const HTTPSession::KeyValue &header, const HTTPBody::Ptr &body, bool no_content_length)
    {
        size_t size = 0;
        if (body && body->RemainSize())
        {
            size = body->RemainSize();
        }
        if ((size_t)size >= SIZE_MAX || size < 0)
        {
            bClose = true;
        }

        HTTPSession::KeyValue &header_out = const_cast<HTTPSession::KeyValue &>(header);

        header_out.emplace(kDate, dateStr());
        header_out.emplace(kConnection, bClose ? "close" : "keep-alive");
        if (!bClose)
        {
            std::string keepAliveString = "timeout=";
            keepAliveString += std::to_string(KEEPALIVESECOND);
            keepAliveString += ", max=100";
            header_out.emplace(kKeepAlive, std::move(keepAliveString));
        }
        if (!origin_.empty())
        {
            header_out.emplace(kAccessControlAllowOrigin, origin_);
            header_out.emplace(kAccessControlAllowCredentials, "true");
        }
        if (!no_content_length && size >= 0 && (size_t)size < SIZE_MAX)
        {
            header_out[kContentLength] = std::to_string(size);
        }
        if (size && !pcContentType)
        {
            pcContentType = "text/plain";
        }
        if ((size || no_content_length) && pcContentType)
        {
            std::string strContentType = pcContentType;
            header_out.emplace(kContentType, std::move(strContentType));
        }
        std::string str;
        str.reserve(256);
        str.reserve(256);
        str += "HTTP/1.1 ";
        str += std::to_string(code);
        str += ' ';
        str += getHttpStatusMessage(code);
        str += "\r\n";
        for (auto &pr : header)
        {
            str += pr.first;
            str += ": ";
            str += pr.second;
            str += "\r\n";
        }
        str += "\r\n";
        SockSender::Send(std::move(str));
        ticker_.ResetTime();

        if (!size)
        {
            if (bClose)
            {
                Shutdown(SockException(ERR_SHUTDOWN, StrPrinter() << "close connection after send http header completed with status code:" << code));
            }
        }
        auto body_data = std::make_shared<AsyncSenderData>(shared_from_this(), body, bClose);
        GetSocket()->SetOnFlush([body_data]()
                                { return AsyncSender::OnSocketFlushed(body_data); });
        AsyncSender::OnSocketFlushed(body_data);
    }
} // namespace cyber

#endif // CYBER_HTTP_SESSION_CPP
