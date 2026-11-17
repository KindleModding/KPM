###
# The contents of this file are licensed under the `CC0` license.
###

from dataclasses import dataclass
from io import BytesIO
import json
import logging
import argparse
import pathlib
import os
import struct
from compression import zstd
import hashlib

COMPRESS_CHUNK_SIZE = (1024 ** 2) * 64

KPKG_VERSION = 3
SUPPORTED_PLATFORMS = {
    "kindle": 1 << 0,
    "kindle5": 1 << 1,
    "kindlepw2": 1 << 2,
    "kindlehf": 1 << 3
}

BLOCKED_IDS = [
    "juno",             # Kindle firmware
    "libbluetooth",     # 
    "btfd"              #
]

VALID_CATEGORIES = [
    "audiovideo",
    "audio",
    "video",
    "book",
    "development",
    "education",
    "health",
    "game",
    "graphics",
    "network",
    "office",
    "science",
    "settings",
    "system",
    "utility",
    "miscellaneous"
]

class ValidationWarning(Exception):
    def __init__(self, message, *args):
        self.message = message
        super().__init__(message, *args)

class ValidationException(Exception):
    def __init__(self, message, *args):
        self.message = message
        super().__init__(message, *args)

def validate_id(id: str):
    if (id in BLOCKED_IDS):
        raise ValidationException(f"ID must not be f{id} (reserved)")
    if (len(id) < 2):
        raise ValidationException("ID must contain at least 2 characters")
    if (not any( [ c.isalpha() for c in id ] )):
        raise ValidationException("ID must contain at least one letter")
    if (not all( [ c.isalnum() or c == '-' or c == '_' for c in id ] )):
        raise ValidationException(f"ID must only contain characters that are alphanumeric, '-', or '_' ")
    if (id[-1] == '-' or id[-1] == '_'):
        raise ValidationException(f"ID not end with '-', or '_' ")
    if (not id.islower()):
        raise ValidationException(f"ID must be lowercase")

def validate_category(category: str, allow_empty: bool = False):
    if len(category) == 0 and allow_empty:
        return
    if len(category) == 0:
        raise ValidationException("Category must not be empty")
    if not category.isalpha():
        raise ValidationException("Category must only contain letters")
    if not category.islower():
        raise ValidationException("Category must be lowercase")
    if not (category in VALID_CATEGORIES):
        raise ValidationWarning(f"Continuing anyway, but category should be one of {','.join(VALID_CATEGORIES)}")

@dataclass
class Version:
    SIZE = 4 * 3
    """Class that represents a semantic version"""
    major: int
    minor: int
    patch: int

    def encode(self):
        return struct.pack("<III", self.major, self.minor, self.patch)

    def decode(buffer: BytesIO):
        return Version(*struct.unpack("<III", buffer.read(Version.SIZE)))

    def encode_obj(self):
            return [ self.major, self.minor, self.patch ]
    
    def decode_obj(obj: list):
        return Version(obj[0], obj[1], obj[2])

    def __repr__(self):
        return f"{self.major}.{self.minor}.{self.patch}"

    def __str__(self):
        return self.__repr__()

    def __eq__(self, other):
        return (
            self.major == other.major and
            self.minor == other.minor and
            self.patch == other.patch
        )

    def __gt__(self, other):
        if self.major != other.major:
            return self.major > other.major
        if self.minor != other.minor:
            return self.minor > other.minor
        if self.patch != other.patch:
            return self.patch > other.patch
        return False

    def __lt__(self, other):
        if self.major != other.major:
            return self.major < other.major
        if self.minor != other.minor:
            return self.minor < other.minor
        if self.patch != other.patch:
            return self.patch < other.patch
        return False
        


@dataclass
class Dependency:
    SIZE = Version.SIZE * 2
    """Class that represents a dependency"""
    min_version: Version|None
    max_version: Version|None

    def encode_obj(self):
        dep = {}
        if self.min_version:
            dep["min"] = self.min_version.encode_obj()
        if self.max_version:
            dep["max"] = self.max_version.encode_obj()
        return dep

    def decode_obj(obj: dict):
        return Dependency(
            Version.decode_obj(obj["min"]) if not obj["min"] is None else None,
            Version.decode_obj(obj["max"]) if not obj["max"] is None else None
        )

