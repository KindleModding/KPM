#pragma once

#include "package.h"
#include <string>
#include <vector>

class RepoPackage
{
public:
    RepoPackage();
private:
    const std::string name;
    const std::string description;
    const std::string icon;
    const std::vector<Arch> supported_arch;
    const std::vector<std::string> supported_kindles;
};