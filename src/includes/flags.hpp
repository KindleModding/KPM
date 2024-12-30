#pragma once
#include <string>

class Flags {
    public:
        static Flags* GetInstance();

        std::string architecture = KPM_ARCH;
        std::string kpkg_dir = "/mnt/us/kpkg";
        bool verbose = false;
        std::string firmware_version;

    private:
        Flags();
        static Flags* instance;
};