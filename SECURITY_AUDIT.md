# Final Pre-GitHub Audit

## Result

No embedded GW2 API key, GitHub token, AWS credential, private-key block, literal bearer credential, or credential-bearing URL was found in the uploaded working project or the main repository history.

## Important findings

1. The uploaded folder still contains Visual Studio/build artifacts (`.vs/`, `x64/`, `*.user`, PDB/object/log files). Several build files contain the local Windows user path. These are excluded from this GitHub-ready package.
2. The uploaded repository's current `origin` points to the Raidcore Nexus template repository, not an Unfinished Business repository. Do not blindly push the original folder using its current remote.
3. The uploaded repository still carries the template's Git history. This package intentionally contains no `.git` directory so a fresh public history can be created.
4. The current `entry.cpp` contains the hardened API-key handling: DPAPI storage, password input, no undo buffer, immediate/idle input wiping, no persistent `validatedApiKey` global, short-lived decrypted/request copies, and best-effort wiping of credential/header buffers.
5. The API credential file lives under Local AppData, not the project tree, and is covered by the strengthened `.gitignore` if copied accidentally.

## Privacy

No hard-coded personal Windows path was found in the source/project files that are included in this package. Absolute local paths were confined to generated build/Visual Studio artifacts that were removed.

## Package strategy

The dependency source needed by the Visual Studio project is vendored in minimal form so this snapshot does not depend on the original template's `.git` metadata or submodule state. Upstream license files are retained.

## Remaining manual check

A successful Windows `Release | x64` build and in-game API validation/restart/Forget-Key test should be completed before tagging a public release.
