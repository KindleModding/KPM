# KPKG

A KPM package is defined as a `lzma-compressed tar file` with the `kpkg` extension.  

The file `must` have a `manifest.json` in its root.

## Manifest
```json
{
    "manifest_version": 1,
    "id": "koreader",
    "name": "KOReader",
    "author": "KOReader Team",
    "description": "KOReader is a feature-rich eBook reader",
    "version": [ 1, 2, 0 ],
    "supported_arch": [ "armhf" ],
    "dependencies":
    [
        {
            "id": "fbink",
            "repository": "org.kindlemodding.repo",
            "type": "<",
            "version": [ 3, 0, 0 ]
        },
        {
            "id": "fbinput"
        }
    ]
}
```

This manifest file defines a package with the id of `koreader` (it would installed with `kpm install koreader`).  
The package has a version of `1.2.0` and depends on `fbink` (any version below `3.0.0` specifically from the `org.kindlemodding.repo` repository.)  

### Manifest Spec
Strings should not contain non-ASCII chars.  

```
manifest_version - The manifest format version (currently: 1)
id - The id of the package, must be lowercase
name - The display name of the package
author - The author of the package
description - The description of the package
version - The version of the package (ie: `v1.2.3` -> `[1, 2, 3]`)
supported_arch - List of supported architectures MUST BE a list containing only `armel`, `armhf` or any combination of the two
supported_devices - OPTIONAL, List of supported Kindle device codes as integers
dependencies - A list of package dependencies
```

### Dependencies

```
id - The id of the package
repository - String ID of the repo - can be omitted or null
type - "<=" or ">=" or "<" or ">" or "=" (can be omitted or null)
version - semver object (can be omitted or null if type is omitted)
```

## Hooks
Script files can be included in the root of the package file and will be run under certain events based on their name.  
All scripts must run under `sh` rather than `bash` and usage of `awk` is discouraged due to unreliability under some firmware versions.  
Hooks are run at the path in which they are placed - so you can reference relative paths in them

```
install.sh - Ran when the package is installed
uninstall.sh - Ran when the package is being uninstalled
launch.sh - Ran when the package is launched by a launcher
```

## Example Repository structure:
```
- manifest.json
- launch.sh
- app_binary
- app_files/
    - asset1.bin
    - asset2.bin
    - asset3.bin
```