from dataclasses import dataclass
import shutil

from kpm_helper import Dependency, Package, Repo, Version
import os

to_generate = []

# Define our packages
class Generatable:
    def __init__(self, id: str, version: Version, dependencies: dict[str, Dependency] = None):
        self.id: str = id
        self.version: Version = version
        to_generate.append(self)
        self.dependencies: dict[str, Dependency] = {}
        if not dependencies is None:
            self.dependencies = dependencies

    def add_dependency(self, generatable: Generatable, min: Version|None = None, max: Version|None = None):
        self.dependencies[generatable.id] = Dependency(min, max)

target_package = Generatable("testpackage", Version(1, 0, 0))
cyclic_package = Generatable("cyclicpackage", Version(1, 0, 0))
cyclic_dep = Generatable("deppackage1", Version(1, 0, 0))
cyclic_dep2 = Generatable("deppackage2", Version(1, 0, 0))
cyclic_dep3 = Generatable("deppackage3", Version(1, 0, 0))
target_package.add_dependency(cyclic_package, Version(1, 0, 0))
cyclic_package.add_dependency(cyclic_dep, Version(1, 0, 0), Version(1, 0, 1))
cyclic_dep.add_dependency(cyclic_dep2, Version(1, 0, 0), Version(1, 0, 1))
cyclic_dep2.add_dependency(cyclic_dep3, Version(1, 0, 0), Version(1, 0, 1))
cyclic_dep3.add_dependency(cyclic_package, Version(1, 0, 0), Version(1, 0, 1))

package_nodep = Generatable("testpackage", Version(1, 1, 0), { "nonexistent": Dependency() })

test_package_2_0_0 = Generatable("testpackage", Version(2, 0, 0))
test_package_2_1_0 = Generatable("testpackage", Version(2, 1, 0))
test_package_2_2_0 = Generatable("testpackage", Version(2, 2, 0))
test_dep_1_0_0 = Generatable("testdep", Version(1, 0, 0))
test_package_3_0_0 = Generatable("testpackage", Version(3, 0, 0))
test_dep_1_5_0 = Generatable("testdep", Version(1, 5, 0))
test_package_2_2_0.add_dependency(test_dep_1_0_0)
test_package_3_0_0.add_dependency(test_dep_1_5_0)



dir = os.path.dirname(__file__)
src_dir = os.path.join(dir, "test_repo_src")
repo_dir = os.path.join(dir, "test_repo")
shutil.rmtree(src_dir, ignore_errors=True)
shutil.rmtree(repo_dir, ignore_errors=True)
os.makedirs(src_dir, exist_ok=True)
os.makedirs(repo_dir, exist_ok=True)

repo = Repo(
    os.path.join(dir, "test_repo"),
    3,
    "testrepo",
    "Test Repo",
    "Test Description",
    []
)

for gen in to_generate:
    print(f"Generating {gen.id}_{gen.version}")
    gen: Generatable
    package_dir = os.path.join(src_dir, f"{gen.id}_{gen.version}")
    os.makedirs(package_dir, exist_ok=True)
    package = Package(
        gen.id,
        f"{gen.id} Name",
        f"{gen.id} Author",
        f"{gen.id} Description",
        "book",
        None,
        gen.version,
        gen.dependencies,
        ["kindlehf", "kindlepw2"]
    )
    package.write_manifest(package_dir)

    with open(os.path.join(package_dir, "install.sh"), 'w') as file:
        file.write(f"#!/bin/sh\necho \"Hello from {gen.id} @ {gen.version} ($1)\"")
    with open(os.path.join(package_dir, "uninstall.sh"), 'w') as file:
        file.write(f"#!/bin/sh\necho \"Goodbye from {gen.id} @ {gen.version} ($1)\"")
    with open(os.path.join(package_dir, "launch.sh"), 'w') as file:
        file.write(f"#!/bin/sh\necho \"Launch from {gen.id} @ {gen.version} ($1)\"")
    with open(os.path.join(package_dir, "startup.sh"), 'w') as file:
        file.write(f"#!/bin/sh\necho \"Startup from {gen.id} @ {gen.version} ($1)\"")

    out_pkg_path = os.path.join(src_dir, f"{gen.id}_{gen.version}.kpkg")
    with open(out_pkg_path, 'w+b') as file:
        package.pack(5, package_dir, file)
        file.seek(0)
        repo.add_from_file(file, False)

for package in repo.packages:
    for artifact in package.artifacts:
        artifact.supported_platforms.append("x86_64")

repo.write_manifest(os.path.join(dir, "test_repo", "manifest.json"))