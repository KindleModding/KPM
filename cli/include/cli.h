#pragma once

struct CLIState {
    bool fbink;
    bool confirm;
    bool debug;
};

extern struct CLIState cli_state;