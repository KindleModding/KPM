# KPM Package Format
This is part of the KPM specification for `KPKG3`  
This package format is supported from KPM `v2.0.0` and above.

## Changelog
### v3
- Switch to zstd compression format
- Switch to custom packing format
- Make `supported_platforms` mandatory

### v2
- Switch to gzip compression format

### v1
- Initial package version

## Purpose of Document
This document describes the unpacked developer format for packages.  
For information on the `.kpkg` file format, see `KPKG Spec.drawio`

### Package Folder Structure
An example valid package's contents:
```
.
├── icon.png
├── install.sh
├── launch.sh
├── manifest.json
└── uninstall.sh
```

## Top-Level Files
A KPM package must have a `manifest.json` file at the top level.

The following file/folder names are reserved at the top-level of the archive and must only be used as described in this document:
### Manifest
- `manifest.json` (used only during packing process, not visible when package is installed)
### Hooks
- `install.sh`
- `uninstall.sh`
- `launch.sh`
- `startup.sh`
### Misc
- `icon.png` - Should be a 256x256 png image under 1MB - An *optional* icon to represent the package in GUIs
### Special Folders
- `rootfs/`

## Package Manifest Format
```json
{
  "manifest_version": 3,
  "id": "test_package",
  "name": "Test Package",
  "author": "Hackerdude",
  "description": "This is an example manifest file!",
  "version": [ 1, 0, 0 ],
  "dependencies": {
    "foobar": {
      "max": [1, 3, 0]
    }
  },
  "supported_platforms": [
    "kindlehf"
  ]
}
```

The package manifest is a JSON file containing a single object with the following fields:
|Field|Description|
|:---:|:---:|
| `manifest_version` | The manifest version, must be set to `3` |
| `id` | The id of the package, must be a lowercase alphanumeric string, may also contain ` -` and `_` symbols, must not be `null` |
| `name` | The human-readable name of the package, must not be `null` |
| `author` | The human-readable name of the package author, must not be `null` |
| `description` | The human-readable description of the package, must not be `null` |
| `version` | The KPM version array representing the version of this package, must not be `null` |
| `dependencies` | An object describing the dependencies of a package, must not be `null`, may be empty |
| `supported_platforms` | A list of supported platform strings, must not be empty or `null` |

### Version Arrays
KPM packages use version arrays to describe their version.  
The version array is an array which must contain 3 non-negative integer values.  
The version array is defined such that the first element in the array is the major version number, the second element in the array is the minor version number and the final element in the array is the patch version number.

For example: `[1, 0, 0]` is newer than `[0, 2, 3]` as the major version is larger.

### Dependency Definition
KPM packages may depend on other packages within a specific version range, this is defined under the `dependency` section.  
The key is the `id` of the dependency package, and the value is an object as such:

|Field|Description|
|:---:|:---:|
| `min` | Optional - The inclusive minimum version for the package to meet this dependency |
| `max` | Optional - The non-inclusive maximum version for the package to meet this dependency |

The `min` and `max` fields must be in the version array format.  
One or both of the `min` and `max` fields may be omitted, wherein that constraint will be ignored.

For example, the following dependency object defines a dependency on `foobar` where the installed version must be `1.3.0` or newer:
```json
{
    "foobar": {
        "min": [ 1, 3, 0 ]
    }
}
```

### Supported Platforms
The platform defines the device and OS architecture of whatever is running KPM. The following platforms are valid:  

|Platform|Description|
|:---:|:---:|
| `kindlehf` | Kindles with hard float support |
| `kindlepw2` | Kindles from the PW2 onwards (without hard float support) |
| `kindle5` | Kindles from the K5 onwards (without hard float support) |
| `kindle` | Legacy Kindle devices older than the K5 |

`supported_platforms` is optional, and when present must be an array containing one or more strings with values from the above table.

