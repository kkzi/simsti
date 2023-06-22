#include "simsti.hpp"

int main(int argc, char **argv)
{
    init_logger("");

    simsti_options opts{ 263, "C:/Users/kizi/desktop/input.dat", 10, 0, 3 };
    run_simsti(opts);
}