@dataclass
class BinaryDependency:
    SIZE = 8 + Version.SIZE*2
    """Class that represents a dependency"""
    id_ptr: int
    min_version: Version
    max_version: Version

    def encode(self):
        min_version = self.min_version
        max_version = self.max_version

        if min_version is None:
            min_version = Version(0, 0, 0)
        if max_version is None:
            max_version = Version(4294967295, 4294967295, 4294967295)

        return struct.pack("<Q", self.id_ptr) + min_version.encode() + max_version.encode()

    def decode(buffer: BytesIO):
            dep = BinaryDependency(None, None, None)
            dep.id_ptr, = struct.unpack("<Q", buffer.read(8))
            dep.min_version = Version.decode(buffer)
            dep.max_version = Version.decode(buffer)
            return dep

@dataclass
class BinaryFileEntry:
    SIZE = 8 + 32 + 8 + 32 + 4 + 8 + 8
    """Class that represents a file entry"""
    path_ptr: int
    installed_checksum: bytes
    installed_size: int
    checksum: bytes
    file_mode: int
    offset: int
    size: int

    def decode(buffer: BytesIO):
        return BinaryFileEntry(*struct.unpack("<Q32sQ32sIQQ", buffer.read(BinaryFileEntry.SIZE)))

    def encode(self):
        return struct.pack("<Q32sQ32sIQQ", self.path_ptr, self.installed_checksum, self.installed_size, self.checksum, self.file_mode, self.offset, self.size)


