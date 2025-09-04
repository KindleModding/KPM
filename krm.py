import argparse
import os
import tarfile
import shutil
import json

parser = argparse.ArgumentParser(prog='KRM',
                    description='Kindle Repository Manager is used to generate and modify repository manifest files',
                    epilog='Created by Hackerdude (https://ko-fi.com/hackerdude)')

parser.add_argument("--init", action='store_true', help="Create a new manifest.json file (interactive)")

parser.add_argument("--add", help="Add an artifact to the repository")

parser.add_argument("filepath", help="The path to the repository manifest file")

args = parser.parse_args()

if (args.init):
    manifest = {
        "manifest_version": 1
    }

    manifest["id"] = input("Enter repository id: ")
    if (' ' in manifest["id"]):
        print("Manifest id must not contain spaces")
        exit(1)
    if (not '.' in manifest["id"]):
        print("Manifest id must be in reverse-domain format")
        print("ie: com.example.repository")
        exit(1)
    if ("org.kindlemodding" in manifest["id"]):
        print("[WARN] Do not use this namespace for your own repositories")

    manifest["name"] = input("Enter repository display name: ")
    manifest["description"] = input("Enter repository description: ")
    manifest["packages"] = {}

    with open(args.filepath, 'w') as file:
        file.write(json.dumps(manifest, indent=2))
    print(f"\n\nWrote manifest file to {args.filepath}")
    print("\n")
    print(json.dumps(manifest, indent=2))

if (args.add):
    repositoryManifest = None
    with open(args.filepath, 'r') as file:
        repositoryManifest = json.loads(file.read())

    print(f"Adding package {args.add} to the repository...")
    print("Reading manifest")
    with tarfile.open(args.add, 'r') as file:
        manifest = None
        try:
            with file.extractfile(file.getmember("manifest.json")) as manifestFile:
                manifest = json.loads(manifestFile.read())
        except:
            print("[ERR] Could not open manifest.json file")
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
    artifactFolder = "packages/artifacts"
    artifactPath = f"{artifactFolder}/{manifest['id']}_{'.'.join(str(x) for x in manifest['version'])}_{'-'.join(manifest['supported_arch'])}.kpkg"

    for artifact in repositoryManifest["packages"][manifest["id"]]["artifacts"]:
        if (artifact["version"][0] == manifest["version"][0] and
            artifact["version"][1] == manifest["version"][1] and
            artifact["version"][2] == manifest["version"][2]):
            different = False
            for supported_arch in artifact["supported_arch"]:
                if (not supported_arch in manifest["supported_arch"]):
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
        "dependencies": manifest["dependencies"],
        "supported_arch": manifest["supported_arch"],
    })

    if ("supported_devices" in manifest):
        repositoryManifest["packages"][manifest["id"]]["artifacts"][-1]["supported_devices"] = manifest["supported_devices"]

    print("Copying artifact file...")
    os.makedirs(os.path.join(os.path.dirname(args.filepath), artifactFolder), exist_ok=True)
    shutil.copy(args.add, os.path.join(os.path.dirname(args.filepath), artifactPath))

    print("Writing updated manifest...")
    with open(args.filepath, 'w') as file:
        file.write(json.dumps(repositoryManifest, indent=2))

    print("Done!")