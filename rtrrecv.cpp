#include "rtrrecv.hpp"
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fstream>
#include <simple/flags.hpp>
#include <simple/str.hpp>
#include <simple/use_spdlog.hpp>

using namespace std::chrono;

struct options
{
    std::string addr{ "127.0.0.1:3070" };
    u16 mode{ 1 };
    u16 channel{ 0 };
    u16 keep_sti{ 0 };
};

int main(int argc, char **argv)
{
    init_logger("");

    auto opts = flags<options>("rtrrecv", "v1",
        {
            { "-addr", "remote address (ip:port)", &options::addr },
            { "-mode", "output mode (1: console, 2: file, 3: console and file)", &options::mode },
            { "-channel", "data channel number", &options::channel },
            { "-keepsti", "keep sti frame head and tail or not(1: yes, 0: no)", &options::keep_sti },
        })(argc, argv);

    auto parts = str::split(opts.addr, ":", false);
    if (parts.size() != 2)
    {
        log_error("invalid addr {}", opts.addr);
        return EXIT_FAILURE;
    }

    std::optional<std::ofstream> file;
    auto process_param = [&file, mode = opts.mode](auto &&frame) {
        if ((mode & 1) > 0)
        {
            log_info("<{}B> {:02X}", frame.size(), fmt::join(frame, " "));
        }
        if ((mode & 2) > 0)
        {
            if (!file)
            {
                auto path = fmt::format("{:%Y%m%d.%H%M%S}.bin", time_point_cast<milliseconds>(system_clock::now()));
                file = std::ofstream(path, std::ios::trunc | std::ios::binary);
            }
            file->write((char *)frame.data(), frame.size());
        }
    };

    run_rtrrecv({
        .addr = std::string{ parts.at(0) },
        .port = str::to_number<u16>(parts.at(1)),
        .channel = opts.channel,
        .keep_sti = opts.keep_sti > 0,
        .frame_func = std::move(process_param),
    });

    return EXIT_SUCCESS;
}
