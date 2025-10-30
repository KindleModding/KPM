#include "kpm/kpm.h"
#include "callback.h"

void logStub(enum Verbosity, char* details, ...)
{
    return;
}

void logProgressStub(uint progress, char* details, ...)
{
    return;
}

bool getInputStub(char* details, ...)
{
    return false;
}

