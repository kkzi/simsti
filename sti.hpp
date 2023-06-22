#pragma once

#include <chrono>
#include <optional>
#include <tuple>

constexpr static int STI_HEAD = 1234567890;
constexpr static int STI_TAIL = -STI_HEAD;

namespace detail
{
    using namespace std::chrono;

    template <uint32_t unit>
    static std::tuple<uint32_t, uint32_t> make_time_tag0(const system_clock::time_point &from)
    {
        auto days = duration_cast<std::chrono::days>(from.time_since_epoch());
        auto today = year_month_day(sys_days(days));
        auto span = from - sys_days{ year_month_day(today.year(), month{ 1 }, day{ 1 }) };
        auto ms = duration_cast<milliseconds>(span).count();
        return { ms / unit, ms % unit };
    }

}  // namespace detail

std::tuple<uint32_t, uint32_t> make_time_tag(uint8_t tag, std::optional<std::chrono::system_clock::time_point> from = std::nullopt)
{
    if (!from) from = std::chrono::system_clock::now();

    switch (tag)
    {
    case 3:
        return detail::make_time_tag0<1000'000>(*from);
    case 0:
    default:
        return detail::make_time_tag0<1000>(*from);
    }
}
