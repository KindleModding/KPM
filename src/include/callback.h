#pragma once

#include "kpm/kpm.h"
#include <sys/types.h>

void logStub(enum Verbosity, char* details, ...);
void logProgressStub(uint progress, char* details, ...);
bool getInputStub(char* details, ...);

extern struct KPMLogging dummyKPMStub;