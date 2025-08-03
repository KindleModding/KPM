#include "package.h"

Package::Package(
    const std::string& id,
    const std::string& name,
    const std::string& description,
    const SemVer version,
    const std::vector<Arch>& supported_arch,
    const std::vector<std::string>& supported_kindle,
    const std::string& entrypoint
): id(id),
    name(name),
    description(description),
    version(version),
    supported_arch(supported_arch),
    supported_kindle(supported_kindle),
    entrypoint(entrypoint)
{}

Package Package::FromJSON(const nlohmann::json& json)
{
    return Package(
        json["id"],
        json["name"],
        json["description"],
        SemVer(json["version"][0], json["version"][1], json["version"][2]),
        json["supported_arch"],
        json["supported_kindle"], // @TODO: Should be optional
        json["entrypoint"]
    );
}

bool Package::ValidateJSON(const nlohmann::json& json)
{
    return json.at("version").is_number() &&
            json.at("version") == 1 &&
            json.at("id").is_string() &&
            json.at("name").is_string() &&
            json.at("description").is_string() &&
            json.at("version").is_array() &&
            json.at("version").size() == 3 &&
            json.at("version").at(0).is_number() &&
            json.at("version").at(1).is_number() &&
            json.at("version").at(2).is_number() &&
            json.at("supported_arch").is_array() &&
            json.at("supported_arch").size() > 0 &&
            json.at("entrypoint").is_string(); // @TODO: Check supported_kindle
}