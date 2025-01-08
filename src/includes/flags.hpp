#pragma once
#include <string>

class Flags {
    public:
        static Flags* GetInstance();

        std::string architecture = KPM_ARCH;
        std::string cache_dir = "/var/cache/kpm";
        std::string kpm_dir = "/var/local/kpm";
        bool verbose = false;
        std::string firmware_version;

    private:
        Flags();
        static Flags* instance;
};