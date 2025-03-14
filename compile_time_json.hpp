#pragma once

#if __cplusplus < 202302L
    #error out of date c++ version, compile with -stdc++=2c
#elif defined(__clang__) && __clang_major__ < 19
    #error out of date clang, compile with latest version
#elif !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 14
    #error out of date g++, compile with latest version
#elif defined(_MSC_VER)
    #error msvc does not yet support the latest c++ features
#else

#include <algorithm>
#include <charconv>
#include <expected> //not needed after changing to c++26's compie time exceptions
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

// [[c++26]]: change all s_assert's to c++26's compile-time exceptions, then no need for std::expected as well
// [[c++26]]: change to c++26's structured bindings

namespace ctf {
    inline namespace compile_time_format {
        namespace detail {
            using namespace std::literals;

            template <typename tp_type_t>
            struct is_subrange : std::false_type {};
            template <std::input_iterator tp_input_iterator1_t, std::input_iterator tp_input_iterator2_t, std::ranges::subrange_kind tp_subrange_kind>
            struct is_subrange<std::ranges::subrange<tp_input_iterator1_t, tp_input_iterator2_t, tp_subrange_kind>> : std::true_type {};
            template <typename tp_type_t>
            concept subrange = is_subrange<std::remove_cvref_t<tp_type_t>>::value;
            template <typename tp_type_t>
            concept distanced_range = requires (tp_type_t&& p_range) { std::ranges::distance(std::forward<tp_type_t>(p_range)); };
            
            template <subrange tp_subrange_t, std::predicate<std::ranges::range_value_t<tp_subrange_t>> tp_predicate_t>
            auto constexpr operator>>(tp_subrange_t&& p_subrange, tp_predicate_t&& p_predicate) -> auto&& {
                return p_subrange.advance(std::ranges::distance(std::ranges::begin(p_subrange), std::ranges::find_if_not(p_subrange, p_predicate)));
            }
            template <subrange tp_subrange_t>
            auto constexpr operator>>(tp_subrange_t&& p_subrange, std::ranges::range_difference_t<tp_subrange_t> p_count) -> auto&& {
                return p_subrange.advance(p_count);
            }
            template <subrange tp_subrange_t, distanced_range tp_distanced_range_t>
            auto constexpr operator>>(tp_subrange_t&& p_subrange, tp_distanced_range_t&& p_range) -> auto&& {
                return p_subrange.advance(std::ranges::distance(p_range));
            }
            
            template <std::array tp_array, char tp_old_char, char tp_new_char>
            auto constexpr array_replace_char = [] {
                auto l_result = tp_array;
                std::ranges::replace(l_result, tp_old_char, tp_new_char);
                return l_result;
            }();

            auto constexpr any_of        = []<typename tp_value_t, class... tp_values_ts>(tp_value_t&& p_value, tp_values_ts... p_values) { return (... || (p_value == p_values)); };
            auto constexpr is_whitespace = [](const char p_char) { return any_of(p_char, ' ', '\t', '\n', '\v', '\f', '\r'); };
            auto constexpr is_digit      = [](const char p_char) { return p_char >= '0' && p_char <= '9'; };
            auto constexpr count_while = []
                <std::ranges::input_range tp_input_range_t, std::predicate<std::ranges::range_value_t<tp_input_range_t>> tp_predicate_t>
                (tp_input_range_t&& p_range, tp_predicate_t&& p_predicate) { return std::ranges::distance(std::ranges::begin(p_range), std::ranges::find_if_not(p_range, p_predicate)); };

            template <std::size_t tp_size>
            struct string {
                std::array<char, tp_size> m_data;
                consteval string(const char (&p_data)[tp_size]) : m_data{std::to_array(p_data)} {}
                consteval string(const std::array<char, tp_size>& p_data) : m_data{p_data} {}
            };
            template <std::size_t tp_size>
            string(const char (&m_data)[tp_size]) -> string<tp_size>;
            template <std::size_t tp_size>
            string(const std::array<char, tp_size>&) -> string<tp_size>;

            template <const std::string_view& tp_string_view>
            auto constexpr string_view_to_array = [] {
                auto l_array = std::array<char, std::ranges::size(tp_string_view)>{};
                std::ranges::copy(tp_string_view, std::ranges::begin(l_array));
                return l_array;
            }();

            template <std::array tp_data, bool tp_is_array, auto tp_string_or_index, template <std::array, bool> class tp_json_entity_tp>
            auto constexpr read_json_impl = [] {
                enum class entity : std::uint8_t { value, object, array };
                auto constexpr static s_result = [] {
                    struct return_type { entity m_entity; std::string_view m_value_or_nested; char m_value_type; };
                    using expected_return_type_t = std::expected<return_type, std::string_view>;
                    auto constexpr static s_assert = []<auto tp_char> (auto&& p_subrange) {
                        if (std::ranges::empty(p_subrange))
                            return expected_return_type_t{std::unexpect, std::ranges::data(array_replace_char<std::to_array("unexpected end of data, expected '_'"), '_', tp_char>)};
                        if (p_subrange.front() != tp_char)
                            return expected_return_type_t{std::unexpect, std::ranges::data(array_replace_char<std::to_array("expected '_'"), '_', tp_char>)};
                        return expected_return_type_t{};
                    };
                    auto constexpr static s_opening_delimiter = tp_is_array ? '[' : '{';
                    auto constexpr static s_closing_delimiter = tp_is_array ? ']' : '}';
                    auto l_subrange       = std::ranges::subrange{tp_data}; //null termination is kept unless string_view'ing here, but it doesn't matter
                    auto l_optional_index = std::conditional_t<tp_is_array, std::size_t, decltype(std::ignore)>{};
                    auto l_key            = std::conditional_t<!tp_is_array, std::string_view, decltype(std::ignore)>{};
                    auto l_found = false;
                    l_subrange >> is_whitespace;
                    if (std::ranges::empty(l_subrange))
                        return expected_return_type_t{std::unexpect, "data was empty"sv};
                    if (auto r = s_assert.template operator()<s_opening_delimiter>(l_subrange); !r) return r;
                    l_subrange >> 1 >> is_whitespace;
                    for (; l_subrange;) {
                        if constexpr (!tp_is_array) {
                            if (auto r = s_assert.template operator()<'\"'>(l_subrange); !r) return r;
                            l_subrange >> 1;
                            l_key = std::string_view{std::ranges::begin(l_subrange), std::ranges::next(std::ranges::begin(l_subrange), std::ranges::distance(std::views::take_while(l_subrange, [](auto a) { return a != '\"'; })))};
                            if (std::ranges::empty(l_key))
                                return expected_return_type_t{std::unexpect, "key was empty"sv}; 
                            l_subrange >> l_key;
                            if (auto r = s_assert.template operator()<'\"'>(l_subrange); !r) return r;
                            l_subrange >> 1 >> is_whitespace;
                            if (auto r = s_assert.template operator()<':'>(l_subrange); !r) return r;
                            l_subrange >> 1 >> is_whitespace;
                        }
                        if constexpr (tp_is_array) {
                            if (l_optional_index == tp_string_or_index)
                                l_found = true;
                        }
                        else if (std::ranges::equal(l_key, std::string_view{std::ranges::data(tp_string_or_index)}))
                            l_found = true;
                        if (any_of(l_subrange.front(), '{', '[')) {
                            if (l_found)
                                return expected_return_type_t{std::in_place, l_subrange.front() == '{' ? entity::object : entity::array, std::string_view{l_subrange}, '\0'};
                            else {
                                l_subrange >> [c_depth = std::size_t{0}, c_opening_delimiter = l_subrange.front(), c_closing_delimiter = l_subrange.front() == '{' ? '}' : ']'](auto a) mutable {
                                    return (a == c_opening_delimiter ? ++c_depth : a == c_closing_delimiter && c_depth != 0 ? --c_depth : c_depth) != 0 || any_of(a, c_opening_delimiter, c_closing_delimiter);
                                };
                            }
                        }
                        else {
                            auto l_is_value_string = l_subrange.front() == '\"';
                            if (l_is_value_string)
                                l_subrange >> 1;
                            auto l_value = std::string_view{
                                std::ranges::begin(l_subrange),
                                std::ranges::next(
                                    std::ranges::begin(l_subrange),
                                    l_is_value_string ?
                                        std::ranges::distance(std::views::take_while(l_subrange, [](auto a) { return a != '\"'; })) :
                                        std::ranges::distance(std::views::take_while(l_subrange, [](auto a) { return !is_whitespace(a) && a != ','; }))
                                )
                            };
                            auto l_is_negative_integer = l_value.front() == '-';
                            auto l_type =
                                l_is_value_string ? 's' :
                                is_digit(l_value.front()) || l_is_negative_integer ? 'i' :
                                std::ranges::equal("true"sv, l_value) || std::ranges::equal("false"sv, l_value) ? 'b' :
                                std::ranges::equal("null"sv, l_value) ? 'n' :
                                'u';
                            if (l_type == 'u')
                                return expected_return_type_t{std::unexpect, "invalid value at key '?'"sv};
                            if (l_type == 'i') {
                                if (l_is_negative_integer && std::ranges::count(l_value, '-') > 1)
                                    return expected_return_type_t{std::unexpect, "negative integrals must contain only one negation operator"sv};
                                if (std::ranges::any_of(l_value, [](auto a) { return !is_digit(a) && a != '.' && a != '-'; }))
                                    return expected_return_type_t{std::unexpect, "numerical values must only contain digits 0 - 9"sv};
                                if (auto l_decimal_point_count = std::ranges::count(l_value, '.')) {
                                    l_type = 'f';
                                    if (l_decimal_point_count > 1)
                                        return expected_return_type_t{std::unexpect, "floating-point value must contain only one decimal point"sv};
                                }
                                if (std::ranges::size(l_value) > 20)
                                    return expected_return_type_t{std::unexpect, "numerical value must not exceed a 20 digits"sv};
                            }
                            if (l_found)
                                return expected_return_type_t{std::in_place, entity::value, std::string_view{l_value}, l_type};
                            l_subrange >> std::ranges::distance(l_value);
                            if (l_is_value_string) {
                                if (auto r = s_assert.template operator()<'\"'>(l_subrange); !r) return r;
                                l_subrange >> 1;
                            }
                        }
                        l_subrange >> is_whitespace;
                        if (l_subrange.front() == s_closing_delimiter)
                            return expected_return_type_t{std::unexpect, "'?' not found"sv};
                        if (auto r = s_assert.template operator()<','>(l_subrange); !r) return r;
                        l_subrange >> 1 >> is_whitespace;
                        if constexpr (tp_is_array)
                            ++l_optional_index;
                    }
                    std::unreachable(); // suppresses false clang error of not no return value
                }();
                if constexpr (!s_result.has_value())
                    static_assert(false, s_result.error());
                else {
                    //change to constexpr structured binding in c++26:
                    //auto [l_entity, l_value_or_nested, l_value_type] = s_result.value();
                    if constexpr (s_result.value().m_entity == entity::value) {
                        if constexpr (s_result.value().m_value_type == 's') {
                            auto constexpr static s_value = s_result.value().m_value_or_nested;
                            return string_view_to_array<s_value>;
                        }
                        else if constexpr (s_result.value().m_value_type == 'i') {
                            auto l_value = std::conditional_t<s_result.value().m_value_or_nested.front() == '-', std::intmax_t, std::uintmax_t>{};
                            std::from_chars(std::ranges::begin(s_result.value().m_value_or_nested), std::ranges::end(s_result.value().m_value_or_nested), l_value); 
                            //std::from_chars int overload is buggy
                            return l_value;
                        }
                        else if constexpr (s_result.value().m_value_type == 'f') {
                            static_assert(false, "c++ does not yet have a constexpr string to float facility");
                            return;
                            //auto l_value = float{}; //change to std::float
                            //std::from_chars(std::ranges::begin(s_result.value().m_value_or_nested), std::ranges::end(s_result.value().m_value_or_nested), l_value);
                            //return l_value;
                            //maybe switch out std::from_chars with compile-time std::format, whichever gets implemented first
                        }
                        else if constexpr (s_result.value().m_value_type == 'b')
                            return s_result.value().m_value_or_nested == "true"sv;
                        else if constexpr (s_result.value().m_value_type == 'n')
                            return nullptr;
                    }
                    else {
                        auto constexpr static s_nested = s_result.value().m_value_or_nested;
                        return tp_json_entity_tp<string_view_to_array<s_nested>, s_result.value().m_entity == entity::array>{};
                    }
                }
            }();
            
            template <std::array tp_data, template <std::array, bool> class tp_json_entity_tp>
            struct json_array {
                template <std::size_t tp_index>
                auto constexpr static has_idx = requires { read_json_impl<tp_data, true, tp_index, tp_json_entity_tp>; };
                template <std::size_t tp_index>
                requires (has_idx<tp_index>)
                auto constexpr static at_idx = read_json_impl<tp_data, true, tp_index, tp_json_entity_tp>;
            };
            template <std::array tp_data, template <std::array, bool> class tp_json_entity_tp>
            struct json_object {
                template <string tp_string>
                auto constexpr static has_key = requires { read_json_impl<tp_data, false, tp_string.m_data, tp_json_entity_tp>; };
                template <string tp_string>
                requires (has_key<tp_string>)
                auto constexpr static at_key = read_json_impl<tp_data, false, tp_string.m_data, tp_json_entity_tp>;
            };
            
            template <std::array tp_data, bool tp_is_array>
            struct json_entity : std::conditional_t<tp_is_array, json_array<tp_data, json_entity>, json_object<tp_data, json_entity>> {};
        }

        template <detail::string tp_data>
        auto constexpr read_json = detail::json_entity<tp_data.m_data, false>{};
    }
}
#endif
