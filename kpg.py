import argparse
import tarfile
import json
import os

parser = argparse.ArgumentParser(prog='KPG',
                    description='Kindle Package Generator is used to pack folders into package files',
                    epilog='Created by Hackerdude (https://ko-fi.com/hackerdude)')

parser.add_argument("path", help="The path to the package folder")

args = parser.parse_args()

if (not os.path.exists(os.path.join(args.path, "manifest.json"))):
    print("[ERR] manifest.json file not found")
    exit(1)

manifest = None
with open(os.path.join(args.path, "manifest.json"), 'r') as file:
    manifest = json.loads(file.read())

print(f"ID: {manifest['id']}")
print(f"Name: {manifest['name']}")
print(f"Author: {manifest['author']}")
print("Packing...")

packageFilename = f"./{manifest['id']}_{'.'.join(str(x) for x in manifest['version'])}_{'-'.join(manifest['supported_arch'])}.kpkg"
with tarfile.open(packageFilename, "w|xz") as file:
    for source_item_name in os.listdir(args.path):
        print(f"- {source_item_name}")
        file.add(os.path.join(args.path, source_item_name), arcname=source_item_name)

print("Done!")
print(f"Saved as {packageFilename}")