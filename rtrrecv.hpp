#pragma once

#include "sti.hpp"
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/endian/conversion.hpp>
#include <csignal>
#include <simple/gsl-lite.hpp>
#include <simple/simple.hpp>
#include <simple/use_spdlog.hpp>
#include <vector>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::endian;

struct rtrrecv_options
{
    std::string addr{ "127.0.0.1" };
    u16 port{ 3070 };
    u16 channel{ 0 };
    bool keep_sti{ false };
    std::function<void(std::vector<u8>)> frame_func;
};


static std::vector<uint8_t> make_tm_request(int channel, int cmd)
{
    constexpr size_t UNIT = sizeof(int);
    std::vector<uint8_t> request(16 * UNIT, 0);
    store_big_s32(request.data() + 0 * UNIT, STI_HEAD);
    store_big_s32(request.data() + 1 * UNIT, (u32)request.size());
    store_big_s32(request.data() + 3 * UNIT, channel);
    store_big_s32(request.data() + 5 * UNIT, cmd);
    store_big_s32(request.data() + 15 * UNIT, STI_TAIL);
    return request;
}

static awaitable<std::optional<std::vector<uint8_t>>> read_sti_frame(tcp::socket &sock)
{
    std::vector<uint8_t> frame(4096, 0);
    auto ptr = frame.data();
    size_t offset = 0;

    // read 64 bytes sti head
    {
        auto n = co_await async_read(sock, buffer(ptr + offset, STI_HEAD_BYTES), use_awaitable);
        if (auto head = load_big_s32(ptr); head != STI_HEAD)
        {
            log_warn("sync word failed: {:#X}", head);
            co_return std::nullopt;
        }
        offset += n;
    }

    auto payload_len = load_big_s32(ptr + 10 * sizeof(int));
    if (payload_len > 4000)
    {
        log_error("payload length out of range, {:#X}", payload_len);
        co_return std::nullopt;
    }

    // read payload
    {
        auto n = co_await async_read(sock, buffer(ptr + offset, payload_len), use_awaitable);
        offset += n;
    }

    // read 4 bytes sti tail
    {
        auto n = co_await async_read(sock, buffer(ptr + offset, 4), use_awaitable);
        if (auto tail = load_big_s32(ptr + offset); tail != STI_TAIL)
        {
            log_warn("sync tail failed: {:#X}", tail);
        }
        offset += n;
    }
    frame.resize(offset);
    co_return frame;
}

static awaitable<void> make_connection(tcp::socket sock, const rtrrecv_options &opts)
{
    auto scope = gsl::finally([&sock] {
        log_info("disconnected");
        sock.close();
        std::raise(SIGTERM);
    });

    try
    {
        co_await sock.async_connect({ address::from_string(opts.addr), opts.port }, use_awaitable);

        log_info("send tm start request");
        co_await async_write(sock, buffer(make_tm_request(opts.channel, 0)), use_awaitable);

        while (sock.is_open())
        {
            if (auto frame = co_await read_sti_frame(sock); frame)
            {
                if (opts.keep_sti)
                    opts.frame_func(std::move(*frame));
                else
                    opts.frame_func({frame->begin() + STI_HEAD_BYTES, frame->end() - STI_TAIL_BYTES });
            }
        }
    }
    catch (...)
    {
        log_error("connect failed");
    }
}

static void run_rtrrecv(const rtrrecv_options &opts)
{
    log_info("connect to {}:{}", opts.addr, opts.port);
    io_context io;
    co_spawn(io, make_connection(std::move(tcp::socket(io)), opts), detached);

    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) {
        io.stop();
    });
    boost::system::error_code ec;
    io.run(ec);
    if (ec)
    {
        log_error("connect to {}:{} failed, message={}", opts.addr, opts.port, ec.message());
    }
}
