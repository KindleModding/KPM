#pragma once

#include <stdbool.h>

struct CLIState {
    bool fbink;
    bool dry;
    bool confirm;
};

extern struct CLIState cli_state;