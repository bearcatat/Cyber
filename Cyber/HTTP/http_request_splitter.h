#if !defined(CYBER_HTTP_REQUEST_SPLITTER_H)
#define CYBER_HTTP_REQUEST_SPLITTER_H
#include <string>
#include "../Socket/buffer.h"

namespace cyber
{
    class HTTPRequestSplitter
    {
    public:
        HTTPRequestSplitter() {}
        virtual ~HTTPRequestSplitter() {}

        virtual void Input(const char *data, size_t len);

    protected:
        virtual ssize_t OnRecvHeader(const char *data, size_t len) = 0;

        virtual void OnRecvContent(const char *data, size_t len) = 0;

        virtual const char *OnSearchPacketTail(const char *data, size_t len);

        void SetContentLen(ssize_t content_len);

        void Reset();

        size_t RemainDataSize();

    private:
        size_t content_len_ = 0;
        size_t remain_data_size_ = 0;
        cyber::BufferLikeString remain_data_;
    };

} // namespace cyber

#endif // CYBER_HTTP_REQUEST_SPLITTER_H
