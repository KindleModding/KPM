#pragma once

#include "package.h"
#include <string>
#include <vector>

class Artifact
{
public:
    Artifact();
private:
    const std::string uri;
    const std::string version;
    const std::vector<Arch> supported_arch;
    const std::vector<std::string> supported_kindles;
};