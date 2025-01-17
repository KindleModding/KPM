import argparse
import pathlib
import json
import tarfile
from os import path
import os

parser = argparse.ArgumentParser("KPBuild", description="Kindle Package Builder")
parser.add_argument("folderpath", type=pathlib.Path)

args = parser.parse_args()
packagePath = args.folderpath

# Check if the manifest.json file exists
if (not path.exists(path.join(packagePath, "manifest.json"))):
    print("manifest.json NOT FOUND - ABORTING!")
    print("Ensure a manifest.json file exists in the root of your package")
    exit(1)

if (not path.exists(path.join(packagePath, "icon.png"))):
    print("icon.png NOT FOUND - ABORTING!")
    print("Ensure your package has a valid icon.png file")
    exit(1)

if (not path.exists(path.join(packagePath, "screenshots/"))):
    print("WARNING: Your package has no screenshots")

buildData = {}
with open(path.join(packagePath, "manifest.json"), 'r', encoding='utf-8') as file:
    buildData = json.loads(file.read())

if ("id" not in buildData.keys()):
    print("Error: manifest.json malformed (missing 'id' key)!")

print("Building Package:")
print(f"id: [{buildData["id"]}]")
print(f"name: [{buildData["display_name"]}]")
print(f"version: [{buildData["version_name"]}]")
print(f"arch: [{buildData["architecture"]}]")

if ("install" in buildData.keys()):
    if (not path.exists(path.join(packagePath, buildData["install"]))):
        print(f"Error: Invalid install script specified {buildData["install"]} (NOT_FOUND)")
        exit(1)

if ("uninstall" in buildData.keys()):
    if (not path.exists(path.join(packagePath, buildData["uninstall"]))):
        print(f"Error: Invalid uninstall script specified {buildData["uninstall"]} (NOT_FOUND)")
        exit(1)

if ("mount" in buildData.keys()):
    for dest in buildData["mount"]:
        if (not path.exists(path.join(packagePath, buildData["mount"][dest]))):
            print(f"Error: Invalid mount location detected {buildData["mount"][dest]} (NOT FOUND)")

print("\n\n")

print("Writing package file")
tar = tarfile.open(packagePath.parents[0].joinpath(buildData["id"] + '_' + buildData["version_name"] + '_' + buildData["architecture"] + '.kpkg'), 'x:gz')
for item in os.listdir(packagePath):
    tar.add(path.abspath(path.join(packagePath, item)), arcname=item)
tar.close()