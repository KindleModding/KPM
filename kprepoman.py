import argparse
import pathlib
import json
import copy
import tarfile
from os import path
import os
import shutil
import getpass

parser = argparse.ArgumentParser("KPBuild", description="Kindle Package Builder")
parser.add_argument("repopath", type=pathlib.Path)
parser.add_argument("operator", choices=["init", "add_package", "remove_package"])
parser.add_argument("target", type=pathlib.Path, nargs='?', default=None)

args = parser.parse_args()

if (args.operator == "init"):
    print("Initialising repository...")
    manifest  = {
        "version": 1,
        "id": "com." + getpass.getuser() + ".repo",
        "packages": []
    }
    repo_id = input(f"Please enter the ID of your repository ({manifest["id"]})")
    if (repo_id != ""):
        manifest["id"] = repo_id
    
    with open(args.repopath.joinpath("manifest.json"), 'w', encoding='utf-8') as file:
        file.write(json.dumps(manifest))
    print(f"Created repo {manifest["id"]}")
elif (args.operator == "add_package"):
    if (args.target == None):
        print("Error: You must specify a package file to add")
        exit(1)

    # Check if repo has a manifest
    repoManifest = {}
    with open(args.repopath.joinpath("manifest.json"), 'r', encoding='utf-8') as file:
        repoManifest = json.loads(file.read())

    kpkg = tarfile.open(args.target)
    # Check if package is valid
    requiredPaths = ["icon.png", "manifest.json"]
    for member in kpkg.getmembers():
        try:
            requiredPaths.remove(member.path)
        except:
            pass
    if (len(requiredPaths) > 0):
        print("ERR: The package archive is missing the following files:")
        print(requiredPaths)
        exit(1)

    packageManifestFile = kpkg.extractfile("manifest.json")
    packageManifest = json.load(packageManifestFile)

    # Check if this package already exists in the repo manifest
    packageIndex = -1
    for i in range(len(repoManifest["packages"])):
        if (repoManifest["packages"][i]["id"] == packageManifest["id"]):
            packageIndex = i
            break

    if (packageIndex == -1):
        repoManifest["packages"].append({
            "id": packageManifest["id"],
            "alias": packageManifest["alias"],
            "display_name": packageManifest["display_name"],
            "description": packageManifest["description"],
            "screenshots": 0,
            "versions": []
        })
        packageIndex = len(repoManifest["packages"]) - 1
    else:
        print("* Package already exists - Updating info")
        repoManifest["packages"][packageIndex]["display_name"] = packageManifest["display_name"]
        repoManifest["packages"][packageIndex]["description"] = packageManifest["description"]
        repoManifest["packages"][packageIndex]["screenshots"] = 0

    print("* Checking version")
    versionIndex = -1
    for i in range(len(repoManifest["packages"][packageIndex]["versions"])):
        version = repoManifest["packages"][packageIndex]["versions"][i]
        if (version["version_name"] == packageManifest["version_name"] and version["version_number"] != packageManifest["version_number"]):
            print(f"ERR: Version name {version["version_name"]} is already defined with version number {version["version_number"]} (tried to add as {version["version_name"]})")
            exit(1)

        if (version["version_name"] != packageManifest["version_name"] and version["version_number"] == packageManifest["version_number"]):
            print(f"ERR: Version number {version["version_number"]} is already defined with version name {version["version_name"]} (tried to add as {version["version_number"]})")
            exit(1)

        if (version["version_name"] == packageManifest["version_name"] and version["version_number"] == packageManifest["version_number"]):
            print(f"WARN: Version {version["version_name"]} ({version["version_number"]}) already in repo - OVERWRITING")
            versionIndex = i

    if (versionIndex == -1):
        repoManifest["packages"][packageIndex]["versions"].append({
            "version_name": packageManifest["version_name"],
            "version_number": packageManifest["version_number"],
            "min_firmware": packageManifest["min_firmware"],
            "max_firmware": packageManifest["max_firmware"],
            "dependencies": packageManifest["dependencies"],
            "supported_arch": packageManifest["supported_arch"]
        })
        versionIndex = len(repoManifest["packages"][packageIndex]["versions"]) - 1
    else:
        repoManifest["packages"][packageIndex]["versions"][versionIndex]["version_name"] = packageManifest["version_name"]
        repoManifest["packages"][packageIndex]["versions"][versionIndex]["version_number"] = packageManifest["version_number"]
        repoManifest["packages"][packageIndex]["versions"][versionIndex]["min_firmware"] = packageManifest["min_firmware"]
        repoManifest["packages"][packageIndex]["versions"][versionIndex]["max_firmware"] = packageManifest["max_firmware"]
        repoManifest["packages"][packageIndex]["versions"][versionIndex]["dependencies"] = packageManifest["dependencies"]
        repoManifest["packages"][packageIndex]["versions"][versionIndex]["supported_arch"] = packageManifest["supported_arch"]
    
    print("* Creating folder")
    os.makedirs(args.repopath.joinpath("packages").joinpath(packageManifest["id"]).joinpath(packageManifest["version_name"]), exist_ok=True)
    print("* Copying icon")
    kpkg.extract("icon.png", args.repopath.joinpath("packages").joinpath(packageManifest["id"]))

    try:
        shutil.rmtree(args.repopath.joinpath("packages").joinpath(packageManifest["id"]).joinpath("screenshots"))
    except:
        pass
    os.makedirs(args.repopath.joinpath("packages").joinpath(packageManifest["id"]).joinpath("screenshots"))
    repoManifest["packages"][packageIndex]["screenshots"] = 0

    for member in kpkg.getmembers():
        if (member.path[:12] == "screenshots/"):
            kpkg.extract(member, args.repopath.joinpath("packages").joinpath(packageManifest["id"]))
            os.rename(args.repopath.joinpath("packages").joinpath(packageManifest["id"]).joinpath(member.path), args.repopath.joinpath("packages").joinpath(packageManifest["id"]).joinpath(f"screenshots/{repoManifest["packages"][packageIndex]["screenshots"]}.png"))
            repoManifest["packages"][packageIndex]["screenshots"] += 1

    print("* Copying package")
    try:
        os.remove(args.repopath.joinpath("packages").joinpath(packageManifest["id"]).joinpath(packageManifest["version_name"]).joinpath(f"{packageManifest["id"]}_{packageManifest["version_name"]}_{packageManifest["supported_arch"]}.kpkg"))
    except:
        pass
    shutil.copy(args.target, args.repopath.joinpath("packages").joinpath(packageManifest["id"]).joinpath(packageManifest["version_name"]).joinpath(f"{packageManifest["id"]}_{packageManifest["version_name"]}_{packageManifest["supported_arch"]}.kpkg"))

    print("* Writing repo manifest")
    with open(args.repopath.joinpath("manifest.json"), 'w', encoding='utf-8') as file:
        file.write(json.dumps(repoManifest))