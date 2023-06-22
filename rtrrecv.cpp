#include "rtrrecv.hpp"
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fstream>

using namespace std::chrono;

int main(int argc, char **argv)
{
    int mode = 1;
    rtrrecv_options opts;

    std::optional<std::ofstream> file;
    opts.frame_func = [&file, mode](auto &&frame) {
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

    run_rtrrecv(opts);

    return EXIT_SUCCESS;
}
