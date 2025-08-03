#pragma once

#include <string>

class Repo
{
public:
    Repo();
private:
    const std::string id;
    const std::string name;
    const std::string description;
};