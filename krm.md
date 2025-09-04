# Kindle Repository Manager

KRM is a script used to generate repository manifest files and add artifacts to repositories.

## Initialise a repository (interactive)
```bash
python krm.py --init ./examples/examplerepo/manifest.json
```

## Add an artifact to a repository
python krm.py --add ./example/examplepagkage.kpkg ./examples/examplerepo/manifest.json