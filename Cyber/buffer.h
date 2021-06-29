#if !defined(CYBER_BUFFER_H)
#define CYBER_BUFFER_H

#include <memory>
#include <string>
#include <functional>
#include <arpa/inet.h>
#include <vector>
#include <list>
#include <string.h>

#include "util.h"

namespace cyber
{
    class Buffer : public noncopyable
    {
    public:
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

    class BufferRaw : public Buffer
    {
    public:
        using Ptr = std::shared_ptr<BufferRaw>;
        static Ptr Create();
        ~BufferRaw() override;
        char *Data() const override
        {
            return data_;
        }
        size_t Size() const override
        {
            return size_;
        }
        void SetCapacity(size_t capacity)
        {
            if (_data)
            {
                do
                {
                    if (capacity > capacity_)
                    {
                        break;
                    }
                    else if (capacity < 2 * 1024 || 2 * capacity < capacity_)
                    {
                        return;
                    }
                } while (false);
                delete[] data_;
            }
            data_ = new char[capacity];
            capacity_ = capacity;
        }
        void SetSize(size_t size)
        {
            if (size > capacity_)
            {
                throw std::invalid_argument("BufferRaw::SetSize out of range");
            }
            size_ = size;
        }
        void assign(const char *data, size_t size = 0)
        {
            if (size <= 0)
            {
                size = strlen(data);
            }
            SetCapacity(size + 1);
            memcpy(data_, data, size);
            data_[size] = 0;
            SetSize(size);
        }
        BufferRaw(size_t capacity = 0)
        {
            if (capacity)
            {
                SetCapacity(capacity);
            }
        }
        BufferRaw(const char *data, size_t size = 0)
        {
            SetSize(data, size);
        }

    private:
        size_t size_ = 0;
        size_t capacity_ = 0;
        char *data_ = nullptr;
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
        typedef std::shared_ptr<BufferList> Ptr;
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