## Package Installation
Packages are installed by unpacking the contents of their `kpkg` file to `/mnt/us/kmc/kpm/packages/<packageid>/`.  
This unpacked data remains there for as long as the package is installed, it is deleted on uninstallation.

## Hooks
Hooks are used to define package behaviour, they are top-level files in the package archive with a specific name.  
All hooks are optional, however packages should provide a `launch.sh` hook.

|Filename|Description|
|:---:|:---:|
| `install.sh` | Installation hook |
| `uninstall.sh` | Uninstallation hook |
| `launch.sh` | Launch hook |
| `startup.sh` | Startup hook |

### Hook Execution
Hooks are executed by `sh`.  

|Filename|Execution|
|:---:|:---:|
| `install.sh` | Executed when installing the package after the package is unpacked |
| `uninstall.sh` | Executed when uninstalling the package before the package files are deleted |
| `launch.sh` | Executed by a launcher or scriptlet to launch the package |
| `startup.sh` | Executed when the device boots |

### Upgrades
When a package is upgraded, the following actions are performed:
- The `uninstall.sh` hook is run with the `upgrade` flag (ie: `sh uninstall.sh upgrade`)
- The package's files are deleted
- The new package's files are unpacked
- The `install.sh` hook is run is run with the `upgrade` flag (ie: `sh install.sh upgrade`)

Note that KPM handles file deletion for the unpacked package, the `uninstall.sh` hook should only delete files that are not part of the unpacked package itself.

### Persistent Data
If packages want to store data between upgrades, they **must not** store it in their unpacked package folder as it will be deleted during upgrades. It is recommended to store configuration and other such data under `/mnt/us/config/<packageid>/`.

`uninstall.sh` **must** delete this configuration data if it is run without the `upgrade` argument.

### Arguments
The `install.sh` and `uninstall.sh` hooks will be executed with an `upgrade` argument (accessible via `$1`) when the following operation is part of a package upgrade.

### Environment Variables
The following environment variables are available within hooks:
|Variable|Description|
|:---:|:---:|
| `$KPM_VERSION_MAJOR` | The major component of the version that the KPM runtime installing this package is on |
| `$KPM_VERSION_MINOR` | The minor component of the version that the KPM runtime installing this package is on |
| `$KPM_VERSION_PATCH` | The patch component of the version that the KPM runtime installing this package is on |
| `$KPM_PLATFORM` | The platform this package is being installed on |

### Hook Filepath
Hook scripts are run with the current working directory set to the unpacked package folder.  
For example, the hook `install.sh` would be able to access files `foo` and `bar` in the following package structure as `./foo` and `./bar`:
```
.
├── bar
├── foo
├── install.sh
└── manifest.json
```

### Scriptlets
It is recommented for scriptlets to be installed via the `install.sh` hook and removed via the `uninstall.sh` hook.

The `uninstall.sh` hook should check the scriptlet's hash against the package's scriptlet to ensure it is not deleting an unrelated scriptlet.

An example of this logic is available in the [reference package](https://github.com/KindleModding/example_kpm_package)

## Rootfs Overlays
The `rootfs` folder in the root of a package is used to build the `overlayfs`.

This is the only supported method of altering rootfs, packages **must** use the `rootfs` folder.

### Capabilities
The `rootfs` overlay system supports modifying the following folders:
```txt
/app
/bin
/data
/etc
/keys
/lib
/lost+found
/opt
/sbin
/system
/tts
/usr
```

On non-MTP devices, `rootfs` overlay files will have `0777` flags.
On MTP devices, they will have whatever flags the overlaying file has.

### Example
If the file `/rootfs/usr/bin/otav3` exists created in the package, when the Kindle boots, the file at `/usr/bin/otav3` will be replaced with it.  
This means when the Kindle accesses `/usr/bin/otav3`, it will actually be accessing `/rootfs/usr/bin/otav3` in the KPM package.

Note that overlays **do not** modify the underlying file, this makes it an incredibly safe method for modifying the rootfs.