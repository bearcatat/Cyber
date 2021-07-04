#if !defined(CYBER_HTTP_REQUEST_SPLITTER_CPP)
#define CYBER_HTTP_REQUEST_SPLITTER_CPP
#include "http_request_splitter.h"
#include "../Util/logger.h"
#include "../Util/util.h"

namespace cyber
{
    void HTTPRequestSplitter::Input(const char *data, size_t len)
    {
        const char *ptr = data;
        if (!remain_data_.Empty())
        {
            remain_data_.Append(data, len);
            data = ptr = remain_data_.Data();
            len = remain_data_.Size();
        }
    SPLITPACKET:

        char &tail_ref = ((char *)ptr)[len];
        char tail_tmp = tail_ref;
        tail_ref = 0;

        const char *index = nullptr;
        remain_data_size_ = len;
        TraceL << "Input";
        while (content_len_ == 0 && remain_data_size_ > 0 && (index = OnSearchPacketTail(ptr, remain_data_size_)) != nullptr)
        {
            if (index == ptr)
            {
                break;
            }
            if (index < ptr || index > ptr + remain_data_size_)
            {
                throw std::out_of_range("splitter error");
            }
            const char *header_ptr = ptr;
            ssize_t header_size = index - ptr;
            ptr = index;
            remain_data_size_ = len - (ptr - data);
            DebugL << header_size;
            content_len_ = OnRecvHeader(header_ptr, header_size);
        }
        if (remain_data_size_ <= 0)
        {
            remain_data_.Clear();
            return;
        }

        tail_ref = tail_tmp;

        if (content_len_ == 0)
        {
            remain_data_.Assign(ptr, remain_data_size_);
            return;
        }

        if (content_len_ > 0)
        {
            if (remain_data_size_ < (size_t)content_len_)
            {
                remain_data_.Assign(ptr, remain_data_size_);
                return;
            }
            OnRecvContent(ptr, content_len_);

            remain_data_size_ -= content_len_;
            ptr += content_len_;
            content_len_ = 0;

            if (remain_data_size_ > 0)
            {
                remain_data_.Assign(ptr, remain_data_size_);
                data = ptr = (char *)remain_data_.Data();
                len = remain_data_.Size();
                goto SPLITPACKET;
            }
            remain_data_.Clear();
            return;
        }
        OnRecvContent(ptr, remain_data_size_);
        remain_data_.Clear();
    }

    void HTTPRequestSplitter::SetContentLen(ssize_t content_len)
    {
        content_len_ = content_len;
    }

    void HTTPRequestSplitter::Reset()
    {
        content_len_ = 0;
        remain_data_size_ = 0;
        remain_data_.Clear();
    }

    const char *HTTPRequestSplitter::OnSearchPacketTail(const char *data, size_t len)
    {
        auto pos = strstr(data, "\r\n\r\n");

        return pos == nullptr ? nullptr : pos + 4;
    }
    size_t HTTPRequestSplitter::RemainDataSize()
    {
        return remain_data_size_;
    }
} // namespace cyber

#endif // CYBER_HTTP_REQUEST_SPLITTER_CPP
