#include "lab126utils.h"
#include <stdio.h>

int getDeviceCode(int* out)
{
  *out = 0;
  return 0;
}

int getPrettyVersion(char** out) {
  asprintf(out, "%s", "Stub! 0.0.0 (0)");
  return 0;
}

int getReleaseVersion(char** out) {
  asprintf(out, "%s", "0.0.0");
  return 0;
}