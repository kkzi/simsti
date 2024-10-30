#pragma once

#include "sti.hpp"
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/endian/conversion.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <simple/simple.hpp>
#include <simple/tcp_server.hpp>
#include <simple/use_spdlog.hpp>
#include <thread>

using namespace std::chrono;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::endian;

struct simsti_options
{
    std::string input{ "" };
    u64 framelen{ 128 };
    u64 frame_offset{ 0 };
    u64 fps{ 2 };
    u16 tm_channel{ 0 };
    u16 time_code{ 0 };
    u16 port{ 3070 };

    bool file_valid{ false };
    bool has_sti_head{ false };
};

struct frame_generator
{
    virtual ~frame_generator() = default;
    virtual bool fill_frame(std::string &frame) = 0;
};

struct file_frame_generator : public frame_generator
{
    file_frame_generator(std::string_view input, size_t offset)
        : input_(input)
        , offset_(offset)
        , file_(std::ifstream(input_, std::ios::binary))
    {
    }

    bool fill_frame(std::string &frame) override
    {
        if (!file_.good())
        {
            log_error("file {} not exists", input_);
            return false;
        }
        if (offset_ > 0)
        {
            file_.read((char *)frame.data(), offset_);
        }
        auto bytes = frame.size();
        file_.read((char *)frame.data(), bytes);
        if (file_.gcount() != bytes)
        {
            file_.clear();
            file_.seekg(0, std::ios::beg);
            log_info("file {} loop {}", input_, ++loop_count_);
        }
        return true;
    }

private:
    std::string input_;
    u64 offset_{ 0 };
    u64 loop_count_{ 0 };
    std::ifstream file_;
};

struct sequence_frame_generator : public frame_generator
{
    bool fill_frame(std::string &frame) override
    {
        std::fill(frame.begin(), frame.end(), count_++);
        return true;
    }

private:
    u8 count_{ 0 };
};

struct sti_session : public std::enable_shared_from_this<sti_session>
{
    sti_session(std::shared_ptr<tcp_session> ss, simsti_options opts)
        : raw_(ss)
        , opts_(std::move(opts))
    {
        addr_ = raw_->peer_address();
        log_info("new connection from {}", addr_);
    }

    ~sti_session()
    {
        log_info("disconnect from {}", addr_);
        stop();
    }

    void start()
    {
        interrupted_ = false;
        thread_ = std::thread([this] {
            generate_tm_frames();
        });
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

        std::unique_ptr<frame_generator> gen;
        if (opts_.file_valid)
            gen = std::make_unique<file_frame_generator>(opts_.input, opts_.frame_offset);
        else
            gen = std::make_unique<sequence_frame_generator>();
        std::string frame(framelen, 0);
        while (!interrupted_)
        {
            auto now = system_clock::now();
            if (duration_cast<microseconds>(now - last).count() < gap)
            {
                if (opts_.fps <= 10) std::this_thread::sleep_for(1ms);
                continue;
            }
            last = now;

            auto ok = gen->fill_frame(frame);
            if (!ok)
            {
                break;
            }
            push_frame(count, frame);
            count++;
        }
        log_info("fps={:.2f}", (double)count / std::max<size_t>(1, duration_cast<seconds>(last - t0).count()));
    }

    void push_frame(uint32_t count, const std::string &frame)
    {
        if (opts_.has_sti_head)
        {
            raw_->send(frame);
        }
        else
        {
            auto &&[field0, field1] = make_time_tag((uint8_t)opts_.time_code);
            std::string copied(frame.size() + 68, 0);
            store_big_s32((uint8_t *)copied.data(), STI_HEAD);
            store_big_s32((uint8_t *)copied.data() + 1 * sizeof(int), (int)copied.size());
            store_big_u32((uint8_t *)copied.data() + 3 * sizeof(int), field0);
            store_big_u32((uint8_t *)copied.data() + 4 * sizeof(int), field1);
            store_big_s32((uint8_t *)copied.data() + 5 * sizeof(int), count);
            store_big_s32((uint8_t *)copied.data() + 10 * sizeof(int), (int)frame.size());
            std::copy(frame.begin(), frame.end(), copied.begin() + 64);
            store_big_s32((uint8_t *)copied.data() + copied.size() - 4, STI_TAIL);
            raw_->send(copied);
        }
    }

    void stop()
    {
        interrupted_ = true;
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

private:
    std::shared_ptr<tcp_session> raw_;
    simsti_options opts_;
    std::string addr_;
    std::thread thread_;
    std::atomic<bool> interrupted_;
};

class sti_server
{
public:
    void run(simsti_options opts)
    {
        if (opts.time_code != 0 && opts.time_code != 3)
        {
            log_error("time code only supported 0(s+ms) or 3(s+us)");
            return;
        }
        opts.file_valid = std::filesystem::exists(opts.input);
        if (!opts.file_valid)
        {
            log_warn("file is empty or not exists, use frame generator with {} bytes", opts.framelen);
        }
        log_info("listening at {}", opts.port);

        if (auto file = std::ifstream(opts.input, std::ios::binary); file.good())
        {
            int head = 0;
            file.read((char *)&head, sizeof(head));
            opts.has_sti_head = head == STI_HEAD;
            file.close();
        }

        boost::asio::io_context io;
        tcp_server server(io, { tcp::v4(), opts.port });
        server
            .on_connect([&](auto &&ss) {
                // sessions[ss] = nullptr;
            })
            .on_disconnect([&](auto &&ss) {
                close_client(ss);
            })
            .on_received([opts, this](auto &&ss, auto &&msg) {
                if (auto head = load_big_s32((uint8_t *)msg.data()); head != STI_HEAD)
                {
                    // stop();
                    ss->stop();
                    return;
                }

                switch (auto data_flow = load_big_s32((uint8_t *)msg.data() + 20); data_flow)
                {
                case 0:
                case 1:
                case 2:
                case 4:
                case 5:
                    make_client(ss, opts);
                    break;
                case 0x80:
                default:
                    close_client(ss);
                    break;
                }
            })
            .start();

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) {
            io.stop();
        });
        io.run();
    }

private:
    void make_client(const std::shared_ptr<tcp_session> &ss, const simsti_options &opts)
    {
        std::scoped_lock lock(mutex_);
        auto client = std::make_shared<sti_session>(ss, opts);
        client->start();
        sessions_[ss] = client;
    }

    void close_client(const std::shared_ptr<tcp_session> &ss)
    {
        std::scoped_lock lock(mutex_);
        if (sessions_.contains(ss)) sessions_.erase(ss);
    }

private:
    std::mutex mutex_;
    std::map<std::shared_ptr<tcp_session>, std::shared_ptr<sti_session>> sessions_;
};