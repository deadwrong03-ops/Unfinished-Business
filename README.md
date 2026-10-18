# Unfinished Business

A Guild Wars 2 Nexus addon that finds unfinished achievement opportunities relevant to your current map and helps surface useful next targets.

<img width="250" height="250" alt="Screenshot 2026-10-15 111958" src="https://github.com/user-attachments/assets/0ff70126-cea3-4617-a6f4-ac9c525021aa" />


<img width="250" height="250" alt="Screenshot 2026-10-15 112106" src="https://github.com/user-attachments/assets/a7b1a6f6-829b-4d16-96c2-a49341747e25" />



## Features

- Current-map achievement opportunity scanning
- Exact, curated, contextual, and limited coverage indicators
- Progress-aware recommendations
- Separate handling for account-tracked and character-specific Adventure Guide achievements
- Filters and ignored-achievement controls
- Achievement reward and actionability parsing
- Automatic account-progress refresh
- Optional NexusGameWiki integration
- Hero-panel fallback when NexusGameWiki is unavailable
- Local GW2 API-key storage protected with Windows DPAPI

## GW2 API Key

Unfinished Business requires a Guild Wars 2 API key with:

- `account`
- `progression`

The key is used only for authenticated requests to the official Guild Wars 2 API.

Saved keys are encrypted locally with Windows DPAPI for the current Windows user. The addon does not intentionally log or display the key after submission.

See [SECURITY.md](SECURITY.md) for additional details.

## NexusGameWiki Integration

When a compatible NexusGameWiki build is installed, Unfinished Business can open achievement wiki articles directly in game.

If NexusGameWiki is unavailable, the addon falls back to copying the achievement name and opening the Hero panel.

## Author

`spectre.9510`

## Status

Early development / testing build.

Current version: `0.1.0`
