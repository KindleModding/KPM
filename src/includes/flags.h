#pragma once
#include <string>

class Flags {
    public:
        static Flags* GetInstance();

        std::string kpkg_dir = "/var/local/kpkg/";
        bool dry = false;
        bool verbose = false;
    private:
        Flags() {}
        static Flags* instance;
};