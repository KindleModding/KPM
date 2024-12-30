#pragma once
#include <cstdarg>
#include <cstdio>
#include "flags.hpp"

namespace Log {
    inline void D(const char* format, ...) {
        if (Flags::GetInstance()->verbose) {
            va_list args;
            va_start(args, format);
            printf("[DEBUG] ");
            vprintf(format, args);
            printf("\n");
            va_end(args);
        }
    }

    inline void I(const char* format, ...) {
        va_list args;
        va_start(args, format);
        printf("[INFO] ");
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }

    inline void W(const char* format, ...) {
        va_list args;
        va_start(args, format);
        printf("[WARN] ");
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }

    inline void E(const char* format, ...) {
        va_list args;
        va_start(args, format);
        printf("[ERROR] ");
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }
}