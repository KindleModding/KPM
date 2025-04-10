# KPM TODO List

## Quality of Life Improvements

- ✅ Add a `search` command to find packages by name or description
- ✅ Add a `list` command to show all available packages
- ✅ Add `list-installed` command to show currently installed packages
- ✅ Implement package removal functionality (`remove` or `uninstall` command)
- ✅ Add package upgrade functionality to update installed packages
- Implement version pinning to prevent accidental package upgrades
- Add package signing/verification for security
- Add a `verify` command to check package integrity
- ✅ Fix <= version comparison bug in parsePackageTarget() function (currently sets GTEQ instead of LTEQ)
- ✅ Add better error handling for network failures during downloads
- Add progress indicators for long-running operations (downloads, installations)
- ✅ Implement package caching to avoid re-downloading unchanged packages
- Add user-friendly output formatting options (JSON, table format)
- Create a basic configuration file for default settings
- Add documentation on package format and repository structure

## Future Enhancements

- Create a simple GUI frontend for the CLI tool
- Add auto-update functionality for KPM itself
- Implement conflict detection between incompatible packages
- Support for package groups/collections
