#if !defined(CYBER_BUFFER_CPP)
#define CYBER_BUFFER_CPP

#include <arpa/inet.h>
#include <cassert>
#include <sys/uio.h>

#include "../Util/util.h"
#include "buffer.h"
#include "../Util/logger.h"

namespace cyber
{
    BufferRaw::Ptr BufferRaw::Create()
    {
        return Ptr(new BufferRaw);
    }

    BufferSock::BufferSock(Buffer::Ptr buffer, sockaddr *addr, int addr_len, OnResult cb)
    {
        if (addr && addr_len)
        {
            addr_ = (sockaddr *)malloc(addr_len);
            addr_len_ = addr_len;
        }
        assert(buffer);
        buffer_ = std::move(buffer);
        result_ = std::move(cb);
    }

    BufferSock::~BufferSock()
    {
        if (addr_)
        {
            free(addr_);
        }
        if (result_)
        {
            result_(0);
            result_ = nullptr;
        }
    }

    char *BufferSock::Data() const
    {
        return buffer_->Data();
    }

    size_t BufferSock::Size() const
    {
        return buffer_->Size();
    }

    void BufferSock::SetSendResult(OnResult cb)
    {
        result_ = std::move(cb);
    }

    void BufferSock::OnSendSuccess()
    {
        if (result_)
        {
            result_(Size());
            result_ = nullptr;
        }
    }

    BufferList::BufferList(List<BufferSock::Ptr> &list) : iovec_(list.size())
    {
        pkt_list_.swap(list);
        auto it = iovec_.begin();
        pkt_list_.ForEach(
            [&](BufferSock::Ptr &buffer)
            {
                it->iov_base = buffer->Data();
                it->iov_len = (decltype(it->iov_len))buffer->Size();
                remain_size_ += it->iov_len;
                ++it;
            });
    }

    bool BufferList::Empty() const
    {
        return iovec_off_ == iovec_.size();
    }

    size_t BufferList::Count() const
    {
        return iovec_.size() - iovec_off_;
    }

    ssize_t BufferList::Send(int fd, int flags)
    {
        auto remain_size = remain_size_;
        while (remain_size_ && SendL(fd, flags) != -1)
            ;
        ssize_t sent = remain_size - remain_size_;
        if (sent > 0)
        {
            return sent;
        }
        return -1;
    }

    void BufferList::reOffset(size_t n)
    {
        remain_size_ -= n;
        size_t offset = 0;
        size_t last_off = iovec_off_;
        for (int i = iovec_off_; i < iovec_.size(); i++)
        {
            auto &ref = iovec_[i];
            offset += ref.iov_len;
            if (offset < n)
            {
                continue;
            }
            size_t remain = offset - n;
            ref.iov_base = (char *)ref.iov_base + ref.iov_len - remain;
            iovec_off_ = i;
            if (remain == 0)
            {
                iovec_off_++;
            }
            break;
        }
        for (int i = last_off; i < iovec_off_; i++)
        {
            auto &front = pkt_list_.front();
            front->OnSendSuccess();
            pkt_list_.pop_front();
        }
    }

    ssize_t BufferList::SendL(int fd, int flags)
    {
        ssize_t n;
        do
        {
            msghdr msg;
            // TCP setting
            msg.msg_name = NULL;
            msg.msg_namelen = 0;

            msg.msg_iov = &(iovec_[iovec_off_]);
            msg.msg_iovlen = (decltype(msg.msg_iovlen))(iovec_.size() - iovec_off_);

            size_t max = __IOV_MAX;

            if (msg.msg_iovlen > max)
            {
                msg.msg_iovlen = max;
            }

            msg.msg_control = NULL;
            msg.msg_controllen = 0;

            n = sendmsg(fd, &msg, flags);

        } while (n == -1);

        if (n >= (ssize_t)remain_size_)
        {
            iovec_off_ = iovec_.size();
            remain_size_ = 0;
            pkt_list_.ForEach(
                [](BufferSock::Ptr &buffer)
                { buffer->OnSendSuccess(); });
            return n;
        }

        if (n > 0)
        {
            reOffset(n);
            return n;
        }
        return n;
    }

} // namespace cyberweb

#endif // CYBER_BUFFER_CPP
