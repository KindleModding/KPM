# KPM
KPM is a lightweight package manager for Kindles

### Repository Format
```
- index.json
- packages
    - com.kindlemodding.example
        - armhf
            - package_v1.0.0.kpkg
        - armel
            - package_v1.0.0.kpkg
- screenshots
    - com.kindlemodding.example
        - screenshots.json
        - 1.png
        - 2.png
        - 3.png
        - 4.png
```

index.json:  
```json
{
    "version": 1,
    "id": "com.kindlemodding.repo",
    "packages":
    {
        "com.kindlemodding.example":
        {
            "name": "Example Package",
            "description": "KMC's example app is an example of all time",
            "supported_arch": [ "armhf", "armel" ],
            "supported_kindle": [],
            "artifacts":
            [
                {
                    "path": "/packages/com.kindlemodding.example/armhf/package_v1.0.0.kpkg",
                    "version": [ 1, 0, 0 ],
                    "supported_arch": [ "armhf" ],
                    "supported_kindle": []
                },
                {
                    "path": "/packages/com.kindlemodding.example/armel/package_v1.0.0.kpkg",
                    "version": [ 1, 0, 0 ],
                    "supported_arch": [ "armel" ],
                    "supported_kindle": []
                }
            ]
        }
    }
}
```

Note: Screenshots are OPTIONAL but may be beneficial when graphical package managers such as `KindleForge` are used  
A `screenshots.json` file can be provided storing a list of every screenshot a package displays, ie:  
```json
[
    "1.png",
    "2.png",
    "3.png",
    "4.png"
]
```

### Package Format
Packages are tar files which may be gzipped.  
They have a flexible structure however a `manifest.json` file and an `install.sh` file is required.  

Example Package Format:
```
    - manifest.json
    - install.sh
    - app_binary
```

### manifest.json format
```json
{
    "version": 1,
    "id": "com.kindlemodding.example",
    "name": "Example Package",
    "description": "KMC's example app is an example app of all time",
    "version": [ 1, 0, 0 ],
    "supported_arch": [ "armhf" ],
    "supported_kindle": [ "" ],
    "entrypoint": "app_binary"
}
```

Explanation:
```
version - The manifest format version (should be 1)
id - The ID of the app, should be a reverse DNS format similar to Android packages and Java class paths
name - The display name of the app
description - A human-readalbe description of an app
version - The version number of an app in JSON-serialised semver format, IE: `v1.2.3 -> [ 1, 2, 3 ]`
supported_arch - A list of compatible architectures supported (currently just `armhf` and `armel`)
supported_kindle - OPTIONAL - A list of Kindles that this app supports (see `generation_nickname` field under `models.json`)
entrypoint - The command to run when this app is executed
actions - See menu actions section 
```

### hooks
The following scripts placed at the _root_ of the package will be called under certain events:  
- install.sh - called once the package has been downloaded and extracted
- uninstall.sh - called before the package is deleted

All of these scripts will be executed via `sh` NOT `bash`


### Installation Procedure
App installation is handled by KPM as follows:
1. App is downloaded to `/mnt/us/kpm/tmp`
2. App is extracted to `/mnt/us/kpm/packages`
3. App is added to the package index database