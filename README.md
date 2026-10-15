# Unfinished Business

A Guild Wars 2 Nexus addon that finds unfinished achievement opportunities relevant to the player's current map and ranks useful next targets.

## Current features

- Current-map opportunity scanning
- Exact, curated, contextual, and limited coverage indicators
- Progress-aware recommendations
- Account-tracked and character-specific Adventure Guide handling
- Filters and ignored achievements
- Achievement reward and actionability parsing
- Optional NexusGameWiki integration with Hero-panel fallback
- Automatic account-progress refresh
- Local API-key storage protected with Windows DPAPI

## Building

1. Open `GW2Nexus-AddonTemplate.sln` in Visual Studio 2022.
2. Select `Release | x64`.
3. Build the solution.
4. The addon DLL is produced as `x64\\UnfinishedBusiness.dll`.

The Visual Studio project keeps the original Raidcore template filenames for now because that is the configuration used during development. The release DLL itself is named `UnfinishedBusiness.dll`.

## GW2 API key

Unfinished Business needs a Guild Wars 2 API key with only these permissions:

- `account`
- `progression`

The key is sent only to `https://api.guildwars2.com` using an HTTPS `Authorization: Bearer` header. It is not placed in a request URL.

Saved keys are encrypted using Windows DPAPI for the current Windows user and are stored outside the project directory under Local AppData. The source does not log or display the key after submission.

See [SECURITY.md](SECURITY.md) for the credential-handling details.

## NexusGameWiki

When a compatible NexusGameWiki build is installed, Unfinished Business raises the `NexusGameWiki.OpenPage.v2` event to open an achievement article in game. If the event is not handled, the addon falls back to copying the achievement name and opening the Hero panel.

## Dependencies

The minimal source files required from Raidcore's Nexus API, Raidcore's ImGui fork, and the Raidcore Mumble API are vendored under `src/` in this GitHub-ready snapshot. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Before pushing

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\\scripts\\check-secrets.ps1
```

You can also install the included pre-commit hook:

```powershell
powershell -ExecutionPolicy Bypass -File .\\scripts\\install-git-hooks.ps1
```

## Author

`spectre.9510`

## Status

Early development / testing build, version `0.1.0`.
