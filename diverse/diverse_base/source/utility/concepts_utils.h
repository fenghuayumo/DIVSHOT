#pragma once
#include <tuple>
namespace diverse
{
    template<typename> struct is_tuple : std::false_type {};

    template<typename ...T> struct is_tuple<std::tuple<T...>> : std::true_type {};

    template <std::size_t... Is>
    constexpr void loop(std::index_sequence<Is...>, auto&& f) {
        (f.template operator() < Is > (), ...);
    }
}