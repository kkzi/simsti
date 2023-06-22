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
    int mode{ 1 };
};

int main(int argc, char **argv)
{
    init_logger("");

    auto opts = flags<options>("rtrrecv", {
                                              { "-addr", "remote address (ip:port), default 127.0.0.1:3070", &options::addr },
                                              { "-mode", "output mode (1: console, 2: file), default 1", &options::mode },
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
            log_info("{:02X}", fmt::join(frame, " "));
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
        .port = str::to_number<uint16_t>(parts.at(1)),
        .frame_func = std::move(process_param),
    });

    return EXIT_SUCCESS;
}
