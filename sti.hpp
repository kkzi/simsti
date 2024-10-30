#pragma once

#include <chrono>
#include <optional>
#include <tuple>

constexpr static auto STI_HEAD = 1234567890;
constexpr static auto STI_TAIL = -STI_HEAD;
constexpr static auto STI_HEAD_BYTES = 64u;
constexpr static auto STI_TAIL_BYTES = 4u;

namespace detail
{
    using namespace std::chrono;

    template <size_t unit>
    static std::tuple<uint32_t, uint32_t> make_time_tag0(const system_clock::time_point &from)
    {
        auto days = duration_cast<std::chrono::days>(from.time_since_epoch());
        auto today = year_month_day(sys_days(days));
        auto span = from - sys_days{ year_month_day(today.year(), month{ 1 }, day{ 1 }) };
        auto us = duration_cast<std::chrono::microseconds>(span).count();
        return { us / 1e6, (us % 1000'000) / unit };
    }

}  // namespace detail

inline std::tuple<uint32_t, uint32_t> make_time_tag(uint8_t tag, std::optional<std::chrono::system_clock::time_point> from = std::nullopt)
{
    if (!from) from = std::chrono::system_clock::now();

    switch (tag)
    {
    case 3:  // s + us
        return detail::make_time_tag0<1>(*from);
    case 0:  // s + ms
    default:
        return detail::make_time_tag0<1000>(*from);
    }
}
