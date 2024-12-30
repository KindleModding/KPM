#include "lab126utils.h"
#include <cstdio>
#include <string>

extern "C" {

int getPrettyVersion(char** out) {
  std::string test = "Kindle 5.17.1 (4320360039)";
  asprintf(out, "%s", test.c_str());
  return 0;
}

int getReleaseVersion(char** out) {
  std::string test = "5.17.1";
  asprintf(out, "%s", test.c_str());
  return 0;
}

}