#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "semver.h"

enum class Arch
{
    ARMEL,
    ARMHF
};

class Package
{
public:
    Package(
        const std::string& id,
        const std::string& name,
        const std::string& description,
        const SemVer version,
        const std::vector<Arch>& supported_arch,
        const std::vector<std::string>& supported_kindles
    );

    static Package FromJSON(const nlohmann::json& json);
    static bool ValidateJSON(const nlohmann::json& json);

    inline std::string GetId() { return id; };
    inline std::string GetName() { return name; };
    inline std::string GetDescription() { return description; };
    inline SemVer GetVersion() { return version; };
    inline std::vector<Arch> GetSupportedArch() { return supported_arch; };
    inline bool CheckSupportsArch(Arch checkArch)
    {
        for (const Arch& arch : supported_arch)
        {
            if (arch == checkArch)
            {
                return true;
            }
        }
        return false;
    };
    inline std::vector<std::string> GetSupportedKindle() { return supported_kindles; };
    inline bool CheckSupportsKindle(const std::string& checkKindle)
    {
        for (const std::string& kindle : supported_kindles)
        {
            if (kindle == checkKindle)
            {
                return true;
            }
        }
        return false;
    };
private:
    const std::string id;
    const std::string name;
    const std::string description;
    const SemVer version;
    const std::vector<Arch> supported_arch;
    const std::vector<std::string> supported_kindles; // ~TODO: Move to enum?
};