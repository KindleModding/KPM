###
# The contents of this file are licensed under the `CC0` license.
###

import argparse
import pathlib
import shutil
import tarfile
import json
import os

KPM_MANIFEST_VERSION=2
valid_supported_platforms = [
    "kindle",
    "kindle5",
    "kindlepw2",
    "kindlehf",
]

class Package:
    def init(args):
        args.path: pathlib.Path

        manifest = {
            "manifest_version": KPM_MANIFEST_VERSION,
            "id": "",
            "name": "",
            "author": "",
            "description": "",
            "version": [],
            "dependencies": []
        }

        while True:
            manifest["id"] = input("Enter package id: ")
            invalid = False
            for c in manifest['id']:
                if (c.isupper() or (c != '_' and c != '-' and not c.isalnum())):
                    print("Package id must only contain lower-case alphanumeric characters or '_'")
                    invalid = True
                    break
            if (invalid):
                continue
            break

        manifest["name"] = input("Enter package display name: ")
        manifest["author"] = input("Enter package author: ")
        manifest["description"] = input("Enter package description: ")
        
        while True:
            version_str = input("Enter package version: ")
            manifest["version"] = []
            for version_component in version_str.split('.'):
                try:
                    manifest["version"].append(int(version_component))
                except:
                    manifest["version"] = []
                    break
            if (len(manifest["version"]) != 3):
                print("[ERROR] Package version must be in the format a.b.c where a, b and c are integers")
                continue
            break

        if (hasattr(args, 'supported_platform')):
            manifest["supported_platforms"] = args.supported_platform

        manifest_path = os.path.join(args.path, "manifest.json")
        with open(manifest_path, 'w') as file:
            file.write(json.dumps(manifest, indent=2))
        print(f"\n\nWrote manifest file to {manifest_path}")
        print("\n")
        print(json.dumps(manifest, indent=2))

    def pack(args):
        args.output_path: str
        args.pkg_path: str

        if (not os.path.exists(os.path.join(args.pkg_path, "manifest.json"))):
            print("[ERR] manifest.json file not found")
            exit(1)

        manifest = None
        with open(os.path.join(args.pkg_path, "manifest.json"), 'r') as file:
            og_manifest = file.read()
            file.seek(0)
            manifest = json.loads(file.read())

        if (manifest["manifest_version"] != KPM_MANIFEST_VERSION):
            if (manifest["manifest_version"] == 1):
                print("[WARN] Manifest v1 is deprecated. Please upgrade to manifest v2.")
            else:
                print(f"[ERR] Expected manifest version {KPM_MANIFEST_VERSION}, got {manifest['manifest_version']}")
                exit(1)

        if (hasattr(args, 'supported_platform') and len(args.supported_platform) > 0):
            manifest["supported_platforms"] = args.supported_platform
            with open(os.path.join(args.pkg_path, "manifest.json"), 'w') as file:
                file.write(json.dumps(manifest))

        print(f"ID: {manifest['id']}")
        print(f"Name: {manifest['name']}")
        print(f"Author: {manifest['author']}")
        print(f"Supported Platforms: {', '.join(manifest['supported_platforms'])}")
        print("Packing...")

        packageFilename = args.output_path
        if (os.path.isdir(packageFilename)):
            packageFilename = os.path.join(args.output_path, f"{manifest['id']}_{'.'.join(str(x) for x in manifest['version'])}_{'-'.join(manifest.get('supported_platforms', ['kindleany']))}.kpkg")
        if (args.compression != 0 and manifest["manifest_version"] != 1):
            file = tarfile.open(packageFilename, "w:gz", compresslevel=args.compression)
        else:
            file = tarfile.open(packageFilename, "w:")

        with file:
            for source_item_name in os.listdir(args.pkg_path):
                if (source_item_name in ['rootfs', 'startup.sh']):
                    print(f"[ERR] A file or folder with the name '{source_item_name}' was detected in the package - This is currently reserved for future use")
                    with open(os.path.join(args.pkg_path, "manifest.json"), 'w') as file:
                        file.write(og_manifest)
                    file.close()
                    try: os.remove(packageFilename)
                    except: pass
                    exit(1)

                print(f"- {source_item_name}")
                file.add(os.path.join(args.pkg_path, source_item_name), arcname=source_item_name)

        with open(os.path.join(args.pkg_path, "manifest.json"), 'w') as file:
            file.write(og_manifest)

        print("Done!")
        print(f"Saved as {packageFilename}")

