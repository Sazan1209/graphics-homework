#ifndef TUPLE_TOOLS_HPP
#define TUPLE_TOOLS_HPP

#include <tuple>

template <typename T, typename... Ts>
concept is_braces_constructible = requires { T{std::declval<Ts>()...}; };

template <class T, std::size_t N>
concept has_tuple_element = requires(T t) {
    typename std::tuple_element_t<N, std::remove_const_t<T>>;
    {
        get<N>(t)
        } -> std::convertible_to<const std::tuple_element_t<N, T>&>;
};

template <class T>
concept tuple_like = !std::is_reference_v<T> && requires(T t) {
    typename std::tuple_size<T>::type;
    requires std::
        derived_from<std::tuple_size<T>, std::integral_constant<std::size_t, std::tuple_size_v<T>>>;
} && []<std::size_t... N>(std::index_sequence<N...>) {
    return (has_tuple_element<T, N> && ...);
}(std::make_index_sequence<std::tuple_size_v<T>>());

struct AnyType
{
    template <class T>
    constexpr operator T(); // NOLINT
};

template <class T>
auto to_tuple(T&& object) noexcept
{
    using Type = std::decay_t<T>;
    if constexpr (
        is_braces_constructible<Type, AnyType, AnyType, AnyType, AnyType, AnyType, AnyType>)
    {
        auto&& [p1, p2, p3, p4, p5, p6] = object;
        return std::make_tuple(p1, p2, p3, p4, p5, p6);
    }
    else if constexpr (
        is_braces_constructible<Type, AnyType, AnyType, AnyType, AnyType, AnyType>)
    {
        auto&& [p1, p2, p3, p4, p5] = object;
        return std::make_tuple(p1, p2, p3, p4, p5);
    }
    else if constexpr (is_braces_constructible<Type, AnyType, AnyType, AnyType, AnyType>)
    {
        auto&& [p1, p2, p3, p4] = object;
        return std::make_tuple(p1, p2, p3, p4);
    }
    else if constexpr (is_braces_constructible<Type, AnyType, AnyType, AnyType>)
    {
        auto&& [p1, p2, p3] = object;
        return std::make_tuple(p1, p2, p3);
    }
    else if constexpr (is_braces_constructible<Type, AnyType, AnyType>)
    {
        auto&& [p1, p2] = object;
        return std::make_tuple(p1, p2);
    }
    else if constexpr (is_braces_constructible<Type, AnyType>)
    {
        auto&& [p1] = object;
        return std::make_tuple(p1);
    }
    else
    {
        return std::make_tuple();
    }
}

#endif // TUPLE_TOOLS_HPP
