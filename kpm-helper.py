###
# The contents of this file are licensed under the `CC0` license.
###

import logging
import argparse
import pathlib
import shutil
import tarfile
import json
import os

logging.basicConfig(format="[%(levelname)s] %(message)s", level=logging.INFO)
logger = logging.getLogger(__name__)

KPM_MANIFEST_VERSION = 3
valid_supported_platforms = [
    "kindle",
    "kindle5",
    "kindlepw2",
    "kindlehf",
]


class Version:
    def __init__(self, major: int, minor: int, patch: int):
        self.major = major
        self.minor = minor
        self.patch = patch

    def encode(self):
        return [self.major, self.minor, self.patch]

    def decode(encoded: list):
        assert len(encoded) == 3
        return Version(int(encoded[0]), int(encoded[1]), int(encoded[2]))

    def __str__(self):
        return f"{self.major}.{self.minor}.{self.patch}"

    def __repr__(self):
        return self.__str__()

    def __eq__(self, other):
        if type(other) != type(self):
            return False

        return (
            self.major == other.major
            and self.minor == other.minor
            and self.patch == other.patch
        )


class Dependency:
    def __init__(self, id: str, min: Version = None, max: Version = None):
        self.id = id
        self.min = min
        self.max = max

    def encode(self):
        encoded = {"id": self.id}
        if self.min:
            encoded["min"] = self.min.encode()
        if self.max:
            encoded["max"] = self.max.encode()
        return encoded

    def decode(encoded: dict):
        return Dependency(
            encoded["id"],
            (
                Version.decode(encoded.get("min", None))
                if encoded.get("min", None)
                else None
            ),
            (
                Version.decode(encoded.get("max", None))
                if encoded.get("max", None)
                else None
            ),
        )


