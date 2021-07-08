#include <system_error>
#include <variant>

template <typename T>
struct SuccessWrapper {
    T value;
};

template <typename T = std::monostate>
SuccessWrapper<T> success()
{
    return SuccessWrapper<T> { T {} };
}

template <typename T>
SuccessWrapper<T> success(T&& t)
{
    return SuccessWrapper<T> { std::forward<T>(t) };
}

template <typename E>
struct ErrorWrapper {
    E value;
};

template <typename E>
ErrorWrapper<E> error(E&& e)
{
    return ErrorWrapper<E> { std::forward<E>(e) };
}

inline ErrorWrapper<std::error_code> errnoError()
{
    return error(std::make_error_code(static_cast<std::errc>(errno)));
}

template <typename T, typename E = std::error_code>
class Result {
public:
    Result(const T& t)
        : value_(t)
    {
    }

    template <typename U>
    Result(SuccessWrapper<U>&& s)
        : value_(T { s.value })
    {
    }

    template <typename U>
    Result(ErrorWrapper<U>&& e)
        : value_(E { e.value })
    {
    }

    bool hasValue() const
    {
        return value_.index() == 0;
    }

    explicit operator bool() const
    {
        return hasValue();
    }

    const T& value() const
    {
        return std::get<0>(value_);
    }

    const E& error() const
    {
        return std::get<1>(value_);
    }

private:
    std::variant<T, E> value_;
};
