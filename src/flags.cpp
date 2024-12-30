#include "flags.hpp"
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
    getReleaseVersion(&version);
    this->firmware_version = version;
}