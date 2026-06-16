#pragma once

#include "kpm/kpm.h"
#include <sys/types.h>

void logStub(enum Verbosity, const char* details, ...);
void logProgressStub(uint progress, const char* details, ...);
bool getInputStub(const char* details, ...);

extern struct KPMLogging dummyKPMStub;