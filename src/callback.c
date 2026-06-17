#include "kpm/kpm.h"
#include "callback.h"

void logStub(enum Verbosity verbosity, const char* details, ...)
{
    return;
}

void logProgressStub(unsigned int progress, const char* details, ...)
{
    return;
}

bool getInputStub(const char* details, ...)
{
    return false;
}

struct KPMIO dummyKPMStub = 
{
    .log = logStub,
    .logProgress = logProgressStub,
    .getInput = getInputStub
};

