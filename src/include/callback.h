#pragma once

#include "kpm/kpm.h"
#include <sys/types.h>

void logStub(enum Verbosity, const char* details, ...);
void logProgressStub(unsigned int progress, const char* details, ...);
bool getInputStub(const char* details, ...);

extern struct KPMIO dummyKPMStub;