# KPM Repository Format
This is part of the KPM specification for `REPOv3`
This repository format is supported from KPM `v2.0.0` and above.

## Changelog
### v3
- Add signed repo manifest support
- Add digest and size fields to artifacts
- Switch to custom package format
- Add `icon` field to packages

### v2
- Use gzip compression format for packages

### v1
- Initial repo version

## Sample
```json
{
  "manifest_version": 3,
  "id": "repo_id",
  "name": "Example Repository",
  "description": "An example repository",
  "pubkey": "/repo.pub",
  "signature": "/repo.sig",
  "packages": {
    "package1": {
      "name": "Package 1",
      "author": "example package",
      "description": "This is an example package",
      "icon": "/icons/package1.png",
      "artifacts": {
        "packages/package1/artifacts/package1_2026.3.0_kindlehf.kpkg": {
          "url": ,
          "installed_size": 1232,
          "digest": "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b",
          "version": [2026, 3, 0],
          "dependencies": {
            "fbink": {
                "min": [ 1, 2, 3 ]
            }
          },
          "supported_platforms": ["kindlehf"]
        },
        "packages/package1/artifacts/package1_2026.3.1_kindlehf.kpkg": {
          "installed_size": 6456,
          "digest": "7806e52121d9f4109a2f6fcab87a134a4a82a161a8497b4d9354d48e1dd5df19",
          "version": [2026, 3, 1],
          "dependencies": {
            "fbink": {}
          },
          "supported_platforms": ["kindlehf"]
        },
        "packages/package1/artifacts/package1_2026.7.2_kindlepw2.kpkg": {
          "installed_size": 21321,
          "digest": "0b7042b67724c0c6eb81aaae113136d6a632bcdff5a249ee4b319d77154958cc",
          "version": [2026, 7, 2],
          "dependencies": {
            "fbink": {}
          },
          "supported_platforms": ["kindlepw2"]
        }
      }
    },
    "package2": {
      "name": "Another eample package",
      "author": "Kindle Modding Community",
      "description": "idk!",
      "artifacts": {
        "packages/package2/artifacts/package2_1.2.3_kindlehf.kpkg": {
          "installed_size": 21433432,
          "digest": "7e114ae223216d9e346a6f43724601f016035b9745d3b7e145d57792bfc48c65",
          "version": [1, 2, 3],
          "dependencies": {},
          "supported_platforms": ["kindlehf"]
        },
        "packages/package2/artifacts/package2_1.2.3_kindlepw2.kpkg": {
          "installed_size": 29034832,
          "digest": "544a6f13c5bd3012b4c77ce6b0a72fb83ff5b240c6985f84c452ddf552d8d842",
          "version": [1, 2, 3],
          "dependencies": {},
          "supported_platforms": ["kindlepw2"]
        }
      }
    }
  }
}
```

## Keys
| Key                | Description                                                            |
|--------------------|------------------------------------------------------------------------|
| `manifest_version` | The repository format version (currently `3`) must be present          |
| `id`               | The repository id, must not be empty or `null`                         |
| `name`             | The human-readable repository name, must not be empty or `null`        |
| `description`      | The human-readable repository description, must not be empty or `null` |
| `pubkey`           | The URL to the public key*                                             |
| `signature`        | The URL to the manifest signature*                                     |
| `packages`         | The repository packages, must not be empty or `null`                   |

> *optional fields, both must either be present or omitted at the same time. Should be a relative path rather or a URL on the same domain.

## Naming
The repository manifest should be called `manifest.json`.  
The repository manifest should be accessible via `/manifest.json` of the repository web server.

## Signing
As of `v3` repositories can be signed via the following algorithms:
- `ed25519`
- `rsa` (up to `16384`)

It is recommended to use `ed25519` or `rsa 2048`.

## Packages
Each package is stored on the `packages` object, the `key` is the package id, and **must** match the artifact files.

| Key           | Description                                                         |
|---------------|---------------------------------------------------------------------|
| `name`        | The human-readable package name, must not be empty or `null`        |
| `author`      | The human-readable author name, must not be empty or `null`         |
| `description` | The human-readable package description, must not be empty or `null` |
| `icon`        | A URL to the package's icon*, may be omitted or `null`              |
| `artifacts`   | An object storing package artifacts, must contain at least one item |

> *Package icon should be a relative URL rather than an absolute one. Package icons should be hosted under the same domain as the package artifacts.

## Artifacts
Artifacts are stored in the `artifacts` object of packages.  
The key must be the URL to the artifact's `kpkg` file.

| Key                   | Description                                                                 |
|-----------------------|-----------------------------------------------------------------------------|
| `installed_size`      | The number of bytes that the full decompressed package's files will consume |
| `digest`              | The digest of the package header                                            |
| `version`             | The package's version (see package spec for version format)                 |
| `dependencies`        | The artifact's required dependencies (see package spec)                     |
| `supported_platforms` | The list of supported platforms (see package spec)                          |