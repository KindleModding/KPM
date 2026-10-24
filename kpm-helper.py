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

logging.basicConfig(format="[%(levelname)s] %(message)s", level=logging.INFO)
logger = logging.getLogger(__name__)

COMPRESS_CHUNK_SIZE = (1024 ** 2) * 64

KPKG_VERSION = 3
SUPPORTED_PLATFORMS = {
    "kindle": 1 << 0,
    "kindle5": 1 << 1,
    "kindlepw2": 1 << 2,
    "kindlehf": 1 << 3
}

valid_supported_platforms = [
    "kindle", "kindle5", "kindlepw2", "kindlehf"
]


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


@dataclass
class Dependency:
    SIZE = Version.SIZE * 2
    """Class that represents a dependency"""
    min_version: Version|None
    max_version: Version|None

    def encode_obj(self):
        return {
            "min": self.min_version.encode_obj(),
            "max": self.max_version.encode_obj()
        }

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
        return struct.pack("<Q", self.id_ptr) + self.min_version.encode() + self.max_version.encode()

    def decode(buffer: BytesIO):
            dep = BinaryDependency(None, None, None)
            dep.id_ptr, = struct.unpack("<Q", buffer.read(8))
            dep.min_version = Version.decode(buffer)
            dep.max_version = Version.decode(buffer)
            return dep

@dataclass
class BinaryFileEntry:
    SIZE = 8 + 32 + 4 + 8 + 32 + 8
    """Class that represents a file entry"""
    path_ptr: int
    checksum: bytes
    file_mode: int
    installed_size: int
    installed_checksum: bytes
    offset: int

    def decode(buffer: BytesIO):
        return BinaryFileEntry(*struct.unpack("<Q32sIQ32sQ", buffer.read(BinaryFileEntry.SIZE)))

    def encode(self):
        return struct.pack("<Q32sIQ32sQ", self.path_ptr, self.checksum, self.file_mode, self.installed_size, self.installed_checksum, self.offset)


