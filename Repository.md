# KPM Repository

A KPM repository is a collection of packages statically hosted on the internet.

A repository requires a `manifest.json` file  

## Manifest
```json
{
    "id": "org.kindlemodding.repo",
    "name": "KindleModding Official Repository",
    "packages":
    [
        {
            "id": "koreader",
            "name": "KOReader",
            "author": "KOReader Team",
            "description": "KOReader is a feature-rich eBook reader",
            "artifacts":
            [
                {
                    "url": "/packages/artifacts/koreader-3.0.0-armel.kpkg",
                    "version": [ 1, 2, 0 ],
                    "dependencies": {
                        "fbink":
                        {
                            "repository": "org.kindlemodding.repo",
                            "type": "<",
                            "version": [ 3, 0, 0 ]
                        }
                    },
                    "supported_arch": [ "armel" ]
                },
                {
                    "url": "/packages/artifacts/koreader-3.0.0-armhf.kpkg",
                    "version": [ 1, 2, 0 ],
                    "dependencies": {
                        "fbink":
                        {
                            "repository": "org.kindlemodding.repo",
                            "type": "<",
                            "version": [ 3, 0, 0 ]
                        }
                    },
                    "supported_arch": [ "armhf" ]
                }
            ]
        }
    ]
}
```

## Repository Management
This `manifest.json` file should be generated using the included Python utilities.

See: `krm.py`