#if !defined(CYBER_HTTP_FILE_MANAGER_CPP)
#define CYBER_HTTP_FILE_MANAGER_CPP

#include "http_file_manager.h"
#include "http_session.h"
#include "http_const.h"

namespace cyber
{
    HTTPResponseInvokeImp::HTTPResponseInvokeImp(const HTTPResponseInvokeImpLambda &lambda)
    {
        lambda_ = lambda;
    }

    void HTTPResponseInvokeImp::operator()(int code, const StrCaseMap &header_out, const HTTPBody::Ptr &body) const
    {
        if (lambda_)
        {
            lambda_(code, header_out, body);
        }
    }

    void HTTPResponseInvokeImp::ResponseFile(const StrCaseMap &request_header, const StrCaseMap &response_header, const std::string &file_path) const
    {
        StrCaseMap &http_header = const_cast<StrCaseMap &>(response_header);
        std::shared_ptr<std::ifstream> file(
            new std::ifstream(file_path.data(), std::ios::in | std::ios::binary), [](std::ifstream *f)
            {
                if (f)
                {
                    f->close();
                }
            });

        if (!file)
        {
            auto str_content_type = StrPrinter() << "text/html" << std::endl;
            http_header["Content-Type"] = str_content_type;
            (*this)(404, http_header, std::make_shared<HTTPStringBody>("File Not Found."));
            return;
        }

        auto &str_range = const_cast<StrCaseMap &>(request_header)["Range"];
        size_t i_range_start = 0;
        size_t i_range_end = 0;
        size_t file_size = HTTPMultiFormBody::FileSize(file);

        int code;
        if (str_range.size() == 0)
        {
            code = 200;
            i_range_end = file_size - 1;
        }
        else
        {
            code = 206;
            i_range_start = atoll(FindField(str_range.data(), "bytes=", "-").data());
            i_range_end = atoll(FindField(str_range.data(), "-", nullptr).data());
            if (i_range_end == 0)
            {
                i_range_end = file_size - 1;
            }
            http_header.emplace("Content_Range", StrPrinter() << "bytes" << i_range_start << "-" << i_range_end << "/" << file_size << std::endl);
        }
        HTTPBody::Ptr file_body = std::make_shared<HTTPFileBody>(file, i_range_start, i_range_end - i_range_start + 1);
        (*this)(code, response_header, file_body);
    }

    static std::string GetFilePath(Parser &parser, std::string static_path)
    {
        auto url = parser.Url();
        if (url == "/")
        {
            url = "/index.html";
        }
        return static_path + url;
    }

    static void SendNotFound(const HTTPFileManager::Invoker &cb)
    {
        cb(404, "text/html", StrCaseMap(), std::make_shared<HTTPStringBody>("Not Found"));
    }

    void HTTPFileManager::OnAccessPath(TCPSession &sender, Parser &parser, const Invoker &cb)
    {
        auto str_file = GetFilePath(parser, static_path_);
        HTTPSession::HttpResponseInvoker invoker = [&](int code, const StrCaseMap &header_out, const HTTPBody::Ptr &body)
        {
            cb(code, HTTPFileManager::GetContentType(str_file.data()), header_out, body);
        };

        invoker.ResponseFile(parser.GetHeader(), StrCaseMap(), str_file);
    }

    const std::string &HTTPFileManager::GetContentType(const char *name)
    {
        return getHttpContentType(name);
    }

    bool HTTPFileManager::IsFile(std::string str_path)
    {
        return access(str_path.data(), F_OK) != -1;
    }
    std::string HTTPFileManager::static_path_ = "/home/iiya/webProject/start/static";
} // namespace cyber

#endif // CYBER_HTTP_FILE_MANAGER_CPP
