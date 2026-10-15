# Security

## API-key permissions

Use a Guild Wars 2 API key with only `account` and `progression` permissions.

## Storage

The addon protects saved credentials with Windows DPAPI using the current Windows user's security context. The encrypted credential is stored at:

`%LOCALAPPDATA%\\UnfinishedBusiness\\api_key.dat`

That file is outside this repository and is explicitly ignored if it is ever copied into the project accidentally.

## UI handling

- API-key entry uses password mode.
- ImGui undo/redo storage is disabled for the credential field.
- The input buffer is wiped immediately after submission.
- An unsubmitted key is wiped after 120 seconds of inactivity.
- The key and its user-assigned API-key name are not shown in addon status text.
- `Forget Key` deletes the saved encrypted file and clears explicit in-memory credential buffers.

## Memory handling

Plaintext credentials must exist briefly in process memory to authenticate. The addon minimizes explicit copies and performs best-effort wiping of buffers it controls, including decrypted DPAPI output and temporary authorization-header strings.

No native application can guarantee that a credential never exists transiently in memory because the C++ runtime, allocator, operating system, networking libraries, debuggers, or crash-dump mechanisms may create copies outside the addon's control.

## Network handling

Authenticated requests go to `api.guildwars2.com` over HTTPS. The key is supplied as an `Authorization: Bearer` header and is not placed in the URL.

## Repository hygiene

Before every public push, run `scripts\\check-secrets.ps1` and inspect `git status` / `git diff --cached`.

If a real API key or token is ever committed, treat it as compromised even if the latest commit is later deleted. Revoke it first, then clean the repository history before publishing again.
