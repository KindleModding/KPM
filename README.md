# KPM
KPM is a lightweight package manager for Kindles  

The contents of this file are licensed under the `CC0` license.

### hooks
The following scripts are all optional and can be placed at the _root_ of the package will be called under certain events:  
- install.sh - called once the package has been downloaded and extracted
- launch.sh - called if launched from a launcher (NOTE: Apps are expected to manage THEIR OWN SCRIPTLETS via `install.sh` & `uninstall.sh`)
- uninstall.sh - called before the package is deleted

All of these scripts will be executed via `sh` NOT `bash`


### Installation Procedure
App installation is handled by KPM as follows:
1. App is downloaded to `/mnt/us/kpm/tmp`
2. App is extracted to `/mnt/us/kpm/packages`
3. App is added to the package index database