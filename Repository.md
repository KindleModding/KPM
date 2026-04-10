# KPM Repository

A KPM repository is a collection of packages statically hosted on the internet.

A repository requires a json file to define its manifest.

### Repository Structure
Note: You should not be managing a repository manually, instead use the `krm.py` script provided with `KPM`

```
- manifest.json
- packages
    - com.kindlemodding.example
        - icon.png
        - artifacts
            - com.kindlemodding.example_1.2.3_kindlepw2.kpkg
            - com.kindlemodding.example_1.2.3_kindlehf.kpkg
- screenshots
    - com.kindlemodding.example
        - screenshots.json
        - 1.png
        - 2.png
        - 3.png
        - 4.png
```

### Repository `manifest.json`:  
```json
{
    "manifest_version": 1,
    "id": "com.kindlemodding.repo",
    "name": "KindleModding Repository",
    "description": "The official KindleModding repository",
    "packages":
    {
        "com.kindlemodding.example":
        {
            "name": "Example Package",
            "description": "KMC's example app is an example of all time",
            "author": "HackerDude",
            "artifacts":
            [
                {
                    "url": "packages/com.kindlemodding.example/kindlehf/package_v1.0.0.kpkg",
                    "depencencies": [],
                    "version": [ 1, 0, 0 ],
                    "supported_platform": [ "kindlehf" ],
                },
                {
                    "url": "packages/com.kindlemodding.example/kindlepw2/package_v1.0.0.kpkg",
                    "depencencies": [],
                    "version": [ 1, 0, 0 ],
                    "supported_platform": [ "kindlepw2" ]
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

## Dependency object
```
id - The id of the package
repository - String ID of the repo - can be omitted or null
type - "<=" or ">=" or "<" or ">" or "=" (can be omitted or null)
version - semver object (can be omitted or null if type is omitted)
```