#pragma once

#include "sti.hpp"
#include <boost/asio.hpp>
#include <boost/endian/conversion.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <simple/use_spdlog.hpp>
#include <vector>

using namespace std::chrono;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::endian;

struct simsti_options
{
    size_t framelen{ 0 };
    std::string input{ "" };
    size_t fps{ 2 };
    uint16_t tm_channel{ 0 };
    uint16_t time_code{ 0 };
    uint16_t port{ 3070 };
    bool has_sti_head{ false };
};

struct tm_session;

template <class T>
struct keeper
{
    void add(std::shared_ptr<T> ptr)
    {
        std::scoped_lock lock(mutex_);
        arr_.push_back(ptr);
    }

    void remove(std::shared_ptr<T> ptr)
    {
        std::scoped_lock lock(mutex_);
        std::erase(arr_, ptr);
    }

private:
    std::mutex mutex_;
    std::vector<std::shared_ptr<T>> arr_;
};

struct tm_session : public std::enable_shared_from_this<tm_session>
{
    tm_session(std::shared_ptr<keeper<tm_session>> keeper, tcp::socket sock, simsti_options opts)
        : keeper_(std::move(keeper))
        , sock_(std::move(sock))
        , opts_(std::move(opts))
        , timer_(sock_.get_executor())
    {
        timer_.expires_at(std::chrono::steady_clock::time_point::max());

        auto ep = sock_.remote_endpoint();
        addr_ = fmt::format("{}:{}", ep.address().to_string(), ep.port());
        log_info("new connection from {}", addr_);
    }

    ~tm_session()
    {
        log_info("disconnect from {}", addr_);
    }

    void start()
    {
        co_spawn(
            sock_.get_executor(),
            [self = shared_from_this()] {
                return self->reader();
            },
            detached);

        co_spawn(
            sock_.get_executor(),
            [self = shared_from_this()] {
                return self->writer();
            },
            detached);
    }

private:
    void generate_tm_frames()
    {
        auto gap = 1e6 / opts_.fps;
        auto framelen = opts_.framelen;
        auto input = opts_.input;

        auto t0 = system_clock::now();
        auto last = t0;
        auto count = 0;
        std::ifstream file(input, std::ios::binary);
        if (!file.good())
        {
            log_error("file {} not exists", input);
            return;
        }

        std::vector<uint8_t> frame(framelen);
        while (!interrupted_ && sock_.is_open())
        {
            auto now = system_clock::now();
            if (duration_cast<microseconds>(now - last).count() < gap)
            {
                if (opts_.fps <= 10) std::this_thread::sleep_for(1us);
                continue;
            }
            last = now;

            file.read((char *)frame.data(), framelen);
            if (file.gcount() != framelen)
            {
                file.clear();
                file.seekg(0, std::ios::beg);
                continue;
            }
            push_frame(count, frame);
            count++;
        }
        log_info("fps={:.2f}", (double)count / std::max<size_t>(1, duration_cast<seconds>(last - t0).count()));
    }

    void push_frame(uint32_t count, const std::vector<uint8_t> &frame)
    {
        if (opts_.has_sti_head)
        {
            // std::scoped_lock lock(mutex_);
            frame_queue_.push(frame);
        }
        else
        {
            auto &&[field0, field1] = make_time_tag((uint8_t)opts_.time_code);
            std::vector<uint8_t> copied(frame.size() + 68, 0);
            store_big_s32(copied.data(), STI_HEAD);
            store_big_s32(copied.data() + 1 * sizeof(int), (int)copied.size());
            store_big_u32(copied.data() + 3 * sizeof(int), field0);
            store_big_u32(copied.data() + 4 * sizeof(int), field1);
            store_big_s32(copied.data() + 5 * sizeof(int), count);
            store_big_s32(copied.data() + 10 * sizeof(int), (int)frame.size());
            std::copy(frame.begin(), frame.end(), copied.begin() + 64);
            store_big_s32(copied.data() + copied.size() - 4, STI_TAIL);

            // std::scoped_lock lock(mutex_);
            frame_queue_.push(copied);
        }
        timer_.cancel_one();
    }

    awaitable<void> reader()
    {
        try
        {
            for (std::vector<uint8_t> frame;;)
            {
                co_await async_read(sock_, dynamic_buffer(frame, 64), use_awaitable);

                if (auto head = load_big_s32(frame.data()); head != STI_HEAD)
                {
                    stop();
                    co_return;
                }

                switch (auto data_flow = load_big_s32(frame.data() + 20); data_flow)
                {
                case 0:
                case 1:
                case 2:
                case 4:
                case 5:
                case 6:
                    thread_ = std::make_unique<std::thread>([self = shared_from_this()] {
                        self->generate_tm_frames();
                    });
                    break;
                case 0x80:
                default:
                    stop();
                    break;
                }
                frame.clear();
            }
        }
        catch (std::exception &)
        {
            stop();
        }
    }

    awaitable<void> writer()
    {
        try
        {
            while (sock_.is_open())
            {
                if (frame_queue_.empty())
                {
                    boost::system::error_code ec;
                    co_await timer_.async_wait(redirect_error(use_awaitable, ec));
                }
                else
                {
                    const auto &frame = frame_queue_.front();
                    co_await boost::asio::async_write(sock_, buffer(frame), use_awaitable);
                    log_info("{:02X}", fmt::join(frame, " "));
                    frame_queue_.pop();
                }
            }
        }
        catch (std::exception &)
        {
            stop();
        }
    }

    void stop()
    {
        interrupted_ = true;
        if (thread_ && thread_->joinable())
        {
            thread_->join();
        }

        sock_.close();
        timer_.cancel();
        keeper_->remove(shared_from_this());
    }

private:
    std::shared_ptr<keeper<tm_session>> keeper_;
    simsti_options opts_;
    tcp::socket sock_;
    std::string addr_;
    steady_timer timer_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> interrupted_;

    std::mutex mutex_;
    std::queue<std::vector<uint8_t>> frame_queue_;
};

static awaitable<void> run_listener(tcp::acceptor acceptor, simsti_options opts)
{
    auto kpr = std::make_shared<keeper<tm_session>>();
    for (;;)
    {
        auto client = co_await acceptor.async_accept(use_awaitable);
        auto session = std::make_shared<tm_session>(kpr, std::move(client), opts);
        session->start();
        kpr->add(session);
    }
}

static void run_simsti(simsti_options opts)
{
    if (opts.time_code != 0 && opts.time_code != 3)
    {
        log_error("time code only supported 0(s+ms) or 3(s+us)");
        return;
    }
    if (!std::filesystem::exists(opts.input))
    {
        log_error("file {} not exists", opts.input);
        return;
    }
    if (auto file = std::ifstream(opts.input, std::ios::binary); file.good())
    {
        int head = 0;
        file.read((char *)&head, sizeof(head));
        opts.has_sti_head = head == STI_HEAD;
    }

    log_info("listening at {}", opts.port);

    io_context io(1);
    co_spawn(io, run_listener(tcp::acceptor(io, { tcp::v4(), opts.port }), std::move(opts)), detached);

    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) {
        io.stop();
    });
    io.run();
}