class Package:
    RESERVED_FILEPATHS = ["rootfs", "startup.sh"]

    def __init__(
        self,
        path: str,
        id: str = None,
        name: str = None,
        author: str = None,
        description: str = None,
        version: Version = None,
        dependencies: list[Dependency] = [],
        supported_platforms: list[str] = None,
        manifest_version: int = KPM_MANIFEST_VERSION,
    ):
        self.path = path
        self.manifest_path = os.path.join(path, "manifest.json")
        self.manifest_version = manifest_version

        # @TODO
        if manifest_version != KPM_MANIFEST_VERSION:
            if manifest_version["manifest_version"] < KPM_MANIFEST_VERSION:
                logger.warning(
                    f"Manifest v{manifest_version['manifest_version']} is deprecated. Please upgrade to manifest v{KPM_MANIFEST_VERSION}."
                )
            else:
                logger.error(
                    f"Expected manifest version {KPM_MANIFEST_VERSION}, got {manifest_version}"
                )
                exit(1)

        # Try to read the manifest
        if os.path.exists(self.manifest_path) and id == None:
            with open(self.manifest_path, "r") as file:
                manifest = json.loads(file.read())
                self.manifest_version = manifest["manifest_version"]

                self.id = manifest["id"]
                self.name = manifest["name"]
                self.author = manifest["author"]
                self.description = manifest["description"]
                self.version = Version.decode(manifest["version"])

                self.dependencies = []
                for dependency in manifest["dependencies"]:
                    self.dependencies.append(Dependency.decode(dependency))

                self.supported_platforms = manifest["supported_platforms"]
        else:
            self.id = id
            self.name = name
            self.author = author
            self.description = description
            self.version = version
            self.dependencies = dependencies
            self.supported_platforms = supported_platforms
        self.validate()

    def set_id(self, id: str):
        self.id = id

    def set_name(self, name: str):
        self.name = name

    def set_author(self, author: str):
        self.author = author

    def set_description(self, description: str):
        self.description = description

    def set_version(self, version: Version):
        self.version = version

    def add_dependency(self, dependency: Dependency):
        for existing_dependency in self.dependencies:
            if existing_dependency.id == dependency.id:
                raise RuntimeError(
                    f"Dependency {dependency.id} already exists in package"
                )

        self.dependencies.append(dependency)

    def remove_dependency(self, id: str):
        for dependency in self.dependencies:
            if dependency.id == id:
                self.dependencies.remove(dependency)
                return

    def add_supported_platform(self, platform: str):
        if not platform in valid_supported_platforms:
            raise RuntimeError(
                f"Supported platform must be one of: {valid_supported_platforms}"
            )
        if self.supported_platforms == None:
            self.supported_platforms = []
        self.supported_platforms.add(platform)

    def remove_supported_platform(self, platform: str):
        self.supported_platforms.remove(platform)

    def validate(self):
        invalid_id = False
        for letter in self.id:
            if letter.isspace() or letter.isupper():
                invalid_id = True
            if not (letter.isascii() or letter in ["-", "_"]):
                invalid_id = True

        if invalid_id:
            raise RuntimeError(
                "id must only contain alphanumeric characters, or _ and -"
            )

        if self.supported_platforms != None:
            for supported_platform in self.supported_platforms:
                if not supported_platform in valid_supported_platforms:
                    raise RuntimeError(
                        f"Supported platform must be one of: {valid_supported_platforms}"
                    )

        if len(self.name.strip()) == 0:
            raise RuntimeError("Package must have a non-empty name")

    def write_manifest(self):
        with open(self.manifest_path, "w") as file:
            manifest = {
                "manifest_version": self.manifest_version,
                "id": self.id,
                "name": self.name,
                "author": self.author,
                "description": self.description,
                "version": self.version.encode(),
                "dependencies": [],
            }

            for dependency in self.dependencies:
                manifest["dependencies"].append(dependency.encode())

            if self.supported_platforms != None:
                manifest["supported_platforms"] = list(self.supported_platforms)
            else:
                manifest["supported_platforms"] = self.supported_platforms

            file.write(json.dumps(manifest, indent=2))

    def pack(self, output_path: str, compression: int = 5):
        self.write_manifest()
        logger.info(f"ID: {self.id}")
        logger.info(f"Name: {self.name}")
        logger.info(f"Author: {self.author}")
        logger.info(
            f"Supported Platforms: {'-'.join(self.supported_platforms if self.supported_platforms else ["kindleany"])}"
        )
        logger.info("Packing...")

        packageFilename = output_path
        if os.path.isdir(packageFilename):
            packageFilename = os.path.join(
                output_path,
                f"{self.id}_{self.version}_{'-'.join(self.supported_platforms if self.supported_platforms else ["kindleany"])}.kpkg",
            )

        if compression == 0:
            file = tarfile.open(packageFilename, "w:")
        elif self.manifest_version >= 2:
            file = tarfile.open(packageFilename, "w:gz", compresslevel=compression)
        elif self.manifest_version >= 3:
            file = tarfile.open(packageFilename, "w:zst", level=compression)
        else:
            file = tarfile.open(
                packageFilename, "w:xz", preset=compression
            )  # For deprecated V1 packages

        with file:
            for source_item_name in os.listdir(self.path):
                if source_item_name in self.RESERVED_FILEPATHS:
                    raise RuntimeError(
                        f"[ERR] A file or folder with the name '{source_item_name}' was detected in the package - This is currently reserved for future use"
                    )

                logger.debug(f"- {source_item_name}")
                file.add(
                    os.path.join(self.path, source_item_name),
                    arcname=source_item_name,
                )

        logger.info("Done!")
        logger.info(f"Saved as {packageFilename}")


class Artifact:
    def __init__(
        self,
        id: str,
        name: str,
        author: str,
        description: str,
        url: str,
        version: Version,
        dependencies: list[Dependency],
        supported_platforms: list[str] | None,
    ):
        self.id = id
        self.name = name
        self.author = author
        self.description = description
        self.url = url
        self.version = version
        self.dependencies = dependencies
        self.supported_platforms = supported_platforms


