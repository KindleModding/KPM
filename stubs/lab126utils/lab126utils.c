#include "lab126utils.h"
#include <stdio.h>
#include <string.h>

int getDeviceCode(int* out)
{
  *out = 0;
  return 0;
}

int getPrettyVersion(char** out) {
  *out = strdup("Stub! 0.0.0 (0)");
  return 0;
}

int getReleaseVersion(char** out) {
  *out = strdup("0.0.0");
  return 0;
}