#pragma once

struct CLIState {
    bool fbink;
    bool confirm;
    bool search_command;
};

extern struct CLIState cli_state;