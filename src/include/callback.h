#pragma once

#include "kpm/kpm.h"
#include <sys/types.h>

void logStub(enum Verbosity, char* details, ...);
void logProgressStub(uint progress, char* details, ...);
char* getInputStub(char* details, ...);

struct KPMLogging dummyKPMStub = 
{
    .log = logStub,
    .logProgress = logProgressStub,
    .getInput = getInputStub
};