@dataclass
class Package:
    """Class that represents a package"""
    id: str
    name: str
    author: str
    description: str
    category: str|None
    subcategory: str
    version: Version
    dependencies: dict[str, Dependency]
    supported_platforms: list[str]

    def log(self):
            logging.info("=== PACKAGE INFO ===")
            logging.info(f"ID:\t\t{self.id}")
            logging.info(f"Name:\t\t{self.name}")
            logging.info(f"Author:\t\t{self.author}")
            logging.info(f"Description:\t{self.description}")
            logging.info(f"Category:\t{self.category}")
            logging.info(f"Subcategory:\t{self.subcategory}")
            logging.info(f"Version:\t\t{self.version}")
            logging.info(f"Supports:\t{','.join(self.supported_platforms)}")
            if len(self.dependencies) != 0:
                logging.info(f"Depends on:")
                for dependency_id in self.dependencies:
                    logging.info(f"\t- {dependency_id} @ {self.dependencies[dependency_id].min_version} - {self.dependencies[dependency_id].max_version}")
            logging.info("")

    def write_manifest(self, package_path: str):
        with open(os.path.join(package_path, "manifest.json"), 'w') as file:
            manifest = {
                "manifest_version": KPKG_VERSION,
                "id": self.id,
                "name": self.name,
                "author": self.author,
                "description": self.description,
                "category": self.category
            }
            if not self.subcategory is None:
                manifest["subcategory"] = self.subcategory
            manifest["version"] = self.version.encode_obj()
            manifest["dependencies"] = { dep_id: self.dependencies[dep_id].encode_obj() for dep_id in self.dependencies }
            if self.supported_platforms:
                manifest["supported_platforms"] = self.supported_platforms
            file.write(json.dumps(manifest, indent=2))

    def read_manifest(package_path: str) -> Package:
        with open(os.path.join(package_path, "manifest.json"), 'r') as file:
            raw_manifest: dict = json.loads(file.read())

        return Package(
            raw_manifest["id"],
            raw_manifest["name"],
            raw_manifest["author"],
            raw_manifest["description"],
            raw_manifest["category"],
            raw_manifest.get("subcategory", None),
            Version.decode_obj(raw_manifest["version"]),
            { dep_id: Dependency.decode_obj(raw_manifest["dependencies"][dep_id]) for dep_id in raw_manifest["dependencies"] },
            raw_manifest.get("supported_platforms")
        )

    def pack(self, compression: int, in_path: str, out_file: BytesIO):
        validate_id(self.id)
        validate_category(self.category)
        try:
            if not self.subcategory is None:
                validate_category(self.subcategory, True)
        except ValidationWarning:
            pass

        strings: list[str] = []
        strings.extend([self.id, self.name, self.author, self.description, self.category, self.subcategory if not self.subcategory is None else ""])

        logging.debug("Walking package files")
        old_path = os.getcwd()
        os.chdir(in_path)
        walked_files: list[str] = []
        for root, dirs, files in os.walk("."):
            for file in files:
                logging.debug(f"- {file}")
                if file == "manifest.json":
                    continue
                walked_files.append(os.path.join(root, file).replace('./', ''))
        os.chdir(old_path)
        if len(walked_files) == 0:
            raise Exception("Package must contain at least one file besides manifest.json.")

        dependencies: list[BinaryDependency] = []
        str_off = len(b'\x00'.join([ string.encode() for string in strings ])) + 1
        for dep_id in self.dependencies:
            strings.append(dep_id)
            dependencies.append(BinaryDependency(
                str_off,
                self.dependencies[dep_id].min_version,
                self.dependencies[dep_id].max_version
            ))
            str_off += len(dep_id.encode()) + 1

        strings.extend(walked_files)

        string_section = b""
        for string in strings:
            if string == walked_files[0]:
                FILE_STR_OFF = len(string_section)
            string_section += string.encode() + b'\x00'

        out_file.write(b"KPKG")
        out_file.write(struct.pack("<B", KPKG_VERSION))
        out_file.write(self.version.encode())
        support_bitfield = 0 # All platforms
        if not self.supported_platforms is None:
            for i in [ SUPPORTED_PLATFORMS[plat] for plat in self.supported_platforms ]:
                support_bitfield |= i
        out_file.write(struct.pack("<HIIQ", support_bitfield, len(self.dependencies), len(walked_files), len(string_section)))

        for dep in dependencies:
            out_file.write(dep.encode())

        FILE_F_ENTRIES_OFF = out_file.tell()
        out_file.seek(BinaryFileEntry.SIZE * len(walked_files), os.SEEK_CUR) # Skip to write our string data

        out_file.write(string_section)
        del string_section

        FILE_F_SEC_OFF = out_file.tell()
        file_entries: list[BinaryFileEntry] = []
        file_entry_string_off = FILE_STR_OFF
        for i in range(len(walked_files)):
            file_off = out_file.tell() - FILE_F_SEC_OFF
            filename = walked_files[i]
            filepath = os.path.join(in_path, filename)

            compressed_size = 0
            compressed_digest = hashlib.sha256()
            og_digest = hashlib.sha256()
            with open(filepath, 'rb') as in_file:
                cmp = zstd.ZstdCompressor(compression)
                data = in_file.read(COMPRESS_CHUNK_SIZE)
                while True:
                    og_digest.update(data)
                    if len(data) > 0:
                        out = cmp.compress(data)
                    else:
                        out = cmp.flush()
                        
                    compressed_size += len(out)
                    compressed_digest.update(out)
                    out_file.write(out)

                    if len(data) == 0:
                        break

                    data = in_file.read(COMPRESS_CHUNK_SIZE)

            file_entries.append(BinaryFileEntry(
                file_entry_string_off,
                og_digest.digest(),
                os.stat(filepath).st_size,
                compressed_digest.digest(),
                os.stat(filepath).st_mode,
                file_off,
                compressed_size
            ))
            file_entry_string_off += len(filename.encode()) + 1

        # Finally write file entries
        out_file.seek(FILE_F_ENTRIES_OFF)
        for entry in file_entries:
            out_file.write(entry.encode())

    def __read_str(buf: bytes, off: int) -> bytes:
        return buf[off:buf.find(b'\0', off)]

    def __bitmap_to_supported(bitmap: int) -> list[str]:
        supported = []
        for key in SUPPORTED_PLATFORMS:
            if (bitmap & SUPPORTED_PLATFORMS[key]) == SUPPORTED_PLATFORMS[key]:
                supported.append(key)
        return supported

    def read_header(in_file: BytesIO) -> tuple[Package, list[BinaryFileEntry], bytes, hashlib._Hash]:
        in_file.seek(0)
        if in_file.read(4) != b'KPKG':
            raise Exception("File is not a KPKG file")

        kpkg_version, = struct.unpack("<B", in_file.read(1))
        if kpkg_version != KPKG_VERSION:
            raise Exception(f"Expected KPKG version {KPKG_VERSION}, got {kpkg_version}")

        package_version: Version = Version.decode(in_file)
        supported_bitmap, dependency_count, file_count, string_section_size = struct.unpack("<HIIQ", in_file.read(2 + 4 + 4 + 8))

        binary_deps: list[BinaryDependency] = []
        for i in range(dependency_count):
            binary_deps.append(BinaryDependency.decode(in_file))

        file_entries: list[BinaryFileEntry] = []
        for i in range(file_count):
            file_entries.append(BinaryFileEntry.decode(in_file))

        string_buf = in_file.read(string_section_size)

        dependencies: dict[str, Dependency] = {}
        for dep in binary_deps:
            dependencies[Package.__read_str(string_buf, dep.id_ptr).decode()] = Dependency( dep.min_version, dep.max_version )

        # Compute header digest
        digest = hashlib.sha256()
        end = in_file.tell()
        in_file.seek(0)
        while in_file.tell() < end:
            digest.update(in_file.read(min(8192, end - in_file.tell())))

        id: bytes = Package.__read_str(string_buf, 0)
        name: bytes = Package.__read_str(string_buf, len(id) + 1)
        author: bytes = Package.__read_str(string_buf, len(id) + 1 + len(name) + 1)
        description: bytes = Package.__read_str(string_buf, len(id) + 1 + len(name) + 1 + len(author) + 1)
        category: bytes = Package.__read_str(string_buf, len(id) + 1 + len(name) + 1 + len(author) + 1 + len(description) + 1)
        subcategory: bytes = Package.__read_str(string_buf, len(id) + 1 + len(name) + 1 + len(author) + 1 + len(description) + 1 + len(category) + 1)
        return (
            Package(
                id.decode(),
                name.decode(),
                author.decode(),
                description.decode(),
                category.decode(),
                subcategory.decode(),
                package_version,
                dependencies,
                Package.__bitmap_to_supported(supported_bitmap)
            ),
            file_entries,
            string_buf,
            digest
        )

    def try_unpack(in_file: BytesIO, out_path: str, target_path: str = None) -> bool:
        package, file_entries, string_buf, hash = Package.read_header(in_file)
        unpacked = False
        file_section_start = in_file.tell()

        for i in range(len(file_entries)):
            file_entry: BinaryFileEntry = file_entries[i]
            size = -1
            if i < len(file_entries)-1:
                size = file_entries[i+1].offset - file_entry.offset

            entry_path = Package.__read_str(string_buf, file_entry.path_ptr).decode()
            if (not target_path is None) and entry_path != target_path:
                continue

            unpacked = True
            filepath = os.path.join(out_path, entry_path)
            logging.info(f"Unpacking '{entry_path}' ({file_entry.installed_checksum.hex()})")
            os.makedirs(os.path.dirname(filepath), exist_ok=True)

            current = 0
            in_file.seek(file_section_start + file_entry.offset)
            with open(filepath, 'wb') as file:
                d = zstd.ZstdDecompressor()
                while True:
                    chunk_size = min(COMPRESS_CHUNK_SIZE, size - current)

                    file.write(d.decompress(in_file.read(chunk_size)))
                    current += chunk_size

                    if current == size:
                        break

        if target_path is None or target_path == "manifest.json":
            package.write_manifest(out_path)
            unpacked = True
        return unpacked

