#pragma once
#include <string>

class Flags {
    public:
        static Flags* GetInstance();

        std::string kpkg_dir = "/var/local/kpkg/";
        bool dry = false;
        bool verbose = false;
        std::string firmware_version;
    private:
        Flags();
        static Flags* instance;
};