#include <iostream>
#include "../Cyber/logger.h"

class TestLog
{
public:
    template <typename T>
    TestLog(const T &t)
    {
        _ss << t;
    };
    ~TestLog(){};

    //通过此友元方法，可以打印自定义数据类型
    friend std::ostream &operator<<(std::ostream &out, const TestLog &obj)
    {
        return out << obj._ss.str();
    }

private:
    std::stringstream _ss;
};

int main(int argc, char const *argv[])
{
    cyber::Logger::Instance().Add(std::make_shared<cyber::ConsoleChannel>());
    cyber::Logger::Instance().SetWriter(std::make_shared<cyber::AsyncLogWriter>());

    TraceL << "object int: " << TestLog((int)1) << std::endl;
    DebugL << "object short: " << TestLog((short)2) << std::endl;
    InfoL << "object float: " << TestLog((float)3.1415) << std::endl;
    WarnL << "object double: " << TestLog((double)1.1546512) << std::endl;
    ErrorL << "object void *: " << TestLog((void *)0x12345678);
    ErrorL << "object string: " << TestLog("test string") << std::endl;

    TraceL << "int" << (int)1 << std::endl;
    DebugL << "short:" << (short)2 << std::endl;
    InfoL << "float:" << (float)3.12345678 << std::endl;
    WarnL << "double:" << (double)4.12345678901234567 << std::endl;
    ErrorL << "void *:" << (void *)0x12345678 << std::endl;
    ErrorL << "without endl!";
    return 0;
}