@dataclass
class RepoDependency:
    id: str
    min: Version|None
    max: Version|None

    def encode_obj(self):
        encoded = {}
        if (self.min):
            encoded["min"] = self.min.encode_obj()
        if (self.max):
            encoded["max"] = self.max.encode_obj()

@dataclass
class RepoArtifact:
    url: str
    installed_size: int
    digest: str
    version: Version
    dependencies: list[RepoDependency]
    supported_platforms: list[str]

@dataclass
class RepoPackage:
    id: str
    name: str
    author: str
    description: str
    category: str
    subcategory: str|None
    icon: str|None
    artifacts: list[RepoArtifact]

    def encode_obj(self):
        encoded = {
            "name": self.name,
            "author": self.author,
            "description": self.description,
            "category": self.category
        }
        if (self.subcategory):
            encoded["subcategory"] = self.subcategory
        if (self.icon):
            encoded["icon"] = self.icon
        encoded["artifacts"] = {}

        for artifact in self.artifacts:
            encoded["artifacts"][artifact.url] = {
                "installed_size": artifact.installed_size,
                "digest": artifact.digest,
                "version": artifact.version.encode_obj(),
                "dependencies": {
                    dependency_id: artifact.dependencies[dependency_id].encode_obj() for dependency_id in artifact.dependencies
                },
                "supported_platforms": artifact.supported_platforms
            }
        return encoded

