# Unfinished Business - Development Log

## v0.2.0-rc1 - November 4, 2026

**Status:** Tested development checkpoint  
**Build:** v0.2.0 VPS TEST  
**Release status:** Not yet published

### VPS Achievement Integration

- Added optional VPS-powered achievement discovery.
- Built an SQLite achievement database from public Guild Wars 2 API information.
- Indexed 1,085 maps, 8,357 achievements, and 33,799 achievement-location associations.
- Installed the achievement API on an Ubuntu VPS.
- Configured a dedicated systemd service and automatic daily refresh timer.
- Integrated HTTPS routing through the existing Caddy server.
- Preserved Copper & Claw and Roundtable services.
- Added asynchronous achievement lookups to the Windows addon.
- Added source and confidence labels for VPS achievement candidates.
- Preserved the existing local scanner as a fallback.
- Kept GW2 API keys and account-progress information on the Windows client.

**Testing:**
- All 9 VPS indexing tests passed.
- HTTPS API endpoints returned HTTP 200.
- Verified achievement retrieval in Gendarran Fields.
- Verified achievement retrieval in Queensdale.
- Confirmed map changes refresh achievement recommendations.
- Confirmed the VPS connection works inside GW2.

### Wizard's Vault: Daily + Weekly

- Added account-specific daily and weekly objective tracking.
- Integrated official GW2 Wizard's Vault API information.
- Added daily and weekly objective progress.
- Added meta-objective completion tracking.
- Added Astral Acclaim reward information.
- Added notification badges showing outstanding objectives.
- Added an animated orange tab notification.
- Added a 15-second notification pulse preview.
- Added notification acknowledgment when viewing the tab.

**Interface improvements:**
- Redesigned objectives using achievement-style cards.
- Added a compact Daily | Weekly selector.
- Added a dedicated scrolling objective list.
- Added colored progress indicators.
- Grouped claimed objectives into collapsible sections.
- Reduced unnecessary vertical spacing.

**Testing:**
- Verified daily objective retrieval in GW2.
- Verified weekly objective retrieval in GW2.
- Confirmed the notification badge displays.
- Confirmed the manual pulse animation works.
- Confirmed the compact Daily | Weekly interface renders correctly.

### Achievement Accuracy Improvements

- Added availability classifications for achievement recommendations.
- Introduced Permanent Lead, Event-Dependent, Historical / Unavailable, and Unverified labels.
- Separated event-dependent achievements from regular recommendations.
- Added a collapsible Historical / Unavailable section.
- Preserved regional and character-specific achievement separation.
- Improved handling of achievements associated with discontinued events.
- Added clearer messaging about unverified achievement availability.

**Testing:**
- Confirmed Queensdale recommendations display availability labels.
- Confirmed historical Champion Rush-Kryta appears under Historical / Unavailable.
- Confirmed historical entries no longer appear among normal map recommendations.
- Confirmed regional and character-specific sections remain visible.

### Security and Reliability

- GW2 API keys remain stored locally using Windows DPAPI.
- Account-specific information is not transmitted to the public VPS achievement index.
- VPS requests use HTTPS.
- The VPS API runs under a dedicated service account.
- The local achievement scanner remains functional when VPS data is unavailable.
- Existing Copper & Claw and Roundtable services remained accessible after deployment.

### Known Limitations

- Map associations do not guarantee an achievement is currently obtainable.
- Achievements without recorded progress are not necessarily unfinished.
- Event-dependent classification requires further refinement.
- Some achievement availability information may be incomplete.
- Automatic VPS refresh completion has not yet been verified.
- Daily and weekly notification behavior needs testing across actual resets.

### Planned Improvements

- Continue improving achievement availability accuracy.
- Expand historical-event detection.
- Verify automatic VPS database refreshes.
- Test daily and weekly notification resets.
- Test achievement recommendations across additional GW2 maps.
- Complete final regression testing before publishing a release.

**Checkpoint:** v0.2.0-rc1  
**Status:** Development testing successful. Additional verification required before public release.

---

## v0.1.2 - November 3, 2026

- Restored the original achievement-card presentation.
- Added three category tabs and distinct tab colors/icons.
- Improved achievement action button styling.
- Added fallback handling for festival calendar HTTP 403 errors.
- Cleaned up redundant progress text.
- Tested the redesigned interface in Guild Wars 2.

**Status at the time:** Preparing release.