class Repo:
    def __init__(
        self,
        manifest_path: str,
        id: str = None,
        name: str = None,
        description: str = None,
        artifacts: list[Artifact] = None,
        manifest_version: int = KPM_MANIFEST_VERSION,
    ):
        self.manifest_path = manifest_path
        self.path = os.path.dirname(self.manifest_path)
        self.manifest_version = manifest_version

        if os.path.exists(self.manifest_path):
            with open(self.manifest_path, "r") as file:
                manifest = json.loads(file.read())
            self.id = manifest["id"]
            self.name = manifest["name"]
            self.description = manifest["description"]
            self.manifest_version = manifest["manifest_version"]
            self.artifacts = []
            for package_id in manifest["packages"]:
                package = manifest["packages"][package_id]
                for artifact in package["artifacts"]:
                    artifact_obj = Artifact(
                        package_id,
                        package["name"],
                        package["author"],
                        package["description"],
                        artifact["url"],
                        Version.decode(artifact["version"]),
                        [],
                        artifact["supported_platforms"],
                    )
                    for dependency in artifact["dependencies"]:
                        artifact_obj.dependencies.append(Dependency.decode(dependency))
                    self.artifacts.append(artifact_obj)
        else:
            self.id = id
            self.name = name
            self.description = description
            self.artifacts = artifacts

    def add_artifact(self, path: str):
        if not os.path.exists(path):
            raise RuntimeError(f"Artifact at {path} does not exist")

        with tarfile.open(path, "r") as file:
            try:
                with file.extractfile(file.getmember("manifest.json")) as manifestFile:
                    manifest: dict = json.loads(manifestFile.read())
            except:
                raise RuntimeError("[ERR] Could not open manifest.json file")

        filename_supported_platforms = "kindleany"
        if (manifest.get("supported_platforms", None) != None):
            filename_supported_platforms = '-'.join(manifest["supported_platforms"])

        for existing_artifact in self.artifacts:
            if (
                existing_artifact.supported_platforms != None
                and manifest["supported_platforms"] != None
            ):
                existing_platforms = list(existing_artifact.supported_platforms)
                platforms = list(manifest["supported_platforms"])
                platforms_overlap = len(set(existing_platforms + platforms)) != len(
                    existing_platforms
                ) + len(platforms)
            else:
                platforms_overlap = (
                    existing_artifact.supported_platforms
                    == manifest["supported_platforms"]
                )

            if (
                platforms_overlap
                and existing_artifact.id == manifest["id"]
                and existing_artifact.version == Version.decode(manifest["version"])
            ):
                raise RuntimeError(
                    f"Artifact {existing_artifact.id} @ {existing_artifact.version} : {filename_supported_platforms} already exists in repository"
                )

        dependencies = []
        for dependency in manifest["dependencies"]:
            dependencies.append(Dependency.decode(dependency))

        artifact_path = os.path.join(
            "packages",
            manifest["id"],
            "artifacts",
            f"{manifest['id']}_{'.'.join(str(x) for x in manifest['version'])}_{filename_supported_platforms}.kpkg",
        )

        full_artifact_path = os.path.join(self.path, artifact_path)
        os.makedirs(os.path.dirname(full_artifact_path), exist_ok=True)
        shutil.copy(path, full_artifact_path)

        self.artifacts.append(
            Artifact(
                manifest["id"],
                manifest["name"],
                manifest["author"],
                manifest["description"],
                artifact_path,
                Version.decode(manifest["version"]),
                dependencies,
                manifest["supported_platforms"],
            )
        )

    def remove_artifact(
        self, id: str, version: Version = None, supported_platforms: list[str] = None
    ):
        to_remove: list[Artifact] = []
        for artifact in self.artifacts:
            if artifact.id == id and (version == None or artifact.version == version):
                platforms_match = True

                if supported_platforms != None:
                    for platform in supported_platforms:
                        if not platform in artifact.supported_platforms:
                            platforms_match = False
                            break

                if platforms_match:
                    to_remove.append(artifact)

        logger.info(f"Found {len(to_remove)} artifact(s) to remove")
        for artifact in to_remove:
            logger.info(
                f"Removing artifact {artifact.name} @ {artifact.version} ({','.join(artifact.supported_platforms)})"
            )
            try:
                os.remove(os.path.join(self.path, artifact.url))
            except:
                logger.warning(f"Could not delete artifact file at {artifact.url}")
            self.artifacts.remove(artifact)

    def write_manifest(self):
        encoded = {
            "manifest_version": self.manifest_version,
            "id": self.id,
            "name": self.name,
            "description": self.description,
            "packages": {},
        }
        for artifact in self.artifacts:
            if not artifact.id in encoded["packages"]:
                encoded["packages"][artifact.id] = {
                    "name": artifact.name,
                    "author": artifact.author,
                    "description": artifact.description,
                    "artifacts": [],
                }

            encoded_artifact = {
                "url": artifact.url,
                "version": Version.encode(artifact.version),
                "dependencies": [],
                "supported_platforms": (
                    list(artifact.supported_platforms)
                    if artifact.supported_platforms
                    else None
                ),
            }
            for dependency in artifact.dependencies:
                encoded_artifact["dependencies"].append(Dependency.encode(dependency))

            encoded["packages"][artifact.id]["artifacts"].append(encoded_artifact)

        with open(self.manifest_path, "w") as file:
            file.write(json.dumps(encoded))