class ArtifactExistsException(Exception):
    def __init__(self, package_id: str, artifact: RepoArtifact, *args):
        super().__init__(*args)
        self.package_id = package_id
        self.artifact = artifact

@dataclass
class Repo:
    """ Repository class """
    repo_path: str
    version: int
    id: str
    name: str
    description: str
    packages: list[RepoPackage]

    def update_package_metadata(self, package_id):
        logging.debug(f"Updating package metadata for {package_id}")
        repo_package = None
        for package in self.packages:
            if package.id == package_id:
                repo_package = package
        if repo_package is None:
            raise Exception(f"Could not find package {package_id}")

        latest_artifact: RepoArtifact = None
        for artifact in repo_package.artifacts:
            if latest_artifact == None or artifact.version > latest_artifact.version:
                latest_artifact = artifact

        if latest_artifact is None:
            raise Exception(f"Could not find artifact for {package_id}")
        logging.debug(f"Got latest artifact: {latest_artifact.version}")

        artifact_path = latest_artifact.url
        if artifact_path[0] == '/':
            artifact_path = artifact_path[1:]

        with open(os.path.join(self.repo_path, artifact_path), 'rb') as file:
            package, _, _, _ = Package.read_header(file)

            package_path = os.path.join(self.repo_path, "packages", repo_package.id)
            if os.path.exists(os.path.join(package_path, "icon.png")):
                os.remove(os.path.join(package_path, "icon.png"))
            if Package.try_unpack(file, package_path, "icon.png"):
                repo_package.icon = f"/packages/{repo_package.id}/icon.png"

        repo_package.name = package.name
        repo_package.author = package.author
        repo_package.description = package.description
        repo_package.category = package.category
        repo_package.subcategory = package.subcategory

    def add_package(self, package: RepoPackage):
        self.packages.append(package)

    def add_from_file(self, in_file: BytesIO, overwrite: bool = False):
        package: Package
        file_entries: list[BinaryFileEntry]
        strings: bytes
        hash: hashlib._Hash

        package, file_entries, strings, hash = Package.read_header(in_file)
        header_hash = hash.hexdigest()

        repo_package = None
        for potential in self.packages:
            if potential.id == package.id:
                repo_package = potential

        if repo_package is None:
            self.packages.append(RepoPackage(
                package.id,
                package.name,
                package.author,
                package.description,
                package.category,
                package.subcategory,
                None,
                []
            ))
            repo_package = self.packages[-1]

        artifact_filename = f"{package.id}_{package.version}_{'_'.join(package.supported_platforms)}.kpkg"
        artifact = RepoArtifact(
            f"/packages/{repo_package.id}/artifacts/{artifact_filename}",
            sum([ entry.installed_size for entry in file_entries ]),
            header_hash,
            package.version,
            [
                RepoDependency(
                    dependency_id,
                    package.dependencies[dependency_id].min_version,
                    package.dependencies[dependency_id].max_version
                ) for dependency_id in package.dependencies
            ],
            package.supported_platforms
        )

        found = False
        for potential_artifact in repo_package.artifacts:
            if (potential_artifact.version == artifact.version):
                found = True

            for platform in potential_artifact.supported_platforms:
                if not platform in artifact.supported_platforms:
                    found = False
            
            for platform in artifact.supported_platforms:
                if not platform in potential_artifact.supported_platforms:
                    found = False

            if found:
                if overwrite:
                    repo_package.artifacts.remove(potential_artifact)
                    break
                else:
                    raise ArtifactExistsException(repo_package.id, artifact)
        repo_package.artifacts.append(artifact)

        dest_path = os.path.join(self.repo_path, "packages", repo_package.id, "artifacts", artifact_filename)
        if os.path.exists(dest_path):
            if not overwrite:
                raise ArtifactExistsException(repo_package.id, artifact)
            else:
                os.remove(dest_path)

        os.makedirs(os.path.join(self.repo_path, "packages", repo_package.id, "artifacts"), exist_ok=True)
        in_file.seek(0, os.SEEK_END)
        in_size = in_file.tell()
        in_file.seek(0, os.SEEK_SET)
        with open(dest_path, 'wb') as out_file:
            os.sendfile(out_file.fileno(), in_file.fileno(), 0, in_size)
            out_file.truncate(out_file.tell())

        self.update_package_metadata(package.id)
        return package

    def read_manifest(manifest_path: str):
        with open(manifest_path, 'r') as file:
            data = json.loads(file.read())

        repo = Repo(
            os.path.dirname(manifest_path),
            data["manifest_version"],
            data["id"],
            data["name"],
            data["description"],
            []
        )
        
        for package_id in data["packages"]:
            repo.add_package(RepoPackage(
                    package_id,
                    data["packages"][package_id]["name"],
                    data["packages"][package_id]["author"],
                    data["packages"][package_id]["description"],
                    data["packages"][package_id]["category"],
                    data["packages"][package_id].get("subcategory", None),
                    data["packages"][package_id].get("icon", None),
                    [
                        RepoArtifact(
                            url,
                            data["packages"][package_id]["artifacts"][url]["installed_size"],
                            data["packages"][package_id]["artifacts"][url]["digest"],
                            Version.decode_obj(data["packages"][package_id]["artifacts"][url]["version"]),
                            [
                                RepoDependency(
                                    dependency_id,
                                    Version.decode_obj(data["packages"][package_id]["artifacts"][url]["dependencies"]["min"]) if "min" in data["packages"][package_id]["artifacts"][url]["dependencies"] else None,
                                    Version.decode_obj(data["packages"][package_id]["artifacts"][url]["dependencies"]["max"]) if "max" in data["packages"][package_id]["artifacts"][url]["dependencies"] else None
                                ) for dependency_id in data["packages"][package_id]["artifacts"][url]["dependencies"]
                            ],
                            data["packages"][package_id]["artifacts"][url]["supported_platforms"],
                        )
                        for url in data["packages"][package_id]["artifacts"]
                    ]
                )
            )

        return repo

    def write_manifest(self, manifest_path: str):
        with open(manifest_path, 'w') as file:
            file.write(json.dumps({
                "manifest_version": KPKG_VERSION,
                "id": self.id,
                "name": self.name,
                "description": self.description,
                "packages": {
                    package.id : package.encode_obj() for package in self.packages
                }
            }))


