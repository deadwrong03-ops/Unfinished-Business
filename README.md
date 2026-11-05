# Unfinished Business

A Guild Wars 2 Nexus addon that discovers achievement opportunities relevant to your current map and helps you decide what to work on next.

<img width="200" height="200" alt="Screenshot 2026-11-04 233930" src="https://github.com/user-attachments/assets/662f2c20-8a5a-459a-ac70-e1640191ce89" />
<img width="200" height="200" alt="Screenshot 2026-11-05 004028" src="https://github.com/user-attachments/assets/26f02e2e-1fb9-44ba-b328-bf9267c4e128" />

<img width="200" height="200" alt="Screenshot 2026-11-04 234038" src="https://github.com/user-attachments/assets/f7b9884d-806a-45a6-a346-1b1e6ba0fb22" />
<img width="200" height="200" alt="Screenshot 2026-11-04 234104" src="https://github.com/user-attachments/assets/4943c991-18b5-47f6-9e5e-e73573e11d22" />
<img width="200" height="200" alt="Screenshot 2026-11-04 234152" src="https://github.com/user-attachments/assets/5f349b35-4b10-4424-aeb0-eddc45316368" />
<img width="200" height="200" alt="Screenshot 2026-11-04 234419" src="https://github.com/user-attachments/assets/edb6dee1-52b3-49b0-8505-3a4d1e1254f0" />



## Project Status

**Latest tested development checkpoint:** v0.2.0-rc1  
**Last tested:** November 4, 2026  
**Status:** Development / Testing

This is a tested development checkpoint, not a published v0.2.0 release.

## Features

### Achievement Discovery

- Automatic current-map achievement scanning
- Optional VPS-powered public achievement database
- Exact, curated, contextual, and regional map matching
- Account-progress-aware recommendations
- Separate handling of character-specific Adventure Guide achievements
- Achievement rewards, progress bars, and expandable details
- Filters, ignored achievements, and recommendation preferences
- Automatic account-progress refresh

### Achievement Availability

Achievements are categorized to help distinguish useful opportunities from misleading results:

- **Permanent Lead:** Likely ongoing achievement
- **Event-Dependent:** Requires a particular activity or event
- **Historical / Unavailable:** Hidden from normal recommendations
- **Unverified:** Current availability cannot be confirmed

Regional leads and character-specific achievements are displayed separately.

Map matches are suggestions, not proof that an achievement is currently obtainable or unfinished.

### Wizard's Vault: Daily + Weekly

- Daily and weekly objectives from the official Guild Wars 2 API
- Compact Daily | Weekly selector
- Individual objective cards with progress bars
- Daily and weekly meta-achievement tracking
- Astral Acclaim reward information
- Completed objectives grouped into collapsible sections
- Outstanding-objective notification badge
- Animated orange notification pulse
- Notification acknowledgment when opening the tab
- Manual 15-second animation preview
- Automatic progress refresh approximately every five minutes

### Masteries and Holidays

- Dedicated Masteries tab
- Mastery-point achievement filtering
- Holiday and festival achievement discovery
- Active festival detection
- Offline festival-date fallback when official calendar information is unavailable

### Interface and Navigation

- Colored tabs and icons
- Compact achievement cards
- Expandable achievement details
- Colored progress indicators
- Open Wiki, Copy Name, Copy + Hero, and Ignore controls
- Optional NexusGameWiki integration
- Hero-panel fallback when NexusGameWiki is unavailable

## VPS Achievement Index

Unfinished Business can connect to a separate Ubuntu VPS service that maintains a public Guild Wars 2 achievement index.

**Public API:**

`https://copperclaw-api.duckdns.org/ub/`

The service indexes public achievement and map information from the Guild Wars 2 API.

The VPS provides candidate achievements associated with the player's current map. The Windows addon checks progress locally.

**Privacy and security:**

- Guild Wars 2 API keys remain on the Windows computer.
- Account-specific achievement progress is not sent to the VPS.
- Only public map IDs are used for VPS achievement lookups.
- Connections use HTTPS.
- The local achievement scanner continues operating if the VPS is unavailable.

The VPS uses a separate service account, database, and automatic daily refresh timer.

## Guild Wars 2 API Key

Unfinished Business requires a Guild Wars 2 API key with the following permissions:

- `account`
- `progression`

Saved API keys are encrypted locally using Windows DPAPI.

The addon does not intentionally log or display saved API keys after submission.

See [SECURITY.md](SECURITY.md) for additional details.

## NexusGameWiki Integration

When a compatible NexusGameWiki build is installed, Unfinished Business can open achievement articles directly inside Guild Wars 2.

If NexusGameWiki is unavailable, the addon falls back to copying the achievement name and opening the Hero panel.

## Development Verification

The following features were tested during November 3–4, 2026:

- VPS achievement database initialization and nine automated tests
- HTTPS communication through Caddy
- Achievement candidate retrieval in Gendarran Fields and Queensdale
- Correct map-change detection and refreshed recommendations
- Daily and weekly objective retrieval
- Compact Daily | Weekly interface
- Notification badge and pulse animation preview
- Historical-achievement filtering
- Character-specific and regional achievement separation

The initial VPS database contained:

- 1,085 maps
- 8,357 achievements
- 33,799 achievement-location associations

These numbers represent the initial indexed snapshot and may change after subsequent refreshes.

### Remaining Testing

- Verify automatic VPS refresh completion
- Verify notification behavior after actual daily and weekly resets
- Test recommendation accuracy across additional maps
- Continue identifying historical and unavailable achievements
- Complete final release-build regression testing

## Development Workflow

1. Edit source code in Visual Studio.
2. Save and rebuild in Release | x64.
3. Test the addon in Guild Wars 2.
4. Record verified changes in DEVELOPMENT_LOG.md.
5. Update README.md when features or installation instructions change.
6. Commit tested changes to GitHub.
7. Create a GitHub release only after final verification.

See [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md) for development history.

## Previous Updates

### v0.1.2

- Redesigned achievement cards with progress bars and clearer summaries
- Added colored Achievements, Masteries, and Holiday tabs
- Added mastery-point achievement filtering
- Improved action-button styling
- Added festival-date fallback handling
- Improved readability and removed redundant progress information

## Author

**spectre.9510**

Developed for the Guild Wars 2 Nexus addon framework.