if __name__ == "__main__":

    def get_id(prompt: str = "Enter id: "):
        id = input(prompt).strip()
        invalid = False
        for letter in id:
            if letter.isspace() or letter.isupper():
                invalid = True
            if not (letter.isascii() or letter in ["-", "_"]):
                invalid = True

        if invalid:
            raise RuntimeError(
                "id must only contain alphanumeric characters, or _ and -"
            )
        return id

    def get_version(prompt: str = "Enter version: "):
        while True:
            try:
                version_str = input(prompt).strip()
                if len(version_str) == 0:
                    return None
                else:
                    return Version.decode(version_str.split("."))
            except Exception as e:
                if (type(e) is KeyboardInterrupt or type(e) is InterruptedError):
                    return
                logger.warning("Invalid version, please try again.")

    parser = argparse.ArgumentParser(
        prog="KPM Helper",
        description="Kindle Package Manager Helper is used for a ton of stuff",
        epilog="Created by Hackerdude (https://ko-fi.com/hackerdude)",
    )
    subparsers = parser.add_subparsers(title="command", required=True)

    ###
    # PACKAGE
    ###
    package_parser = subparsers.add_parser("package", help="Used to manage packages")
    pack_subparsers = package_parser.add_subparsers(title="subcommand", required=True)

    # init
    def create_package(args):
        while True:
            id = get_id("Enter package id: ")

            name = input("Enter package name: ").strip()
            if len(name) == 0:
                raise RuntimeError("Package must have a non-empty name")

            author = input("Enter package author: ").strip()
            description = input("Enter package description: ").strip()
            break

        package = Package(
            args.path,
            id,
            name,
            author,
            description,
            Version(1, 0, 0),
            [],
            args.supported_platform if len(args.supported_platform) > 0 else None,
        )
        package.validate()
        package.write_manifest()

    package_init_parser = pack_subparsers.add_parser(
        "init",
        help="Initialise a package folder by creating a manifest file (interactive)",
    )
    package_init_parser.add_argument(
        "path", help="The path to the package folder", type=pathlib.Path
    )
    package_init_parser.add_argument(
        "--supported_platform",
        help="Add a supported platform to the manifest",
        action="append",
        default=[],
        choices=valid_supported_platforms,
    )
    package_init_parser.set_defaults(func=create_package)

    # pack
    def pack_package(args):
        assert os.path.isdir(args.pkg_path)
        package = Package(args.pkg_path)
        package.pack(args.output_path, args.compression)

    package_pack_parser = pack_subparsers.add_parser(
        "pack", help="Pack a package folder into a kpkg file"
    )
    package_pack_parser.add_argument(
        "pkg_path", help="The path to the package folder", type=pathlib.Path
    )
    package_pack_parser.add_argument(
        "output_path", help="The folder to put the kpkg file in", type=pathlib.Path
    )
    package_pack_parser.add_argument(
        "--compression",
        help="The compression level (0-9) (defaults to 5)",
        type=int,
        default=5,
    )
    package_pack_parser.set_defaults(func=pack_package)

    # add dependency
    def package_add_dependency(args):
        assert os.path.isdir(args.pkg_path)
        package = Package(args.pkg_path)

        package.add_dependency(
            Dependency(
                get_id("Enter dependency id: "),
                get_version("Enter minimum version (inclusive): "),
                get_version("Enter maximum version (exclusive): "),
            )
        )

        package.write_manifest()

    package_add_dependency_parser = pack_subparsers.add_parser(
        "add_dependency", help="Pack a package folder into a kpkg file"
    )
    package_add_dependency_parser.add_argument(
        "pkg_path", help="The path to the package folder", type=pathlib.Path
    )
    package_add_dependency_parser.set_defaults(func=package_add_dependency)


    # remove dependency
    def package_remove_dependency(args):
        assert os.path.isdir(args.pkg_path)
        package = Package(args.pkg_path)

        package.remove_dependency(get_id("Enter dependency id: "))
        package.write_manifest()

    package_remove_dependency_parser = pack_subparsers.add_parser(
        "remove_dependency", help="Pack a package folder into a kpkg file"
    )
    package_remove_dependency_parser.add_argument(
        "pkg_path", help="The path to the package folder", type=pathlib.Path
    )
    package_remove_dependency_parser.set_defaults(func=package_remove_dependency)


    ###
    # REPO
    ###
    repo_parser = subparsers.add_parser("repo", help="Manage repositories")
    repo_subparsers = repo_parser.add_subparsers(title="subcommand", required=True)

    # repo init
    def repo_init(args):
        manifest_path = args.path
        if os.path.isdir(manifest_path):
            manifest_path = os.path.join(manifest_path, "manifest.json")

        id = get_id("Enter repository id: ")
        name = input("Enter repository name: ").strip()
        if len(name) == 0:
            raise RuntimeError("Repository must have a non-empty name")
        description = input("Enter repository description: ").strip()

        repo = Repo(manifest_path, id, name, description, [])
        repo.write_manifest()

    repo_init_parser = repo_subparsers.add_parser(
        "init",
        help="Initialise a repository folder by creating a manifest file (interactive)",
    )
    repo_init_parser.add_argument(
        "path",
        help="The path to the repository folder or manifest file",
        type=pathlib.Path,
    )
    repo_init_parser.set_defaults(func=repo_init)

    # add
    def repo_add(args):
        manifest_path = args.repo_path
        if os.path.isdir(manifest_path):
            manifest_path = os.path.join(manifest_path, "manifest.json")
        repo = Repo(manifest_path)

        repo.add_artifact(args.package_path)
        repo.write_manifest()

    repo_add_parser = repo_subparsers.add_parser(
        "add", help="Add an artifact to the repository"
    )
    repo_add_parser.add_argument(
        "repo_path", help="The path to the repository folder", type=pathlib.Path
    )
    repo_add_parser.add_argument(
        "package_path", help="The path to the package file", type=pathlib.Path
    )
    repo_add_parser.set_defaults(func=repo_add)

    # remove
    def repo_remove(args):
        manifest_path = args.repo_path
        if os.path.isdir(manifest_path):
            manifest_path = os.path.join(manifest_path, "manifest.json")
        repo = Repo(manifest_path)

        target_version = None
        if args.version != None:
            target_version = Version.decode(args.version.split("."))

        repo.remove_artifact(args.id, target_version, args.supported_platform)
        repo.write_manifest()

    repo_remove_parser = repo_subparsers.add_parser(
        "remove", help="Remove an artifact from the repository"
    )
    repo_remove_parser.add_argument(
        "repo_path", help="The path to the repository folder", type=pathlib.Path
    )
    repo_remove_parser.add_argument(
        "id", help="The id of the artifact to remove", type=str
    )
    repo_remove_parser.add_argument(
        "--version", help="The specific version to remove", type=str
    )
    repo_remove_parser.add_argument(
        "--supported_platform",
        help="The supported platform of the artifact to remove",
        action="append",
        default=[],
        choices=valid_supported_platforms,
    )
    repo_remove_parser.set_defaults(func=repo_remove)

    args = parser.parse_args()
    args.func(args)
