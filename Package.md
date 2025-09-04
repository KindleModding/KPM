# KPKG

A KPM package is defined as a `gzipped tar file` with the `kpkg` extension.  

The file `must` have a `manifest.json` in its root.

## Manifest
```json
{
    "id": "koreader",
    "name": "KOReader",
    "author": "KOReader Team",
    "description": "KOReader is a feature-rich eBook reader",
    "version": [ 1, 2, 0 ],
    "supported_arch": [ "armhf" ],
    "dependencies":
    {
        "fbink":
        {
            "repository": "org.kindlemodding.repo",
            "type": "<",
            "version": [ 3, 0, 0 ]
        }
    },
}
```

This manifest file defines a package with the id of `koreader` (it would installed with `kpm install koreader`).  
The package has a version of `1.2.0` and depends on `fbink` (any version below `3.0.0` specifically from the `org.kindlemodding.repo` repository.)  

### Manifest Spec
Strings should not contain non-ASCII chars.  

```
id - The id of the package, must be lowercase
name - The display name of the package
author - The author of the package
description - The description of the package
version - The version of the package
supported_arch - List of supported architectures MUST BE a list containing only `armel`, `armhf` or any combination of the two
supported_devices - OPTIONAL, List of supported Kindle device codes as integers
dependencies - A list of package dependencies
```

### Dependencies
Dependencies are stored with an object, where the key is the id of the dependency (ie: `fbink`).

```
repository - OPTIONAL, specify a specific repository ID to obtain this package, avoid using this if you can
type - OPTIONAL, The type of dependency version comparison, can be `<`, `>`, `=`, `<=` or `>=`
version - The version to compare, required only if `type` is specified and vice-versa
```

## Hooks
Script files can be included in the root of the package file and will be run under certain events based on their name.  
All scripts must run under `sh`.  

```
install.sh - Ran when the package is installed
uninstall.sh - Ran when the package is being uninstalled
launch.sh - Ran when the package is launched
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