@dataclass
class Package:
    """Class that represents a package"""
    id: str
    name: str
    author: str
    description: str
    version: Version
    dependencies: dict[str, Dependency]
    supported_platforms: list[str]|None

    def write_manifest(self, package_path: str):
        with open(os.path.join(package_path, "manifest.json"), 'w') as file:
            manifest = {
                "manifest_version": KPKG_VERSION,
                "id": self.id,
                "name": self.name,
                "author": self.author,
                "description": self.description,
                "version": self.version.encode_obj(),
                "dependencies": { dep_id: self.dependencies[dep_id].encode_obj() for dep_id in self.dependencies }
            }
            if self.supported_platforms:
                manifest["supported_platforms"] = self.supported_platforms
            file.write(json.dumps(manifest, indent=2))

    def read_manifest(package_path: str):
        with open(os.path.join(package_path, "manifest.json"), 'r') as file:
            raw_manifest: dict = json.loads(file.read())

        return Package(
            raw_manifest["id"],
            raw_manifest["name"],
            raw_manifest["author"],
            raw_manifest["description"],
            Version.decode_obj(raw_manifest["version"]),
            { dep_id: Dependency.decode_obj(raw_manifest["dependencies"][dep_id]) for dep_id in raw_manifest["dependencies"] },
            raw_manifest.get("supported_platforms")
        )

    def pack(self, compression: int, in_path: str, out_file: BytesIO):
        strings: list[str] = []
        strings.extend([self.id, self.name, self.author, self.description])

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

        FILE_SEC_OFF = out_file.tell()
        out_file.seek(BinaryFileEntry.SIZE * len(walked_files), os.SEEK_CUR) # Skip to write our string data

        out_file.write(string_section)
        del string_section

        file_entries: list[BinaryFileEntry] = []
        file_entry_string_off = FILE_STR_OFF
        for i in range(len(walked_files)):
            file_off = out_file.tell()
            filename = walked_files[i]
            filepath = os.path.join(in_path, filename)

            compressed_size = 0
            m = hashlib.sha256()
            n = hashlib.sha256()
            with open(filepath, 'rb') as in_file:
                cmp = zstd.ZstdCompressor(compression)
                data = in_file.read(COMPRESS_CHUNK_SIZE)
                while True:
                    n.update(data)
                    if len(data) > 0:
                        out = cmp.compress(data)
                    else:
                        out = cmp.flush()
                        
                    compressed_size += len(out)
                    m.update(out)
                    out_file.write(out)

                    if len(data) == 0:
                        break

                    data = in_file.read(COMPRESS_CHUNK_SIZE)

            file_entries.append(BinaryFileEntry(
                file_entry_string_off,
                m.digest(),
                os.stat(filepath).st_mode,
                os.stat(filepath).st_size,
                n.digest(),
                file_off
            ))
            file_entry_string_off += len(filename.encode()) + 1

        # Finally write file entries
        out_file.seek(FILE_SEC_OFF)
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

    def read_header(in_file: BytesIO) -> tuple[Package, list[BinaryFileEntry], bytes]:
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

        id: bytes = Package.__read_str(string_buf, 0)
        name: bytes = Package.__read_str(string_buf, len(id) + 1)
        author: bytes = Package.__read_str(string_buf, len(name) + 1)
        description: bytes = Package.__read_str(string_buf, len(author) + 1)
        return (
            Package(
                id.decode(),
                name.decode(),
                author.decode(),
                description.decode(),
                package_version,
                dependencies,
                Package.__bitmap_to_supported(supported_bitmap)
            ),
            file_entries,
            string_buf
        )

    def unpack(in_file: BytesIO, out_path: str):
        package, file_entries, string_buf = Package.read_header(in_file)

        for i in range(len(file_entries)):
            file_entry = file_entries[i]
            size = -1
            if i < len(file_entries)-1:
                size = file_entries[i+1] - file_entry.offset
            filepath = os.path.join(out_path, Package.__read_str(string_buf, file_entry.path_ptr).decode())
            logging.info(f"Unpacking '{filepath}' ({file_entry.installed_checksum.hex()})")
            os.makedirs(os.path.dirname(filepath), exist_ok=True)

            current = 0
            in_file.seek(file_entry.offset)
            with open(filepath, 'wb') as file:
                d = zstd.ZstdDecompressor()
                while True:
                    chunk_size = COMPRESS_CHUNK_SIZE
                    if current+chunk_size > size:
                        chunk_size = size - current

                    file.write(d.decompress(in_file.read(chunk_size)))
                    current += chunk_size

                    if current == size:
                        break

        package.write_manifest(out_path)
                
            


if __name__ == "__main__":
    def get_id(prompt: str = "Enter id: "):
        while True:
            id = input(prompt).strip()
            invalid = False
            for letter in id:
                if letter.isspace() or letter.isupper():
                    invalid = True
                if not (letter.isascii() or letter in ["-", "_"]):
                    invalid = True
            if invalid:
                logger.warning("id must only contain alphanumeric characters, or _ and -")
                continue
            return id

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

    parser = argparse.ArgumentParser(
        prog="KPM Helper",
        description="Kindle Package Manager Helper is used for a ton of stuff",
        epilog="v3.0.0 - Created by Hackerdude (https://hackerdude.tech)",
    )
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

        author = input("Enter package author: ").strip()
        description = input("Enter package description: ").strip()

        package = Package(
            id,
            name,
            author,
            description,
            Version(1, 0, 0),
            [],
            args.supported_platform if len(args.supported_platform) > 0 else None,
        )
        os.makedirs(args.path, exist_ok=True)
        package.write_manifest(args.path)

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
    package_init_parser.set_defaults(func=init_package)

    # pack
    def pack_package(args):
        assert os.path.isdir(args.pkg_path)
        package: Package = Package.read_manifest(args.pkg_path) # @TODO: Output path stuff
        with open(args.output_path, 'wb') as file:
            package.pack(args.compression, args.pkg_path, file)

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
            Package.unpack(file, args.output_path)

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

        package.dependencies.append(
            Dependency(
                get_id("Enter dependency id: "),
                get_version("Enter minimum version (inclusive): "),
                get_version("Enter maximum version (exclusive): "),
            )
        )

        package.write_manifest(args.pkg_path)

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
        package.write_manifest(args.pkg_path)

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
