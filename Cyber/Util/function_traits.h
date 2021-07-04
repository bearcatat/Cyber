#if !defined(CYBER_FUNCTION_TRAITS_H)
#define CYBER_FUNCTION_TRAITS_H

#include <tuple>
#include <functional>

namespace cyber
{
    template <typename T>
    struct FunctionTraits;

    template <typename Ret, typename... Args>
    struct FunctionTraits<Ret(Args...)>
    {
    public:
        enum
        {
            arity = sizeof...(Args)
        };
        typedef Ret FunctionType(Args...);
        typedef Ret ReturnType;
        using stl_function_type = std::function<FunctionType>;

        typedef Ret (*pointer)(Args...);

        template <size_t I>
        struct args
        {
            static_assert(I < arity, "index is out of range, index must less than sizeof Args");
            using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
        };
    };

    template <typename Ret, typename... Args>
    struct FunctionTraits<Ret (*)(Args...)> : FunctionTraits<Ret(Args...)>
    {
    };

    template <typename Ret, typename... Args>
    struct FunctionTraits<std::function<Ret(Args...)>> : FunctionTraits<Ret(Args...)>
    {
    };

    //member function
#define FUNCTION_TRAITS(...)                                                                                    \
    template <typename ReturnType, typename ClassType, typename... Args>                                        \
    struct FunctionTraits<ReturnType (ClassType::*)(Args...) __VA_ARGS__> : FunctionTraits<ReturnType(Args...)> \
    {                                                                                                           \
    };

    FUNCTION_TRAITS()
    FUNCTION_TRAITS(const)
    FUNCTION_TRAITS(volatile)
    FUNCTION_TRAITS(const volatile)

    template <typename Callable>
    struct FunctionTraits : FunctionTraits<decltype(&Callable::operator())>
    {
    };

} // namespace cyber

#endif // CYBER_FUNCTION_TRAITS_H
