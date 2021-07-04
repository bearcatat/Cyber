#if !defined(CYBER_HTTP_BODY_CPP)
#define CYBER_HTTP_BODY_CPP
#include "http_body.h"
#include <algorithm>
#include "../Util/logger.h"
#include "../Util/uv_errno.h"
namespace cyber
{
    size_t HTTPMultiFormBody::FileSize(std::shared_ptr<std::ifstream> &file)
    {
        file->seekg(0, std::ios::end);
        size_t file_size = file->tellg();
        file->seekg(0, std::ios::beg);
        return file_size;
    }

    HTTPStringBody::HTTPStringBody(const std::string &str)
    {
        str_ = str;
    }
    ssize_t HTTPStringBody::RemainSize()
    {
        return str_.size() - offset_;
    }

    Buffer::Ptr HTTPStringBody::ReadData(size_t size)
    {
        size = std::min((size_t)RemainSize(), str_.size());
        if (!size)
        {
            return nullptr;
        }
        BufferRaw::Ptr ret = BufferRaw::Create();
        ret->Assign(str_.substr(offset_, size).data(), size);
        offset_ += size;
        return ret;
    }

    HTTPFileBody::HTTPFileBody(const std::shared_ptr<std::ifstream> &file, size_t offset, size_t max_size)
    {
        Init(file, offset, max_size);
    }
    ssize_t HTTPFileBody::RemainSize()
    {
        return max_size_ - offset_;
    }

    Buffer::Ptr HTTPFileBody::ReadData(size_t size)
    {
        size = std::min((size_t)RemainSize(), size);
        if (!size)
        {
            return nullptr;
        }
        BufferRaw::Ptr ret = BufferRaw::Create();
        ret->SetCapacity(size + 1);
        file_->read(ret->Data(), size);
        ssize_t i_read = file_->gcount();
        if (i_read > 0)
        {
            ret->SetSize(i_read);
            offset_ += i_read;
            return std::move(ret);
        }
        offset_ = max_size_;
        WarnL << "read file err:" << get_uv_errmsg();
        return nullptr;
    }

    void HTTPFileBody::Init(const std::shared_ptr<std::ifstream> &file, size_t offset, size_t max_size)
    {
        file_ = file;
        max_size_ = max_size;
        offset_ = 0;
        file->seekg(offset, std::ios::beg);
    }

} // namespace cyber

#endif // CYBER_HTTP_BODY_CPP
