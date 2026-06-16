#pragma once

#include <stdbool.h>

struct CLIState {
    bool fbink;
    bool confirm;
};

extern struct CLIState cli_state;