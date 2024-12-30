#include "flags.h"
#include "log.h"
#include <cstdio>
#include "lab126utils.h"

Flags* Flags::instance = nullptr;
Flags* Flags::GetInstance() {
    if (instance == nullptr) {
        instance = new Flags();
    }
    return instance;
}

Flags::Flags() {
    char* version;
    // Read the version
    const int result = getReleaseVersion(&version);
    this->firmware_version = version;
}