class Repo:
    def init(args):
        args.path: pathlib.Path

        manifest = {
            "manifest_version": KPM_MANIFEST_VERSION
        }

        while True:
            manifest["id"] = input("Enter repository id: ")
            if ("kindlemodding" in manifest["id"]):
                print("[WARN] Do not use the term 'kindlemodding' for your own repositories")

            invalid = False
            for c in manifest['id']:
                if (c.isupper() or (c != '_' and c != '-' and not c.isalnum())):
                    print("Repository id must only contain lower-case alphanumeric characters or '_'")
                    invalid = True
                    break
            if (invalid):
                continue
            break

        manifest["name"] = input("Enter repository display name: ")
        manifest["description"] = input("Enter repository description: ")
        manifest["packages"] = {}

        manifest_path = os.path.join(args.path, "manifest.json")
        with open(manifest_path, 'w') as file:
            file.write(json.dumps(manifest, indent=2))
        print(f"\n\nWrote manifest file to {manifest_path}")
        print("\n")
        print(json.dumps(manifest, indent=2))

    def add(args):
        args.repo_path: str
        args.package_path: str
        args.no_rename: bool

        repositoryManifest = None
        try:
            with open(args.repo_path, 'r') as file:
                repositoryManifest = json.loads(file.read())
        except:
            args.repo_path = os.path.join(args.repo_path, "manifest.json")
            with open(args.repo_path, 'r') as file:
                repositoryManifest = json.loads(file.read())

        if (repositoryManifest["manifest_version"] != KPM_MANIFEST_VERSION):
            if (repositoryManifest["manifest_version"] == 1):
                pass
            else:
                print(f"[ERR] Expected manifest version {KPM_MANIFEST_VERSION}, got {repositoryManifest['manifest_version']}")
                exit(1)

        print(f"Adding package {args.package_path} to the repository...")
        print("Reading manifest")
        iconData = None
        manifest = None
        with tarfile.open(args.package_path, 'r') as file:
            try:
                with file.extractfile(file.getmember("manifest.json")) as manifestFile:
                    manifest = json.loads(manifestFile.read())
            except:
                print("[ERR] Could not open manifest.json file")
                exit(1)

            try:
                with file.extractfile(file.getmember("icon.png")) as iconFile:
                    iconData = iconFile.read()
            except:
                print("[WARN] Could not open icon.png file")
            
        if (manifest["manifest_version"] != KPM_MANIFEST_VERSION):
            if (manifest["manifest_version"] == 1):
                pass
            else:
                print(f"[ERR] Expected manifest version {KPM_MANIFEST_VERSION}, got {repositoryManifest['manifest_version']}")
                exit(1)

        if (repositoryManifest["manifest_version"] != manifest["manifest_version"]):
            print(f"[ERR] Manifest versions must match, got package manifest v{manifest["manifest_version"]}, got repo manifest v{repositoryManifest['manifest_version']}")
            exit(1)

        if (not manifest["id"] in repositoryManifest["packages"]):
            repositoryManifest["packages"][manifest["id"]] = { "name": "", "author": "", "description": "", "artifacts": [] }

        if (repositoryManifest["packages"][manifest["id"]]["name"] != manifest["name"]):
            print("Updating name...")
            repositoryManifest["packages"][manifest["id"]]["name"] = manifest["name"]

        if (repositoryManifest["packages"][manifest["id"]]["author"] != manifest["author"]):
            print("Updating author...")
            repositoryManifest["packages"][manifest["id"]]["author"] = manifest["author"]

        if (repositoryManifest["packages"][manifest["id"]]["description"] != manifest["description"]):
            print("Updating description...")
            repositoryManifest["packages"][manifest["id"]]["description"] = manifest["description"]

        print("Adding artifact...")
        packageFolder = os.path.join("packages", manifest['id'])
        artifactFolder = os.path.join(packageFolder, "artifacts")
        if (args.no_rename):
            artifactPath = os.path.join(artifactFolder, f"{manifest['id']}_{'.'.join(str(x) for x in manifest['version'])}_{'-'.join(manifest.get('supported_platforms', ['kindleany']))}.kpkg")
        else:
            artifactPath = os.path.join(artifactFolder, os.path.basename(args.package_path))

        for artifact in repositoryManifest["packages"][manifest["id"]]["artifacts"]:
            if (artifact["version"][0] == manifest["version"][0] and
                artifact["version"][1] == manifest["version"][1] and
                artifact["version"][2] == manifest["version"][2]):
                different = False
                for supported_platform in artifact.get('supported_platforms', []):
                    if (not supported_platform in manifest.get('supported_platforms', [])):
                        different = True
                
                if (not different):
                    print("[WARN] Artifact already in repository - would you like to replace it?")
                    answer = input("[y/N] ")
                    if (answer.upper() != 'Y'):
                        print("Aborted.")
                        exit(1)
                    
                    repositoryManifest["packages"][manifest["id"]]["artifacts"].remove(artifact)
                

        repositoryManifest["packages"][manifest["id"]]["artifacts"].append({
            "url": artifactPath,
            "version": manifest["version"],
            "dependencies": manifest["dependencies"]
        })
        if ("supported_platforms" in manifest):
            repositoryManifest["packages"][manifest["id"]]["artifacts"][-1]["supported_platforms"] = manifest["supported_platforms"]

        if ("supported_devices" in manifest):
            repositoryManifest["packages"][manifest["id"]]["artifacts"][-1]["supported_devices"] = manifest["supported_devices"]

        print("Copying artifact file...")
        os.makedirs(os.path.join(os.path.dirname(args.repo_path), artifactFolder), exist_ok=True)
        shutil.copy(args.package_path, os.path.join(os.path.dirname(args.repo_path), artifactPath))

        if (iconData != None):
            print("Writing icon.png")
            with open(os.path.join(os.path.dirname(args.repo_path), packageFolder, "icon.png"), 'wb') as file:
                file.write(iconData)

        print("Writing updated manifest...")
        with open(args.repo_path, 'w') as file:
            file.write(json.dumps(repositoryManifest, indent=2))

        print("Done!")

