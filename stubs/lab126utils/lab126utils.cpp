#include "lab126utils.h"
#include <cstdio>
#include <string>

extern "C" {

int getPrettyVersion(char** out) {
  std::string test = "Stub! 0.0.0 (0)";
  asprintf(out, "%s", test.c_str());
  return 0;
}

int getReleaseVersion(char** out) {
  std::string test = "0.0.0";
  asprintf(out, "%s", test.c_str());
  return 0;
}

}