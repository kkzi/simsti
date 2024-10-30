#include "simsti.hpp"
#include <simple/flags.hpp>

int main(int argc, char **argv)
{
    init_logger("");

    auto opts = flags<simsti_options>("simsti" , "v1", {
                                                       { "-input", "[required] input file path", &simsti_options::input },
                                                       { "-framelen", "[required] frame length", &simsti_options::framelen },
                                                       { "-frameoffset", "frame offset", &simsti_options::frame_offset },
                                                       { "-fps", "frame per seconds, default 2", &simsti_options::fps },
                                                       { "-channel", "tm channel, default 0", &simsti_options::tm_channel },
                                                       { "-port", "tm channel listening port, default 3070", &simsti_options::port },
                                                       { "-timecode", "time code (0/3), default 0", &simsti_options::time_code },
                                                   })(argc, argv);

    // simsti_options opts{ 263, "C:/Users/kizi/desktop/input.dat", 10, 0, 3 };
    sti_server s;
    s.run(opts);
}
