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

char* getInputStub(char* details, ...)
{
    return "";
}

