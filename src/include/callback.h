#ifndef CALLBACK_H
#define CALLBACK_H

#include "kpm/kpm.h"
#include <sys/types.h>

void logStub(KPMIO_Verbosity, const char *details, ...);
void logProgressStub(unsigned int progress, const char *details, ...);
bool getInputStub(const char *details, ...);

extern KPMIO dummyKPMStub;

#endif