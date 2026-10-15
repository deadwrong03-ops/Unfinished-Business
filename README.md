# Unfinished Business

A Guild Wars 2 Nexus addon that finds unfinished achievement opportunities relevant to the player's current map and ranks useful next targets.

<img width="250" height="250" alt="Screenshot 2026-10-15 111958" src="https://github.com/user-attachments/assets/f41eefb9-8e3c-4f04-9d91-eeb284157851" />
<img width="250" height="250" alt="Screenshot 2026-10-15 112106" src="https://github.com/user-attachments/assets/3da36f10-54b8-4cb3-b459-be1bf8015c72" />


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

The minimal source files required from Raidcore's Nexus API, Raidcore's ImGui fork, and the Raidcore Mumble API are vendored under `src/` in this GitHub-ready snapshot.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