if __name__ == "__main__":
    def get_id(prompt: str = "Enter id: "):
        while True:
            id = input(prompt).strip()
            try:
                validate_id(id)
            except ValidationException as e:
                logging.warning(e.message)
                continue
            return id

    def get_supported_platforms(prompt: str = "Enter a supported platform to add (leave blank to stop): "):
        supported_platforms = []
        while True:
            supported_platform = input(prompt).strip()
            if len(supported_platform) == 0:
                if (len(supported_platforms) == 0):
                    logging.warning(f"You must enter at least one supported version from: {','.join(SUPPORTED_PLATFORMS.keys())}")
                    continue
                else:
                    return supported_platforms
            if not supported_platform in SUPPORTED_PLATFORMS:
                logging.warning(f"Supported platform must be one of: {','.join(SUPPORTED_PLATFORMS.keys())}")
                continue
            supported_platforms.append(supported_platform)

    def get_version(prompt: str = "Enter version: "):
        while True:
            try:
                version_str = input(prompt).strip()
                if len(version_str) == 0:
                    return None
                else:
                    return Version.decode_obj([ int(v) for v in version_str.split(".") ])
            except Exception as e:
                if (type(e) is KeyboardInterrupt or type(e) is InterruptedError):
                    return
                logger.warning("Invalid version, please try again.")

    def get_category(prompt: str = "Enter category: ", allow_empty: bool = False):
        while True:
            category = input(prompt).strip()
            try:
                validate_category(category, allow_empty)
            except ValidationWarning as e:
                logging.warning(e.message)
            except ValidationException as e:
                logging.warning(e.message)
                continue
            if len(category) == 0:
                category = None
            return category

    parser = argparse.ArgumentParser(
        prog="KPM Helper",
        description="Kindle Package Manager Helper is used for a ton of stuff",
        epilog="v3.0.0 - Created by Hackerdude (https://hackerdude.tech)",
    )
    parser.add_argument("--verbose", action='store_true', default=False)
    subparsers = parser.add_subparsers(title="command", required=True)

    ###
    # PACKAGE
    ###
    package_parser = subparsers.add_parser("package", help="Used to manage packages")
    pack_subparsers = package_parser.add_subparsers(title="subcommand", required=True)

    # init
    def init_package(args):
        id = get_id("Enter package id: ")

        while True:
            name = input("Enter package name: ").strip()
            if len(name) != 0:
                break
            logger.warning("Package name must not be empty.")

        package = Package(
            id,
            name,
            input("Enter package author: ").strip(),
            input("Enter package description: ").strip(),
            get_category(),
            get_category("Enter subcategory: ", allow_empty=True),
            Version(1, 0, 0),
            {},
            get_supported_platforms()
        )
        os.makedirs(args.path, exist_ok=True)
        package.log()
        package.write_manifest(args.path)
        logging.info(f"Initialised package at {args.path}")

    package_init_parser = pack_subparsers.add_parser(
        "init",
        help="Initialise a package folder by creating a manifest file (interactive)",
    )
    package_init_parser.add_argument(
        "path", help="The path to the package folder", type=pathlib.Path
    )
    package_init_parser.set_defaults(func=init_package)

    # pack
    def pack_package(args):
        assert os.path.isdir(args.pkg_path)
        package: Package = Package.read_manifest(args.pkg_path) # @TODO: Output path stuff
        package.log()

        output_path = args.output_path
        if os.path.isdir(output_path):
            output_path = os.path.join(output_path, f"{package.id}_{package.version}_{'_'.join(package.supported_platforms)}.kpkg")

        logging.info("Packing...")
        with open(output_path, 'wb') as file:
            package.pack(args.compression, args.pkg_path, file)
        logging.info(f"Packed to {output_path}")

    package_pack_parser = pack_subparsers.add_parser(
        "pack", help="Pack a package folder into a kpkg file"
    )
    package_pack_parser.add_argument(
        "pkg_path", help="The path to the package folder", type=pathlib.Path
    )
    package_pack_parser.add_argument(
        "output_path", help="The path to the output kpkg file", type=pathlib.Path
    )
    package_pack_parser.add_argument(
        "--compression",
        help="The compression level (0-9) (defaults to 5)",
        type=int,
        default=5,
    )
    package_pack_parser.set_defaults(func=pack_package)

    # unpack
    def unpack_package(args):
        assert os.path.isfile(args.kpkg_path)
        with open(args.kpkg_path, 'rb') as file:
            Package.try_unpack(file, args.output_path)
        logging.info(f"Unpacked to {args.output_path}")

    package_unpack_parser = pack_subparsers.add_parser(
        "unpack", help="Extract a package"
    )
    package_unpack_parser.add_argument(
        "kpkg_path", help="The path to the package", type=pathlib.Path
    )
    package_unpack_parser.add_argument(
        "output_path", help="The path to extract to", type=pathlib.Path
    )
    package_unpack_parser.set_defaults(func=unpack_package)

    # add dependency
    def package_add_dependency(args):
        assert os.path.isdir(args.pkg_path)
        package: Package = Package.read_manifest(args.pkg_path)

        package.dependencies[get_id("Enter dependency id: ")] = Dependency(
            get_version("Enter minimum version (inclusive) or leave blank: "),
            get_version("Enter maximum version (exclusive) or leave blank: ")
        )

        package.log()
        package.write_manifest(args.pkg_path)
        logging.info(f"Package manifest updated.")

    package_add_dependency_parser = pack_subparsers.add_parser(
        "add_dependency", help="Pack a package folder into a kpkg file"
    )
    package_add_dependency_parser.add_argument(
        "pkg_path", help="The path to the package folder", type=pathlib.Path, default='./'
    )
    package_add_dependency_parser.set_defaults(func=package_add_dependency)


    # remove dependency
    def package_remove_dependency(args):
        assert os.path.isdir(args.pkg_path)
        package = Package(args.pkg_path)

        target_id = get_id("Enter dependency id: ")
        if not target_id in package.dependencies:
            logging.info(f"Dependency '{target_id}' not found in package!")
            return
        del package.dependencies[target_id]
        
        package.log()
        package.write_manifest(args.pkg_path)
        logging.info("Package manifest updated.")

    package_remove_dependency_parser = pack_subparsers.add_parser(
        "remove_dependency", help="Pack a package folder into a kpkg file"
    )
    package_remove_dependency_parser.add_argument(
        "pkg_path", help="The path to the package folder", type=pathlib.Path, default='./'
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

        repo = Repo(os.path.dirname(manifest_path), KPKG_VERSION, id, name, description, [])
        repo.write_manifest(manifest_path)
        logging.info(f"Initialised repository manifest at {manifest_path}")

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
        repo: Repo = Repo.read_manifest(manifest_path)

        try:
            with open(args.package_path, 'rb') as file:
                package = repo.add_from_file(file, args.force)
        except ArtifactExistsException as e:
            logging.error(f"{e.package_id} @ {e.artifact.version} : {','.join(e.artifact.supported_platforms)} already exists in repository")
            logging.error("Specify --force if you wish to overwrite this artifact")
            exit(1)

        repo.write_manifest(manifest_path)
        package.log()
        logging.info("Package added to repository.")

    repo_add_parser = repo_subparsers.add_parser(
        "add", help="Add an artifact to the repository"
    )
    repo_add_parser.add_argument(
        "repo_path", help="The path to the repository folder", type=pathlib.Path
    )
    repo_add_parser.add_argument(
        "package_path", help="The path to the package file", type=pathlib.Path
    )
    repo_add_parser.add_argument(
        "--force", help="Whether or not to overwrite an existing artifact", action='store_true', default=False
    )
    repo_add_parser.set_defaults(func=repo_add)

    # remove
    def repo_remove(args):
        manifest_path = args.repo_path
        if os.path.isdir(manifest_path):
            manifest_path = os.path.join(manifest_path, "manifest.json")
        repo: Repo = Repo.read_manifest(manifest_path)

        target_version = None
        if args.version != None:
            target_version = Version(
                int(args.version.split(".")[0]),
                int(args.version.split(".")[1]),
                int(args.version.split(".")[2])
            )

        for_removal: list[RepoArtifact] = []
        target_package = None
        for package in repo.packages:
            if package.id == args.id:
                target_package = package
                for artifact in package.artifacts:
                    should_remove_artifact = False
                    if target_version is None or artifact.version == target_version:
                        if len(args.supported_platform) == 0:
                            should_remove_artifact = True
                        else:
                            for supported_platform in args.supported_platform:
                                if supported_platform in artifact.supported_platforms:
                                    should_remove_artifact = True
                    if should_remove_artifact:
                        for_removal.append(artifact)
                break

        for artifact in for_removal:
            artifact_url = artifact.url
            if artifact_url[0] == '/':
                artifact_url = artifact_url[1:]
            artifact_path = os.path.join(repo.repo_path, artifact_url)
            logging.info(f"Removing artifact {target_package.id} @ {artifact.version} : {','.join(artifact.supported_platforms)}")
            if os.path.exists(artifact_path):
                os.remove(artifact_path)
            target_package.artifacts.remove(artifact)

        if (args.version is None and len(args.supported_platform) == 0) or len(target_package.artifacts) == 0:
            logging.info(f"Removing package {target_package.id}")
            repo.packages.remove(target_package)
        else:
            repo.update_package_metadata(target_package.id)
        repo.write_manifest(manifest_path)
        logging.info("Repository updated.")

    repo_remove_parser = repo_subparsers.add_parser(
        "remove", help="Remove a package or artifact from the repository"
    )
    repo_remove_parser.add_argument(
        "repo_path", help="The path to the repository folder", type=pathlib.Path
    )
    repo_remove_parser.add_argument(
        "id", help="The id of the package to remove", type=str
    )
    repo_remove_parser.add_argument(
        "--version", help="The specific version to remove", type=str
    )
    repo_remove_parser.add_argument(
        "--supported_platform",
        help="The supported platform of the artifact to remove",
        action="append",
        default=[],
        choices=list(SUPPORTED_PLATFORMS.keys()),
    )
    repo_remove_parser.set_defaults(func=repo_remove)

    args = parser.parse_args()
    if args.verbose:
        logging.basicConfig(format="[%(levelname)s] %(message)s", level=logging.DEBUG)
    else:
        logging.basicConfig(format="[%(levelname)s] %(message)s", level=logging.INFO)
    logger = logging.getLogger(__name__)
    args.func(args)
