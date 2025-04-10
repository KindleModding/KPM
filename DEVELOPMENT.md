# KPM Development Guide

## Table of Contents

1. [Project Overview](#project-overview)
2. [Repository Structure](#repository-structure)
3. [Architecture Overview](#architecture-overview)
4. [Development Environment Setup](#development-environment-setup)
5. [Build Instructions](#build-instructions)
6. [Testing](#testing)
7. [Package Format](#package-format)
8. [Repository Format](#repository-format)
9. [Command Line Interface](#command-line-interface)
10. [Development Workflow](#development-workflow)
11. [TODO List](#todo-list)

## Project Overview

Kindle Package Manager (KPM) is a package management system designed for Kindle e-readers. It provides a command-line interface for installing, managing, and updating software packages on Kindle devices.

### Background

Kindle devices have limited software management capabilities out of the box. KPM aims to solve this by providing:

- A centralized way to discover and install software packages
- Dependency resolution to manage package requirements
- A repository system to host and distribute packages
- Support for multiple Kindle architectures (kindlepw2, kindlehf)

### Project Goals

- Create an easy-to-use CLI tool for package management
- Maintain a lightweight, understandable codebase
- Support repositories hosted on static sites (e.g., GitHub Pages)
- Make package creation accessible for the Kindle modding community

## Repository Structure

```
KPM/
├── .github/              # GitHub CI workflows
├── assets/               # Static assets (e.g., repo definitions)
├── src/                  # Source code
│   ├── includes/         # Header files
│   └── meson.build       # Build configuration for source code
├── stubs/                # Stub implementations for development
│   └── lab126utils/      # Stubs for Kindle-specific libraries
├── subprojects/          # External dependencies (meson wraps)
├── test_package/         # Example test package
├── test_repo/            # Example test repository
├── .clangd               # Clang configuration
├── .gitignore            # Git ignore rules
├── LICENSE               # GPL-3.0 license
├── README.md             # Basic project information
├── TODO.md               # Todo list for the project
├── kpbuild.py            # Tool to build .kpkg packages
├── kprepoman.py          # Tool to manage package repositories
├── meson.build           # Main build configuration
└── test.sh               # Test script for KPM functionality
```

## Architecture Overview

KPM follows a modular design with several key components:

### Core Components

1. **Package Management**: Handles the installation, updating, and removal of packages
2. **Repository Management**: Manages package repositories, including adding, removing, and updating
3. **Dependency Resolution**: Resolves package dependencies to ensure proper installation order
4. **Database Management**: Maintains a local SQLite database of packages, repositories, and installations
5. **Networking**: Handles HTTP requests for repo and package fetching using libcurl
6. **Package Archive Handling**: Processes .kpkg files with libarchive

### Key Classes and Files

- `kpm.cpp`: Main entry point and command-line interface
- `database.cpp/hpp`: Database interface for package and repository metadata
- `repositories.cpp/hpp`: Repository handling functions
- `utils.cpp/hpp`: Utility functions including version comparison
- `simpleGET.cpp/hpp`: HTTP request wrapper for libcurl (single files)
- `multiDownload.cpp/hpp`: HTTP request wrapper for multiple concurrent downloads
- `flags.cpp/hpp`: Command-line flag parsing and global settings

## Development Environment Setup

### Prerequisites

- C++ compiler with C++17 support
- Meson build system (v0.56.0 or newer)
- libcurl development libraries
- libarchive development libraries
- SQLite development libraries

For cross-compilation for Kindle devices:

- KindleModding koxtoolchain (kindlepw2 and/or kindlehf)
- kindle-sdk

### Setting up a Development Environment

1. Clone the repository:

```bash
git clone https://github.com/KindleModding/KPM.git
cd KPM
```

2. Install dependencies (Ubuntu/Debian):

```bash
sudo apt-get install meson libcurl4-openssl-dev libarchive-dev zlib1g-dev libsqlite3-dev
```

3. For Kindle cross-compilation, follow these additional steps:
   - Download and extract the toolchain:
   ```bash
   wget https://github.com/KindleModding/koxtoolchain/releases/latest/download/kindlepw2.tar.gz
   tar -xzf kindlepw2.tar.gz -C ~
   ```
   - Clone and set up the kindle-sdk:
   ```bash
   git clone --recurse-submodules https://github.com/KindleModding/kindle-sdk.git
   cd kindle-sdk
   sh ./gen-sdk.sh kindlepw2
   cd ..
   ```

## Build Instructions

### Building for Development (Local Architecture)

```bash
meson setup builddir
meson compile -C builddir
```

### Cross-compiling for Kindle PW2

```bash
meson setup --cross-file ~/x-tools/arm-kindlepw2-linux-gnueabi/meson-crosscompile.txt builddir_kindlepw2
meson compile -C builddir_kindlepw2
```

### Cross-compiling for Kindle HF (newer models)

```bash
meson setup --cross-file ~/x-tools/arm-kindlehf-linux-gnueabihf/meson-crosscompile.txt builddir_kindlehf
meson compile -C builddir_kindlehf
```

## Testing

KPM includes tools for testing package management operations:

### Creating a Test Package

1. Create a package directory with required files:

```bash
mkdir -p test_package/screenshots
```

2. Create a `manifest.json` file:

```json
{
  "id": "com.test.helloworld",
  "display_name": "Hello World",
  "alias": "helloworld",
  "description": "A simple test package for KPM",
  "version_name": "1.0",
  "version_number": 100,
  "min_firmware": "5.0.0",
  "max_firmware": "",
  "architecture": "kindlepw2",
  "dependencies": [],
  "install": "install.sh"
}
```

3. Create an install script (`install.sh`):

```bash
#!/bin/sh
echo "Hello from the test package installer!"
mkdir -p /tmp/hello-world
echo "Hello World!" > /tmp/hello-world/hello.txt
```

4. Add an icon (required):

```bash
# Create an empty placeholder or add a real icon
touch test_package/icon.png
```

5. Build the package:

```bash
python kpbuild.py test_package
```

### Setting up a Test Repository

1. Create and initialize a repository:

```bash
mkdir -p test_repo
python kprepoman.py test_repo init
```

2. Add a package to the repository:

```bash
python kprepoman.py test_repo add_package com.test.helloworld_1.0_kindlepw2.kpkg
```

### Testing KPM Operations

When testing KPM on your development machine (which is likely different from a Kindle device), you'll need to use the `--force_architecture` and `--force_firmware` flags to match the package requirements:

```bash
# Add repository
./builddir/src/kpm -v --kpm_dir "./builddir/src/kpm_folder" --cache_dir "./builddir/src/kpm_cache" add-repo file://$(pwd)/test_repo

# Update repository index
./builddir/src/kpm -v --kpm_dir "./builddir/src/kpm_folder" --cache_dir "./builddir/src/kpm_cache" update

# Install package (with architecture and firmware overrides)
./builddir/src/kpm -v --kpm_dir "./builddir/src/kpm_folder" --cache_dir "./builddir/src/kpm_cache" --force_architecture "kindlepw2" --force_firmware "5.0.0" install com.test.helloworld
```

The `--force_architecture` flag is essential when testing packages built for Kindle devices on a different architecture.
The `--force_firmware` flag is needed to satisfy firmware version requirements in the package manifest.

These flags allow you to test KPM functionality on your development machine without having to build and deploy to a real Kindle device for every code change.

## Package Format

KPM packages (.kpkg) are gzipped tar archives with a specific structure:

```
package_name.kpkg/
├── manifest.json     # Package metadata (required)
├── icon.png          # Package icon (required)
├── install.sh        # Installation script (optional)
├── uninstall.sh      # Uninstallation script (optional)
├── screenshots/      # Screenshots for the package (optional)
└── ...               # Other package files
```

### Package Manifest Format

```json
{
  "id": "com.example.package", // Package ID (unique)
  "alias": "package", // Short name (optional)
  "display_name": "Example Package",
  "description": "An example package",
  "version_name": "1.0", // Human-readable version
  "version_number": 100, // Integer version for comparisons
  "min_firmware": "5.0.0", // Minimum compatible firmware
  "max_firmware": "", // Maximum compatible firmware (empty = no limit)
  "architecture": "kindlepw2", // Target architecture
  "dependencies": [
    // Package dependencies
    "com.example.dependency>=1.0"
  ],
  "install": "install.sh", // Installation script (optional)
  "uninstall": "uninstall.sh" // Uninstallation script (optional)
}
```

### Dependency Format

Dependencies follow this format: `[repository_id/]package_name[<=>version]`

Examples:

- `com.example.package`: Any version
- `repo.id/com.example.package`: Any version from specific repo
- `com.example.package=1.0`: Exact version
- `com.example.package>=1.0`: Version 1.0 or newer
- `com.example.package<=1.0`: Version 1.0 or older

## Repository Format

A KPM repository has the following structure:

```
repository/
├── manifest.json                                  # Repository manifest
└── packages/
    └── com.example.package/                       # Package directory
        ├── icon.png                               # Package icon
        ├── screenshots/                           # Screenshots directory
        │   ├── 0.png
        │   └── ...
        └── 1.0/                                   # Version directory
            └── com.example.package_1.0_kindlepw2.kpkg  # Package file
```

### Repository Manifest Format

```json
{
  "version": 1, // Repository format version
  "id": "com.example.repo", // Repository ID
  "packages": [
    {
      "id": "com.example.package",
      "alias": "package",
      "display_name": "Example Package",
      "description": "An example package",
      "screenshots": 2, // Number of screenshots
      "versions": [
        {
          "version_name": "1.0",
          "version_number": 100,
          "min_firmware": "5.0.0",
          "max_firmware": "",
          "dependencies": ["com.example.dependency>=1.0"],
          "architecture": "kindlepw2"
        }
      ]
    }
  ]
}
```

## Command Line Interface

KPM offers the following command-line operations:

```
kpm [flags] <operation> [targets]
```

### Operations

- `update`: Update package repositories
- `add-repo <url>`: Add a repository
- `remove-repo <id>`: Remove a repository
- `list-repos`: List repositories
- `install <package>`: Install package(s)

### Flags

- `-v`: Enable verbose output
- `--kpm_dir <path>`: Set KPM data directory (default: /var/local/kpm)
- `--cache_dir <path>`: Set cache directory (default: /var/cache/kpm)
- `--force_architecture <arch>`: Override architecture detection
- `--force_firmware <version>`: Override firmware version detection

## Development Workflow

1. **Start with an Issue**: Identify a feature or bug to work on from the TODO list
2. **Set Up Environment**: Follow the setup and build instructions
3. **Implement Changes**: Modify the code to address the issue
4. **Test**:
   - Create test packages and repositories using `kpbuild.py` and `kprepoman.py`
   - Test KPM operations with your changes
5. **Build for All Targets**: Ensure your changes work on all supported architectures

### Coding Style

- Follow the existing style of the codebase
- Use C++17 features where appropriate
- Add comments for complex logic
- Update relevant documentation when adding new features

## TODO List

### Quality of Life Improvements

- Add a `search` command to find packages by name or description
- Add a `list` command to show all available packages
- Add `list-installed` command to show currently installed packages
- Implement package removal functionality (`remove` or `uninstall` command)
- Add package upgrade functionality to update installed packages
- Implement version pinning to prevent accidental package upgrades
- Add package signing/verification for security
- Add a `verify` command to check package integrity
- Fix <= version comparison bug in parsePackageTarget() function (currently sets GTEQ instead of LTEQ)
- Add better error handling for network failures during downloads
- Add progress indicators for long-running operations (downloads, installations)
- Implement package caching to avoid re-downloading unchanged packages
- Add user-friendly output formatting options (JSON, table format)
- Create a basic configuration file for default settings
- Add documentation on package format and repository structure

### Future Enhancements

- Create a simple GUI frontend for the CLI tool
- Add auto-update functionality for KPM itself
- Implement conflict detection between incompatible packages
- Support for package groups/collections
