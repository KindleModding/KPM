#include "flags.h"

Flags* Flags::instance = nullptr;
Flags* Flags::GetInstance() {
    if (instance == nullptr) {
        instance = new Flags();
    }
    return instance;
}