#include <nlohmann/json_fwd.hpp>
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
    Package(nlohmann::json json); // Parse package from JSON

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
    inline std::vector<std::string> GetSupportedKindle() { return supported_kindle; };
    inline bool CheckSupportsKindle(const std::string& checkKindle)
    {
        for (const std::string& kindle : supported_kindle)
        {
            if (kindle == checkKindle)
            {
                return true;
            }
        }
        return false;
    };
    inline std::string GetEntrypoint() { return entrypoint; };
private:
    std::string id;
    std::string name;
    std::string description;
    SemVer version;
    std::vector<Arch> supported_arch;
    std::vector<std::string> supported_kindle; // ~TODO: Move to enum?
    std::string entrypoint;
};