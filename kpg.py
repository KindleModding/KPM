import json
import argparse

parser = argparse.ArgumentParser(prog='KPG',
                    description='Kindle Package Generator is used to generate and modify repository manifest files',
                    epilog='Created by Hackerdude (https://ko-fi.com/hackerdude)')

parser.add_argument("--init", action='store_true', help="Create a new manifest.json file (interactive)")

parser.add_argument("--add", help="Add an artifact to the repository")

parser.add_argument("filepath", help="The path to the repository manifest file")

args = parser.parse_args()

if (args.init):
    manifest = {
        "version": 1
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
    manifest["packages"] = []

    with open(args.filepath, 'w') as file:
        file.write(json.dumps(manifest, indent=2))
    print(f"\n\nWrote manifest file to {args.filepath}")
    print("\n")
    print(json.dumps(manifest, indent=2))

if (args.add):
    print(f"Adding package {args.add} to the repository...")
    print("Reading manifest")
    