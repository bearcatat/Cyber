#if !defined(CYBER_BUFFER_H)
#define CYBER_BUFFER_H

#include <memory>
#include <string>
#include <functional>
#include <arpa/inet.h>
#include <vector>
#include <list>

#include "util.h"

namespace cyberweb
{
    class Buffer : public noncopyable
    {
        typedef std::shared_ptr<Buffer> Ptr;
        Buffer(){};
        virtual ~Buffer(){};

        virtual char *Data() const = 0;
        virtual size_t Size() const = 0;

        virtual std::string ToString() const
        {
            return std::string(data(), Size());
        }

        virtual size_t GetCapacity() const
        {
            return Size();
        }
    };

    class BufferList;
    class BufferSock : public Buffer
    {
    public:
        using Ptr = std::shared_ptr<BufferSock>;
        typedef std::function<void(size_t size)> OnResult;
        friend class BufferList;

        BufferSock(Buffer::Ptr buffer, sockaddr *addr = nullptr, int addr_len = 0, OnResult cb = nullptr);
        ~BufferSock();

        char *Data() const override;
        size_t Size() const override;

        void SetSendResult(OnResult cb);
        void OnSendSuccess();

    private:
        int addr_len_ = 0;
        sockaddr *addr_ = nullptr;
        Buffer::Ptr buffer_;
        OnResult result_;
    };

    class BufferList : public noncopyable
    {
    public:
        typedef std::shared_ptr<BufferSock>;
        BufferList(List<BufferSock::Ptr> &list);
        ~BufferList() {}

        bool Empty();
        size_t Count();
        ssize_t Send(int fd, int flags);

    private:
        void reOffset(size_t n);
        ssize_t SendL(int fd, int flags);

    private:
        size_t iovec_off_ = 0;
        size_t remain_size_ = 0;
        std::vector<iovec> iovec_;
        List<BufferSock::Ptr> pkt_list_;
    };
} // namespace cyberweb

#endif // CYBER_BUFFER_H
