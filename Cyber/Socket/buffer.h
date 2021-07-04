#if !defined(CYBER_BUFFER_H)
#define CYBER_BUFFER_H

#include <memory>
#include <string>
#include <functional>
#include <arpa/inet.h>
#include <vector>
#include <list>
#include <string.h>

#include "../Util/util.h"

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
            return std::string(Data(), Size());
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
        ~BufferRaw() override{};
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
            if (data_)
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
        void Assign(const char *data, size_t size = 0)
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

    protected:
        BufferRaw(size_t capacity = 0)
        {
            if (capacity)
            {
                SetCapacity(capacity);
            }
        }
        BufferRaw(const char *data, size_t size = 0)
        {
            Assign(data, size);
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

        bool Empty() const;
        size_t Count() const;
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
    class BufferLikeString : public Buffer
    {

    public:
        ~BufferLikeString() override{};
        BufferLikeString()
        {
            erase_head_ = 0;
            erase_tail_ = 0;
        }
        BufferLikeString(std::string str)
        {
            str_ = std::move(str);
            erase_head_ = 0;
            erase_tail_ = 0;
        }
        BufferLikeString &operator=(std::string str)
        {
            str_ = std::move(str);
            erase_head_ = 0;
            erase_tail_ = 0;
            return *this;
        }
        BufferLikeString(const char *str)
        {
            str_ = str;
            erase_head_ = 0;
            erase_tail_ = 0;
        }
        BufferLikeString &operator=(const char *str)
        {
            str_ = str;
            erase_head_ = 0;
            erase_tail_ = 0;
            return *this;
        }
        BufferLikeString(BufferLikeString &&that)
        {
            str_ = std::move(that.str_);
            erase_head_ = that.erase_head_;
            erase_tail_ = that.erase_tail_;
            that.erase_head_ = 0;
            that.erase_tail_ = 0;
        }
        BufferLikeString &operator=(BufferLikeString &&that)
        {
            str_ = std::move(that.str_);
            erase_head_ = that.erase_head_;
            erase_tail_ = that.erase_tail_;
            that.erase_head_ = 0;
            that.erase_tail_ = 0;
            return *this;
        }
        BufferLikeString(BufferLikeString &that)
        {
            str_ = that.str_;
            erase_head_ = that.erase_head_;
            erase_tail_ = that.erase_tail_;
        }
        BufferLikeString &operator=(BufferLikeString &that)
        {
            str_ = that.str_;
            erase_head_ = that.erase_head_;
            erase_tail_ = that.erase_tail_;
            return *this;
        }
        char *Data() const override
        {
            return (char *)str_.data() + erase_head_;
        }
        size_t Size() const override
        {
            return str_.size() - erase_head_ - erase_tail_;
        }
        BufferLikeString &Erase(size_t pos = 0, size_t n = std::string::npos)
        {
            if (pos == 0)
            {
                if (n != std::string::npos)
                {
                    if (n > Size())
                    {
                        throw std::out_of_range("BufferLikeString:erase out of range in head");
                    }
                    erase_head_ += n;
                    Data()[Size()] = '\0';
                    return *this;
                }
                erase_head_ = 0;
                erase_tail_ = str_.size();
                Data()[0] = '\0';
                return *this;
            }
            if (n == std::string::npos || pos + n >= Size())
            {
                if (pos >= Size())
                {
                    throw std::out_of_range("BufferLikeString:erase out of range in tail");
                }
                erase_tail_ += Size() - pos;
                Data()[Size()] = '\0';
                return *this;
            }

            if (pos + n > Size())
            {
                throw std::out_of_range("BufferLikeString:erase out of range in middle");
            }
            str_.erase(erase_head_ + pos, n);
            return *this;
        }

        BufferLikeString &Append(const BufferLikeString &str)
        {
            return Append(str.Data(), str.Size());
        }
        BufferLikeString &Append(const std::string &str)
        {
            return Append(str.data(), str.size());
        }

        BufferLikeString &Append(const char *data)
        {
            return Append(data, strlen(data));
        }
        BufferLikeString &Append(const char *data, size_t len)
        {
            if (len <= 0)
            {
                return *this;
            }
            if (erase_head_ > str_.capacity() / 2)
            {
                MoveData();
            }
            if (erase_tail_ == 0)
            {
                str_.append(data, len);
                return *this;
            }
            str_.insert(erase_head_ + Size(), data, len);
            return *this;
        }

        void PushBack(char c)
        {
            if (erase_tail_ == 0)
            {
                str_.push_back(c);
                return;
            }
            Data()[Size()] = c;
            --erase_tail_;
            Data()[Size()] = '\0';
        }

        BufferLikeString &Insert(size_t pos, const char *s, size_t len)
        {
            str_.insert(erase_head_ + pos, s, len);
            return *this;
        }

        BufferLikeString &Assign(const char *data, size_t len)
        {
            if (len <= 0)
            {
                return *this;
            }
            if (data >= str_.data() && data < str_.data() + str_.size())
            {
                erase_head_ = data - str_.data();
                if (data + len > str_.data() + str_.size())
                {
                    throw std::out_of_range("BufferLikeString::assign out of range");
                }
                erase_tail_ = str_.data() + str_.size() - (data + len);
                return *this;
            }
            str_.assign(data, len);
            erase_head_ = 0;
            erase_tail_ = 0;
            return *this;
        }

        void Clear()
        {
            erase_head_ = 0;
            erase_tail_ = 0;
            str_.clear();
        }

        char &operator[](size_t pos)
        {
            if (pos >= Size())
            {
                throw std::out_of_range("BufferLikeString::operator[] out_of_range");
            }
            return Data()[pos];
        }

        const char &operator[](size_t pos) const
        {
            if (pos >= Size())
            {
                throw std::out_of_range("BufferLikeString::operator[] out_of_range");
            }
            return Data()[pos];
        }

        size_t Capicity() const
        {
            return str_.capacity();
        }

        void Reserve(size_t size)
        {
            str_.reserve();
        }

        bool Empty() const
        {
            return Size() <= 0;
        }

        std::string substr(size_t pos, size_t n = std::string::npos) const
        {
            if (n == std::string::npos)
            {
                if (pos >= Size())
                {
                    throw std::out_of_range("BufferLikeString::substr out of range");
                }
            }
            if (pos + erase_head_ > Size())
            {
                throw std::out_of_range("BufferLikeString::substr out of range");
            }
            return str_.substr(pos + erase_head_, n);
        }

    private:
        void MoveData()
        {
            if (erase_head_)
            {
                str_.erase(0, erase_head_);
                erase_head_ = 0;
            }
        }

    private:
        /* data */
        size_t erase_head_;
        size_t erase_tail_;
        std::string str_;
    };
} // namespace cyberweb

#endif // CYBER_BUFFER_H