parser = argparse.ArgumentParser(prog='KPM Helper',
                    description='Kindle Package Manager Helper is used for a ton of stuff',
                    epilog='Created by Hackerdude (https://ko-fi.com/hackerdude)')
subparsers = parser.add_subparsers(title="command", required=True)

###
# PACKAGE
###
package_parser = subparsers.add_parser("package", help="Used to manage packages")
pack_subparsers = package_parser.add_subparsers(title="subcommand", required=True)

# init
package_init_parser = pack_subparsers.add_parser("init", help="Initialise a package folder by creating a manifest file (interactive)")
package_init_parser.add_argument("path", help="The path to the package folder", type=pathlib.Path)
package_init_parser.add_argument("--supported_platform", help="Add a supported platform to the manifest", action='append', default=[], choices=valid_supported_platforms)
package_init_parser.set_defaults(func=Package.init)

# pack
package_pack_parser = pack_subparsers.add_parser("pack", help="Pack a package folder into a kpkg file")
package_pack_parser.add_argument("pkg_path", help="The path to the package folder", type=pathlib.Path)
package_pack_parser.add_argument("output_path", help="The folder to put the kpkg file in", type=pathlib.Path)
package_pack_parser.add_argument("--compression", help="The compression level (0-9) (defaults to 3)", type=int, default=3)
package_pack_parser.add_argument("--supported_platform", help="Add a supported platform to the manifest", action='append', default=[], choices=valid_supported_platforms)
package_pack_parser.set_defaults(func=Package.pack)

###
# REPO
###
repo_parser = subparsers.add_parser("repo", help="Manage repositories")
repo_subparsers = repo_parser.add_subparsers(title="subcommand", required=True)

# init
repo_init_parser = repo_subparsers.add_parser("init", help="Initialise a repository folder by creating a manifest file (interactive)")
repo_init_parser.add_argument("path", help="The path to the repository folder", type=pathlib.Path)
repo_init_parser.set_defaults(func=Repo.init)

# add
repo_add_parser = repo_subparsers.add_parser("add", help="Add an artifact to the repository")
repo_add_parser.add_argument("repo_path", help="The path to the repository folder", type=pathlib.Path)
repo_add_parser.add_argument("package_path", help="The path to the package file", type=pathlib.Path)
repo_add_parser.add_argument("--no_rename", help="Do not rename package file when copying to the repo", action='store_true')
repo_add_parser.set_defaults(func=Repo.add)

args = parser.parse_args()
args.func(args)