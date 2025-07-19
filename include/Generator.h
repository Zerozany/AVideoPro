_Pragma("once");
#include <coroutine>
#include <exception>

template <typename T>
struct Generator
{
    struct promise_type
    {
        auto get_return_object() -> Generator;

        auto initial_suspend() noexcept -> std::suspend_always;

        auto final_suspend() noexcept -> std::suspend_always;

        auto yield_value(T* _value) noexcept -> std::suspend_always;

        auto unhandled_exception() -> void;

        auto return_void() -> void;

        T* value{nullptr};
    };

public:
    explicit(true) Generator(std::coroutine_handle<promise_type> _handle);
    ~Generator() noexcept;

    auto nextValue() noexcept -> bool;

    auto current() const noexcept -> T*;

private:
    std::coroutine_handle<promise_type> m_handle;
};

/// \brief achieve

template <typename T>
inline Generator<T>::Generator(std::coroutine_handle<promise_type> _handle) : m_handle(_handle)
{
}

template <typename T>
inline Generator<T>::~Generator() noexcept
{
    if (m_handle)
    {
        m_handle.destroy();
    }
}

template <typename T>
inline auto Generator<T>::nextValue() noexcept -> bool
{
    if (!m_handle || m_handle.done())
    {
        return false;
    }
    m_handle.resume();
    return !m_handle.done();
}

template <typename T>
inline auto Generator<T>::current() const noexcept -> T*
{
    return m_handle.promise().value;
}

template <typename T>
inline auto Generator<T>::promise_type::get_return_object() -> Generator
{
    return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
}

template <typename T>
inline auto Generator<T>::promise_type::initial_suspend() noexcept -> std::suspend_always
{
    return {};
}

template <typename T>
inline auto Generator<T>::promise_type::final_suspend() noexcept -> std::suspend_always
{
    return {};
}

template <typename T>
inline auto Generator<T>::promise_type::yield_value(T* _value) noexcept -> std::suspend_always
{
    value = _value;
    return {};
}

template <typename T>
inline auto Generator<T>::promise_type::unhandled_exception() -> void
{
    std::terminate();
}

template <typename T>
inline auto Generator<T>::promise_type::return_void() -> void
{
}
