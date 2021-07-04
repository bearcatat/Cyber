#if !defined(CYBER_HTTP_BODY_H)
#define CYBER_HTTP_BODY_H
#include <memory>
#include "../Socket/buffer.h"
#include <fstream>

namespace cyber
{
    class HTTPBody : public std::enable_shared_from_this<HTTPBody>
    {
    public:
        typedef std::shared_ptr<HTTPBody> Ptr;
        HTTPBody(/* args */){};
        virtual ~HTTPBody(){};

        virtual ssize_t RemainSize() { return 0; }

        virtual Buffer::Ptr ReadData(size_t size) { return nullptr; };

        virtual void ReadDataAsync(size_t size, const std::function<void(const Buffer::Ptr &buf)> &cb)
        {
            cb(ReadData(size));
        }
    };

    class HTTPStringBody : public HTTPBody
    {

    public:
        typedef std::shared_ptr<HTTPStringBody> Ptr;
        HTTPStringBody(const std::string &str);
        virtual ~HTTPStringBody() {}
        ssize_t RemainSize() override;
        Buffer::Ptr ReadData(size_t size) override;

    private:
        /* data */
        size_t offset_ = 0;
        mutable std::string str_;
    };

    class HTTPFileBody : public HTTPBody
    {
    public:
        typedef std::shared_ptr<HTTPFileBody> Ptr;
        HTTPFileBody(const std::shared_ptr<std::ifstream> &file, size_t offset, size_t max_size);
        ~HTTPFileBody() override {}
        ssize_t RemainSize() override;
        Buffer::Ptr ReadData(size_t size) override;

    private:
        void Init(const std::shared_ptr<std::ifstream> &file, size_t offset, size_t max_size);

    private:
        size_t max_size_;
        size_t offset_;
        std::shared_ptr<std::ifstream> file_;
        
    };

    class HTTPMultiFormBody : public HTTPBody
    {
    public:
        static size_t FileSize(std::shared_ptr<std::ifstream> &file);
    };

} // namespace cyber

#endif // CYBER_HTTP_BODY_H
