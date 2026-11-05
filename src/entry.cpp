///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - All rights reserved.
///
/// This code is licensed under the MIT license.
/// You should have received a copy of the license along with this source file.
/// You may obtain a copy of the license at: https://opensource.org/license/MIT
///
/// Name         :  entry.cpp
/// Description  :  Unfinished Business - GW2 achievement opportunity scanner.
///----------------------------------------------------------------------------------------------------

#include <Windows.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "winhttp.lib")

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"

/* proto */
struct AchievementProgress;

void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void AddonRender();
void RenderLimitedCoverageLeads();
void AddonOptions();

void StartAchievementCategorySync();
void StartCurrentMapDefinitionSync(const std::vector<int>& aIDs);
void StartApiKeyValidation();

void StartFestivalScheduleSync();
void UpdateFestivalScheduleSync();
void DetectActiveFestivalFromAchievementCategories();
void UpdateActiveFestivalOpportunityScan();
void UpdateFestivalDefinitionSync();
void BuildFestivalOpportunityList();
void RenderActiveFestivalSection();
void StartApiKeyValidationForKey(const std::string& aApiKey);
void StartAchievementSyncWithKey(const std::string& aApiKey);
void SecureClearString(std::string& aValue);
void SecureClearApiKeyBuffer();

const AchievementProgress* FindAchievementProgress(int aID);
bool IsCharacterAdventureGuideAchievement(int aID);
bool IsRetiredCategoryName(const std::string& aName);
void BuildCurrentMapOpportunityList();
bool UBContainsWholePhrase(const std::string& aText, const std::string& aPhrase);
void SaveUserPreferences();
ImVec4 UBAccentColor();
ImVec4 UBSuccessColor();
ImVec4 UBWarningColor();
ImVec4 UBMutedInfoColor();
void LoadUserPreferences();
std::string ToLowerCopy(const std::string& aValue);

/* globals */
AddonDefinition_t AddonDef = {};
HMODULE hSelf = nullptr;
AddonAPI_t* APIDefs = nullptr;
NexusLinkData_t* NexusLink = nullptr;
Mumble::Data* MumbleLink = nullptr;

constexpr const char* UB_NEXUS_GAME_WIKI_OPEN_EVENT =
"NexusGameWiki.OpenPage.v2";

struct NexusGameWikiOpenPageRequest
{
    const char* Title = nullptr;
    bool Handled = false;
};

/* map lookup state */
struct MapLookupResult
{
    uint32_t MapID = 0;
    bool Success = false;
    std::string Name;
    std::string RegionName;
    std::string Error;
};

uint32_t lastObservedMapID = 0;
uint32_t displayedMapID = 0;
std::string currentMapName = "Waiting for map data...";
std::string currentMapRegionName;

bool mapLookupActive = false;
std::future<MapLookupResult> mapLookupFuture;

/* optional, public-only VPS map index state (v0.2.0 test) */
struct UBVpsCandidate
{
    int ID = 0;
    std::string Scope;       // "map" or "regional" only
    std::string Confidence;
    std::string Source;
    std::string Evidence;
};

struct UBVpsLookupResult
{
    uint32_t MapID = 0;
    bool Success = false;
    bool Truncated = false;
    std::vector<UBVpsCandidate> Candidates;
    std::string Error;
};

bool ubVpsIndexEnabled = true;
bool ubVpsLookupActive = false;
bool ubVpsRescanPending = false;
uint32_t ubVpsInFlightMapID = 0;
size_t ubVpsMergedCount = 0;
size_t ubVpsRegionalCount = 0;
std::future<UBVpsLookupResult> ubVpsLookupFuture;
std::unordered_map<uint32_t, UBVpsLookupResult> ubVpsCache;
std::unordered_set<uint32_t> ubVpsAttemptedMaps;
std::unordered_map<int, UBVpsCandidate> ubVpsCurrentCandidates;
std::unordered_set<int> ubVpsOnlyCandidateIDs;
std::string ubVpsStatus = "Waiting for map ID...";

/* API key state */
struct ApiKeyValidationResult
{
    bool Success = false;
    bool HasAccountPermission = false;
    bool HasProgressionPermission = false;
    std::string Error;
};

char apiKeyBuffer[160] = {};
bool apiKeyBufferHasInput = false;
std::chrono::steady_clock::time_point apiKeyBufferLastTouched;
constexpr int apiKeyInputIdleWipeSeconds = 120;

bool apiValidationActive = false;
bool apiKeyValid = false;
bool apiKeyHasProgression = false;
std::string apiKeyStatus = "No API key validated.";
std::string pendingValidationKey;
std::future<ApiKeyValidationResult> apiValidationFuture;


bool apiKeyLoadedFromDisk = false;
std::string apiKeyStorageStatus = "API key is not saved yet.";


/* user recommendation preferences */
bool hideStoryAchievements = false;
bool hideEventAchievements = false;
bool hideCollectionAchievements = false;
bool hideNotStartedAchievements = false;

bool preferStartedAchievements = true;
bool preferMasteryRewards = true;
bool preferAchievementPoints = false;

bool autoRefreshAchievementProgress = true;
constexpr int achievementAutoRefreshSeconds = 300;
bool achievementSyncTimestampValid = false;
std::chrono::steady_clock::time_point lastAchievementSyncCompleted;

// Reusable diagnostics probe. 6670 = Completing Renown Hearts II.
int achievementProgressProbeID = 6670;

std::unordered_set<int> ignoredAchievementIDs;
bool opportunityFiltersDirty = false;

/* Wizard's Vault daily/weekly objectives: direct GW2 API, never the VPS. */
struct UBVaultObjective
{
    int ID = 0;
    std::string Title;
    std::string Track;
    int Acclaim = 0;
    int Current = 0;
    int Complete = 0;
    bool Claimed = false;
};

struct UBVaultPeriod
{
    bool Loaded = false;
    int MetaCurrent = 0;
    int MetaComplete = 0;
    int MetaAcclaim = 0;
    bool MetaClaimed = false;
    std::vector<UBVaultObjective> Objectives;
    std::string Error;
};

struct UBVaultSyncResult
{
    UBVaultPeriod Daily;
    UBVaultPeriod Weekly;
    uint64_t Generation = 0;
};

UBVaultPeriod ubVaultDaily;
UBVaultPeriod ubVaultWeekly;
std::future<UBVaultSyncResult> ubVaultFuture;
bool ubVaultSyncActive = false;
bool ubVaultLastAttemptValid = false;
bool ubVaultAnimateTab = true;
// In-session visual test, does not change acknowledgement or saved settings.
double ubVaultPulsePreviewUntil = 0.0;
uint64_t ubVaultGeneration = 0;
std::chrono::steady_clock::time_point ubVaultLastAttempt;
std::string ubVaultStatus = "Waiting for validated GW2 API key...";
std::string ubVaultAcknowledgedSignature;
std::string ubVaultLatestSignature;
constexpr int ubVaultRefreshSeconds = 300;

/* achievement state */
struct AchievementProgress
{
    int ID = 0;
    int Current = -1;
    int Max = -1;
    bool Done = false;
    std::vector<int> Bits;
};

struct AchievementSyncResult
{
    bool Success = false;
    size_t RecordsLoaded = 0;
    size_t Completed = 0;
    size_t InProgress = 0;
    std::vector<AchievementProgress> Records;
    std::string Error;
};

bool achievementSyncActive = false;
bool achievementDataLoaded = false;
size_t achievementRecordsLoaded = 0;
size_t achievementCompleted = 0;
size_t achievementInProgress = 0;
std::string achievementSyncStatus = "Not synced yet.";
std::vector<AchievementProgress> achievementProgress;
std::future<AchievementSyncResult> achievementSyncFuture;


/* achievement definition state */
struct AchievementTier
{
    int Count = 0;
    int Points = 0;
};

struct AchievementDefinition
{
    int ID = 0;
    std::string Name;
    std::string Description;
    std::string Requirement;

    std::vector<AchievementTier> Tiers;

    bool RewardsMastery = false;
    std::string MasteryRegion;
    bool RewardsTitle = false;
    int RewardCoins = 0;
    int RewardItemEntries = 0;
};

struct AchievementDefinitionSyncResult
{
    bool Success = false;
    size_t DefinitionsLoaded = 0;
    std::unordered_map<int, AchievementDefinition> Definitions;
    std::string Error;
};

bool achievementDefinitionSyncActive = false;
bool achievementDefinitionsLoaded = false;
size_t achievementDefinitionsLoadedCount = 0;
std::string achievementDefinitionStatus = "Not loaded yet.";
std::unordered_map<int, AchievementDefinition> achievementDefinitions;
std::future<AchievementDefinitionSyncResult> achievementDefinitionSyncFuture;


/* achievement category state */
struct AchievementCategory
{
    int ID = 0;
    std::string Name;
    std::vector<int> AchievementIDs;
};

struct AchievementCategorySyncResult
{
    bool Success = false;
    std::vector<AchievementCategory> Categories;
    std::string Error;
};

bool achievementCategorySyncActive = false;
bool achievementCategorySyncAttempted = false;
bool achievementCategoriesLoaded = false;
std::string achievementCategoryStatus = "Not loaded yet.";
std::vector<AchievementCategory> achievementCategories;
std::future<AchievementCategorySyncResult> achievementCategorySyncFuture;


/* seasonal festival state */
struct FestivalDate
{
    int Year = 0;
    int Month = 0;
    int Day = 0;
};

struct FestivalDescriptor
{
    const char* Key = "";
    const char* DisplayName = "";
    const char* WikiPageBase = "";
    std::vector<std::string> CategoryNameFragments;
    std::vector<std::string> HostMaps;
};

struct FestivalScheduleResult
{
    bool Success = false;
    bool Active = false;

    std::string FestivalKey;
    std::string FestivalName;
    std::string WikiPageTitle;

    FestivalDate StartDate;
    FestivalDate EndDate;

    std::string Error;
    bool UsedOfflineFallback = false;
};

const std::vector<FestivalDescriptor> FestivalDescriptors =
{
    {
        "lunar_new_year",
        "Lunar New Year",
        "Lunar New Year",
        { "Lunar New Year" },
        { "Divinity's Reach", "Dragon Ball Arena" }
    },
    {
        "super_adventure_festival",
        "Super Adventure Festival",
        "Super Adventure Festival",
        { "Super Adventure" },
        { "Rata Sum" }
    },
    {
        "dragon_bash",
        "Dragon Bash",
        "Dragon Bash",
        { "Dragon Bash" },
        { "Hoelbrak" }
    },
    {
        "festival_of_the_four_winds",
        "Festival of the Four Winds",
        "Festival of the Four Winds",
        { "Festival of the Four Winds" },
        { "Labyrinthine Cliffs", "The Crown Pavilion" }
    },
    {
        "halloween",
        "Shadow of the Mad King",
        "Shadow of the Mad King",
        { "Halloween", "Shadow of the Mad King", "Lunatic Wardrobe" },
        { "Lion's Arch", "Mad King's Labyrinth", "Mad King's Clock Tower" }
    },
    {
        "wintersday",
        "A Very Merry Wintersday",
        "A Very Merry Wintersday",
        { "Wintersday" },
        { "Divinity's Reach" }
    }
};

bool festivalScheduleSyncActive = false;
bool festivalScheduleSyncAttempted = false;
bool festivalScheduleLastCheckValid = false;
std::chrono::steady_clock::time_point festivalScheduleLastCheck;
std::future<FestivalScheduleResult> festivalScheduleFuture;

bool activeFestivalAvailable = false;
std::string activeFestivalKey;
std::string activeFestivalName;
std::string activeFestivalWikiPageTitle;
FestivalDate activeFestivalStartDate;
FestivalDate activeFestivalEndDate;
std::string festivalScheduleStatus =
"Waiting for the official GW2 event calendar...";
std::string festivalDetectionSource =
"Detected from the official Guild Wars 2 event calendar.";

bool festivalOpportunityScanDirty = true;
bool festivalDefinitionSyncActive = false;
bool festivalDefinitionsReady = false;
std::future<AchievementDefinitionSyncResult> festivalDefinitionSyncFuture;

std::vector<std::string> festivalMatchedCategoryNames;
std::vector<int> festivalCategoryIDs;
std::vector<int> festivalOpportunityIDs;


/* curated map-to-category aliases */
struct MapCategoryAlias
{
    const char* MapName;

    // Categories dedicated to this map. All unfinished achievements from
    // these categories are eligible.
    std::vector<std::string> PrimaryCategoryNames;

    // Broader categories that may contain achievements from many maps.
    // Achievements from these categories are only kept when their public
    // definition text explicitly references this map.
    std::vector<std::string> SecondaryCategoryNames;

    // Lower-case text terms used to confirm secondary-category relevance.
    std::vector<std::string> MatchTerms;
};

const std::vector<MapCategoryAlias> MapCategoryAliases =
{
    {
        "Southsun Cove",
        {
            "Southsun Survival",
            "Crab Toss"
        },
        {
            "Roller Beetle Racing",
            "Adventure Guide: Volume Five"
        },
        {
            "southsun cove",
            "southsun"
        }
    },

    // Core Tyria starter maps share several Character Adventure Guide
    // achievements. We only surface an achievement when its own public
    // text explicitly names the map the player is currently standing in.
    {
        "Queensdale",
        {},
        {
            "Adventure Guide: Volume One"
        },
        {
            "queensdale"
        }
    },
    {
        "Metrica Province",
        {},
        {
            "Adventure Guide: Volume One"
        },
        {
            "metrica province"
        }
    },
    {
        "Caledon Forest",
        {},
        {
            "Adventure Guide: Volume One"
        },
        {
            "caledon forest"
        }
    },
    {
        "Wayfarer Foothills",
        {},
        {
            "Adventure Guide: Volume One"
        },
        {
            "wayfarer foothills"
        }
    },
    {
        "Plains of Ashford",
        {},
        {
            "Adventure Guide: Volume One"
        },
        {
            "plains of ashford"
        }
    },

    // Adventure Guide: Volume Two
    {
        "Snowden Drifts",
        {},
        {
            "Adventure Guide: Volume Two"
        },
        {
            "snowden drifts"
        }
    },
    {
        "Diessa Plateau",
        {},
        {
            "Adventure Guide: Volume Two"
        },
        {
            "diessa plateau"
        }
    },
    {
        "Brisban Wildlands",
        {},
        {
            "Adventure Guide: Volume Two"
        },
        {
            "brisban wildlands"
        }
    },
    {
        "Kessex Hills",
        {},
        {
            "Adventure Guide: Volume Two"
        },
        {
            "kessex hills"
        }
    },

    // Adventure Guide: Volume Three
    {
        "Fields of Ruin",
        {},
        {
            "Adventure Guide: Volume Three"
        },
        {
            "fields of ruin"
        }
    },
    {
        "Blazeridge Steppes",
        {},
        {
            "Adventure Guide: Volume Three"
        },
        {
            "blazeridge steppes"
        }
    },
    {
        "Lornar's Pass",
        {},
        {
            "Adventure Guide: Volume Three"
        },
        {
            "lornar's pass"
        }
    },
    {
        "Dredgehaunt Cliffs",
        {},
        {
            "Adventure Guide: Volume Three"
        },
        {
            "dredgehaunt cliffs"
        }
    },
    {
        "Harathi Hinterlands",
        {},
        {
            "Adventure Guide: Volume Three"
        },
        {
            "harathi hinterlands"
        }
    },

    // Adventure Guide: Volume Four
    {
        "Sparkfly Fen",
        {},
        {
            "Adventure Guide: Volume Four"
        },
        {
            "sparkfly fen"
        }
    },
    {
        "Mount Maelstrom",
        {},
        {
            "Adventure Guide: Volume Four"
        },
        {
            "mount maelstrom"
        }
    },
    {
        "Iron Marches",
        {},
        {
            "Adventure Guide: Volume Four"
        },
        {
            "iron marches"
        }
    },
    {
        "Fireheart Rise",
        {},
        {
            "Adventure Guide: Volume Four"
        },
        {
            "fireheart rise"
        }
    },
    {
        "Timberline Falls",
        {},
        {
            "Adventure Guide: Volume Four"
        },
        {
            "timberline falls"
        }
    },

    // Adventure Guide: Volume Five
    {
        "Frostgorge Sound",
        {},
        {
            "Adventure Guide: Volume Five"
        },
        {
            "frostgorge sound"
        }
    },

    // ArenaNet's Volume Five text refers to these maps collectively as
    // "the Ruins of Orr", so the regional phrase is an intentional alias.
    {
        "Straits of Devastation",
        {},
        {
            "Adventure Guide: Volume Five"
        },
        {
            "straits of devastation",
            "ruins of orr"
        }
    },
    {
        "Malchor's Leap",
        {},
        {
            "Adventure Guide: Volume Five"
        },
        {
            "malchor's leap",
            "ruins of orr"
        }
    },
    {
        "Cursed Shore",
        {},
        {
            "Adventure Guide: Volume Five"
        },
        {
            "cursed shore",
            "ruins of orr"
        }
    }
};

/* current-map opportunity state */
bool currentMapDefinitionSyncActive = false;
bool currentMapDefinitionsReady = false;
std::future<AchievementDefinitionSyncResult> currentMapDefinitionSyncFuture;
std::string currentMapDefinitionRequestMap;

std::string lastOpportunityMapName;
std::string currentOpportunityCategoryName;
std::string currentOpportunityStatus = "Waiting for achievement category data...";
std::vector<int> currentOpportunityCategoryIDs;
std::vector<int> currentMapOpportunityIDs;

// Source tracking for diagnostics and source precedence.
std::unordered_set<int> currentOpportunityDirectIDs;

// IDs sourced only from broad cross-map categories. These must pass an explicit
// text relevance check after their achievement definitions are downloaded.
std::unordered_set<int> currentOpportunityTextFilteredIDs;
std::vector<std::string> currentOpportunityMatchTerms;

// v0.2 coverage discovery: exact map names are promoted only when they appear
// explicitly in public achievement text or category names. Regional matches
// are marked separately because the objective may be on another nearby map.
std::unordered_set<int> currentOpportunityRegionalIDs;
size_t currentAutoMapMatchCount = 0;
size_t currentRegionalMatchCount = 0;
bool currentDiscoveryScanComplete = false;

enum class MapCoverageMode
{
    None,
    ExactCategory,
    CuratedMapping,
    ContextualDiscovery
};

MapCoverageMode currentCoverageMode = MapCoverageMode::None;
std::vector<std::string> currentCoverageDirectCategories;
std::vector<std::string> currentCoverageContextualCategories;

///----------------------------------------------------------------------------------------------------
/// SecureClearString:
///     Best-effort clearing for plaintext credentials held in std::string.
///----------------------------------------------------------------------------------------------------
void SecureClearString(std::string& aValue)
{
    if (!aValue.empty())
    {
        SecureZeroMemory(
            &aValue[0],
            aValue.size()
        );
    }

    aValue.clear();
}

///----------------------------------------------------------------------------------------------------
/// SecureClearApiKeyBuffer:
///----------------------------------------------------------------------------------------------------
void SecureClearApiKeyBuffer()
{
    SecureZeroMemory(
        apiKeyBuffer,
        sizeof(apiKeyBuffer)
    );

    apiKeyBufferHasInput = false;
}

///----------------------------------------------------------------------------------------------------
/// MaybeExpireApiKeyInput:
///     Clears an unsubmitted plaintext key from the UI buffer after idle time.
///----------------------------------------------------------------------------------------------------
void MaybeExpireApiKeyInput()
{
    if (!apiKeyBufferHasInput ||
        apiValidationActive)
    {
        return;
    }

    const long long idleSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() -
            apiKeyBufferLastTouched
        ).count();

    if (idleSeconds >= apiKeyInputIdleWipeSeconds)
    {
        SecureClearApiKeyBuffer();
    }
}

///----------------------------------------------------------------------------------------------------
/// GetApiKeyStorageDirectory:
///     Uses the current Windows user's Local AppData folder.
///----------------------------------------------------------------------------------------------------
std::string GetApiKeyStorageDirectory()
{
    char localAppData[MAX_PATH] = {};

    const DWORD length = GetEnvironmentVariableA(
        "LOCALAPPDATA",
        localAppData,
        MAX_PATH
    );

    if (length == 0 || length >= MAX_PATH)
    {
        return {};
    }

    return std::string(localAppData) + "\\UnfinishedBusiness";
}

///----------------------------------------------------------------------------------------------------
/// GetApiKeyStoragePath:
///----------------------------------------------------------------------------------------------------
std::string GetApiKeyStoragePath()
{
    const std::string directory =
        GetApiKeyStorageDirectory();

    if (directory.empty())
    {
        return {};
    }

    return directory + "\\api_key.dat";
}

///----------------------------------------------------------------------------------------------------
/// SaveEncryptedApiKey:
///     Protects the key with Windows DPAPI before writing it to disk.
///     The encrypted blob is tied to the current Windows user account.
///----------------------------------------------------------------------------------------------------
bool SaveEncryptedApiKey(
    const std::string& aApiKey,
    std::string& aError)
{
    aError.clear();

    if (aApiKey.empty())
    {
        aError = "Cannot save an empty API key.";
        return false;
    }

    const std::string directory =
        GetApiKeyStorageDirectory();

    const std::string path =
        GetApiKeyStoragePath();

    if (directory.empty() || path.empty())
    {
        aError = "Could not determine Local AppData path.";
        return false;
    }

    if (!CreateDirectoryA(directory.c_str(), nullptr))
    {
        const DWORD createError = GetLastError();

        if (createError != ERROR_ALREADY_EXISTS)
        {
            aError =
                "Could not create API key storage directory.";
            return false;
        }
    }

    DATA_BLOB inputBlob = {};
    inputBlob.pbData =
        reinterpret_cast<BYTE*>(
            const_cast<char*>(aApiKey.data())
            );
    inputBlob.cbData =
        static_cast<DWORD>(aApiKey.size());

    DATA_BLOB outputBlob = {};

    if (!CryptProtectData(
        &inputBlob,
        L"Unfinished Business GW2 API Key",
        nullptr,
        nullptr,
        nullptr,
        CRYPTPROTECT_UI_FORBIDDEN,
        &outputBlob))
    {
        aError = "Windows could not encrypt the API key.";
        return false;
    }

    std::ofstream file(
        path,
        std::ios::binary | std::ios::trunc
    );

    if (!file)
    {
        LocalFree(outputBlob.pbData);
        aError = "Could not open API key storage file.";
        return false;
    }

    file.write(
        reinterpret_cast<const char*>(outputBlob.pbData),
        outputBlob.cbData
    );

    const bool writeSucceeded = file.good();

    file.close();
    LocalFree(outputBlob.pbData);

    if (!writeSucceeded)
    {
        aError = "Could not write encrypted API key.";
        return false;
    }

    return true;
}

///----------------------------------------------------------------------------------------------------
/// LoadEncryptedApiKey:
///----------------------------------------------------------------------------------------------------
bool LoadEncryptedApiKey(
    std::string& aApiKey,
    std::string& aError)
{
    aApiKey.clear();
    aError.clear();

    const std::string path =
        GetApiKeyStoragePath();

    if (path.empty())
    {
        aError = "Could not determine API key storage path.";
        return false;
    }

    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        // No saved key is a normal first-run condition.
        return false;
    }

    std::vector<BYTE> encrypted(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    file.close();

    if (encrypted.empty())
    {
        aError = "Saved API key file was empty.";
        return false;
    }

    DATA_BLOB inputBlob = {};
    inputBlob.pbData = encrypted.data();
    inputBlob.cbData =
        static_cast<DWORD>(encrypted.size());

    DATA_BLOB outputBlob = {};

    if (!CryptUnprotectData(
        &inputBlob,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CRYPTPROTECT_UI_FORBIDDEN,
        &outputBlob))
    {
        aError =
            "Windows could not decrypt the saved API key.";
        return false;
    }

    aApiKey.assign(
        reinterpret_cast<const char*>(outputBlob.pbData),
        outputBlob.cbData
    );

    SecureZeroMemory(
        outputBlob.pbData,
        outputBlob.cbData
    );

    LocalFree(outputBlob.pbData);

    return !aApiKey.empty();
}

///----------------------------------------------------------------------------------------------------
/// DeleteSavedApiKey:
///----------------------------------------------------------------------------------------------------
void DeleteSavedApiKey()
{
    const std::string path =
        GetApiKeyStoragePath();

    if (!path.empty())
    {
        DeleteFileA(path.c_str());
    }

    apiKeyLoadedFromDisk = false;
    apiKeyStorageStatus = "API key is not saved.";
}

///----------------------------------------------------------------------------------------------------
/// GetPreferenceStoragePath:
///----------------------------------------------------------------------------------------------------
std::string GetPreferenceStoragePath()
{
    const std::string directory =
        GetApiKeyStorageDirectory();

    if (directory.empty())
    {
        return {};
    }

    return directory + "\\settings.txt";
}

///----------------------------------------------------------------------------------------------------
/// SaveUserPreferences:
///----------------------------------------------------------------------------------------------------
void SaveUserPreferences()
{
    const std::string directory =
        GetApiKeyStorageDirectory();

    const std::string path =
        GetPreferenceStoragePath();

    if (directory.empty() || path.empty())
    {
        return;
    }

    if (!CreateDirectoryA(directory.c_str(), nullptr))
    {
        const DWORD createError = GetLastError();

        if (createError != ERROR_ALREADY_EXISTS)
        {
            return;
        }
    }

    std::ofstream file(
        path,
        std::ios::out | std::ios::trunc
    );

    if (!file)
    {
        return;
    }

    file
        << "hide_story=" << (hideStoryAchievements ? 1 : 0) << "\n"
        << "hide_event=" << (hideEventAchievements ? 1 : 0) << "\n"
        << "hide_collection=" << (hideCollectionAchievements ? 1 : 0) << "\n"
        << "hide_not_started=" << (hideNotStartedAchievements ? 1 : 0) << "\n"
        << "prefer_started=" << (preferStartedAchievements ? 1 : 0) << "\n"
        << "prefer_mastery=" << (preferMasteryRewards ? 1 : 0) << "\n"
        << "prefer_ap=" << (preferAchievementPoints ? 1 : 0) << "\n"
        << "auto_refresh=" << (autoRefreshAchievementProgress ? 1 : 0) << "\n"
        << "vps_index_enabled=" << (ubVpsIndexEnabled ? 1 : 0) << "\n"
        << "vault_animate=" << (ubVaultAnimateTab ? 1 : 0) << "\n"
        << "vault_ack=" << ubVaultAcknowledgedSignature << "\n";

    file << "ignored=";

    bool first = true;

    for (int id : ignoredAchievementIDs)
    {
        if (!first)
        {
            file << ",";
        }

        file << id;
        first = false;
    }

    file << "\n";
}

///----------------------------------------------------------------------------------------------------
/// ParsePreferenceBool:
///----------------------------------------------------------------------------------------------------
bool ParsePreferenceBool(const std::string& aValue)
{
    return
        aValue == "1" ||
        aValue == "true" ||
        aValue == "TRUE";
}

///----------------------------------------------------------------------------------------------------
/// LoadUserPreferences:
///----------------------------------------------------------------------------------------------------
void LoadUserPreferences()
{
    const std::string path =
        GetPreferenceStoragePath();

    if (path.empty())
    {
        return;
    }

    std::ifstream file(path);

    if (!file)
    {
        return;
    }

    ignoredAchievementIDs.clear();

    std::string line;

    while (std::getline(file, line))
    {
        const size_t equalsPos =
            line.find('=');

        if (equalsPos == std::string::npos)
        {
            continue;
        }

        const std::string key =
            line.substr(0, equalsPos);

        const std::string value =
            line.substr(equalsPos + 1);

        if (key == "hide_story")
        {
            hideStoryAchievements =
                ParsePreferenceBool(value);
        }
        else if (key == "hide_event")
        {
            hideEventAchievements =
                ParsePreferenceBool(value);
        }
        else if (key == "hide_collection")
        {
            hideCollectionAchievements =
                ParsePreferenceBool(value);
        }
        else if (key == "hide_not_started")
        {
            hideNotStartedAchievements =
                ParsePreferenceBool(value);
        }
        else if (key == "prefer_started")
        {
            preferStartedAchievements =
                ParsePreferenceBool(value);
        }
        else if (key == "prefer_mastery")
        {
            preferMasteryRewards =
                ParsePreferenceBool(value);
        }
        else if (key == "prefer_ap")
        {
            preferAchievementPoints =
                ParsePreferenceBool(value);
        }
        else if (key == "auto_refresh")
        {
            autoRefreshAchievementProgress =
                ParsePreferenceBool(value);
        }
        else if (key == "vps_index_enabled")
        {
            ubVpsIndexEnabled = ParsePreferenceBool(value);
        }
        else if (key == "vault_animate")
        {
            ubVaultAnimateTab = ParsePreferenceBool(value);
        }
        else if (key == "vault_ack")
        {
            ubVaultAcknowledgedSignature = value;
        }
        else if (key == "ignored")
        {
            std::stringstream stream(value);
            std::string token;

            while (std::getline(stream, token, ','))
            {
                if (token.empty())
                {
                    continue;
                }

                try
                {
                    ignoredAchievementIDs.insert(
                        std::stoi(token)
                    );
                }
                catch (...)
                {
                }
            }
        }
    }

    opportunityFiltersDirty = true;
}

///----------------------------------------------------------------------------------------------------
/// IgnoreAchievement:
///----------------------------------------------------------------------------------------------------
void IgnoreAchievement(int aID)
{
    ignoredAchievementIDs.insert(aID);
    SaveUserPreferences();
    opportunityFiltersDirty = true;
}

///----------------------------------------------------------------------------------------------------
/// RestoreAllIgnoredAchievements:
///----------------------------------------------------------------------------------------------------
void RestoreAllIgnoredAchievements()
{
    ignoredAchievementIDs.clear();
    SaveUserPreferences();
    opportunityFiltersDirty = true;
}

///----------------------------------------------------------------------------------------------------
/// Trim:
///----------------------------------------------------------------------------------------------------
std::string Trim(const std::string& aValue)
{
    size_t start = 0;

    while (start < aValue.size() &&
        std::isspace(static_cast<unsigned char>(aValue[start])))
    {
        ++start;
    }

    size_t end = aValue.size();

    while (end > start &&
        std::isspace(static_cast<unsigned char>(aValue[end - 1])))
    {
        --end;
    }

    return aValue.substr(start, end - start);
}

///----------------------------------------------------------------------------------------------------
/// ExtractJsonStringField:
///     Minimal JSON helper for simple string fields returned by the GW2 API.
///----------------------------------------------------------------------------------------------------
std::string ExtractJsonStringField(
    const std::string& aJson,
    const std::string& aField)
{
    const std::string key = "\"" + aField + "\"";
    size_t keyPos = aJson.find(key);

    if (keyPos == std::string::npos)
    {
        return {};
    }

    size_t colonPos = aJson.find(':', keyPos + key.length());

    if (colonPos == std::string::npos)
    {
        return {};
    }

    size_t quotePos = aJson.find('"', colonPos + 1);

    if (quotePos == std::string::npos)
    {
        return {};
    }

    std::string result;
    bool escaped = false;

    for (size_t i = quotePos + 1; i < aJson.size(); ++i)
    {
        const char ch = aJson[i];

        if (escaped)
        {
            switch (ch)
            {
            case '"':  result.push_back('"');  break;
            case '\\': result.push_back('\\'); break;
            case '/':  result.push_back('/');  break;
            case 'b':  result.push_back('\b'); break;
            case 'f':  result.push_back('\f'); break;
            case 'n':  result.push_back('\n'); break;
            case 'r':  result.push_back('\r'); break;
            case 't':  result.push_back('\t'); break;
            default:   result.push_back(ch);   break;
            }

            escaped = false;
            continue;
        }

        if (ch == '\\')
        {
            escaped = true;
            continue;
        }

        if (ch == '"')
        {
            return result;
        }

        result.push_back(ch);
    }

    return {};
}

///----------------------------------------------------------------------------------------------------
/// ExtractJsonIntField:
///----------------------------------------------------------------------------------------------------
bool ExtractJsonIntField(
    const std::string& aJson,
    const std::string& aField,
    int& aValue)
{
    const std::string key = "\"" + aField + "\"";
    size_t keyPos = aJson.find(key);

    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = aJson.find(':', keyPos + key.length());

    if (pos == std::string::npos)
    {
        return false;
    }

    ++pos;

    while (pos < aJson.size() &&
        std::isspace(static_cast<unsigned char>(aJson[pos])))
    {
        ++pos;
    }

    bool negative = false;

    if (pos < aJson.size() && aJson[pos] == '-')
    {
        negative = true;
        ++pos;
    }

    if (pos >= aJson.size() ||
        !std::isdigit(static_cast<unsigned char>(aJson[pos])))
    {
        return false;
    }

    int value = 0;

    while (pos < aJson.size() &&
        std::isdigit(static_cast<unsigned char>(aJson[pos])))
    {
        value = (value * 10) + (aJson[pos] - '0');
        ++pos;
    }

    aValue = negative ? -value : value;
    return true;
}

///----------------------------------------------------------------------------------------------------
/// ExtractJsonBoolField:
///----------------------------------------------------------------------------------------------------
bool ExtractJsonBoolField(
    const std::string& aJson,
    const std::string& aField,
    bool& aValue)
{
    const std::string key = "\"" + aField + "\"";
    size_t keyPos = aJson.find(key);

    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t pos = aJson.find(':', keyPos + key.length());

    if (pos == std::string::npos)
    {
        return false;
    }

    ++pos;

    while (pos < aJson.size() &&
        std::isspace(static_cast<unsigned char>(aJson[pos])))
    {
        ++pos;
    }

    if (aJson.compare(pos, 4, "true") == 0)
    {
        aValue = true;
        return true;
    }

    if (aJson.compare(pos, 5, "false") == 0)
    {
        aValue = false;
        return true;
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// ParseRootJsonIntArray:
///     Parses a root-level JSON array such as [1,2,3]. Used for the
///     achievement-category index because that endpoint is the authoritative
///     list of categories currently exposed by the live GW2 API.
///----------------------------------------------------------------------------------------------------
std::vector<int> ParseRootJsonIntArray(
    const std::string& aJson)
{
    std::vector<int> values;

    size_t pos = 0;

    while (pos < aJson.size())
    {
        while (pos < aJson.size() &&
            !std::isdigit(
                static_cast<unsigned char>(
                    aJson[pos])) &&
            aJson[pos] != '-')
        {
            ++pos;
        }

        if (pos >= aJson.size())
        {
            break;
        }

        bool negative = false;

        if (aJson[pos] == '-')
        {
            negative = true;
            ++pos;
        }

        int value = 0;
        bool foundDigit = false;

        while (pos < aJson.size() &&
            std::isdigit(
                static_cast<unsigned char>(
                    aJson[pos])))
        {
            foundDigit = true;
            value =
                (value * 10) +
                (aJson[pos] - '0');

            ++pos;
        }

        if (foundDigit)
        {
            values.push_back(
                negative ? -value : value
            );
        }
    }

    return values;
}

///----------------------------------------------------------------------------------------------------
/// ExtractJsonIntArray:
///----------------------------------------------------------------------------------------------------
std::vector<int> ExtractJsonIntArray(
    const std::string& aJson,
    const std::string& aField)
{
    std::vector<int> values;

    const std::string key = "\"" + aField + "\"";
    size_t keyPos = aJson.find(key);

    if (keyPos == std::string::npos)
    {
        return values;
    }

    size_t start = aJson.find('[', keyPos + key.length());

    if (start == std::string::npos)
    {
        return values;
    }

    size_t end = aJson.find(']', start + 1);

    if (end == std::string::npos)
    {
        return values;
    }

    size_t pos = start + 1;

    while (pos < end)
    {
        while (pos < end &&
            !std::isdigit(static_cast<unsigned char>(aJson[pos])) &&
            aJson[pos] != '-')
        {
            ++pos;
        }

        if (pos >= end)
        {
            break;
        }

        bool negative = false;

        if (aJson[pos] == '-')
        {
            negative = true;
            ++pos;
        }

        int value = 0;
        bool foundDigit = false;

        while (pos < end &&
            std::isdigit(static_cast<unsigned char>(aJson[pos])))
        {
            foundDigit = true;
            value = (value * 10) + (aJson[pos] - '0');
            ++pos;
        }

        if (foundDigit)
        {
            values.push_back(negative ? -value : value);
        }
    }

    return values;
}

///----------------------------------------------------------------------------------------------------
/// JsonStringArrayContains:
///----------------------------------------------------------------------------------------------------
bool JsonStringArrayContains(
    const std::string& aJson,
    const std::string& aField,
    const std::string& aValue)
{
    const std::string key = "\"" + aField + "\"";
    size_t keyPos = aJson.find(key);

    if (keyPos == std::string::npos)
    {
        return false;
    }

    size_t arrayStart = aJson.find('[', keyPos + key.length());

    if (arrayStart == std::string::npos)
    {
        return false;
    }

    size_t arrayEnd = aJson.find(']', arrayStart + 1);

    if (arrayEnd == std::string::npos)
    {
        return false;
    }

    const std::string needle = "\"" + aValue + "\"";
    size_t valuePos = aJson.find(needle, arrayStart + 1);

    return valuePos != std::string::npos && valuePos < arrayEnd;
}

///----------------------------------------------------------------------------------------------------
/// HttpGet:
///----------------------------------------------------------------------------------------------------
bool HttpGetOnce(
    const std::wstring& aPath,
    const std::string& aBearerToken,
    DWORD& aStatusCode,
    std::string& aResponse,
    std::string& aError)
{
    aStatusCode = 0;
    aResponse.clear();
    aError.clear();

    HINTERNET hSession = WinHttpOpen(
        L"UnfinishedBusiness/0.1",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (hSession == nullptr)
    {
        aError = "Could not open HTTP session.";
        return false;
    }

    WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 15000);

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        L"api.guildwars2.com",
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    if (hConnect == nullptr)
    {
        WinHttpCloseHandle(hSession);
        aError = "Could not connect to api.guildwars2.com.";
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        aPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (hRequest == nullptr)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        aError = "Could not create GW2 API request.";
        return false;
    }

    if (!aBearerToken.empty())
    {
        std::string header =
            "Authorization: Bearer " + aBearerToken + "\r\n";

        std::wstring wideHeader(
            header.begin(),
            header.end()
        );

        const BOOL headerAdded =
            WinHttpAddRequestHeaders(
                hRequest,
                wideHeader.c_str(),
                -1L,
                WINHTTP_ADDREQ_FLAG_ADD |
                WINHTTP_ADDREQ_FLAG_REPLACE
            );

        if (!header.empty())
        {
            SecureZeroMemory(
                &header[0],
                header.size()
            );
        }

        if (!wideHeader.empty())
        {
            SecureZeroMemory(
                &wideHeader[0],
                wideHeader.size() * sizeof(wchar_t)
            );
        }

        if (!headerAdded)
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            aError = "Could not add API authorization header.";
            return false;
        }
    }

    BOOL sent = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );

    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        aError = "GW2 API request failed.";
        return false;
    }

    DWORD statusSize = sizeof(aStatusCode);

    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &aStatusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    while (true)
    {
        DWORD bytesAvailable = 0;

        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
        {
            aError = "Could not read GW2 API response.";
            break;
        }

        if (bytesAvailable == 0)
        {
            break;
        }

        std::string buffer(bytesAvailable, '\0');
        DWORD bytesRead = 0;

        if (!WinHttpReadData(
            hRequest,
            &buffer[0],
            bytesAvailable,
            &bytesRead))
        {
            aError = "Could not read GW2 API response.";
            break;
        }

        buffer.resize(bytesRead);
        aResponse += buffer;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return aError.empty();
}

///----------------------------------------------------------------------------------------------------
/// HttpGet:
///     Retries temporary transport/server failures automatically.
///----------------------------------------------------------------------------------------------------
bool HttpGet(
    const std::wstring& aPath,
    const std::string& aBearerToken,
    DWORD& aStatusCode,
    std::string& aResponse,
    std::string& aError)
{
    constexpr int maxAttempts = 3;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt)
    {
        aStatusCode = 0;
        aResponse.clear();
        aError.clear();

        const bool requestCompleted =
            HttpGetOnce(
                aPath,
                aBearerToken,
                aStatusCode,
                aResponse,
                aError
            );

        // A completed request with a non-temporary HTTP response should be
        // returned immediately. Callers still decide which status codes are valid.
        if (requestCompleted)
        {
            const bool retryableStatus =
                aStatusCode == 408 ||
                aStatusCode == 429 ||
                (aStatusCode >= 500 && aStatusCode <= 599);

            if (!retryableStatus)
            {
                return true;
            }
        }

        // 401/403 are credential/permission problems, not transient failures.
        if (aStatusCode == 401 || aStatusCode == 403)
        {
            return requestCompleted;
        }

        if (attempt < maxAttempts)
        {
            // Short backoff: 350 ms, then 700 ms.
            std::this_thread::sleep_for(
                std::chrono::milliseconds(350 * attempt)
            );
        }
    }

    if (aError.empty())
    {
        if (aStatusCode != 0)
        {
            aError =
                "GW2 API request failed after retries (HTTP " +
                std::to_string(aStatusCode) + ").";
        }
        else
        {
            aError =
                "GW2 API request failed after retries.";
        }
    }

    return false;
}

// Wizard's Vault API (account + progression). Public VPS calls are separate;
// the key is sent ONLY to api.guildwars2.com using the existing HttpGet client.
UBVaultPeriod UBFetchVaultPeriod(
    const std::wstring& aPath, const std::string& aApiKey)
{
    UBVaultPeriod period;
    DWORD status = 0;
    std::string response;
    std::string error;
    if (!HttpGet(aPath, aApiKey, status, response, error))
    {
        period.Error = error.empty() ? "GW2 API request failed." : error;
        return period;
    }
    if (status != 200)
    {
        period.Error = "GW2 API returned HTTP " + std::to_string(status) + ".";
        return period;
    }

    if (!ExtractJsonIntField(response, "meta_progress_current", period.MetaCurrent) ||
        !ExtractJsonIntField(response, "meta_progress_complete", period.MetaComplete) ||
        !ExtractJsonBoolField(response, "meta_reward_claimed", period.MetaClaimed))
    {
        period.Error = "Wizard's Vault response schema was not recognized.";
        return period;
    }
    ExtractJsonIntField(response, "meta_reward_astral", period.MetaAcclaim);

    const size_t key = response.find("\"objectives\"");
    if (key == std::string::npos)
    {
        period.Error = "Wizard's Vault response has no objectives field.";
        return period;
    }
    size_t start = response.find('[', key + 12);
    if (start == std::string::npos)
    {
        period.Error = "Wizard's Vault objectives array is malformed.";
        return period;
    }

    // Parse only top-level objective objects; honor quotes/escapes while
    // locating boundaries to avoid confusing braces in objective titles.
    bool inString = false;
    bool escaped = false;
    int objectDepth = 0;
    size_t objectStart = std::string::npos;
    bool foundArrayEnd = false;
    for (size_t i = start + 1; i < response.size(); ++i)
    {
        const char ch = response[i];
        if (inString)
        {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') { inString = true; continue; }
        if (ch == '{')
        {
            if (objectDepth++ == 0) objectStart = i;
        }
        else if (ch == '}' && objectDepth > 0)
        {
            if (--objectDepth == 0 && objectStart != std::string::npos)
            {
                const std::string item = response.substr(objectStart, i - objectStart + 1);
                UBVaultObjective objective;
                if (!ExtractJsonIntField(item, "id", objective.ID) ||
                    objective.ID <= 0)
                {
                    period.Error = "Wizard's Vault objective is missing an ID.";
                    return period;
                }
                objective.Title = ExtractJsonStringField(item, "title");
                objective.Track = ExtractJsonStringField(item, "track");
                ExtractJsonIntField(item, "acclaim", objective.Acclaim);
                ExtractJsonIntField(item, "progress_current", objective.Current);
                ExtractJsonIntField(item, "progress_complete", objective.Complete);
                if (!ExtractJsonBoolField(item, "claimed", objective.Claimed) ||
                    objective.Title.empty())
                {
                    period.Error = "Wizard's Vault objective schema was not recognized.";
                    return period;
                }
                period.Objectives.push_back(std::move(objective));
                objectStart = std::string::npos;
                if (period.Objectives.size() > 50)
                {
                    period.Error = "Too many Wizard's Vault objectives returned.";
                    return period;
                }
            }
        }
        else if (ch == ']' && objectDepth == 0)
        {
            foundArrayEnd = true;
            break;
        }
    }
    if (!foundArrayEnd)
    {
        period.Error = "Wizard's Vault objectives response was incomplete.";
        return period;
    }
    period.Loaded = true;
    return period;
}

int UBVaultPendingCount(const UBVaultPeriod& aPeriod)
{
    if (!aPeriod.Loaded) return 0;
    int count = 0;
    for (const UBVaultObjective& objective : aPeriod.Objectives)
    {
        // A completed, unclaimed task still merits an alert to claim its AA.
        if (!objective.Claimed) ++count;
    }
    if (aPeriod.MetaComplete > 0 &&
        aPeriod.MetaCurrent >= aPeriod.MetaComplete &&
        !aPeriod.MetaClaimed) ++count;
    return count;
}

std::string UBVaultSignature()
{
    if (!ubVaultDaily.Loaded && !ubVaultWeekly.Loaded) return {};
    std::string signature;
    auto append = [&](const char* label, const UBVaultPeriod& period)
        {
            if (!period.Loaded) return;
            signature += label;
            std::vector<int> ids;
            for (const UBVaultObjective& objective : period.Objectives)
                ids.push_back(objective.ID);
            std::sort(ids.begin(), ids.end());
            for (int id : ids) signature += std::to_string(id) + ".";
            signature += ";";
        };
    append("D", ubVaultDaily);
    append("W", ubVaultWeekly);
    return signature;
}

void UBStartVaultSyncWithKey(const std::string& aKey)
{
    if (ubVaultSyncActive || aKey.empty()) return;
    ubVaultSyncActive = true;
    ubVaultLastAttemptValid = true;
    ubVaultLastAttempt = std::chrono::steady_clock::now();
    ubVaultStatus = "Updating Wizard's Vault objectives...";
    const uint64_t generation = ubVaultGeneration;
    std::string copy = aKey;
    ubVaultFuture = std::async(std::launch::async,
        [copy, generation]() mutable
        {
            UBVaultSyncResult result;
            result.Generation = generation;
            result.Daily = UBFetchVaultPeriod(
                L"/v2/account/wizardsvault/daily?lang=en", copy);
            result.Weekly = UBFetchVaultPeriod(
                L"/v2/account/wizardsvault/weekly?lang=en", copy);
            SecureClearString(copy);
            return result;
        });
    SecureClearString(copy);
}

void UBStartVaultSync()
{
    if (!apiKeyValid || ubVaultSyncActive) return;
    std::string key;
    std::string error;
    if (!LoadEncryptedApiKey(key, error))
    {
        ubVaultStatus = "Saved API key unavailable. Revalidate in options.";
        ubVaultLastAttemptValid = true;
        ubVaultLastAttempt = std::chrono::steady_clock::now();
        SecureClearString(key);
        return;
    }
    UBStartVaultSyncWithKey(key);
    SecureClearString(key);
}

void UBUpdateVaultSync()
{
    if (!ubVaultSyncActive ||
        ubVaultFuture.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready) return;
    UBVaultSyncResult result = ubVaultFuture.get();
    ubVaultSyncActive = false;
    if (!apiKeyValid || result.Generation != ubVaultGeneration)
    {
        ubVaultLastAttemptValid = false; // Schedule fresh account data next frame.
        return;
    }

    // A failed refresh must not leave yesterday's objectives looking current.
    ubVaultDaily = std::move(result.Daily);
    ubVaultWeekly = std::move(result.Weekly);
    ubVaultStatus = (ubVaultDaily.Loaded && ubVaultWeekly.Loaded)
        ? "Dailies and weeklies synced."
        : (ubVaultDaily.Loaded || ubVaultWeekly.Loaded)
        ? "One Wizard's Vault period available; other request failed."
        : "Wizard's Vault sync failed. Check API permissions or retry.";
    ubVaultLatestSignature = UBVaultSignature();
}

// Vault rendering is defined after the shared card and progress helpers so
// Daily + Weekly uses the same styling as the Achievements tab.
void UBRenderVaultPeriod(const char* aTitle, const UBVaultPeriod& aPeriod);
void UBRenderVaultTab();

///----------------------------------------------------------------------------------------------------
/// ForumHttpGetOnce:
///     Reads the official Guild Wars 2 forum event calendar.
///----------------------------------------------------------------------------------------------------
bool ForumHttpGetOnce(
    const std::wstring& aPath,
    DWORD& aStatusCode,
    std::string& aResponse,
    std::string& aError)
{
    aStatusCode = 0;
    aResponse.clear();
    aError.clear();

    HINTERNET hSession = WinHttpOpen(
        L"UnfinishedBusiness/0.1 (https://github.com/deadwrong03-ops/Unfinished-Business; GW2 Nexus addon)",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (hSession == nullptr)
    {
        aError = "Could not open official event-calendar HTTP session.";
        return false;
    }

    WinHttpSetTimeouts(
        hSession,
        3000,
        3000,
        3000,
        6000
    );

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        L"en-forum.guildwars2.com",
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    if (hConnect == nullptr)
    {
        WinHttpCloseHandle(hSession);
        aError = "Could not connect to the official GW2 event calendar.";
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        aPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (hRequest == nullptr)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        aError = "Could not create official event-calendar request.";
        return false;
    }

    const wchar_t* headers =
        L"Accept: text/html,application/xhtml+xml;q=0.9,*/*;q=0.8\r\n"
        L"Accept-Language: en-US,en;q=0.9\r\n";

    WinHttpAddRequestHeaders(
        hRequest,
        headers,
        -1L,
        WINHTTP_ADDREQ_FLAG_ADD |
        WINHTTP_ADDREQ_FLAG_REPLACE
    );

    const BOOL sent = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );

    if (!sent ||
        !WinHttpReceiveResponse(
            hRequest,
            nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        aError = "Official GW2 event-calendar request failed.";
        return false;
    }

    DWORD statusSize = sizeof(aStatusCode);

    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE |
        WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &aStatusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    while (true)
    {
        DWORD bytesAvailable = 0;

        if (!WinHttpQueryDataAvailable(
            hRequest,
            &bytesAvailable))
        {
            aError =
                "Could not read the official GW2 event calendar.";
            break;
        }

        if (bytesAvailable == 0)
        {
            break;
        }

        std::string buffer(
            bytesAvailable,
            '\0'
        );

        DWORD bytesRead = 0;

        if (!WinHttpReadData(
            hRequest,
            &buffer[0],
            bytesAvailable,
            &bytesRead))
        {
            aError =
                "Could not read the official GW2 event calendar.";
            break;
        }

        buffer.resize(bytesRead);
        aResponse += buffer;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return aError.empty();
}

///----------------------------------------------------------------------------------------------------
/// ForumHttpGet:
///----------------------------------------------------------------------------------------------------
bool ForumHttpGet(
    const std::wstring& aPath,
    DWORD& aStatusCode,
    std::string& aResponse,
    std::string& aError)
{
    constexpr int maxAttempts = 2;

    for (int attempt = 1;
        attempt <= maxAttempts;
        ++attempt)
    {
        aStatusCode = 0;
        aResponse.clear();
        aError.clear();

        const bool completed =
            ForumHttpGetOnce(
                aPath,
                aStatusCode,
                aResponse,
                aError
            );

        if (completed)
        {
            const bool retryable =
                aStatusCode == 408 ||
                aStatusCode == 429 ||
                (aStatusCode >= 500 &&
                    aStatusCode <= 599);

            if (!retryable)
            {
                return true;
            }
        }

        if (attempt < maxAttempts)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(500)
            );
        }
    }

    if (aError.empty())
    {
        aError =
            "Official GW2 event-calendar request failed.";
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// WikiHttpGetOnce:
///     Small unauthenticated HTTPS client for the Guild Wars 2 Wiki.
///----------------------------------------------------------------------------------------------------
bool WikiHttpGetOnce(
    const std::wstring& aPath,
    DWORD& aStatusCode,
    std::string& aResponse,
    std::string& aError)
{
    aStatusCode = 0;
    aResponse.clear();
    aError.clear();

    HINTERNET hSession = WinHttpOpen(
        L"UnfinishedBusiness/0.1 (https://github.com/deadwrong03-ops/Unfinished-Business; GW2 Nexus addon)",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (hSession == nullptr)
    {
        aError = "Could not open wiki HTTP session.";
        return false;
    }

    WinHttpSetTimeouts(
        hSession,
        3000,
        3000,
        3000,
        6000
    );

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        L"wiki.guildwars2.com",
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    if (hConnect == nullptr)
    {
        WinHttpCloseHandle(hSession);
        aError = "Could not connect to the Guild Wars 2 Wiki.";
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        aPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (hRequest == nullptr)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        aError = "Could not create wiki request.";
        return false;
    }

    const wchar_t* wikiHeaders =
        L"Api-User-Agent: UnfinishedBusiness/0.1 "
        L"(https://github.com/deadwrong03-ops/Unfinished-Business)\r\n"
        L"Accept: application/json,text/plain;q=0.9,*/*;q=0.8\r\n";

    if (!WinHttpAddRequestHeaders(
        hRequest,
        wikiHeaders,
        -1L,
        WINHTTP_ADDREQ_FLAG_ADD |
        WINHTTP_ADDREQ_FLAG_REPLACE))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        aError =
            "Could not add Guild Wars 2 Wiki request headers.";
        return false;
    }

    const BOOL sent = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );

    if (!sent ||
        !WinHttpReceiveResponse(hRequest, nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        aError = "Guild Wars 2 Wiki request failed.";
        return false;
    }

    DWORD statusSize = sizeof(aStatusCode);

    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE |
        WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &aStatusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    while (true)
    {
        DWORD bytesAvailable = 0;

        if (!WinHttpQueryDataAvailable(
            hRequest,
            &bytesAvailable))
        {
            aError =
                "Could not read Guild Wars 2 Wiki response.";
            break;
        }

        if (bytesAvailable == 0)
        {
            break;
        }

        std::string buffer(
            bytesAvailable,
            '\0'
        );

        DWORD bytesRead = 0;

        if (!WinHttpReadData(
            hRequest,
            &buffer[0],
            bytesAvailable,
            &bytesRead))
        {
            aError =
                "Could not read Guild Wars 2 Wiki response.";
            break;
        }

        buffer.resize(bytesRead);
        aResponse += buffer;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return aError.empty();
}

///----------------------------------------------------------------------------------------------------
/// WikiHttpGet:
///----------------------------------------------------------------------------------------------------
bool WikiHttpGet(
    const std::wstring& aPath,
    DWORD& aStatusCode,
    std::string& aResponse,
    std::string& aError)
{
    constexpr int maxAttempts = 2;

    for (int attempt = 1;
        attempt <= maxAttempts;
        ++attempt)
    {
        aStatusCode = 0;
        aResponse.clear();
        aError.clear();

        const bool completed =
            WikiHttpGetOnce(
                aPath,
                aStatusCode,
                aResponse,
                aError
            );

        if (completed)
        {
            const bool retryable =
                aStatusCode == 408 ||
                aStatusCode == 429 ||
                (aStatusCode >= 500 &&
                    aStatusCode <= 599);

            if (!retryable)
            {
                return true;
            }
        }

        if (attempt < maxAttempts)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(500)
            );
        }
    }

    if (aError.empty())
    {
        aError =
            "Guild Wars 2 Wiki request failed.";
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// UrlEncodeAscii:
///----------------------------------------------------------------------------------------------------
std::string UrlEncodeAscii(
    const std::string& aValue)
{
    static const char hex[] =
        "0123456789ABCDEF";

    std::string result;

    for (unsigned char ch : aValue)
    {
        const bool safe =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' ||
            ch == '_' ||
            ch == '.';

        if (safe)
        {
            result.push_back(
                static_cast<char>(ch)
            );
        }
        else
        {
            result.push_back('%');
            result.push_back(
                hex[(ch >> 4) & 0x0F]
            );
            result.push_back(
                hex[ch & 0x0F]
            );
        }
    }

    return result;
}

///----------------------------------------------------------------------------------------------------
/// GetFestivalDescriptorByKey:
///----------------------------------------------------------------------------------------------------
const FestivalDescriptor* GetFestivalDescriptorByKey(
    const std::string& aKey)
{
    for (const FestivalDescriptor& festival :
        FestivalDescriptors)
    {
        if (aKey == festival.Key)
        {
            return &festival;
        }
    }

    return nullptr;
}

///----------------------------------------------------------------------------------------------------
/// FindNextFestivalDate:
///     Finds the next English "Month D, YYYY" date in wiki intro text.
///----------------------------------------------------------------------------------------------------
bool FindNextFestivalDate(
    const std::string& aText,
    size_t aStart,
    FestivalDate& aDate,
    size_t& aNextPosition)
{
    static const char* monthNames[] =
    {
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"
    };

    size_t bestPosition = std::string::npos;
    int bestMonth = 0;
    size_t bestMonthLength = 0;

    for (int index = 0; index < 12; ++index)
    {
        const std::string monthName =
            monthNames[index];

        const size_t position =
            aText.find(
                monthName,
                aStart
            );

        if (position != std::string::npos &&
            (bestPosition == std::string::npos ||
                position < bestPosition))
        {
            bestPosition = position;
            bestMonth = index + 1;
            bestMonthLength =
                monthName.size();
        }
    }

    if (bestPosition == std::string::npos)
    {
        return false;
    }

    size_t pos =
        bestPosition + bestMonthLength;

    while (pos < aText.size() &&
        std::isspace(
            static_cast<unsigned char>(
                aText[pos])))
    {
        ++pos;
    }

    if (pos >= aText.size() ||
        !std::isdigit(
            static_cast<unsigned char>(
                aText[pos])))
    {
        return false;
    }

    int day = 0;

    while (pos < aText.size() &&
        std::isdigit(
            static_cast<unsigned char>(
                aText[pos])))
    {
        day =
            (day * 10) +
            (aText[pos] - '0');

        ++pos;
    }

    while (pos < aText.size() &&
        (aText[pos] == ',' ||
            std::isspace(
                static_cast<unsigned char>(
                    aText[pos]))))
    {
        ++pos;
    }

    if (pos + 4 > aText.size())
    {
        return false;
    }

    int year = 0;

    for (int index = 0; index < 4; ++index)
    {
        if (!std::isdigit(
            static_cast<unsigned char>(
                aText[pos + index])))
        {
            return false;
        }

        year =
            (year * 10) +
            (aText[pos + index] - '0');
    }

    aDate.Year = year;
    aDate.Month = bestMonth;
    aDate.Day = day;
    aNextPosition = pos + 4;

    return
        aDate.Year > 2000 &&
        aDate.Month >= 1 &&
        aDate.Month <= 12 &&
        aDate.Day >= 1 &&
        aDate.Day <= 31;
}

///----------------------------------------------------------------------------------------------------
/// ParseFestivalDateRange:
///----------------------------------------------------------------------------------------------------
bool ParseFestivalDateRange(
    const std::string& aText,
    FestivalDate& aStartDate,
    FestivalDate& aEndDate)
{
    size_t nextPosition = 0;

    if (!FindNextFestivalDate(
        aText,
        0,
        aStartDate,
        nextPosition))
    {
        return false;
    }

    if (!FindNextFestivalDate(
        aText,
        nextPosition,
        aEndDate,
        nextPosition))
    {
        return false;
    }

    return true;
}

///----------------------------------------------------------------------------------------------------
/// FestivalDateKey:
///----------------------------------------------------------------------------------------------------
int FestivalDateKey(
    const FestivalDate& aDate)
{
    return
        (aDate.Year * 10000) +
        (aDate.Month * 100) +
        aDate.Day;
}

///----------------------------------------------------------------------------------------------------
/// GetLocalFestivalDate:
///----------------------------------------------------------------------------------------------------
FestivalDate GetLocalFestivalDate()
{
    SYSTEMTIME localTime = {};
    GetLocalTime(&localTime);

    FestivalDate date;
    date.Year = localTime.wYear;
    date.Month = localTime.wMonth;
    date.Day = localTime.wDay;

    return date;
}

///----------------------------------------------------------------------------------------------------
/// IsFestivalDateActive:
///----------------------------------------------------------------------------------------------------
bool IsFestivalDateActive(
    const FestivalDate& aToday,
    const FestivalDate& aStartDate,
    const FestivalDate& aEndDate)
{
    const int todayKey =
        FestivalDateKey(aToday);

    return
        todayKey >=
        FestivalDateKey(aStartDate) &&
        todayKey <=
        FestivalDateKey(aEndDate);
}

///----------------------------------------------------------------------------------------------------
/// KnownFestivalWindow:
///     Offline safety net for confirmed, year-specific dates only. NEVER infer
///     the same dates for later years. Update the list after official schedules
///     are published. The main forum calendar remains the primary detector.
///
///     DateKey is YYYYMMDD (UTC calendar date). EndMinuteUTC is the first
///     minute the festival is considered over; 1440 means the end of the UTC
///     calendar day when a more precise shutdown time is not documented.
///
///     Sources checked 2026-11-03:
///     - Lunar New Year: official forum Feb 03 - Feb 24, 2026.
///     - Super Adventure Festival: ArenaNet news Apr 14 - May 05, 2026.
///     - Dragon Bash: GW2 Wiki event history Jun 02 - Jun 23, 2026.
///     - Four Winds: ArenaNet update Aug 11 - Sep 01, 2026.
///     - Halloween: ArenaNet Oct 13 - Nov 03, 2026; GW2 Wiki advertises
///       noon Pacific / UTC-7 on Nov 03 as the cutoff (19:00 UTC).
///     - Wintersday 2026 was NOT added: a final official end date had not
///       been confirmed at the time this fallback was authored.
///----------------------------------------------------------------------------------------------------
struct KnownFestivalWindow
{
    const char* Key;
    FestivalDate Start;
    FestivalDate End;
    int EndMinuteUTC = 1440;
};

const std::vector<KnownFestivalWindow> KnownFestivalWindows =
{
    { "lunar_new_year",            { 2026, 2, 3 },  { 2026, 2, 24 }, 1440 },
    { "super_adventure_festival", { 2026, 4, 14 }, { 2026, 5, 5 },  1440 },
    { "dragon_bash",              { 2026, 6, 2 },  { 2026, 6, 23 }, 1440 },
    { "festival_of_the_four_winds", { 2026, 8, 11 }, { 2026, 9, 1 }, 1440 },
    { "halloween",                { 2026, 10, 13 }, { 2026, 11, 3 }, 1140 }
};

const KnownFestivalWindow* FindKnownFestivalWindow(const std::string& aKey)
{
    for (const KnownFestivalWindow& window : KnownFestivalWindows)
    {
        if (aKey == window.Key)
        {
            return &window;
        }
    }

    return nullptr;
}

bool IsWithinKnownFestivalWindow(
    const KnownFestivalWindow& aWindow,
    const SYSTEMTIME& aUtc)
{
    const FestivalDate today =
    {
        static_cast<int>(aUtc.wYear),
        static_cast<int>(aUtc.wMonth),
        static_cast<int>(aUtc.wDay)
    };

    const int dateKey = FestivalDateKey(today);
    const int startKey = FestivalDateKey(aWindow.Start);
    const int endKey = FestivalDateKey(aWindow.End);

    if (dateKey < startKey || dateKey > endKey)
    {
        return false;
    }

    if (dateKey == endKey)
    {
        const int currentMinuteUTC =
            static_cast<int>(aUtc.wHour) * 60 +
            static_cast<int>(aUtc.wMinute);

        if (currentMinuteUTC >= aWindow.EndMinuteUTC)
        {
            return false;
        }
    }

    return true;
}

/// Return a verified, offline result when the live calendar cannot be read.
/// Unknown dates are never guessed and do not become false positives.
FestivalScheduleResult OfflineFestivalScheduleFallback(
    const std::string& aCalendarError)
{
    FestivalScheduleResult result;
    result.Success = true;
    result.UsedOfflineFallback = true;

    SYSTEMTIME utc = {};
    GetSystemTime(&utc);

    for (const KnownFestivalWindow& window : KnownFestivalWindows)
    {
        if (!IsWithinKnownFestivalWindow(window, utc))
        {
            continue;
        }

        const FestivalDescriptor* festival =
            GetFestivalDescriptorByKey(window.Key);

        if (festival == nullptr)
        {
            continue;
        }

        result.Active = true;
        result.FestivalKey = festival->Key;
        result.FestivalName = festival->DisplayName;
        result.StartDate = window.Start;
        result.EndDate = window.End;
        return result;
    }

    // A failed network request does not prove there is no festival. In an
    // uncovered year/window we make no positive claim and display the error.
    result.Active = false;
    result.Error =
        "Official festival calendar unavailable (" + aCalendarError +
        "); no verified offline festival is active. "
        "Dates for future festivals must be confirmed.";
    return result;
}

/// The clock guard runs every frame, independent of the 12-hour HTTP refresh.
/// It also rejects stale 'Happening Now' HTML after the known end cutoff.
void ExpireFestivalAfterVerifiedWindow()
{
    if (!activeFestivalAvailable)
    {
        return;
    }

    const KnownFestivalWindow* window =
        FindKnownFestivalWindow(activeFestivalKey);

    if (window == nullptr)
    {
        return;
    }

    SYSTEMTIME utc = {};
    GetSystemTime(&utc);

    if (IsWithinKnownFestivalWindow(*window, utc))
    {
        return;
    }

    activeFestivalAvailable = false;
    activeFestivalKey.clear();
    activeFestivalName.clear();
    activeFestivalWikiPageTitle.clear();
    activeFestivalStartDate = {};
    activeFestivalEndDate = {};

    festivalMatchedCategoryNames.clear();
    festivalCategoryIDs.clear();
    festivalOpportunityIDs.clear();
    festivalDefinitionsReady = false;
    festivalOpportunityScanDirty = false;
    festivalDetectionSource.clear();
    festivalScheduleStatus =
        "Verified festival dates have ended. "
        "Waiting for the next official calendar update.";
}

///----------------------------------------------------------------------------------------------------
/// BuildFestivalWikiPageTitle:
///----------------------------------------------------------------------------------------------------
std::string BuildFestivalWikiPageTitle(
    const FestivalDescriptor& aFestival,
    int aYear)
{
    return
        std::string(aFestival.WikiPageBase) +
        " " +
        std::to_string(aYear);
}

///----------------------------------------------------------------------------------------------------
/// FetchFestivalIntro:
///----------------------------------------------------------------------------------------------------
bool FetchFestivalIntro(
    const std::string& aPageTitle,
    std::string& aIntro,
    std::string& aError)
{
    aIntro.clear();
    aError.clear();

    const std::string encodedTitle =
        UrlEncodeAscii(aPageTitle);

    const std::string pathUtf8 =
        "/api.php?action=parse"
        "&prop=wikitext"
        "&section=0"
        "&redirects=1"
        "&format=json"
        "&formatversion=2"
        "&page=" +
        encodedTitle;

    const std::wstring path(
        pathUtf8.begin(),
        pathUtf8.end()
    );

    DWORD statusCode = 0;
    std::string response;

    if (!WikiHttpGet(
        path,
        statusCode,
        response,
        aError))
    {
        return false;
    }

    if (statusCode != 200)
    {
        if (statusCode == 403)
        {
            aError =
                "Guild Wars 2 Wiki denied the schedule request (HTTP 403).";
        }
        else
        {
            aError =
                "Guild Wars 2 Wiki returned HTTP " +
                std::to_string(statusCode) + ".";
        }

        return false;
    }

    // action=parse returns an error object if the page does not exist.
    if (response.find("\"error\"") !=
        std::string::npos)
    {
        aError =
            "Festival schedule page is not published yet.";

        return false;
    }

    aIntro =
        ExtractJsonStringField(
            response,
            "wikitext"
        );

    if (aIntro.empty())
    {
        aError =
            "Festival schedule page did not contain readable wikitext.";

        return false;
    }

    return true;
}

///----------------------------------------------------------------------------------------------------
/// FetchFestivalSchedule:
///     Uses the Wiki's year-specific festival pages as the live schedule.
///----------------------------------------------------------------------------------------------------
FestivalScheduleResult FetchFestivalSchedule()
{
    FestivalScheduleResult result;

    DWORD statusCode = 0;
    std::string response;
    std::string error;

    if (!ForumHttpGet(
        L"/events/",
        statusCode,
        response,
        error))
    {
        return OfflineFestivalScheduleFallback(
            error.empty() ? "request failed" : error
        );
    }

    if (statusCode != 200)
    {
        return OfflineFestivalScheduleFallback(
            "HTTP " + std::to_string(statusCode)
        );
    }

    const std::string lowerResponse =
        ToLowerCopy(response);

    size_t searchPosition = 0;
    bool sawHappeningNow = false;

    while (true)
    {
        const size_t happeningPosition =
            lowerResponse.find(
                "happening now",
                searchPosition
            );

        if (happeningPosition ==
            std::string::npos)
        {
            break;
        }

        sawHappeningNow = true;

        const size_t proposedWindowEnd =
            happeningPosition +
            static_cast<size_t>(12000);

        const size_t windowEnd =
            proposedWindowEnd < lowerResponse.size()
            ? proposedWindowEnd
            : lowerResponse.size();

        const std::string nearby =
            lowerResponse.substr(
                happeningPosition,
                windowEnd - happeningPosition
            );

        for (const FestivalDescriptor& festival :
            FestivalDescriptors)
        {
            const std::string primaryName =
                ToLowerCopy(
                    festival.DisplayName
                );

            std::string alternateName;

            if (std::string(festival.Key) ==
                "halloween")
            {
                alternateName =
                    "shadow of the mad king";
            }
            else if (std::string(festival.Key) ==
                "wintersday")
            {
                alternateName =
                    "wintersday";
            }

            const bool matchesPrimary =
                nearby.find(primaryName) !=
                std::string::npos;

            const bool matchesAlternate =
                !alternateName.empty() &&
                nearby.find(alternateName) !=
                std::string::npos;

            if (!matchesPrimary &&
                !matchesAlternate)
            {
                continue;
            }

            result.Success = true;
            result.Active = true;
            result.FestivalKey =
                festival.Key;
            result.FestivalName =
                festival.DisplayName;

            return result;
        }

        searchPosition =
            happeningPosition + 13;
    }

    result.Success = true;
    result.Active = false;

    if (sawHappeningNow)
    {
        result.Error =
            "The official calendar has active events, but none matched a supported annual festival.";
    }

    return result;
}


///----------------------------------------------------------------------------------------------------
/// StartFestivalScheduleSync:
///----------------------------------------------------------------------------------------------------
void StartFestivalScheduleSync()
{
    if (festivalScheduleSyncActive)
    {
        return;
    }

    festivalScheduleSyncActive = true;
    festivalScheduleSyncAttempted = true;
    festivalScheduleStatus =
        "Checking the official Guild Wars 2 event calendar...";

    festivalScheduleFuture =
        std::async(
            std::launch::async,
            []()
            {
                return FetchFestivalSchedule();
            }
        );
}

///----------------------------------------------------------------------------------------------------
/// UpdateFestivalScheduleSync:
///----------------------------------------------------------------------------------------------------
void UpdateFestivalScheduleSync()
{
    if (!festivalScheduleSyncActive)
    {
        return;
    }

    if (festivalScheduleFuture.wait_for(
        std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    FestivalScheduleResult result =
        festivalScheduleFuture.get();

    festivalScheduleSyncActive = false;
    festivalScheduleLastCheck =
        std::chrono::steady_clock::now();
    festivalScheduleLastCheckValid = true;

    if (!result.Success)
    {
        festivalScheduleStatus =
            result.Error.empty()
            ? "Festival schedule check failed."
            : result.Error;

        return;
    }

    activeFestivalAvailable =
        result.Active;

    activeFestivalKey =
        result.FestivalKey;

    activeFestivalName =
        result.FestivalName;

    activeFestivalWikiPageTitle =
        result.WikiPageTitle;

    activeFestivalStartDate =
        result.StartDate;

    activeFestivalEndDate =
        result.EndDate;

    festivalMatchedCategoryNames.clear();
    festivalCategoryIDs.clear();
    festivalOpportunityIDs.clear();
    festivalDefinitionsReady = false;
    festivalOpportunityScanDirty = true;

    if (result.Active)
    {
        festivalDetectionSource = result.UsedOfflineFallback
            ? "Detected from verified 2026 offline festival dates (calendar unavailable)."
            : "Detected from the official Guild Wars 2 event calendar.";

        festivalScheduleStatus = result.UsedOfflineFallback
            ? "Official calendar unavailable; using the verified 2026 festival schedule."
            : "Active festival detected from the official GW2 event calendar.";
    }
    else
    {
        festivalDetectionSource.clear();
        festivalScheduleStatus = result.UsedOfflineFallback
            ? result.Error
            : "No supported annual festival is marked active on the official GW2 event calendar.";
    }

    // A previously cached live response may be stale on a festival's end day.
    ExpireFestivalAfterVerifiedWindow();
}

///----------------------------------------------------------------------------------------------------
/// MaybeRefreshFestivalSchedule:
///----------------------------------------------------------------------------------------------------
void MaybeRefreshFestivalSchedule()
{
    if (festivalScheduleSyncActive)
    {
        return;
    }

    if (!festivalScheduleSyncAttempted)
    {
        StartFestivalScheduleSync();
        return;
    }

    if (!festivalScheduleLastCheckValid)
    {
        return;
    }

    const long long elapsedHours =
        std::chrono::duration_cast<
        std::chrono::hours>(
            std::chrono::steady_clock::now() -
            festivalScheduleLastCheck
        ).count();

    if (elapsedHours >= 12)
    {
        StartFestivalScheduleSync();
    }
}


///----------------------------------------------------------------------------------------------------
/// LookupMapName:
///----------------------------------------------------------------------------------------------------
MapLookupResult LookupMapName(uint32_t aMapID)
{
    MapLookupResult result;
    result.MapID = aMapID;

    DWORD statusCode = 0;
    std::string response;
    std::string error;

    const std::wstring path =
        L"/v2/maps/" + std::to_wstring(aMapID) + L"?lang=en";

    if (!HttpGet(path, "", statusCode, response, error))
    {
        result.Error = error;
        return result;
    }

    if (statusCode != 200)
    {
        result.Error =
            "GW2 API returned HTTP " + std::to_string(statusCode) + ".";
        return result;
    }

    result.Name = ExtractJsonStringField(response, "name");
    result.RegionName = ExtractJsonStringField(response, "region_name");

    if (result.Name.empty())
    {
        result.Error = "Map name was missing from the GW2 API response.";
        return result;
    }

    result.Success = true;
    return result;
}

///----------------------------------------------------------------------------------------------------
/// ValidateApiKey:
///----------------------------------------------------------------------------------------------------
ApiKeyValidationResult ValidateApiKey(const std::string& aApiKey)
{
    ApiKeyValidationResult result;

    DWORD statusCode = 0;
    std::string response;
    std::string error;

    if (!HttpGet(
        L"/v2/tokeninfo",
        aApiKey,
        statusCode,
        response,
        error))
    {
        result.Error = error;
        return result;
    }

    if (statusCode == 401 || statusCode == 403)
    {
        result.Error = "API key was rejected.";
        return result;
    }

    if (statusCode != 200)
    {
        result.Error =
            "GW2 API returned HTTP " + std::to_string(statusCode) + ".";
        return result;
    }

    result.HasAccountPermission =
        JsonStringArrayContains(response, "permissions", "account");
    result.HasProgressionPermission =
        JsonStringArrayContains(response, "permissions", "progression");

    if (!result.HasAccountPermission)
    {
        result.Error = "API key is missing the account permission.";
        return result;
    }

    if (!result.HasProgressionPermission)
    {
        result.Error =
            "API key is valid, but it is missing the progression permission.";
        return result;
    }

    result.Success = true;
    return result;
}

///----------------------------------------------------------------------------------------------------
/// ParseAchievementProgress:
///----------------------------------------------------------------------------------------------------
AchievementSyncResult ParseAchievementProgress(const std::string& aJson)
{
    AchievementSyncResult result;

    size_t pos = 0;

    while (true)
    {
        size_t objectStart = aJson.find('{', pos);

        if (objectStart == std::string::npos)
        {
            break;
        }

        int depth = 0;
        size_t objectEnd = std::string::npos;

        for (size_t i = objectStart; i < aJson.size(); ++i)
        {
            if (aJson[i] == '{')
            {
                ++depth;
            }
            else if (aJson[i] == '}')
            {
                --depth;

                if (depth == 0)
                {
                    objectEnd = i;
                    break;
                }
            }
        }

        if (objectEnd == std::string::npos)
        {
            result.Error = "Achievement response contained incomplete JSON.";
            return result;
        }

        const std::string objectJson =
            aJson.substr(objectStart, objectEnd - objectStart + 1);

        AchievementProgress progress;

        if (ExtractJsonIntField(objectJson, "id", progress.ID))
        {
            ExtractJsonIntField(objectJson, "current", progress.Current);
            ExtractJsonIntField(objectJson, "max", progress.Max);
            ExtractJsonBoolField(objectJson, "done", progress.Done);
            progress.Bits = ExtractJsonIntArray(objectJson, "bits");

            result.Records.push_back(progress);

            if (progress.Done)
            {
                ++result.Completed;
            }
            else
            {
                ++result.InProgress;
            }
        }

        pos = objectEnd + 1;
    }

    result.RecordsLoaded = result.Records.size();

    if (result.RecordsLoaded == 0)
    {
        result.Error = "No achievement records were found in the API response.";
        return result;
    }

    result.Success = true;
    return result;
}

///----------------------------------------------------------------------------------------------------
/// SyncAchievements:
///----------------------------------------------------------------------------------------------------
AchievementSyncResult SyncAchievements(const std::string& aApiKey)
{
    AchievementSyncResult result;

    DWORD statusCode = 0;
    std::string response;
    std::string error;

    if (!HttpGet(
        L"/v2/account/achievements",
        aApiKey,
        statusCode,
        response,
        error))
    {
        result.Error = error;
        return result;
    }

    if (statusCode == 401 || statusCode == 403)
    {
        result.Error =
            "Achievement request was rejected. Revalidate the API key.";
        return result;
    }

    if (statusCode != 200)
    {
        result.Error =
            "GW2 API returned HTTP " + std::to_string(statusCode) + ".";
        return result;
    }

    return ParseAchievementProgress(response);
}

///----------------------------------------------------------------------------------------------------
/// StripGw2Markup:
///     Removes simple Guild Wars 2 text markup such as <c=@reminder>...</c>.
///----------------------------------------------------------------------------------------------------
std::string StripGw2Markup(const std::string& aText)
{
    std::string result;
    result.reserve(aText.size());

    bool insideTag = false;

    for (char ch : aText)
    {
        if (ch == '<')
        {
            insideTag = true;
            continue;
        }

        if (ch == '>' && insideTag)
        {
            insideTag = false;
            continue;
        }

        if (!insideTag)
        {
            result.push_back(ch);
        }
    }

    return result;
}

///----------------------------------------------------------------------------------------------------
/// ExtractJsonArraySection:
///     Returns a named JSON array, including its outer [ ] characters.
///----------------------------------------------------------------------------------------------------
std::string ExtractJsonArraySection(
    const std::string& aJson,
    const std::string& aField)
{
    const std::string key = "\"" + aField + "\"";
    size_t keyPos = aJson.find(key);

    if (keyPos == std::string::npos)
    {
        return {};
    }

    size_t start = aJson.find('[', keyPos + key.length());

    if (start == std::string::npos)
    {
        return {};
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (size_t i = start; i < aJson.size(); ++i)
    {
        const char ch = aJson[i];

        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (ch == '\\')
            {
                escaped = true;
            }
            else if (ch == '"')
            {
                inString = false;
            }

            continue;
        }

        if (ch == '"')
        {
            inString = true;
            continue;
        }

        if (ch == '[')
        {
            ++depth;
        }
        else if (ch == ']')
        {
            --depth;

            if (depth == 0)
            {
                return aJson.substr(start, i - start + 1);
            }
        }
    }

    return {};
}

///----------------------------------------------------------------------------------------------------
/// SplitTopLevelObjects:
///     Splits a JSON array into top-level object strings while ignoring nested objects.
///----------------------------------------------------------------------------------------------------
std::vector<std::string> SplitTopLevelObjects(const std::string& aJson)
{
    std::vector<std::string> objects;

    int depth = 0;
    size_t objectStart = std::string::npos;
    bool inString = false;
    bool escaped = false;

    for (size_t i = 0; i < aJson.size(); ++i)
    {
        const char ch = aJson[i];

        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (ch == '\\')
            {
                escaped = true;
            }
            else if (ch == '"')
            {
                inString = false;
            }

            continue;
        }

        if (ch == '"')
        {
            inString = true;
            continue;
        }

        if (ch == '{')
        {
            if (depth == 0)
            {
                objectStart = i;
            }

            ++depth;
        }
        else if (ch == '}')
        {
            if (depth > 0)
            {
                --depth;

                if (depth == 0 && objectStart != std::string::npos)
                {
                    objects.push_back(
                        aJson.substr(objectStart, i - objectStart + 1)
                    );

                    objectStart = std::string::npos;
                }
            }
        }
    }

    return objects;
}

///----------------------------------------------------------------------------------------------------
/// UBFetchPublicVpsIndex:
///     GETs only a public map ID. No GW2 key, Authorization header,
///     account identity, character name, or account progress is sent here.
///     WinHTTP performs its normal HTTPS certificate validation.
///----------------------------------------------------------------------------------------------------
UBVpsLookupResult UBFetchPublicVpsIndex(uint32_t aMapID)
{
    UBVpsLookupResult result;
    result.MapID = aMapID;

    if (aMapID == 0)
    {
        result.Error = "No map ID available.";
        return result;
    }

    HINTERNET session = WinHttpOpen(
        L"UnfinishedBusiness/0.2 (public map index)",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!session)
    {
        result.Error = "Could not open VPS HTTP session.";
        return result;
    }

    WinHttpSetTimeouts(session, 3000, 3000, 3000, 8000);

    HINTERNET connection = WinHttpConnect(
        session, L"copperclaw-api.duckdns.org",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection)
    {
        result.Error = "Could not connect to public VPS index.";
        WinHttpCloseHandle(session);
        return result;
    }

    // The sole variable in the URL is the decimal public map ID.
    const std::wstring path = L"/ub/v1/maps/" +
        std::to_wstring(aMapID) + L"/achievements";
    HINTERNET request = WinHttpOpenRequest(
        connection, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!request)
    {
        result.Error = "Could not create public VPS request.";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    std::string response;
    DWORD status = 0;
    bool transportOK = false;
    const BOOL sent = WinHttpSendRequest(
        request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (sent && WinHttpReceiveResponse(request, nullptr))
    {
        DWORD statusBytes = sizeof(status);
        if (WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusBytes,
            WINHTTP_NO_HEADER_INDEX) && status == 200)
        {
            transportOK = true;
            constexpr size_t maxJsonBytes = 1024 * 1024;
            for (;;)
            {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available))
                {
                    transportOK = false;
                    break;
                }
                if (available == 0)
                {
                    break;
                }
                if (response.size() + static_cast<size_t>(available) >
                    maxJsonBytes)
                {
                    result.Error = "VPS response exceeded the size limit.";
                    transportOK = false;
                    break;
                }
                std::string chunk(available, '\0');
                DWORD received = 0;
                if (!WinHttpReadData(request, &chunk[0], available, &received))
                {
                    transportOK = false;
                    break;
                }
                if (received == 0)
                {
                    transportOK = false;
                    break;
                }
                chunk.resize(received);
                response += chunk;
            }
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (!transportOK)
    {
        if (result.Error.empty())
        {
            result.Error = status != 0
                ? "VPS index returned HTTP " + std::to_string(status) + "."
                : "Public VPS index request failed.";
        }
        return result;
    }

    // Validate the schema and map identity before accepting public candidates.
    int schema = 0;
    int responseMapID = 0;
    if (!ExtractJsonIntField(response, "schema_version", schema) || schema != 1)
    {
        result.Error = "Unexpected public VPS index schema.";
        return result;
    }
    const size_t mapKey = response.find("\"map\"");
    const size_t mapClose = mapKey != std::string::npos
        ? response.find('}', mapKey) : std::string::npos;
    if (mapClose == std::string::npos ||
        !ExtractJsonIntField(response.substr(mapKey, mapClose - mapKey + 1),
            "id", responseMapID) ||
        responseMapID <= 0 || static_cast<uint32_t>(responseMapID) != aMapID)
    {
        result.Error = "VPS returned an unexpected map ID.";
        return result;
    }

    const std::string candidatesArray =
        ExtractJsonArraySection(response, "candidates");
    if (candidatesArray.empty())
    {
        result.Error = "Public VPS candidate list was missing.";
        return result;
    }

    std::unordered_set<int> seen;
    for (const std::string& object : SplitTopLevelObjects(candidatesArray))
    {
        UBVpsCandidate candidate;
        if (!ExtractJsonIntField(object, "id", candidate.ID) ||
            candidate.ID <= 0 || !seen.insert(candidate.ID).second)
        {
            continue;
        }
        candidate.Scope = ExtractJsonStringField(object, "scope");
        if (candidate.Scope != "map" && candidate.Scope != "regional")
        {
            continue;
        }
        candidate.Confidence = ExtractJsonStringField(object, "confidence");
        candidate.Source = ExtractJsonStringField(object, "source");
        candidate.Evidence = ExtractJsonStringField(object, "evidence");
        result.Candidates.push_back(std::move(candidate));
        if (result.Candidates.size() >= 200)
        {
            break;
        }
    }

    ExtractJsonBoolField(response, "truncated", result.Truncated);
    result.Success = true;
    return result;
}

///----------------------------------------------------------------------------------------------------
/// UBUpdatePublicVpsIndex:
///     All state mutations occur on the render thread after async completion.
///     A map change never applies an old response to the new map.
///----------------------------------------------------------------------------------------------------
void UBUpdatePublicVpsIndex()
{
    const uint32_t mapID = MumbleLink ? MumbleLink->Context.MapID : 0;

    if (ubVpsLookupActive && ubVpsLookupFuture.valid() &&
        ubVpsLookupFuture.wait_for(std::chrono::milliseconds(0)) ==
        std::future_status::ready)
    {
        UBVpsLookupResult completed = ubVpsLookupFuture.get();
        ubVpsLookupActive = false;
        ubVpsInFlightMapID = 0;
        if (completed.Success)
        {
            const uint32_t completedMap = completed.MapID;
            ubVpsCache[completedMap] = std::move(completed);
            if (ubVpsIndexEnabled && completedMap == mapID)
            {
                ubVpsRescanPending = true;
            }
        }
        else if (completed.MapID == mapID)
        {
            ubVpsStatus = completed.Error.empty()
                ? "VPS index unavailable; local scan continues."
                : completed.Error + " Local scan continues.";
        }
    }

    if (!ubVpsIndexEnabled)
    {
        ubVpsStatus = "Off (local scan only).";
        return;
    }
    if (mapID == 0)
    {
        ubVpsStatus = "Waiting for a public map ID...";
        return;
    }
    auto cached = ubVpsCache.find(mapID);
    if (cached != ubVpsCache.end())
    {
        ubVpsStatus = "Loaded public map index (candidate data, not proof).";
        return;
    }
    if (ubVpsLookupActive)
    {
        ubVpsStatus = "Loading public map index in background...";
        return;
    }
    if (ubVpsAttemptedMaps.count(mapID) != 0)
    {
        // Do not retry a failing public endpoint every frame.
        ubVpsStatus = "VPS index unavailable for this map; local scan continues.";
        return;
    }

    ubVpsAttemptedMaps.insert(mapID);
    ubVpsLookupActive = true;
    ubVpsInFlightMapID = mapID;
    ubVpsStatus = "Loading public map index in background...";
    ubVpsLookupFuture = std::async(std::launch::async,
        [mapID]() { return UBFetchPublicVpsIndex(mapID); });
}

///----------------------------------------------------------------------------------------------------
/// ParseAchievementDefinitionExtras:
///     Reads AP tiers and useful reward types from a public achievement definition.
///----------------------------------------------------------------------------------------------------
void ParseAchievementDefinitionExtras(
    const std::string& aJson,
    AchievementDefinition& aDefinition)
{
    aDefinition.Tiers.clear();
    aDefinition.RewardsMastery = false;
    aDefinition.MasteryRegion.clear();
    aDefinition.RewardsTitle = false;
    aDefinition.RewardCoins = 0;
    aDefinition.RewardItemEntries = 0;

    // Parse tiers directly from the tiers array. This intentionally avoids
    // relying on the generic object splitter because achievement definitions
    // contain several nested arrays/objects.
    const std::string tiersJson =
        ExtractJsonArraySection(aJson, "tiers");

    size_t tierPos = 0;

    while (!tiersJson.empty())
    {
        size_t countKey = tiersJson.find("\"count\"", tierPos);

        if (countKey == std::string::npos)
        {
            break;
        }

        size_t objectEnd = tiersJson.find('}', countKey);

        if (objectEnd == std::string::npos)
        {
            break;
        }

        const std::string tierChunk =
            tiersJson.substr(countKey, objectEnd - countKey + 1);

        AchievementTier tier;

        if (ExtractJsonIntField(tierChunk, "count", tier.Count) &&
            ExtractJsonIntField(tierChunk, "points", tier.Points))
        {
            aDefinition.Tiers.push_back(tier);
        }

        tierPos = objectEnd + 1;
    }

    const std::string rewardsJson =
        ExtractJsonArraySection(aJson, "rewards");

    if (rewardsJson.empty())
    {
        return;
    }

    size_t rewardPos = 0;

    while (true)
    {
        size_t typeKey = rewardsJson.find("\"type\"", rewardPos);

        if (typeKey == std::string::npos)
        {
            break;
        }

        size_t objectStart = rewardsJson.rfind('{', typeKey);
        size_t objectEnd = rewardsJson.find('}', typeKey);

        if (objectStart == std::string::npos ||
            objectEnd == std::string::npos ||
            objectEnd <= objectStart)
        {
            break;
        }

        const std::string rewardJson =
            rewardsJson.substr(
                objectStart,
                objectEnd - objectStart + 1
            );

        const std::string type =
            ExtractJsonStringField(rewardJson, "type");

        if (type == "Mastery")
        {
            aDefinition.RewardsMastery = true;
            aDefinition.MasteryRegion =
                ExtractJsonStringField(rewardJson, "region");
        }
        else if (type == "Title")
        {
            aDefinition.RewardsTitle = true;
        }
        else if (type == "Coins")
        {
            int count = 0;

            if (ExtractJsonIntField(rewardJson, "count", count))
            {
                aDefinition.RewardCoins += count;
            }
        }
        else if (type == "Item")
        {
            ++aDefinition.RewardItemEntries;
        }

        rewardPos = objectEnd + 1;
    }
}

///----------------------------------------------------------------------------------------------------
/// GetTotalAchievementAP:
///----------------------------------------------------------------------------------------------------
int GetTotalAchievementAP(const AchievementDefinition& aDefinition)
{
    int total = 0;

    for (const AchievementTier& tier : aDefinition.Tiers)
    {
        total += tier.Points;
    }

    return total;
}

///----------------------------------------------------------------------------------------------------
/// GetRemainingAchievementAP:
///     Estimates AP still available from tiers above the account's current count.
///----------------------------------------------------------------------------------------------------
int GetRemainingAchievementAP(
    int aID,
    const AchievementDefinition& aDefinition)
{
    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    const int currentCount =
        progress != nullptr && progress->Current >= 0
        ? progress->Current
        : 0;

    int remaining = 0;

    for (const AchievementTier& tier : aDefinition.Tiers)
    {
        if (tier.Count > currentCount)
        {
            remaining += tier.Points;
        }
    }

    return remaining;
}

///----------------------------------------------------------------------------------------------------
/// BuildRewardSummary:
///----------------------------------------------------------------------------------------------------
std::string BuildRewardSummary(
    int aID,
    const AchievementDefinition& aDefinition)
{
    std::string summary;

    const int totalAP =
        GetTotalAchievementAP(aDefinition);

    const int remainingAP =
        GetRemainingAchievementAP(aID, aDefinition);

    if (totalAP > 0)
    {
        if (remainingAP > 0 && remainingAP < totalAP)
        {
            summary +=
                std::to_string(remainingAP) +
                " AP left / " +
                std::to_string(totalAP) +
                " total";
        }
        else if (remainingAP > 0)
        {
            summary +=
                std::to_string(remainingAP) +
                " AP";
        }
        else
        {
            summary +=
                std::to_string(totalAP) +
                " AP total";
        }
    }

    if (aDefinition.RewardsMastery)
    {
        if (!summary.empty())
        {
            summary += " | ";
        }

        summary += "Mastery Point";

        if (!aDefinition.MasteryRegion.empty())
        {
            summary += " (" + aDefinition.MasteryRegion + ")";
        }
    }

    if (aDefinition.RewardsTitle)
    {
        if (!summary.empty())
        {
            summary += " | ";
        }

        summary += "Title";
    }

    if (aDefinition.RewardCoins > 0)
    {
        if (!summary.empty())
        {
            summary += " | ";
        }

        const int gold = aDefinition.RewardCoins / 10000;
        const int silver = (aDefinition.RewardCoins % 10000) / 100;
        const int copper = aDefinition.RewardCoins % 100;

        if (gold > 0)
        {
            summary += std::to_string(gold) + "g ";
        }

        if (silver > 0 || gold > 0)
        {
            summary += std::to_string(silver) + "s ";
        }

        summary += std::to_string(copper) + "c";
    }

    if (aDefinition.RewardItemEntries > 0)
    {
        if (!summary.empty())
        {
            summary += " | ";
        }

        summary +=
            std::to_string(aDefinition.RewardItemEntries) +
            (aDefinition.RewardItemEntries == 1
                ? " item reward"
                : " item rewards");
    }

    return summary;
}

///----------------------------------------------------------------------------------------------------
/// BuildAchievementIdBatch:
///     Builds a comma-separated list of achievement IDs for GW2 API batch requests.
///----------------------------------------------------------------------------------------------------
std::string BuildAchievementIdBatch(
    const std::vector<int>& aIDs,
    size_t aStart,
    size_t aCount)
{
    std::string result;

    const size_t end =
        (aStart + aCount < aIDs.size())
        ? aStart + aCount
        : aIDs.size();

    for (size_t i = aStart; i < end; ++i)
    {
        if (!result.empty())
        {
            result += ",";
        }

        result += std::to_string(aIDs[i]);
    }

    return result;
}

///----------------------------------------------------------------------------------------------------
/// SyncAchievementDefinitions:
///     Fetches public definitions for the unfinished achievement IDs.
///----------------------------------------------------------------------------------------------------
AchievementDefinitionSyncResult SyncAchievementDefinitions(
    const std::vector<AchievementProgress>& aProgress)
{
    AchievementDefinitionSyncResult result;

    std::vector<int> unfinishedIDs;

    for (const AchievementProgress& progress : aProgress)
    {
        if (!progress.Done)
        {
            unfinishedIDs.push_back(progress.ID);
        }
    }

    if (unfinishedIDs.empty())
    {
        result.Error = "No unfinished achievement IDs were available.";
        return result;
    }

    const size_t batchSize = 100;

    for (size_t start = 0; start < unfinishedIDs.size(); start += batchSize)
    {
        const std::string idList =
            BuildAchievementIdBatch(unfinishedIDs, start, batchSize);

        const std::wstring path =
            L"/v2/achievements?ids=" +
            std::wstring(idList.begin(), idList.end()) +
            L"&lang=en";

        DWORD statusCode = 0;
        std::string response;
        std::string error;

        if (!HttpGet(path, "", statusCode, response, error))
        {
            result.Error = error;
            return result;
        }

        // 200 = every requested ID was valid.
        // 206 = partial content: at least one requested ID was unavailable,
        //       but the response still contains the valid achievement definitions.
        if (statusCode != 200 && statusCode != 206)
        {
            result.Error =
                "Achievement definition request returned HTTP " +
                std::to_string(statusCode) + ".";
            return result;
        }

        const std::vector<std::string> objects =
            SplitTopLevelObjects(response);

        for (const std::string& objectJson : objects)
        {
            AchievementDefinition definition;

            if (!ExtractJsonIntField(objectJson, "id", definition.ID))
            {
                continue;
            }

            definition.Name =
                ExtractJsonStringField(objectJson, "name");

            definition.Description =
                StripGw2Markup(
                    ExtractJsonStringField(
                        objectJson,
                        "description"
                    )
                );

            definition.Requirement =
                StripGw2Markup(
                    ExtractJsonStringField(
                        objectJson,
                        "requirement"
                    )
                );

            ParseAchievementDefinitionExtras(
                objectJson,
                definition
            );

            result.Definitions[definition.ID] =
                std::move(definition);
        }
    }

    result.DefinitionsLoaded = result.Definitions.size();

    if (result.DefinitionsLoaded == 0)
    {
        result.Error =
            "No achievement definitions were returned by the GW2 API.";
        return result;
    }

    result.Success = true;
    return result;
}

///----------------------------------------------------------------------------------------------------
/// StartAchievementDefinitionSync:
///----------------------------------------------------------------------------------------------------
void StartAchievementDefinitionSync()
{
    if (achievementDefinitionSyncActive ||
        achievementProgress.empty())
    {
        return;
    }

    const std::vector<AchievementProgress> progressCopy =
        achievementProgress;

    achievementDefinitionSyncActive = true;
    achievementDefinitionsLoaded = false;
    achievementDefinitionsLoadedCount = achievementDefinitions.size();
    // Keep the public definitions already loaded by a map/festival sync.
    achievementDefinitionStatus =
        "Loading unfinished achievement definitions...";

    achievementDefinitionSyncFuture = std::async(
        std::launch::async,
        [progressCopy]()
        {
            return SyncAchievementDefinitions(progressCopy);
        }
    );
}

///----------------------------------------------------------------------------------------------------
/// UpdateAchievementDefinitionSync:
///----------------------------------------------------------------------------------------------------
void UpdateAchievementDefinitionSync()
{
    if (!achievementDefinitionSyncActive)
    {
        return;
    }

    if (achievementDefinitionSyncFuture.wait_for(
        std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        return;
    }

    AchievementDefinitionSyncResult result =
        achievementDefinitionSyncFuture.get();

    achievementDefinitionSyncActive = false;

    if (!result.Success)
    {
        achievementDefinitionsLoaded = false;
        achievementDefinitionStatus =
            result.Error.empty()
            ? "Achievement definition sync failed."
            : result.Error;
        return;
    }

    // Merge instead of replacing: a map/festival request may have added
    // definitions while the account-wide download was running.
    for (auto& pair : result.Definitions)
    {
        achievementDefinitions[pair.first] = std::move(pair.second);
    }
    achievementDefinitionsLoadedCount = achievementDefinitions.size();
    achievementDefinitionsLoaded = true;
    // Re-evaluate the live map for newly discovered text references.
    lastOpportunityMapName.clear();

    const size_t missingDefinitions =
        achievementInProgress > achievementDefinitionsLoadedCount
        ? achievementInProgress - achievementDefinitionsLoadedCount
        : 0;

    if (missingDefinitions > 0)
    {
        achievementDefinitionStatus =
            "Achievement definitions loaded. " +
            std::to_string(missingDefinitions) +
            " unavailable/deprecated definition(s) were skipped.";
    }
    else
    {
        achievementDefinitionStatus =
            "Achievement definitions loaded successfully.";
    }
}

///----------------------------------------------------------------------------------------------------
/// ExtractCategoryAchievementIDs:
///     Supports both the older numeric achievement array and newer object form.
///----------------------------------------------------------------------------------------------------
std::vector<int> ExtractCategoryAchievementIDs(const std::string& aJson)
{
    const std::string key = "\"achievements\"";
    size_t keyPos = aJson.find(key);

    if (keyPos == std::string::npos)
    {
        return {};
    }

    size_t arrayStart = aJson.find('[', keyPos + key.length());

    if (arrayStart == std::string::npos)
    {
        return {};
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    size_t arrayEnd = std::string::npos;

    for (size_t i = arrayStart; i < aJson.size(); ++i)
    {
        const char ch = aJson[i];

        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (ch == '\\')
            {
                escaped = true;
            }
            else if (ch == '"')
            {
                inString = false;
            }

            continue;
        }

        if (ch == '"')
        {
            inString = true;
            continue;
        }

        if (ch == '[')
        {
            ++depth;
        }
        else if (ch == ']')
        {
            --depth;

            if (depth == 0)
            {
                arrayEnd = i;
                break;
            }
        }
    }

    if (arrayEnd == std::string::npos)
    {
        return {};
    }

    const std::string arrayJson =
        aJson.substr(arrayStart, arrayEnd - arrayStart + 1);

    const std::vector<std::string> objects =
        SplitTopLevelObjects(arrayJson);

    std::vector<int> ids;

    if (!objects.empty())
    {
        for (const std::string& objectJson : objects)
        {
            int id = 0;

            if (ExtractJsonIntField(objectJson, "id", id))
            {
                ids.push_back(id);
            }
        }

        return ids;
    }

    // Legacy schema: achievements is simply an array of integer IDs.
    return ExtractJsonIntArray(aJson, "achievements");
}

///----------------------------------------------------------------------------------------------------
/// SyncAchievementCategories:
///     Loads the public achievement category catalog.
///----------------------------------------------------------------------------------------------------
AchievementCategorySyncResult SyncAchievementCategories()
{
    AchievementCategorySyncResult result;

    DWORD statusCode = 0;
    std::string response;
    std::string error;

    // IMPORTANT:
    // The root category endpoint is the live index. Festival categories can
    // disappear from this list while their festival is inactive. Querying
    // ?ids=all can still expose dormant historical category objects, which
    // makes it unsuitable for detecting the currently active festival.
    if (!HttpGet(
        L"/v2/achievements/categories",
        "",
        statusCode,
        response,
        error))
    {
        result.Error = error;
        return result;
    }

    if (statusCode != 200 &&
        statusCode != 206)
    {
        result.Error =
            "Achievement category index returned HTTP " +
            std::to_string(statusCode) + ".";

        return result;
    }

    const std::vector<int> liveCategoryIDs =
        ParseRootJsonIntArray(response);

    if (liveCategoryIDs.empty())
    {
        result.Error =
            "No live achievement category IDs were returned by the GW2 API.";

        return result;
    }

    constexpr size_t batchSize = 100;

    for (size_t startIndex = 0;
        startIndex < liveCategoryIDs.size();
        startIndex += batchSize)
    {
        const std::string idList =
            BuildAchievementIdBatch(
                liveCategoryIDs,
                startIndex,
                batchSize
            );

        const std::wstring path =
            L"/v2/achievements/categories?ids=" +
            std::wstring(
                idList.begin(),
                idList.end()
            ) +
            L"&lang=en";

        statusCode = 0;
        response.clear();
        error.clear();

        if (!HttpGet(
            path,
            "",
            statusCode,
            response,
            error))
        {
            result.Error = error;
            return result;
        }

        if (statusCode != 200 &&
            statusCode != 206)
        {
            result.Error =
                "Achievement category detail request returned HTTP " +
                std::to_string(statusCode) + ".";

            return result;
        }

        const std::vector<std::string> objects =
            SplitTopLevelObjects(response);

        for (const std::string& objectJson :
            objects)
        {
            AchievementCategory category;

            if (!ExtractJsonIntField(
                objectJson,
                "id",
                category.ID))
            {
                continue;
            }

            category.Name =
                ExtractJsonStringField(
                    objectJson,
                    "name"
                );

            category.AchievementIDs =
                ExtractCategoryAchievementIDs(
                    objectJson
                );

            if (!category.Name.empty())
            {
                result.Categories.push_back(
                    std::move(category)
                );
            }
        }
    }

    if (result.Categories.empty())
    {
        result.Error =
            "No live achievement categories were returned by the GW2 API.";

        return result;
    }

    result.Success = true;
    return result;
}


///----------------------------------------------------------------------------------------------------
/// FetchAchievementDefinitionsForIDs:
///     Loads public definitions for an arbitrary set of achievement IDs.
///----------------------------------------------------------------------------------------------------
AchievementDefinitionSyncResult FetchAchievementDefinitionsForIDs(
    const std::vector<int>& aIDs)
{
    AchievementDefinitionSyncResult result;

    if (aIDs.empty())
    {
        result.Error = "No achievement IDs were supplied.";
        return result;
    }

    const size_t batchSize = 100;

    for (size_t start = 0; start < aIDs.size(); start += batchSize)
    {
        const std::string idList =
            BuildAchievementIdBatch(aIDs, start, batchSize);

        const std::wstring path =
            L"/v2/achievements?ids=" +
            std::wstring(idList.begin(), idList.end()) +
            L"&lang=en";

        DWORD statusCode = 0;
        std::string response;
        std::string error;

        if (!HttpGet(path, "", statusCode, response, error))
        {
            result.Error = error;
            return result;
        }

        if (statusCode != 200 && statusCode != 206)
        {
            result.Error =
                "Current-map definition request returned HTTP " +
                std::to_string(statusCode) + ".";
            return result;
        }

        const std::vector<std::string> objects =
            SplitTopLevelObjects(response);

        for (const std::string& objectJson : objects)
        {
            AchievementDefinition definition;

            if (!ExtractJsonIntField(objectJson, "id", definition.ID))
            {
                continue;
            }

            definition.Name =
                ExtractJsonStringField(objectJson, "name");

            definition.Description =
                StripGw2Markup(
                    ExtractJsonStringField(
                        objectJson,
                        "description"
                    )
                );

            definition.Requirement =
                StripGw2Markup(
                    ExtractJsonStringField(
                        objectJson,
                        "requirement"
                    )
                );

            ParseAchievementDefinitionExtras(
                objectJson,
                definition
            );

            result.Definitions[definition.ID] =
                std::move(definition);
        }
    }

    result.DefinitionsLoaded = result.Definitions.size();

    if (result.DefinitionsLoaded == 0)
    {
        result.Error =
            "No current-map achievement definitions were returned.";
        return result;
    }

    result.Success = true;
    return result;
}

///----------------------------------------------------------------------------------------------------
/// FindAchievementProgress:
///----------------------------------------------------------------------------------------------------
const AchievementProgress* FindAchievementProgress(int aID)
{
    for (const AchievementProgress& progress : achievementProgress)
    {
        if (progress.ID == aID)
        {
            return &progress;
        }
    }

    return nullptr;
}

///----------------------------------------------------------------------------------------------------
/// IsCharacterAdventureGuideAchievement:
///     Character Adventure Guide progress is tracked per character in-game
///     and is not exposed by /v2/account/achievements.
///----------------------------------------------------------------------------------------------------
bool IsCharacterAdventureGuideAchievement(int aID)
{
    for (const AchievementCategory& category :
        achievementCategories)
    {
        if (category.Name.find("Adventure Guide: Volume ") != 0)
        {
            continue;
        }

        if (std::find(
            category.AchievementIDs.begin(),
            category.AchievementIDs.end(),
            aID) != category.AchievementIDs.end())
        {
            return true;
        }
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// GetAchievementProgressProbeText:
///----------------------------------------------------------------------------------------------------
std::string GetAchievementProgressProbeText(int aID)
{
    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    if (IsCharacterAdventureGuideAchievement(aID))
    {
        if (progress == nullptr)
        {
            return
                "Character-specific achievement: progress is not exposed by "
                "/v2/account/achievements. NOT FOUND is expected.";
        }

        return
            "Character-specific achievement unexpectedly has an account record; "
            "showing the returned data for diagnostics.";
    }

    if (progress == nullptr)
    {
        return "Account record: NOT FOUND in the last /v2/account/achievements sync.";
    }

    std::string result =
        "Account record: FOUND | current=" +
        std::to_string(progress->Current) +
        " | max=" +
        std::to_string(progress->Max) +
        " | done=" +
        std::string(progress->Done ? "true" : "false");

    if (!progress->Bits.empty())
    {
        result +=
            " | bits=" +
            std::to_string(progress->Bits.size());
    }

    return result;
}

///----------------------------------------------------------------------------------------------------
/// GetAchievementProgressProbeName:
///----------------------------------------------------------------------------------------------------
std::string GetAchievementProgressProbeName(int aID)
{
    auto it =
        achievementDefinitions.find(aID);

    if (it == achievementDefinitions.end() ||
        it->second.Name.empty())
    {
        return "(definition name unavailable)";
    }

    return it->second.Name;
}

///----------------------------------------------------------------------------------------------------
/// GetAchievementProgressRatio:
///     Returns -1 when no numeric current/max progress is available.
///----------------------------------------------------------------------------------------------------
double GetAchievementProgressRatio(int aID)
{
    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    if (progress == nullptr ||
        progress->Current < 0 ||
        progress->Max <= 0)
    {
        return -1.0;
    }

    return
        static_cast<double>(progress->Current) /
        static_cast<double>(progress->Max);
}

///----------------------------------------------------------------------------------------------------
/// GetOpportunitySortTier:
///     Lower tiers are shown first.
///     0 = numeric progress, 1 = started without numeric progress, 2 = not started.
///----------------------------------------------------------------------------------------------------
int GetOpportunitySortTier(int aID)
{
    if (IsCharacterAdventureGuideAchievement(aID))
    {
        return 3;
    }

    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    if (progress == nullptr)
    {
        return 2;
    }

    if (progress->Current >= 0 && progress->Max > 0)
    {
        return 0;
    }

    return 1;
}

///----------------------------------------------------------------------------------------------------
/// IsContextualMapOpportunity:
///----------------------------------------------------------------------------------------------------
bool IsContextualMapOpportunity(int aID)
{
    return
        currentOpportunityTextFilteredIDs.find(aID) !=
        currentOpportunityTextFilteredIDs.end();
}

///----------------------------------------------------------------------------------------------------
/// UBMapLeadKind:
///     Location evidence and account progress are independent. An achievement
///     mentioning a region is NOT evidence it can be completed on this map.
///     Character Adventure Guide progress is per-character and unavailable
///     through the account achievement API.
///----------------------------------------------------------------------------------------------------
enum class UBMapLeadKind
{
    OnMap,
    Regional,
    CharacterSpecific
};

UBMapLeadKind UBClassifyMapLead(int aID)
{
    if (IsCharacterAdventureGuideAchievement(aID))
    {
        return UBMapLeadKind::CharacterSpecific;
    }

    if (currentOpportunityDirectIDs.count(aID) > 0)
    {
        return UBMapLeadKind::OnMap;
    }

    const auto found = achievementDefinitions.find(aID);
    if (found != achievementDefinitions.end() &&
        !currentMapName.empty())
    {
        const AchievementDefinition& definition = found->second;
        const std::string haystack = ToLowerCopy(
            definition.Name + " " + definition.Description + " " +
            definition.Requirement);

        if (UBContainsWholePhrase(haystack, ToLowerCopy(currentMapName)))
        {
            return UBMapLeadKind::OnMap;
        }
    }

    // Includes curated regional aliases (e.g. Ruins of Orr) which the old
    // scanner incorrectly counted as locally actionable.
    return UBMapLeadKind::Regional;
}

const char* UBGetOpportunitySourceLabel(int aID)
{
    auto vps = ubVpsCurrentCandidates.find(aID);
    if (vps != ubVpsCurrentCandidates.end())
    {
        if (ubVpsOnlyCandidateIDs.count(aID) == 0)
        {
            return "Local scan + VPS public candidate index";
        }
        if (vps->second.Scope == "regional")
        {
            return "VPS public regional lead (not verified on this map)";
        }
        return "VPS public map candidate (not a verified task)";
    }

    const UBMapLeadKind kind = UBClassifyMapLead(aID);
    if (kind == UBMapLeadKind::CharacterSpecific)
    {
        return "Character-specific lead (progress unavailable)";
    }
    if (kind == UBMapLeadKind::Regional)
    {
        return "Regional lead (not verified on this map)";
    }
    return currentOpportunityDirectIDs.count(aID) > 0
        ? "Map category / curated mapping"
        : "Explicit map reference";
}

///----------------------------------------------------------------------------------------------------
/// GetOpportunityReason:
///----------------------------------------------------------------------------------------------------
std::string GetOpportunityReason(int aID)
{
    const AchievementProgress* progress = FindAchievementProgress(aID);
    const UBMapLeadKind kind = UBClassifyMapLead(aID);

    if (kind == UBMapLeadKind::CharacterSpecific)
    {
        return "Potential Character Adventure Guide objective. Your "
            "character's progress is not available from the GW2 API; "
            "check the in-game Achievements panel. Map relevance does not "
            "verify that this character still needs the objective.";
    }

    if (kind == UBMapLeadKind::Regional)
    {
        return "Regional lead associated with " + currentMapRegionName +
            ". The objective may be on another map. This is not a "
            "confirmed current-map task.";
    }

    const bool direct = currentOpportunityDirectIDs.count(aID) > 0;
    const std::string evidence = direct
        ? "Matched a map-specific or curated achievement category. "
        : "The public achievement text explicitly mentions " +
        currentMapName + ". ";

    if (progress == nullptr)
    {
        return evidence + "No account progress is recorded; unlock "
            "requirements and availability have not been verified.";
    }

    if (GetAchievementProgressRatio(aID) >= 0.75)
    {
        return evidence + "The account reports at least 75% progress. "
            "Exact completion location may still depend on the objective.";
    }

    return evidence + "The account has recorded unfinished progress. "
        "Exact completion location may still depend on the objective.";
}

///----------------------------------------------------------------------------------------------------
/// OpportunityActionability:
///----------------------------------------------------------------------------------------------------
enum class OpportunityActionability
{
    General,
    Story,
    Event,
    Collection,
    Mastery,
    Unknown
};

struct OpportunityMetadata
{
    OpportunityActionability Actionability =
        OpportunityActionability::Unknown;

    bool RequiresSetup = false;
    std::string Tag;
    std::string Note;
};

///----------------------------------------------------------------------------------------------------
/// ToLowerCopy:
///----------------------------------------------------------------------------------------------------
std::string ToLowerCopy(const std::string& aValue)
{
    std::string result = aValue;

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        }
    );

    return result;
}

///----------------------------------------------------------------------------------------------------
/// InferOpportunityMetadata:
///     Conservative first-pass classifier using only explicit API text.
///     Ambiguous achievements stay Unknown rather than being guessed.
///----------------------------------------------------------------------------------------------------
OpportunityMetadata InferOpportunityMetadata(int aID)
{
    OpportunityMetadata metadata;

    auto definitionIt =
        achievementDefinitions.find(aID);

    if (definitionIt == achievementDefinitions.end())
    {
        return metadata;
    }

    const AchievementDefinition& definition =
        definitionIt->second;

    const std::string combined =
        ToLowerCopy(
            definition.Name + " " +
            definition.Description + " " +
            definition.Requirement
        );

    if (combined.find("story") != std::string::npos ||
        combined.find("episode") != std::string::npos ||
        combined.find("chapter") != std::string::npos)
    {
        metadata.Actionability =
            OpportunityActionability::Story;
        metadata.RequiresSetup = true;
        metadata.Tag = "Story";
        metadata.Note =
            "This achievement appears to depend on story progress.";
        return metadata;
    }

    if (combined.find("collection") != std::string::npos)
    {
        metadata.Actionability =
            OpportunityActionability::Collection;
        metadata.RequiresSetup = true;
        metadata.Tag = "Collection";
        metadata.Note =
            "This achievement appears to be part of a collection.";
        return metadata;
    }

    if (combined.find("event") != std::string::npos ||
        combined.find("meta-event") != std::string::npos ||
        combined.find("meta event") != std::string::npos)
    {
        metadata.Actionability =
            OpportunityActionability::Event;
        metadata.RequiresSetup = true;
        metadata.Tag = "Event";
        metadata.Note =
            "This achievement appears to depend on an event.";
        return metadata;
    }

    if (combined.find("mastery") != std::string::npos)
    {
        metadata.Actionability =
            OpportunityActionability::Mastery;
        metadata.Tag = "Mastery";
        metadata.Note =
            "The achievement text explicitly references mastery content.";
        return metadata;
    }

    if (!definition.Requirement.empty())
    {
        metadata.Actionability =
            OpportunityActionability::General;
        metadata.Tag = "General";
        metadata.Note =
            "This achievement has a direct requirement listed by the GW2 API.";
    }

    return metadata;
}

///----------------------------------------------------------------------------------------------------
/// GetActionabilityLabel:
///----------------------------------------------------------------------------------------------------
std::string GetActionabilityLabel(int aID)
{
    const OpportunityMetadata metadata =
        InferOpportunityMetadata(aID);

    return metadata.Tag;
}

// Availability is intentionally distinct from location evidence. The public
// GW2 achievement and map APIs do not prove that an achievement is unlocked,
// currently active or obtainable on this account.
enum class UBAvailabilityStatus
{
    PermanentLead,
    EventDependent,
    Historical,
    Unverified
};

struct UBAvailabilityInfo
{
    UBAvailabilityStatus Status = UBAvailabilityStatus::Unverified;
    const char* Label = "Unverified";
    const char* Explanation =
        "Public map evidence alone does not establish current availability.";
};

UBAvailabilityInfo UBGetAvailability(int aID)
{
    // Curated historical family confirmed by the GW2 Wiki's historical
    // Champion Rush and Bonus Events pages. ID 4892 is the Kryta example.
    const auto definitionIt = achievementDefinitions.find(aID);
    const AchievementDefinition* definition =
        definitionIt == achievementDefinitions.end() ? nullptr : &definitionIt->second;
    const std::string name = definition ? ToLowerCopy(definition->Name) : "";
    if (aID == 4892 ||
        name.find("bonus event: daily champion rush") == 0)
    {
        return { UBAvailabilityStatus::Historical, "Historical / unavailable",
            "Legacy Champion Rush bonus achievement. The GW2 Wiki marks "
            "this event family as historical; do not treat its map text "
            "as an active objective." };
    }

    bool foundCategory = false;
    bool historicalCategory = false;
    bool limitedTimeCategory = false;
    for (const AchievementCategory& category : achievementCategories)
    {
        if (std::find(category.AchievementIDs.begin(),
            category.AchievementIDs.end(), aID) == category.AchievementIDs.end())
        {
            continue;
        }
        foundCategory = true;
        const std::string categoryName = ToLowerCopy(category.Name);
        if (IsRetiredCategoryName(category.Name))
        {
            historicalCategory = true;
        }
        if (categoryName.find("bonus event") != std::string::npos ||
            categoryName.find("rush") != std::string::npos ||
            categoryName.find("festival") != std::string::npos ||
            categoryName.find("daily") == 0 ||
            categoryName.find("weekly") == 0)
        {
            limitedTimeCategory = true;
        }
    }
    if (historicalCategory)
    {
        return { UBAvailabilityStatus::Historical, "Historical / unavailable",
            "Listed under a historical or retired achievement category. "
            "Hidden from normal map recommendations." };
    }
    if (name.find("bonus event:") == 0 || limitedTimeCategory)
    {
        return { UBAvailabilityStatus::EventDependent, "Event-dependent",
            "This is associated with a time-limited event or rotating "
            "category. The addon has not verified that it is currently active." };
    }
    if (IsCharacterAdventureGuideAchievement(aID))
    {
        return { UBAvailabilityStatus::Unverified, "Character progress unverified",
            "The public account API does not expose this character's "
            "Adventure Guide status. Check the Hero panel." };
    }
    if (UBClassifyMapLead(aID) == UBMapLeadKind::Regional)
    {
        return { UBAvailabilityStatus::Unverified, "Regional / unverified",
            "A regional reference is not proof of an objective on this map." };
    }
    if (!definition)
    {
        return { UBAvailabilityStatus::Unverified, "Unverified",
            "Achievement definition is missing; cannot determine availability." };
    }
    const OpportunityMetadata metadata = InferOpportunityMetadata(aID);
    if (metadata.Actionability == OpportunityActionability::Event)
    {
        return { UBAvailabilityStatus::EventDependent, "Event-dependent",
            "Requires an in-game event. Map association is known, but the "
            "event's live state and achievement eligibility are not verified." };
    }
    if (!foundCategory || definition->Requirement.empty())
    {
        return { UBAvailabilityStatus::Unverified, "Unverified",
            "Not enough public category or requirement evidence to infer "
            "whether the objective can currently be completed." };
    }
    return { UBAvailabilityStatus::PermanentLead, "Permanent lead (likely)",
        "A non-seasonal map match with a published requirement. This is "
        "a likely ongoing objective, NOT proof that this account has "
        "unlocked it or that it is currently obtainable." };
}

void UBRenderAvailability(int aID, bool aShowExplanation)
{
    const UBAvailabilityInfo available = UBGetAvailability(aID);
    ImVec4 color = UBMutedInfoColor();
    switch (available.Status)
    {
    case UBAvailabilityStatus::PermanentLead:
        color = UBSuccessColor();
        break;
    case UBAvailabilityStatus::EventDependent:
        color = UBWarningColor();
        break;
    case UBAvailabilityStatus::Historical:
        color = ImVec4(0.95f, 0.48f, 0.42f, 1.0f);
        break;
    case UBAvailabilityStatus::Unverified:
        break;
    }
    ImGui::TextColored(color, "Availability: %s", available.Label);
    if (aShowExplanation)
    {
        ImGui::TextWrapped("Availability evidence: %s", available.Explanation);
    }
    else if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", available.Explanation);
    }
}

///----------------------------------------------------------------------------------------------------
/// IsOpportunityFiltered:
///----------------------------------------------------------------------------------------------------
bool IsOpportunityFiltered(int aID)
{
    if (ignoredAchievementIDs.find(aID) !=
        ignoredAchievementIDs.end())
    {
        return true;
    }

    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    if (hideNotStartedAchievements &&
        progress == nullptr &&
        !IsCharacterAdventureGuideAchievement(aID))
    {
        return true;
    }

    const OpportunityMetadata metadata =
        InferOpportunityMetadata(aID);

    if (hideStoryAchievements &&
        metadata.Actionability ==
        OpportunityActionability::Story)
    {
        return true;
    }

    if (hideEventAchievements &&
        metadata.Actionability ==
        OpportunityActionability::Event)
    {
        return true;
    }

    if (hideCollectionAchievements &&
        metadata.Actionability ==
        OpportunityActionability::Collection)
    {
        return true;
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// OpportunityRecommendation:
///----------------------------------------------------------------------------------------------------
struct OpportunityRecommendation
{
    int ID = 0;
    double Score = 0.0;
    std::vector<std::string> Reasons;
};

///----------------------------------------------------------------------------------------------------
/// BuildRecommendation:
///     First-pass recommendation heuristic. The score is intentionally internal.
///----------------------------------------------------------------------------------------------------
OpportunityRecommendation BuildRecommendation(int aID)
{
    OpportunityRecommendation recommendation;
    recommendation.ID = aID;

    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    auto definitionIt =
        achievementDefinitions.find(aID);

    const AchievementDefinition* definition =
        definitionIt != achievementDefinitions.end()
        ? &definitionIt->second
        : nullptr;

    const bool characterSpecific =
        IsCharacterAdventureGuideAchievement(aID);

    if (characterSpecific)
    {
        // ArenaNet does not expose Character Adventure Guide progress.
        // Keep these eligible without inventing a completion-based reason.
        recommendation.Score += 12.0;
    }
    else if (progress != nullptr)
    {
        // Started achievements get a baseline advantage over untouched ones.
        recommendation.Score += 20.0;

        if (preferStartedAchievements)
        {
            recommendation.Score += 25.0;
        }

        const double ratio =
            GetAchievementProgressRatio(aID);

        if (ratio >= 0.0)
        {
            const int percent =
                static_cast<int>((ratio * 100.0) + 0.5);

            // Completion momentum is the strongest general signal.
            if (percent >= 90)
            {
                recommendation.Score += 80.0;
                recommendation.Reasons.push_back(
                    std::to_string(percent) + "% complete"
                );
            }
            else if (percent >= 75)
            {
                recommendation.Score += 60.0;
                recommendation.Reasons.push_back(
                    std::to_string(percent) + "% complete"
                );
            }
            else if (percent >= 50)
            {
                recommendation.Score += 35.0;
                recommendation.Reasons.push_back(
                    std::to_string(percent) + "% complete"
                );
            }
            else if (percent >= 25)
            {
                recommendation.Score += 15.0;
                recommendation.Reasons.push_back(
                    std::to_string(percent) + "% complete"
                );
            }
            else
            {
                recommendation.Score +=
                    static_cast<double>(percent) * 0.25;
            }

            // Being literally one count/objective away is the strongest boost.
            if (progress->Max > 0 &&
                progress->Current >= 0 &&
                progress->Max - progress->Current == 1)
            {
                recommendation.Score += 100.0;
                recommendation.Reasons.insert(
                    recommendation.Reasons.begin(),
                    "only 1 step remaining"
                );
            }
        }
        else
        {
            recommendation.Score += 10.0;
            recommendation.Reasons.push_back(
                "already started"
            );
        }
    }

    if (definition != nullptr)
    {
        const int remainingAP =
            GetRemainingAchievementAP(
                aID,
                *definition
            );

        // AP matters, but should not overpower progress.
        if (remainingAP > 0)
        {
            const int cappedAP =
                remainingAP > 15
                ? 15
                : remainingAP;

            const double apMultiplier =
                preferAchievementPoints
                ? 3.0
                : 1.5;

            recommendation.Score +=
                static_cast<double>(cappedAP) *
                apMultiplier;

            recommendation.Reasons.push_back(
                std::to_string(remainingAP) +
                " AP available"
            );
        }

        // Mastery rewards are highly valuable even when untouched.
        if (definition->RewardsMastery)
        {
            recommendation.Score +=
                preferMasteryRewards
                ? 70.0
                : 45.0;

            if (!definition->MasteryRegion.empty())
            {
                recommendation.Reasons.push_back(
                    "Mastery Point (" +
                    definition->MasteryRegion +
                    ")"
                );
            }
            else
            {
                recommendation.Reasons.push_back(
                    "Mastery Point reward"
                );
            }
        }

        if (definition->RewardsTitle)
        {
            recommendation.Score += 8.0;
            recommendation.Reasons.push_back(
                "title reward"
            );
        }

        if (definition->RewardItemEntries > 0)
        {
            recommendation.Score += 4.0;

            recommendation.Reasons.push_back(
                definition->RewardItemEntries == 1
                ? "item reward"
                : std::to_string(
                    definition->RewardItemEntries
                ) + " item rewards"
            );
        }

        if (definition->RewardCoins > 0)
        {
            recommendation.Score += 2.0;
            recommendation.Reasons.push_back(
                "coin reward"
            );
        }
    }

    if (progress == nullptr && !characterSpecific)
    {
        // Untouched account achievements should usually sit below active cleanup work.
        recommendation.Score -= 10.0;

        if (recommendation.Reasons.empty())
        {
            recommendation.Reasons.push_back(
                "No account progress recorded"
            );
        }
    }

    const OpportunityMetadata metadata =
        InferOpportunityMetadata(aID);

    if (metadata.RequiresSetup)
    {
        // Still recommend valuable gated achievements, but rank them below
        // equally valuable things that are immediately actionable.
        recommendation.Score -= 18.0;

        if (!metadata.Tag.empty())
        {
            recommendation.Reasons.push_back(
                metadata.Tag + " setup"
            );
        }
    }
    else if (metadata.Actionability ==
        OpportunityActionability::General)
    {
        recommendation.Score += 5.0;
    }

    return recommendation;
}

///----------------------------------------------------------------------------------------------------
/// CountCharacterSpecificOpportunities:
///----------------------------------------------------------------------------------------------------
size_t CountCharacterSpecificOpportunities()
{
    size_t count = 0;

    for (int id : currentMapOpportunityIDs)
    {
        if (IsCharacterAdventureGuideAchievement(id))
        {
            ++count;
        }
    }

    return count;
}

///----------------------------------------------------------------------------------------------------
/// HasAccountTrackedOpportunities:
///----------------------------------------------------------------------------------------------------
bool HasAccountTrackedOpportunities()
{
    for (int id : currentMapOpportunityIDs)
    {
        if (!IsCharacterAdventureGuideAchievement(id))
        {
            return true;
        }
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// BuildCurrentMapRecommendations:
///----------------------------------------------------------------------------------------------------
std::vector<OpportunityRecommendation> BuildCurrentMapRecommendations()
{
    std::vector<OpportunityRecommendation> recommendations;

    for (int id : currentMapOpportunityIDs)
    {
        // Character Adventure Guide progress is not exposed by the API,
        // so these objectives should not compete in a progress-based ranking.
        if (IsCharacterAdventureGuideAchievement(id))
        {
            continue;
        }

        recommendations.push_back(
            BuildRecommendation(id)
        );
    }

    std::sort(
        recommendations.begin(),
        recommendations.end(),
        [](const OpportunityRecommendation& aLeft,
            const OpportunityRecommendation& aRight)
        {
            if (aLeft.Score != aRight.Score)
            {
                return aLeft.Score > aRight.Score;
            }

            auto leftDefinition =
                achievementDefinitions.find(aLeft.ID);

            auto rightDefinition =
                achievementDefinitions.find(aRight.ID);

            const std::string leftName =
                leftDefinition != achievementDefinitions.end()
                ? leftDefinition->second.Name
                : std::string();

            const std::string rightName =
                rightDefinition != achievementDefinitions.end()
                ? rightDefinition->second.Name
                : std::string();

            return leftName < rightName;
        }
    );

    if (recommendations.size() > 5)
    {
        recommendations.resize(5);
    }

    return recommendations;
}

///----------------------------------------------------------------------------------------------------
/// JoinRecommendationReasons:
///----------------------------------------------------------------------------------------------------
std::string JoinRecommendationReasons(
    const std::vector<std::string>& aReasons)
{
    std::string result;

    for (const std::string& reason : aReasons)
    {
        if (!result.empty())
        {
            result += " | ";
        }

        result += reason;
    }

    return result;
}

///----------------------------------------------------------------------------------------------------
/// Forward declaration:
///----------------------------------------------------------------------------------------------------
void CopyAchievementNameAndOpenHero(const char* aName);

///----------------------------------------------------------------------------------------------------
/// OpenAchievementInNexusGameWiki:
///----------------------------------------------------------------------------------------------------
void OpenAchievementInNexusGameWiki(const char* aName)
{
    if (aName == nullptr || aName[0] == '\0')
    {
        return;
    }

    NexusGameWikiOpenPageRequest request{};
    request.Title = aName;
    request.Handled = false;

    if (APIDefs != nullptr &&
        APIDefs->Events_Raise != nullptr)
    {
        APIDefs->Events_Raise(
            UB_NEXUS_GAME_WIKI_OPEN_EVENT,
            &request
        );
    }

    if (request.Handled)
    {
        return;
    }

    // NexusGameWiki is not installed, not enabled, or does not support
    // the bridge. Fall back to the workflow that always works.
    CopyAchievementNameAndOpenHero(aName);

    if (APIDefs != nullptr &&
        APIDefs->GUI_SendAlert != nullptr)
    {
        APIDefs->GUI_SendAlert(
            "NexusGameWiki unavailable. Copied achievement name "
            "and opened the Hero panel instead."
        );
    }
}
///----------------------------------------------------------------------------------------------------
/// Unfinished Business color helpers:
///----------------------------------------------------------------------------------------------------
ImVec4 UBAccentColor()
{
    return ImVec4(0.45f, 0.78f, 1.00f, 1.00f);
}

ImVec4 UBSuccessColor()
{
    return ImVec4(0.48f, 0.85f, 0.53f, 1.00f);
}

ImVec4 UBWarningColor()
{
    return ImVec4(0.98f, 0.80f, 0.36f, 1.00f);
}

ImVec4 UBDangerColor()
{
    return ImVec4(0.95f, 0.48f, 0.42f, 1.00f);
}

ImVec4 UBSpecialColor()
{
    return ImVec4(0.78f, 0.62f, 1.00f, 1.00f);
}

ImVec4 UBMutedInfoColor()
{
    return ImVec4(0.70f, 0.82f, 0.96f, 1.00f);
}


///----------------------------------------------------------------------------------------------------
/// Classic Unfinished Business presentation helpers (ImGui 1.80 compatible).
/// Render in a separate draw-list channel so card backgrounds sit BEHIND text,
/// buttons, popups, and the existing tree-node interactions.
///----------------------------------------------------------------------------------------------------
float UBPresentationScale()
{
    const float scale = ImGui::GetFontSize() / 13.0f;
    return scale > 0.75f ? scale : 0.75f;
}

///----------------------------------------------------------------------------------------------------
/// UBColoredActionButton:
///     Consistent colored achievement actions.  Only the button presentation
///     is changed; callers retain their existing Wiki / clipboard / ignore logic.
///----------------------------------------------------------------------------------------------------
enum class UBActionButtonKind
{
    Wiki,
    CopyName,
    CopyAndHero,
    Ignore
};

bool UBColoredActionButton(const char* aLabel, UBActionButtonKind aKind)
{
    ImVec4 background;
    ImVec4 hovered;
    ImVec4 active;
    ImVec4 border;

    switch (aKind)
    {
    case UBActionButtonKind::Wiki:
        // Reference lookup: blue.
        background = ImVec4(0.13f, 0.31f, 0.53f, 1.0f);
        hovered = ImVec4(0.19f, 0.44f, 0.72f, 1.0f);
        active = ImVec4(0.10f, 0.24f, 0.42f, 1.0f);
        border = ImVec4(0.37f, 0.67f, 0.96f, 0.75f);
        break;
    case UBActionButtonKind::CopyName:
        // Clipboard-only: teal.
        background = ImVec4(0.09f, 0.37f, 0.35f, 1.0f);
        hovered = ImVec4(0.14f, 0.51f, 0.47f, 1.0f);
        active = ImVec4(0.07f, 0.29f, 0.27f, 1.0f);
        border = ImVec4(0.35f, 0.78f, 0.70f, 0.75f);
        break;
    case UBActionButtonKind::CopyAndHero:
        // Clipboard + in-game Hero panel: purple.
        background = ImVec4(0.34f, 0.25f, 0.53f, 1.0f);
        hovered = ImVec4(0.49f, 0.35f, 0.70f, 1.0f);
        active = ImVec4(0.26f, 0.19f, 0.41f, 1.0f);
        border = ImVec4(0.74f, 0.60f, 0.98f, 0.75f);
        break;
    default:
        // Ignore / removal from recommendations: muted red.
        background = ImVec4(0.46f, 0.19f, 0.21f, 1.0f);
        hovered = ImVec4(0.64f, 0.27f, 0.30f, 1.0f);
        active = ImVec4(0.35f, 0.15f, 0.16f, 1.0f);
        border = ImVec4(0.95f, 0.49f, 0.51f, 0.75f);
        break;
    }

    const float scale = UBPresentationScale();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Button, background);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.97f, 0.98f, 1.0f, 1.0f));

    const bool clicked = ImGui::SmallButton(aLabel);

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    return clicked;
}

struct UBVisualCard
{
    ImDrawList* DrawList = nullptr;
    ImDrawListSplitter Splitter;
    ImVec2 Start = ImVec2(0.0f, 0.0f);
    float Width = 0.0f;
    bool Subtle = false;
    bool Compact = false;
};

void UBBeginVisualCard(UBVisualCard& aCard, bool aSubtle = false,
    bool aCompact = false)
{
    const float scale = UBPresentationScale();
    aCard.Start = ImGui::GetCursorScreenPos();
    aCard.Width = ImGui::GetContentRegionAvail().x;
    aCard.Subtle = aSubtle;
    aCard.Compact = aCompact;
    aCard.DrawList = ImGui::GetWindowDrawList();
    aCard.Splitter.Split(aCard.DrawList, 2);
    aCard.Splitter.SetCurrentChannel(aCard.DrawList, 1);

    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0.0f, (aCompact ? 2.5f : 5.0f) * scale));
    ImGui::Indent((aCompact ? 9.0f : 11.0f) * scale);
}

void UBEndVisualCard(UBVisualCard& aCard)
{
    const float scale = UBPresentationScale();
    ImGui::Unindent((aCard.Compact ? 9.0f : 11.0f) * scale);
    ImGui::Dummy(ImVec2(0.0f, (aCard.Compact ? 3.0f : 6.0f) * scale));
    ImGui::EndGroup();

    const ImVec2 bottom = ImGui::GetItemRectMax();
    float bottomY = bottom.y;
    const float minHeight = aCard.Compact ? 22.0f : 28.0f;
    if (bottomY < aCard.Start.y + minHeight * scale)
    {
        bottomY = aCard.Start.y + minHeight * scale;
    }

    aCard.Splitter.SetCurrentChannel(aCard.DrawList, 0);
    const ImVec4 fill = aCard.Subtle
        ? ImVec4(0.095f, 0.115f, 0.145f, 0.93f)
        : ImVec4(0.105f, 0.130f, 0.165f, 0.96f);
    const ImU32 fillColor = ImGui::GetColorU32(fill);
    const ImU32 borderColor = ImGui::GetColorU32(
        ImVec4(0.23f, 0.28f, 0.34f, 0.60f));

    aCard.DrawList->AddRectFilled(
        aCard.Start,
        ImVec2(aCard.Start.x + aCard.Width, bottomY),
        fillColor,
        5.0f * scale);
    aCard.DrawList->AddRect(
        aCard.Start,
        ImVec2(aCard.Start.x + aCard.Width, bottomY),
        borderColor,
        5.0f * scale);

    aCard.Splitter.SetCurrentChannel(aCard.DrawList, 1);
    aCard.Splitter.Merge(aCard.DrawList);
    ImGui::Dummy(ImVec2(0.0f, (aCard.Compact ? 2.0f : 4.0f) * scale));
}

void UBRenderThinProgressBar(double aRatio)
{
    const float scale = UBPresentationScale();
    const float width = ImGui::GetContentRegionAvail().x - 5.0f * scale;
    if (width <= 0.0f)
    {
        return;
    }

    float fraction = static_cast<float>(aRatio);
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float height = 4.0f * scale;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        p, ImVec2(p.x + width, p.y + height),
        ImGui::GetColorU32(ImVec4(0.29f, 0.34f, 0.42f, 0.80f)),
        2.0f * scale);
    if (fraction > 0.0f)
    {
        drawList->AddRectFilled(
            p, ImVec2(p.x + width * fraction, p.y + height),
            ImGui::GetColorU32(UBWarningColor()),
            2.0f * scale);
    }
    ImGui::Dummy(ImVec2(width, height));
}

// Wizard's Vault cards deliberately reuse the achievement card backdrop,
// padding, and outline.  The progress strip adds status-specific color.
// Daily/Weekly shares the Achievements card frame but uses compact spacing.
// The regular achievement cards are not affected.
void UBRenderVaultProgressStrip(double aRatio, const ImVec4& aColor)
{
    const float scale = UBPresentationScale();
    const float width = ImGui::GetContentRegionAvail().x - 5.0f * scale;
    if (width <= 0.0f) return;

    const float fraction = static_cast<float>((std::max)(0.0,
        (std::min)(1.0, aRatio)));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float height = 4.0f * scale;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(pos,
        ImVec2(pos.x + width, pos.y + height),
        ImGui::GetColorU32(ImVec4(0.24f, 0.29f, 0.36f, 0.90f)),
        2.0f * scale);
    if (fraction > 0.0f)
    {
        drawList->AddRectFilled(pos,
            ImVec2(pos.x + width * fraction, pos.y + height),
            ImGui::GetColorU32(aColor), 2.0f * scale);
    }
    ImGui::Dummy(ImVec2(width, height));
}

// Draw one objective in the same card theme as the Achievements tab.
// Its collapsed state is two text lines plus a thin progress strip.
void UBRenderVaultObjectiveCard(const UBVaultObjective& objective)
{
    ImGui::PushID(objective.ID);
    const int current = (std::max)(0, objective.Current);
    const int maximum = (std::max)(0, objective.Complete);
    const bool ready = !objective.Claimed && maximum > 0 &&
        current >= maximum;
    const double ratio = maximum > 0
        ? (std::min)(1.0, static_cast<double>(current) /
            static_cast<double>(maximum)) : 0.0;
    const ImVec4 color = objective.Claimed ? UBSuccessColor()
        : ready ? UBWarningColor() : UBAccentColor();

    UBVisualCard card;
    UBBeginVisualCard(card, objective.Claimed, true);
    const std::string label = objective.Title + "##vault_objective";
    const bool expanded = ImGui::TreeNodeEx(label.c_str(),
        ImGuiTreeNodeFlags_SpanAvailWidth);

    if (objective.Claimed)
        ImGui::TextColored(UBSuccessColor(), "Claimed  |  %d Astral Acclaim",
            objective.Acclaim);
    else if (ready)
        ImGui::TextColored(UBWarningColor(), "Reward ready to claim  |  %d Astral Acclaim",
            objective.Acclaim);
    else
        ImGui::TextColored(UBMutedInfoColor(), "%d/%d progress  |  %d Astral Acclaim  |  %s",
            current, maximum, objective.Acclaim,
            objective.Track.empty() ? "Wizard's Vault" : objective.Track.c_str());
    UBRenderVaultProgressStrip(ratio, color);

    if (expanded)
    {
        ImGui::Indent();
        ImGui::Spacing();
        ImGui::Text("Objective progress: %d / %d (%d%%)",
            current, maximum, static_cast<int>(ratio * 100.0 + 0.5));
        ImGui::Text("Track: %s  |  Reward: %d Astral Acclaim",
            objective.Track.empty() ? "Wizard's Vault" : objective.Track.c_str(),
            objective.Acclaim);
        if (ready)
            ImGui::TextColored(UBWarningColor(),
                "Claim this reward in the Wizard's Vault in game.");
        else if (objective.Claimed)
            ImGui::TextColored(UBSuccessColor(),
                "This objective's reward has been claimed.");
        ImGui::TextDisabled("Objective ID: %d", objective.ID);
        ImGui::Unindent();
        ImGui::TreePop();
    }
    UBEndVisualCard(card);
    ImGui::PopID();
}

void UBRenderVaultPeriod(const char* aTitle, const UBVaultPeriod& aPeriod)
{
    ImGui::PushID(aTitle);
    if (!aPeriod.Loaded)
    {
        ImGui::TextWrapped("%s", aPeriod.Error.empty()
            ? "Waiting for account data..." : aPeriod.Error.c_str());
        ImGui::PopID();
        return;
    }

    int claimedCount = 0;
    int readyCount = 0;
    for (const UBVaultObjective& objective : aPeriod.Objectives)
    {
        if (objective.Claimed) ++claimedCount;
        else if (objective.Complete > 0 &&
            objective.Current >= objective.Complete) ++readyCount;
    }
    const int pending = UBVaultPendingCount(aPeriod);
    ImGui::TextColored(UBAccentColor(), "%s", aTitle);
    ImGui::SameLine();
    ImGui::TextDisabled("%d outstanding  |  %d ready to claim  |  %d claimed",
        pending, readyCount, claimedCount);
    ImGui::Spacing();

    if (aPeriod.MetaComplete > 0)
    {
        const bool metaReady = aPeriod.MetaCurrent >= aPeriod.MetaComplete;
        const ImVec4 metaColor = aPeriod.MetaClaimed ? UBSuccessColor()
            : metaReady ? UBWarningColor() : UBAccentColor();
        UBVisualCard card;
        UBBeginVisualCard(card, true, true);
        ImGui::TextColored(metaColor, "%s meta: %d/%d objectives%s",
            aTitle, aPeriod.MetaCurrent, aPeriod.MetaComplete,
            aPeriod.MetaClaimed ? "  |  Claimed" :
            metaReady ? "  |  Reward ready to claim" : "");
        const double ratio = static_cast<double>(aPeriod.MetaCurrent) /
            static_cast<double>(aPeriod.MetaComplete);
        UBRenderVaultProgressStrip(ratio, metaColor);
        if (metaReady && !aPeriod.MetaClaimed && ImGui::IsItemHovered())
            ImGui::SetTooltip("Collect the meta reward in the Wizard's Vault.");
        UBEndVisualCard(card);
    }

    if (aPeriod.Objectives.empty())
    {
        ImGui::TextDisabled("No objectives returned for this period.");
        ImGui::PopID();
        return;
    }

    // Outstanding objectives stay visible. Claimed objectives move into a
    // disclosure group that is closed by default, but can be opened any time.
    for (const UBVaultObjective& objective : aPeriod.Objectives)
        if (!objective.Claimed) UBRenderVaultObjectiveCard(objective);

    if (claimedCount > 0)
    {
        const std::string claimedLabel = "Claimed (" +
            std::to_string(claimedCount) + ")##UBVaultClaimed";
        if (ImGui::CollapsingHeader(claimedLabel.c_str()))
        {
            for (const UBVaultObjective& objective : aPeriod.Objectives)
                if (objective.Claimed) UBRenderVaultObjectiveCard(objective);
        }
    }
    if (pending == 0)
        ImGui::TextColored(UBSuccessColor(), "All objectives claimed.");

    ImGui::PopID();
}

void UBRenderVaultTab()
{
    // Per-session selection; no account data or notifications are modified.
    static int selectedPeriod = 0; // 0 = Daily, 1 = Weekly
    bool periodChanged = false;
    ImGui::TextColored(UBAccentColor(), "Wizard's Vault objectives");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", ubVaultStatus.c_str());

    // Preserve the working pulse preview and manual refresh controls.
    if (ImGui::SmallButton("Preview tab pulse (15 seconds)"))
        ubVaultPulsePreviewUntil = ImGui::GetTime() + 15.0;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Preview the orange tab glow; switch to Achievements to see it on an inactive tab.");
    ImGui::SameLine();
    if (!ubVaultSyncActive && apiKeyValid)
    {
        if (ImGui::SmallButton("Refresh daily / weekly progress"))
            UBStartVaultSync();
    }
    else if (ubVaultSyncActive)
        ImGui::TextDisabled("Refreshing in background...");
    else
        ImGui::TextDisabled("Validate an API key with account + progression permissions in Options.");

    // Side-by-side selector keeps the other period one click away, regardless
    // of the length of the list or the current scroll position.
    const float scale = UBPresentationScale();
    const float gap = 6.0f * scale;
    const float available = ImGui::GetContentRegionAvail().x;
    const float buttonWidth = (std::max)(80.0f * scale,
        (available - gap) * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f * scale);

    auto renderPeriodButton = [&](int which, const std::string& label)
        {
            const bool selected = selectedPeriod == which;
            ImGui::PushStyleColor(ImGuiCol_Button, selected
                ? ImVec4(0.17f, 0.34f, 0.46f, 0.94f)
                : ImVec4(0.11f, 0.16f, 0.22f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(0.24f, 0.43f, 0.56f, 0.98f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(0.19f, 0.38f, 0.52f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, selected
                ? ImVec4(0.37f, 0.70f, 0.93f, 0.95f)
                : ImVec4(0.22f, 0.29f, 0.36f, 0.75f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f * scale);
            if (ImGui::Button(label.c_str(), ImVec2(buttonWidth, 0.0f)))
            {
                if (selectedPeriod != which)
                {
                    selectedPeriod = which;
                    periodChanged = true;
                }
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        };

    renderPeriodButton(0, "Daily (" +
        std::to_string(UBVaultPendingCount(ubVaultDaily)) + ")##UBVaultDaily");
    ImGui::SameLine(0.0f, gap);
    renderPeriodButton(1, "Weekly (" +
        std::to_string(UBVaultPendingCount(ubVaultWeekly)) + ")##UBVaultWeekly");
    ImGui::PopStyleVar();
    ImGui::Spacing();

    // Only the objective list scrolls; the selector and pulse/refresh controls
    // remain visible above it even after scrolling far down a weekly list.
    ImGui::BeginChild("##UBVaultPeriodScroll", ImVec2(0.0f, 0.0f), false);
    if (periodChanged) ImGui::SetScrollY(0.0f);
    if (selectedPeriod == 0)
        UBRenderVaultPeriod("Daily", ubVaultDaily);
    else
        UBRenderVaultPeriod("Weekly", ubVaultWeekly);
    ImGui::Spacing();
    ImGui::TextDisabled("Progress updates automatically about every five minutes.");
    ImGui::EndChild();
}

void UBRenderProgressCardSummary(int aID, const AchievementProgress* aProgress,
    const std::string& aReason)
{
    const bool characterOnly = IsCharacterAdventureGuideAchievement(aID);
    if (characterOnly)
    {
        ImGui::TextColored(UBSpecialColor(), "Progress in-game only");
    }
    else if (aProgress == nullptr)
    {
        ImGui::TextColored(UBDangerColor(), "No recorded progress");
    }
    else if (aProgress->Current >= 0 && aProgress->Max > 0)
    {
        const double ratio = GetAchievementProgressRatio(aID);
        const int pct = static_cast<int>(ratio * 100.0 + 0.5);
        ImGui::TextColored(UBWarningColor(), "%d/%d (%d%%)",
            aProgress->Current, aProgress->Max, pct);
        UBRenderThinProgressBar(ratio);
        ImGui::TextDisabled("%d%% complete", pct);
    }
    else
    {
        ImGui::TextColored(UBWarningColor(), "In progress");
    }

    if (!aReason.empty())
    {
        ImGui::TextWrapped("%s", aReason.c_str());
    }
}

void UBRenderSummaryStat(const char* aLabel, size_t aValue,
    const ImVec4& aColor, float aWidth)
{
    const float scale = UBPresentationScale();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float height = 58.0f * scale;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        p, ImVec2(p.x + aWidth, p.y + height),
        ImGui::GetColorU32(ImVec4(0.105f, 0.13f, 0.165f, 0.96f)),
        5.0f * scale);
    drawList->AddRect(
        p, ImVec2(p.x + aWidth, p.y + height),
        ImGui::GetColorU32(ImVec4(0.23f, 0.28f, 0.34f, 0.52f)),
        5.0f * scale);
    const std::string valueText = std::to_string(aValue);
    drawList->AddText(
        ImVec2(p.x + 12.0f * scale, p.y + 9.0f * scale),
        ImGui::GetColorU32(aColor), valueText.c_str());
    drawList->AddText(
        ImVec2(p.x + 12.0f * scale, p.y + 30.0f * scale),
        ImGui::GetColorU32(ImVec4(0.72f, 0.76f, 0.82f, 1.0f)),
        aLabel);
    ImGui::Dummy(ImVec2(aWidth, height));
}

void UBRenderMapSummaryStats(size_t aAlmostDone, size_t aInProgress,
    size_t aNotStarted, size_t aCharacterSpecific)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const float gap = 8.0f * UBPresentationScale();
    const float statWidth = (width - 2.0f * gap) / 3.0f;
    if (statWidth <= 1.0f)
    {
        return;
    }
    UBRenderSummaryStat("Almost done", aAlmostDone, UBSuccessColor(), statWidth);
    ImGui::SameLine(0.0f, gap);
    UBRenderSummaryStat("In progress", aInProgress, UBWarningColor(), statWidth);
    ImGui::SameLine(0.0f, gap);
    UBRenderSummaryStat("No progress", aNotStarted, UBDangerColor(), statWidth);
    if (aCharacterSpecific > 0)
    {
        ImGui::TextColored(UBSpecialColor(),
            "%llu character-specific achievement(s) have progress in-game only",
            static_cast<unsigned long long>(aCharacterSpecific));
    }
}

ImVec4 GetOpportunityDisplayColor(int aID)
{
    if (IsCharacterAdventureGuideAchievement(aID))
    {
        return UBSpecialColor();
    }

    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    if (progress == nullptr)
    {
        return UBDangerColor();
    }

    if (progress->Current >= 0 && progress->Max > 0)
    {
        const double ratio =
            GetAchievementProgressRatio(aID);

        if (ratio >= 0.75)
        {
            return UBSuccessColor();
        }
    }

    return UBWarningColor();
}

///----------------------------------------------------------------------------------------------------
/// Forward declaration:
///----------------------------------------------------------------------------------------------------
void RenderRecommendedOpportunity(
    const OpportunityRecommendation& aRecommendation,
    bool aMapContext = false);

///----------------------------------------------------------------------------------------------------
/// CopyAchievementName:
///----------------------------------------------------------------------------------------------------
void CopyAchievementName(const char* aName)
{
    if (aName == nullptr || aName[0] == '\0')
    {
        return;
    }

    ImGui::SetClipboardText(aName);

    if (APIDefs != nullptr &&
        APIDefs->GUI_SendAlert != nullptr)
    {
        APIDefs->GUI_SendAlert(
            "Achievement name copied to clipboard."
        );
    }
}

///----------------------------------------------------------------------------------------------------
/// CopyAchievementNameAndOpenHero:
///----------------------------------------------------------------------------------------------------
void CopyAchievementNameAndOpenHero(const char* aName)
{
    CopyAchievementName(aName);

    if (APIDefs == nullptr ||
        APIDefs->GameBinds_InvokeAsync == nullptr)
    {
        return;
    }

    if (APIDefs->GameBinds_IsBound != nullptr &&
        !APIDefs->GameBinds_IsBound(GB_UiHero))
    {
        if (APIDefs->GUI_SendAlert != nullptr)
        {
            APIDefs->GUI_SendAlert(
                "Hero panel game bind is not configured."
            );
        }

        return;
    }

    APIDefs->GameBinds_InvokeAsync(
        GB_UiHero,
        50
    );
}

///----------------------------------------------------------------------------------------------------
/// GetCoverageDisplayLabel:
///----------------------------------------------------------------------------------------------------
const char* GetCoverageDisplayLabel()
{
    if (currentCoverageMode == MapCoverageMode::ExactCategory)
    {
        return "Exact";
    }

    if (currentCoverageMode == MapCoverageMode::ContextualDiscovery)
    {
        return "Contextual / regional";
    }

    if (currentCoverageMode == MapCoverageMode::CuratedMapping)
    {
        const bool hasDirect =
            !currentCoverageDirectCategories.empty();

        const bool hasContextual =
            !currentCoverageContextualCategories.empty();

        if (hasDirect && hasContextual)
        {
            return "Curated + contextual";
        }

        if (hasContextual)
        {
            return "Contextual";
        }

        return "Curated";
    }

    return "Limited";
}

///----------------------------------------------------------------------------------------------------
/// GetCoverageDisplayColor:
///----------------------------------------------------------------------------------------------------
ImVec4 GetCoverageDisplayColor()
{
    if (currentCoverageMode == MapCoverageMode::ExactCategory)
    {
        return UBSuccessColor();
    }

    if (currentCoverageMode == MapCoverageMode::ContextualDiscovery)
    {
        return UBSpecialColor();
    }

    if (currentCoverageMode == MapCoverageMode::CuratedMapping)
    {
        if (!currentCoverageContextualCategories.empty() &&
            currentCoverageDirectCategories.empty())
        {
            return UBSpecialColor();
        }

        return UBAccentColor();
    }

    return UBDangerColor();
}

///----------------------------------------------------------------------------------------------------
/// GetCoverageTooltip:
///----------------------------------------------------------------------------------------------------
const char* GetCoverageTooltip()
{
    if (currentCoverageMode == MapCoverageMode::ExactCategory)
    {
        return
            "Exact: this map has a matching GW2 achievement category. "
            "This is the strongest coverage mode.";
    }

    if (currentCoverageMode == MapCoverageMode::ContextualDiscovery)
    {
        return "Contextual discovery: matching public achievement text and "
            "regional references. Regional goals may be elsewhere and "
            "coverage is not exhaustive.";
    }

    if (currentCoverageMode == MapCoverageMode::CuratedMapping)
    {
        const bool hasDirect =
            !currentCoverageDirectCategories.empty();

        const bool hasContextual =
            !currentCoverageContextualCategories.empty();

        if (hasDirect && hasContextual)
        {
            return
                "Curated + contextual: known map-specific categories are "
                "combined with broader categories whose achievement text "
                "explicitly matches this map.";
        }

        if (hasContextual)
        {
            return
                "Contextual: opportunities are being found from broader "
                "achievement categories using explicit map-name matching.";
        }

        return
            "Curated: Unfinished Business uses a hand-mapped set of "
            "achievement categories for this map.";
    }

    return
        "Limited: Unfinished Business does not yet have reliable coverage "
        "for this map. Results may be incomplete.";
}

///----------------------------------------------------------------------------------------------------
/// RenderCharacterSpecificOpportunity:
///----------------------------------------------------------------------------------------------------
void RenderCharacterSpecificOpportunity(int aID)
{
    OpportunityRecommendation opportunity =
        BuildRecommendation(aID);

    RenderRecommendedOpportunity(opportunity, true);
}

///----------------------------------------------------------------------------------------------------
/// RenderRecommendedOpportunity:
///----------------------------------------------------------------------------------------------------
void RenderRecommendedOpportunity(
    const OpportunityRecommendation& aRecommendation,
    bool aMapContext)
{
    auto definitionIt =
        achievementDefinitions.find(aRecommendation.ID);

    const AchievementDefinition* definition =
        definitionIt != achievementDefinitions.end()
        ? &definitionIt->second
        : nullptr;

    const AchievementProgress* progress =
        FindAchievementProgress(aRecommendation.ID);

    const char* name =
        definition != nullptr && !definition->Name.empty()
        ? definition->Name.c_str()
        : "(Unknown achievement)";

    std::string primaryReason;

    // The card already displays the numeric percentage above its progress
    // bar. Prefer a different recommendation reason to avoid saying the same
    // percentage twice (for example, "33% complete" on two lines).
    for (const std::string& reason : aRecommendation.Reasons)
    {
        if (progress != nullptr &&
            progress->Current >= 0 && progress->Max > 0)
        {
            const int percent = static_cast<int>(
                GetAchievementProgressRatio(aRecommendation.ID) * 100.0 + 0.5);
            if (reason == std::to_string(percent) + "% complete")
            {
                continue;
            }
        }

        primaryReason = reason;
        break;
    }

    if (primaryReason.empty() &&
        aRecommendation.Reasons.empty() &&
        IsCharacterAdventureGuideAchievement(
            aRecommendation.ID))
    {
        if (definition != nullptr)
        {
            primaryReason =
                BuildRewardSummary(
                    aRecommendation.ID,
                    *definition
                );
        }

        // If there is no reward summary, the progress text already explains
        // the limitation clearly. Keep the row compact.
    }

    // Classic spacious achievement card: name, independent progress text,
    // a visible progress bar and a short reason. Keep the existing detailed
    // tree contents, context menu and actions untouched below this header.
    UBVisualCard visualCard;
    UBBeginVisualCard(visualCard);

    const std::string label =
        std::string(name) + "##recommended_" +
        std::to_string(aRecommendation.ID);

    const bool recommendationOpen = ImGui::TreeNodeEx(
        label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);

    if (ImGui::BeginPopupContextItem(
        ("##recommended_context_" +
            std::to_string(aRecommendation.ID)).c_str()))
    {
        if (ImGui::MenuItem("Copy achievement name"))
        {
            CopyAchievementName(name);
        }

        if (ImGui::MenuItem("Open in NexusGameWiki"))
        {
            OpenAchievementInNexusGameWiki(name);
        }

        if (ImGui::MenuItem("Copy + open Hero panel"))
        {
            CopyAchievementNameAndOpenHero(name);
        }

        ImGui::EndPopup();
    }

    UBRenderProgressCardSummary(
        aRecommendation.ID, progress, primaryReason);
    if (aMapContext)
    {
        UBRenderAvailability(aRecommendation.ID, false);
    }

    if (aMapContext &&
        UBClassifyMapLead(aRecommendation.ID) == UBMapLeadKind::Regional)
    {
        ImGui::TextColored(UBMutedInfoColor(),
            "Regional lead: location not confirmed on this map");
    }

    const std::string actionabilityTag =
        GetActionabilityLabel(aRecommendation.ID);
    if (!actionabilityTag.empty() && actionabilityTag != "General")
    {
        ImGui::TextDisabled("Type: %s", actionabilityTag.c_str());
    }

    if (recommendationOpen)
    {
        ImGui::Indent();

        if (!aRecommendation.Reasons.empty())
        {
            ImGui::TextColored(
                UBAccentColor(),
                "Why recommended:"
            );

            for (const std::string& reason :
                aRecommendation.Reasons)
            {
                ImGui::BulletText(
                    "%s",
                    reason.c_str()
                );
            }
        }

        ImGui::Text(
            "Source: %s",
            aMapContext
            ? UBGetOpportunitySourceLabel(aRecommendation.ID)
            : "Festival achievement"
        );
        if (aMapContext)
        {
            UBRenderAvailability(aRecommendation.ID, true);
        }

        if (aMapContext)
        {
            auto vps = ubVpsCurrentCandidates.find(aRecommendation.ID);
            if (vps != ubVpsCurrentCandidates.end())
            {
                ImGui::TextDisabled("VPS scope: %s | confidence: %s",
                    vps->second.Scope.c_str(),
                    vps->second.Confidence.c_str());
                if (!vps->second.Evidence.empty())
                {
                    ImGui::TextWrapped("Public index evidence: %s",
                        vps->second.Evidence.c_str());
                }
            }
        }

        const OpportunityMetadata metadata =
            InferOpportunityMetadata(
                aRecommendation.ID
            );

        if (!metadata.Tag.empty())
        {
            ImGui::Spacing();

            ImGui::Text(
                "Type: %s",
                metadata.Tag.c_str()
            );

            if (!metadata.Note.empty())
            {
                ImGui::TextWrapped(
                    "%s",
                    metadata.Note.c_str()
                );
            }
        }

        if (definition != nullptr &&
            !definition->Requirement.empty())
        {
            ImGui::Spacing();

            ImGui::TextWrapped(
                "What to do: %s",
                definition->Requirement.c_str()
            );
        }

        if (definition != nullptr &&
            !definition->Description.empty())
        {
            ImGui::Spacing();

            ImGui::TextWrapped(
                "Description: %s",
                definition->Description.c_str()
            );
        }

        if (definition != nullptr)
        {
            const std::string rewardSummary =
                BuildRewardSummary(
                    aRecommendation.ID,
                    *definition
                );

            if (!rewardSummary.empty())
            {
                ImGui::Spacing();

                ImGui::TextWrapped(
                    "Rewards: %s",
                    rewardSummary.c_str()
                );
            }
        }

        ImGui::Spacing();

        if (UBColoredActionButton("Open Wiki", UBActionButtonKind::Wiki))
        {
            OpenAchievementInNexusGameWiki(name);
        }

        ImGui::SameLine();

        if (UBColoredActionButton("Copy name", UBActionButtonKind::CopyName))
        {
            CopyAchievementName(name);
        }

        ImGui::SameLine();

        if (UBColoredActionButton("Copy + Hero", UBActionButtonKind::CopyAndHero))
        {
            CopyAchievementNameAndOpenHero(name);
        }

        ImGui::SameLine();

        if (UBColoredActionButton("Ignore this achievement", UBActionButtonKind::Ignore))
        {
            IgnoreAchievement(
                aRecommendation.ID
            );
        }

        ImGui::Text(
            "Achievement ID: %d",
            aRecommendation.ID
        );

        ImGui::Unindent();
        ImGui::TreePop();
    }

    UBEndVisualCard(visualCard);
}

///----------------------------------------------------------------------------------------------------
/// RenderOpportunityRow:
///----------------------------------------------------------------------------------------------------
void RenderOpportunityRow(int aID)
{
    auto definitionIt =
        achievementDefinitions.find(aID);

    const AchievementDefinition* definition =
        definitionIt != achievementDefinitions.end()
        ? &definitionIt->second
        : nullptr;

    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    const char* name =
        definition != nullptr && !definition->Name.empty()
        ? definition->Name.c_str()
        : "(Unknown achievement)";

    UBVisualCard visualCard;
    UBBeginVisualCard(visualCard, true);

    const std::string label =
        std::string(name) + "##achievement_" + std::to_string(aID);

    const bool opportunityOpen = ImGui::TreeNodeEx(
        label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);

    if (ImGui::BeginPopupContextItem(
        ("##achievement_context_" +
            std::to_string(aID)).c_str()))
    {
        if (ImGui::MenuItem("Copy achievement name"))
        {
            CopyAchievementName(name);
        }

        if (ImGui::MenuItem("Open in NexusGameWiki"))
        {
            OpenAchievementInNexusGameWiki(name);
        }

        if (ImGui::MenuItem("Copy + open Hero panel"))
        {
            CopyAchievementNameAndOpenHero(name);
        }

        ImGui::EndPopup();
    }

    std::string compactReason;
    if (definition != nullptr)
    {
        compactReason = BuildRewardSummary(aID, *definition);
    }
    UBRenderProgressCardSummary(aID, progress, compactReason);
    UBRenderAvailability(aID, false);

    if (opportunityOpen)
    {
        ImGui::Indent();

        ImGui::TextColored(
            UBAccentColor(),
            "Why shown:"
        );

        ImGui::TextWrapped(
            "%s",
            GetOpportunityReason(aID).c_str()
        );

        ImGui::Text(
            "Source: %s",
            UBGetOpportunitySourceLabel(aID)
        );
        UBRenderAvailability(aID, true);

        auto publicEvidence = ubVpsCurrentCandidates.find(aID);
        if (publicEvidence != ubVpsCurrentCandidates.end())
        {
            ImGui::TextDisabled("VPS scope: %s | confidence: %s",
                publicEvidence->second.Scope.c_str(),
                publicEvidence->second.Confidence.c_str());
            if (!publicEvidence->second.Evidence.empty())
            {
                ImGui::TextWrapped("Public index evidence: %s",
                    publicEvidence->second.Evidence.c_str());
            }
        }

        const OpportunityMetadata metadata =
            InferOpportunityMetadata(aID);

        if (!metadata.Tag.empty())
        {
            ImGui::Text(
                "Type: %s",
                metadata.Tag.c_str()
            );

            if (!metadata.Note.empty())
            {
                ImGui::TextWrapped(
                    "%s",
                    metadata.Note.c_str()
                );
            }
        }

        ImGui::Spacing();

        if (definition != nullptr &&
            !definition->Description.empty())
        {
            ImGui::TextWrapped(
                "Description: %s",
                definition->Description.c_str()
            );
        }
        else
        {
            ImGui::TextWrapped(
                "Description: No description provided by the GW2 API."
            );
        }

        ImGui::Spacing();

        if (definition != nullptr &&
            !definition->Requirement.empty())
        {
            ImGui::TextWrapped(
                "Requirement: %s",
                definition->Requirement.c_str()
            );
        }
        else
        {
            ImGui::TextWrapped(
                "Requirement: No requirement text provided by the GW2 API."
            );
        }

        ImGui::Spacing();

        if (IsCharacterAdventureGuideAchievement(aID))
        {
            ImGui::TextWrapped(
                "Progress: Character-specific. The Guild Wars 2 API does "
                "not expose Character Adventure Guide progress; check the "
                "in-game Achievements panel for the exact current value."
            );
        }
        else if (progress == nullptr)
        {
            ImGui::TextColored(
                UBDangerColor(),
                "Progress: No account progress record (availability unverified)"
            );
        }
        else if (progress->Current >= 0 && progress->Max > 0)
        {
            ImGui::Text(
                "Progress: %d / %d",
                progress->Current,
                progress->Max
            );
        }
        else
        {
            ImGui::TextColored(
                UBWarningColor(),
                "Progress: Started, numeric progress unavailable"
            );
        }

        if (definition != nullptr)
        {
            const int totalAP =
                GetTotalAchievementAP(*definition);

            const int remainingAP =
                GetRemainingAchievementAP(
                    aID,
                    *definition
                );

            if (totalAP > 0)
            {
                ImGui::Text(
                    "Achievement Points: %d remaining / %d total",
                    remainingAP,
                    totalAP
                );
            }

            const std::string rewardSummary =
                BuildRewardSummary(aID, *definition);

            if (!rewardSummary.empty())
            {
                ImGui::TextWrapped(
                    "Rewards: %s",
                    rewardSummary.c_str()
                );
            }
        }

        ImGui::Spacing();

        if (UBColoredActionButton("Open Wiki", UBActionButtonKind::Wiki))
        {
            OpenAchievementInNexusGameWiki(name);
        }

        ImGui::SameLine();

        if (UBColoredActionButton("Copy name", UBActionButtonKind::CopyName))
        {
            CopyAchievementName(name);
        }

        ImGui::SameLine();

        if (UBColoredActionButton("Copy + Hero", UBActionButtonKind::CopyAndHero))
        {
            CopyAchievementNameAndOpenHero(name);
        }

        ImGui::SameLine();

        if (UBColoredActionButton("Ignore this achievement", UBActionButtonKind::Ignore))
        {
            IgnoreAchievement(aID);
        }

        ImGui::Text("Achievement ID: %d", aID);

        ImGui::Unindent();
        ImGui::TreePop();
    }

    UBEndVisualCard(visualCard);
}

///----------------------------------------------------------------------------------------------------
/// CountCurrentMapOpportunityGroups:
///----------------------------------------------------------------------------------------------------
void CountCurrentMapOpportunityGroups(
    size_t& aAlmostDone,
    size_t& aInProgress,
    size_t& aNotStarted,
    size_t& aCharacterSpecific)
{
    aAlmostDone = 0;
    aInProgress = 0;
    aNotStarted = 0;
    aCharacterSpecific = 0;

    for (int id : currentMapOpportunityIDs)
    {
        if (IsCharacterAdventureGuideAchievement(id))
        {
            ++aCharacterSpecific;
            continue;
        }

        const AchievementProgress* progress =
            FindAchievementProgress(id);

        if (progress == nullptr)
        {
            ++aNotStarted;
            continue;
        }

        const double ratio = GetAchievementProgressRatio(id);

        if (ratio >= 0.75)
        {
            ++aAlmostDone;
        }
        else
        {
            ++aInProgress;
        }
    }
}

///----------------------------------------------------------------------------------------------------
/// UBContainsWholePhrase:
///     Avoids partial-word category and achievement text matches.
///----------------------------------------------------------------------------------------------------
bool UBContainsWholePhrase(
    const std::string& aText,
    const std::string& aPhrase)
{
    if (aPhrase.size() < 5 || aText.empty())
    {
        return false;
    }

    size_t at = aText.find(aPhrase);
    while (at != std::string::npos)
    {
        const size_t end = at + aPhrase.size();
        const bool leftBoundary = at == 0 ||
            !std::isalnum(static_cast<unsigned char>(aText[at - 1]));
        const bool rightBoundary = end == aText.size() ||
            !std::isalnum(static_cast<unsigned char>(aText[end]));
        if (leftBoundary && rightBoundary)
        {
            return true;
        }
        at = aText.find(aPhrase, at + 1);
    }
    return false;
}

///----------------------------------------------------------------------------------------------------
/// DoesAchievementMatchCurrentMapText:
///     Broad and regional categories must explicitly mention the relevant
///     place in public text before they can become recommendations.
///----------------------------------------------------------------------------------------------------
bool DoesAchievementMatchCurrentMapText(int aID)
{
    auto definitionIt =
        achievementDefinitions.find(aID);

    if (definitionIt == achievementDefinitions.end())
    {
        // Definition may not have been downloaded yet. It will be reconsidered
        // after the current-map definition sync completes.
        return false;
    }

    const AchievementDefinition& definition =
        definitionIt->second;

    const std::string combined =
        ToLowerCopy(
            definition.Name + " " +
            definition.Description + " " +
            definition.Requirement
        );

    if (currentOpportunityRegionalIDs.find(aID) !=
        currentOpportunityRegionalIDs.end())
    {
        return UBContainsWholePhrase(
            combined, ToLowerCopy(currentMapRegionName));
    }

    for (const std::string& term :
        currentOpportunityMatchTerms)
    {
        if (!term.empty() &&
            UBContainsWholePhrase(combined, term))
        {
            return true;
        }
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// BuildCurrentMapOpportunityList:
///----------------------------------------------------------------------------------------------------
void BuildCurrentMapOpportunityList()
{
    currentMapOpportunityIDs.clear();

    for (int id : currentOpportunityCategoryIDs)
    {
        if (currentOpportunityTextFilteredIDs.find(id) !=
            currentOpportunityTextFilteredIDs.end())
        {
            if (!DoesAchievementMatchCurrentMapText(id))
            {
                continue;
            }
        }

        const AchievementProgress* progress =
            FindAchievementProgress(id);

        // No account-progress record means the achievement has not been
        // started, so it is still unfinished.
        if ((progress == nullptr || !progress->Done) &&
            !IsOpportunityFiltered(id))
        {
            currentMapOpportunityIDs.push_back(id);
        }
    }

    std::sort(
        currentMapOpportunityIDs.begin(),
        currentMapOpportunityIDs.end(),
        [](int aLeft, int aRight)
        {
            const int leftTier = GetOpportunitySortTier(aLeft);
            const int rightTier = GetOpportunitySortTier(aRight);

            if (leftTier != rightTier)
            {
                return leftTier < rightTier;
            }

            if (leftTier == 0)
            {
                const double leftRatio =
                    GetAchievementProgressRatio(aLeft);

                const double rightRatio =
                    GetAchievementProgressRatio(aRight);

                if (leftRatio != rightRatio)
                {
                    return leftRatio > rightRatio;
                }
            }

            auto leftDefinition =
                achievementDefinitions.find(aLeft);

            auto rightDefinition =
                achievementDefinitions.find(aRight);

            const std::string leftName =
                leftDefinition != achievementDefinitions.end()
                ? leftDefinition->second.Name
                : std::string();

            const std::string rightName =
                rightDefinition != achievementDefinitions.end()
                ? rightDefinition->second.Name
                : std::string();

            return leftName < rightName;
        }
    );
}

///----------------------------------------------------------------------------------------------------
/// StartAchievementCategorySync:
///----------------------------------------------------------------------------------------------------
void StartAchievementCategorySync()
{
    if (achievementCategorySyncActive)
    {
        return;
    }

    achievementCategorySyncAttempted = true;
    achievementCategorySyncActive = true;
    achievementCategoriesLoaded = false;
    achievementCategoryStatus =
        "Loading achievement categories...";

    achievementCategorySyncFuture = std::async(
        std::launch::async,
        []()
        {
            return SyncAchievementCategories();
        }
    );
}

///----------------------------------------------------------------------------------------------------
/// UpdateAchievementCategorySync:
///----------------------------------------------------------------------------------------------------
void UpdateAchievementCategorySync()
{
    if (!achievementCategorySyncActive)
    {
        return;
    }

    if (achievementCategorySyncFuture.wait_for(
        std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        return;
    }

    AchievementCategorySyncResult result =
        achievementCategorySyncFuture.get();

    achievementCategorySyncActive = false;

    if (!result.Success)
    {
        achievementCategoriesLoaded = false;
        achievementCategoryStatus =
            result.Error.empty()
            ? "Achievement category sync failed."
            : result.Error;
        return;
    }

    achievementCategories = std::move(result.Categories);
    achievementCategoriesLoaded = true;
    achievementCategoryStatus =
        "Achievement categories loaded.";

    if (activeFestivalAvailable)
    {
        festivalOpportunityScanDirty = true;
    }
}

///----------------------------------------------------------------------------------------------------
/// StartCurrentMapDefinitionSync:
///----------------------------------------------------------------------------------------------------
void StartCurrentMapDefinitionSync(const std::vector<int>& aIDs)
{
    if (currentMapDefinitionSyncActive || aIDs.empty())
    {
        return;
    }

    // Reuse session-cached definitions. Map changes should not redownload
    // definitions which the account-wide or prior map scan already fetched.
    std::vector<int> missingIDs;
    missingIDs.reserve(aIDs.size());
    std::unordered_set<int> seen;

    for (int id : aIDs)
    {
        if (seen.insert(id).second &&
            achievementDefinitions.find(id) == achievementDefinitions.end())
        {
            missingIDs.push_back(id);
        }
    }

    currentMapDefinitionRequestMap = currentMapName;

    if (missingIDs.empty())
    {
        currentMapDefinitionsReady = true;
        BuildCurrentMapOpportunityList();
        currentOpportunityStatus =
            "Current-map opportunities ready from cached definitions.";
        return;
    }

    currentMapDefinitionSyncActive = true;
    currentMapDefinitionsReady = false;
    currentOpportunityStatus =
        "Loading " + std::to_string(missingIDs.size()) +
        " uncached map achievement definition(s)...";

    currentMapDefinitionSyncFuture = std::async(
        std::launch::async,
        [missingIDs]()
        {
            return FetchAchievementDefinitionsForIDs(missingIDs);
        }
    );
}

///----------------------------------------------------------------------------------------------------
/// UpdateCurrentMapDefinitionSync:
///----------------------------------------------------------------------------------------------------
void UpdateCurrentMapDefinitionSync()
{
    if (!currentMapDefinitionSyncActive)
    {
        return;
    }

    if (currentMapDefinitionSyncFuture.wait_for(
        std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        return;
    }

    AchievementDefinitionSyncResult result =
        currentMapDefinitionSyncFuture.get();

    currentMapDefinitionSyncActive = false;

    if (result.Success)
    {
        for (auto& pair : result.Definitions)
        {
            achievementDefinitions[pair.first] =
                std::move(pair.second);
        }
    }

    // If the player moved during a request, never apply the old map's status
    // or opportunity list to the new map. Rescan the new map next frame.
    if (currentMapDefinitionRequestMap != currentMapName)
    {
        currentMapDefinitionsReady = false;
        lastOpportunityMapName.clear();
        return;
    }

    if (!result.Success)
    {
        currentMapDefinitionsReady = false;
        currentOpportunityStatus =
            result.Error.empty()
            ? "Current-map definition sync failed."
            : result.Error;
        return;
    }

    currentMapDefinitionsReady = true;
    BuildCurrentMapOpportunityList();
    currentOpportunityStatus =
        "Current-map opportunities ready.";
}

///----------------------------------------------------------------------------------------------------
/// FindCategoryByName:
///----------------------------------------------------------------------------------------------------
const AchievementCategory* FindCategoryByName(
    const std::string& aName)
{
    for (const AchievementCategory& category :
        achievementCategories)
    {
        if (category.Name == aName)
        {
            return &category;
        }
    }

    return nullptr;
}

///----------------------------------------------------------------------------------------------------
/// GetFestivalLiveMarkerCategory:
///     Festival achievement categories are exposed by the official GW2 API
///     only while their festival is active. These temporary daily categories
///     therefore act as a reliable live festival signal without an external
///     calendar or website scrape.
///----------------------------------------------------------------------------------------------------
const char* GetFestivalLiveMarkerCategory(
    const std::string& aFestivalKey)
{
    if (aFestivalKey == "lunar_new_year")
    {
        return "Daily Lunar New Year";
    }

    if (aFestivalKey == "super_adventure_festival")
    {
        return "Daily Super Adventure Festival";
    }

    if (aFestivalKey == "dragon_bash")
    {
        return "Daily Dragon Bash";
    }

    if (aFestivalKey == "festival_of_the_four_winds")
    {
        return "Daily Festival of the Four Winds";
    }

    if (aFestivalKey == "halloween")
    {
        return "Halloween Daily";
    }

    if (aFestivalKey == "wintersday")
    {
        return "Wintersday Daily";
    }

    return nullptr;
}

///----------------------------------------------------------------------------------------------------
/// DetectActiveFestivalFromAchievementCategories:
///     Uses the live /v2/achievements/categories data we already load.
///     Temporary festival categories disappear from the API outside their
///     active festival, so no separate seasonal schedule request is needed.
///----------------------------------------------------------------------------------------------------
void DetectActiveFestivalFromAchievementCategories()
{
    if (!achievementCategoriesLoaded)
    {
        return;
    }

    const std::string previousFestivalKey =
        activeFestivalKey;

    const bool previouslyActive =
        activeFestivalAvailable;

    const FestivalDescriptor* detectedFestival =
        nullptr;

    for (const FestivalDescriptor& festival :
        FestivalDescriptors)
    {
        const char* markerName =
            GetFestivalLiveMarkerCategory(
                festival.Key
            );

        if (markerName == nullptr)
        {
            continue;
        }

        if (FindCategoryByName(markerName) != nullptr)
        {
            detectedFestival = &festival;
            break;
        }
    }

    if (detectedFestival == nullptr)
    {
        activeFestivalAvailable = false;
        activeFestivalKey.clear();
        activeFestivalName.clear();
        activeFestivalWikiPageTitle.clear();
        activeFestivalStartDate = {};
        activeFestivalEndDate = {};

        festivalScheduleStatus =
            "No active annual festival is exposed by the GW2 API.";

        if (previouslyActive)
        {
            festivalMatchedCategoryNames.clear();
            festivalCategoryIDs.clear();
            festivalOpportunityIDs.clear();
            festivalDefinitionsReady = false;
        }

        festivalOpportunityScanDirty = false;
        return;
    }

    activeFestivalAvailable = true;
    activeFestivalKey =
        detectedFestival->Key;

    activeFestivalName =
        detectedFestival->DisplayName;

    activeFestivalWikiPageTitle.clear();
    activeFestivalStartDate = {};
    activeFestivalEndDate = {};

    festivalScheduleStatus =
        "Active festival detected from the live GW2 category index.";

    const bool festivalChanged =
        !previouslyActive ||
        previousFestivalKey !=
        activeFestivalKey;

    if (festivalChanged)
    {
        festivalMatchedCategoryNames.clear();
        festivalCategoryIDs.clear();
        festivalOpportunityIDs.clear();
        festivalDefinitionsReady = false;
    }

    festivalOpportunityScanDirty = true;
}

///----------------------------------------------------------------------------------------------------
/// FindMapCategoryAliases:
///----------------------------------------------------------------------------------------------------
const MapCategoryAlias* FindMapCategoryAliases(
    const std::string& aMapName)
{
    for (const MapCategoryAlias& alias :
        MapCategoryAliases)
    {
        if (aMapName == alias.MapName)
        {
            return &alias;
        }
    }

    return nullptr;
}

///----------------------------------------------------------------------------------------------------
/// AppendUniqueAchievementIDs:
///----------------------------------------------------------------------------------------------------
void AppendUniqueAchievementIDs(
    std::vector<int>& aDestination,
    const std::vector<int>& aSource)
{
    std::unordered_set<int> seen(
        aDestination.begin(),
        aDestination.end()
    );

    for (int id : aSource)
    {
        if (seen.insert(id).second)
        {
            aDestination.push_back(id);
        }
    }
}

///----------------------------------------------------------------------------------------------------
/// DoesFestivalCategoryMatch:
///----------------------------------------------------------------------------------------------------
bool DoesFestivalCategoryMatch(
    const FestivalDescriptor& aFestival,
    const std::string& aCategoryName)
{
    const std::string lowerName =
        ToLowerCopy(aCategoryName);

    for (const std::string& fragment :
        aFestival.CategoryNameFragments)
    {
        const std::string lowerFragment =
            ToLowerCopy(fragment);

        if (!lowerFragment.empty() &&
            lowerName.find(lowerFragment) !=
            std::string::npos)
        {
            return true;
        }
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// IsCurrentMapFestivalHost:
///----------------------------------------------------------------------------------------------------
bool IsCurrentMapFestivalHost()
{
    if (!activeFestivalAvailable)
    {
        return false;
    }

    const FestivalDescriptor* festival =
        GetFestivalDescriptorByKey(
            activeFestivalKey
        );

    if (festival == nullptr)
    {
        return false;
    }

    for (const std::string& hostMap :
        festival->HostMaps)
    {
        if (currentMapName == hostMap)
        {
            return true;
        }
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// GetFestivalDateDisplay:
///----------------------------------------------------------------------------------------------------
std::string GetFestivalDateDisplay(
    const FestivalDate& aDate)
{
    static const char* monthNames[] =
    {
        "",
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec"
    };

    if (aDate.Month < 1 ||
        aDate.Month > 12)
    {
        return {};
    }

    return
        std::string(monthNames[aDate.Month]) +
        " " +
        std::to_string(aDate.Day);
}

///----------------------------------------------------------------------------------------------------
/// BuildFestivalOpportunityList:
///----------------------------------------------------------------------------------------------------
void BuildFestivalOpportunityList()
{
    festivalOpportunityIDs.clear();

    if (!activeFestivalAvailable ||
        !festivalDefinitionsReady)
    {
        return;
    }

    for (int id : festivalCategoryIDs)
    {
        const AchievementProgress* progress =
            FindAchievementProgress(id);

        if ((progress == nullptr ||
            !progress->Done) &&
            !IsOpportunityFiltered(id))
        {
            festivalOpportunityIDs.push_back(id);
        }
    }

    std::sort(
        festivalOpportunityIDs.begin(),
        festivalOpportunityIDs.end(),
        [](int aLeft, int aRight)
        {
            const OpportunityRecommendation left =
                BuildRecommendation(aLeft);

            const OpportunityRecommendation right =
                BuildRecommendation(aRight);

            if (left.Score != right.Score)
            {
                return left.Score > right.Score;
            }

            auto leftDefinition =
                achievementDefinitions.find(aLeft);

            auto rightDefinition =
                achievementDefinitions.find(aRight);

            const std::string leftName =
                leftDefinition !=
                achievementDefinitions.end()
                ? leftDefinition->second.Name
                : std::string();

            const std::string rightName =
                rightDefinition !=
                achievementDefinitions.end()
                ? rightDefinition->second.Name
                : std::string();

            return leftName < rightName;
        }
    );
}

///----------------------------------------------------------------------------------------------------
/// BuildFestivalRecommendations:
///----------------------------------------------------------------------------------------------------
std::vector<OpportunityRecommendation>
BuildFestivalRecommendations()
{
    std::vector<OpportunityRecommendation>
        recommendations;

    for (int id : festivalOpportunityIDs)
    {
        recommendations.push_back(
            BuildRecommendation(id)
        );
    }

    std::sort(
        recommendations.begin(),
        recommendations.end(),
        [](const OpportunityRecommendation& aLeft,
            const OpportunityRecommendation& aRight)
        {
            if (aLeft.Score != aRight.Score)
            {
                return aLeft.Score > aRight.Score;
            }

            auto leftDefinition =
                achievementDefinitions.find(
                    aLeft.ID
                );

            auto rightDefinition =
                achievementDefinitions.find(
                    aRight.ID
                );

            const std::string leftName =
                leftDefinition !=
                achievementDefinitions.end()
                ? leftDefinition->second.Name
                : std::string();

            const std::string rightName =
                rightDefinition !=
                achievementDefinitions.end()
                ? rightDefinition->second.Name
                : std::string();

            return leftName < rightName;
        }
    );

    if (recommendations.size() > 5)
    {
        recommendations.resize(5);
    }

    return recommendations;
}

///----------------------------------------------------------------------------------------------------
/// StartFestivalDefinitionSync:
///----------------------------------------------------------------------------------------------------
void StartFestivalDefinitionSync()
{
    if (festivalDefinitionSyncActive ||
        festivalCategoryIDs.empty())
    {
        return;
    }

    festivalDefinitionSyncActive = true;
    festivalDefinitionsReady = false;

    const std::vector<int> idsCopy =
        festivalCategoryIDs;

    festivalDefinitionSyncFuture =
        std::async(
            std::launch::async,
            [idsCopy]()
            {
                return
                    FetchAchievementDefinitionsForIDs(
                        idsCopy
                    );
            }
        );
}

///----------------------------------------------------------------------------------------------------
/// UpdateFestivalDefinitionSync:
///----------------------------------------------------------------------------------------------------
void UpdateFestivalDefinitionSync()
{
    if (!festivalDefinitionSyncActive)
    {
        return;
    }

    if (festivalDefinitionSyncFuture.wait_for(
        std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    AchievementDefinitionSyncResult result =
        festivalDefinitionSyncFuture.get();

    festivalDefinitionSyncActive = false;

    if (!result.Success)
    {
        festivalDefinitionsReady = false;
        return;
    }

    for (auto& pair : result.Definitions)
    {
        achievementDefinitions[pair.first] =
            std::move(pair.second);
    }

    festivalDefinitionsReady = true;
    BuildFestivalOpportunityList();
}

///----------------------------------------------------------------------------------------------------
/// UpdateActiveFestivalOpportunityScan:
///----------------------------------------------------------------------------------------------------
void UpdateActiveFestivalOpportunityScan()
{
    if (!activeFestivalAvailable ||
        !achievementCategoriesLoaded ||
        !achievementDataLoaded)
    {
        return;
    }

    if (!festivalOpportunityScanDirty)
    {
        return;
    }

    const FestivalDescriptor* festival =
        GetFestivalDescriptorByKey(
            activeFestivalKey
        );

    if (festival == nullptr)
    {
        festivalOpportunityScanDirty = false;
        return;
    }

    festivalMatchedCategoryNames.clear();
    festivalCategoryIDs.clear();
    festivalOpportunityIDs.clear();
    festivalDefinitionsReady = false;

    for (const AchievementCategory& category :
        achievementCategories)
    {
        if (IsRetiredCategoryName(
            category.Name))
        {
            continue;
        }

        if (!DoesFestivalCategoryMatch(
            *festival,
            category.Name))
        {
            continue;
        }

        festivalMatchedCategoryNames.push_back(
            category.Name
        );

        AppendUniqueAchievementIDs(
            festivalCategoryIDs,
            category.AchievementIDs
        );
    }

    festivalOpportunityScanDirty = false;

    if (festivalCategoryIDs.empty())
    {
        return;
    }

    StartFestivalDefinitionSync();
}

///----------------------------------------------------------------------------------------------------
/// RenderActiveFestivalSection:
///----------------------------------------------------------------------------------------------------
void RenderActiveFestivalSection()
{
    if (!activeFestivalAvailable)
    {
        return;
    }

    ImGui::Separator();

    ImGui::TextColored(
        UBSpecialColor(),
        "Active Festival: %s",
        activeFestivalName.c_str()
    );

    if (IsCurrentMapFestivalHost())
    {
        ImGui::SameLine();

        ImGui::TextColored(
            UBSuccessColor(),
            "| Host location"
        );
    }

    ImGui::TextDisabled(
        "%s",
        festivalDetectionSource.c_str()
    );

    if (!apiKeyValid ||
        !achievementDataLoaded ||
        !achievementCategoriesLoaded)
    {
        ImGui::TextDisabled(
            "Festival achievements will appear when account "
            "achievement data is ready."
        );

        ImGui::Spacing();
        return;
    }

    if (festivalDefinitionSyncActive)
    {
        ImGui::TextDisabled(
            "Loading festival achievement details..."
        );

        ImGui::Spacing();
        return;
    }

    if (festivalCategoryIDs.empty())
    {
        ImGui::TextDisabled(
            "The festival is active, but no matching achievement "
            "categories were found in the GW2 API."
        );

        ImGui::Spacing();
        return;
    }

    if (!festivalDefinitionsReady)
    {
        ImGui::TextDisabled(
            "Preparing festival opportunities..."
        );

        ImGui::Spacing();
        return;
    }

    ImGui::TextDisabled(
        "%llu unfinished | %llu matched categories",
        static_cast<unsigned long long>(
            festivalOpportunityIDs.size()
            ),
        static_cast<unsigned long long>(
            festivalMatchedCategoryNames.size()
            )
    );

    if (festivalOpportunityIDs.empty())
    {
        ImGui::TextDisabled(
            "No unfinished festival achievements match your "
            "current filters."
        );

        ImGui::Spacing();
        return;
    }

    const std::vector<OpportunityRecommendation>
        recommendations =
        BuildFestivalRecommendations();

    if (!recommendations.empty())
    {
        ImGui::Spacing();

        ImGui::TextColored(
            UBSpecialColor(),
            IsCurrentMapFestivalHost()
            ? "Recommended festival opportunities here"
            : "Recommended festival opportunities"
        );

        for (size_t index = 0;
            index < recommendations.size();
            ++index)
        {
            ImGui::PushID(
                700000 +
                static_cast<int>(index)
            );

            RenderRecommendedOpportunity(
                recommendations[index]
            );

            ImGui::PopID();

        }
    }

    ImGui::Spacing();

    if (ImGui::CollapsingHeader(
        "All unfinished festival achievements"))
    {
        if (ImGui::BeginChild(
            "##FestivalOpportunityList",
            ImVec2(0.0f, 260.0f),
            true))
        {
            for (size_t index = 0;
                index < festivalOpportunityIDs.size();
                ++index)
            {
                const int id =
                    festivalOpportunityIDs[index];

                ImGui::PushID(
                    710000 +
                    static_cast<int>(index)
                );

                RenderRecommendedOpportunity(
                    BuildRecommendation(id)
                );

                ImGui::PopID();

            }
        }

        ImGui::EndChild();
    }

    ImGui::Spacing();
}


///----------------------------------------------------------------------------------------------------
/// CoverageLead:
///----------------------------------------------------------------------------------------------------
struct CoverageLead
{
    int AchievementID = 0;
    std::string Name;
    std::vector<std::string> Categories;
    bool IsRetired = false;
};

///----------------------------------------------------------------------------------------------------
/// AchievementDefinitionMentionsCurrentMap:
///----------------------------------------------------------------------------------------------------
bool AchievementDefinitionMentionsCurrentMap(
    const AchievementDefinition& aDefinition)
{
    if (currentMapName.empty() ||
        currentMapName == "Loading map name..." ||
        currentMapName.find("Map lookup failed:") == 0)
    {
        return false;
    }

    const std::string mapNeedle =
        ToLowerCopy(currentMapName);

    const std::string combined =
        ToLowerCopy(
            aDefinition.Name + " " +
            aDefinition.Description + " " +
            aDefinition.Requirement
        );

    return
        !mapNeedle.empty() &&
        combined.find(mapNeedle) != std::string::npos;
}

///----------------------------------------------------------------------------------------------------
/// GetAchievementCategoryNames:
///----------------------------------------------------------------------------------------------------
std::vector<std::string> GetAchievementCategoryNames(int aID)
{
    std::vector<std::string> names;

    for (const AchievementCategory& category :
        achievementCategories)
    {
        if (std::find(
            category.AchievementIDs.begin(),
            category.AchievementIDs.end(),
            aID) != category.AchievementIDs.end())
        {
            names.push_back(category.Name);
        }
    }

    return names;
}

///----------------------------------------------------------------------------------------------------
/// IsRetiredCategoryName:
///----------------------------------------------------------------------------------------------------
bool IsRetiredCategoryName(const std::string& aName)
{
    const std::string lowered =
        ToLowerCopy(aName);

    return
        lowered.find("retired") != std::string::npos ||
        lowered.find("historical") != std::string::npos;
}

///----------------------------------------------------------------------------------------------------
/// IsRetiredAchievement:
///----------------------------------------------------------------------------------------------------
bool IsRetiredAchievement(int aID)
{
    const std::vector<std::string> categories =
        GetAchievementCategoryNames(aID);

    for (const std::string& categoryName : categories)
    {
        if (IsRetiredCategoryName(categoryName))
        {
            return true;
        }
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// BuildUnmappedCoverageLeads:
///     Finds unfinished achievements whose public definition names the
///     current map but which are not part of the current map mapping.
///----------------------------------------------------------------------------------------------------
std::vector<CoverageLead> BuildUnmappedCoverageLeads()
{
    std::vector<CoverageLead> leads;

    if (!achievementDefinitionsLoaded ||
        currentMapName.empty())
    {
        return leads;
    }

    std::unordered_set<int> mappedIDs(
        currentOpportunityCategoryIDs.begin(),
        currentOpportunityCategoryIDs.end()
    );

    for (const auto& pair : achievementDefinitions)
    {
        const int id = pair.first;
        const AchievementDefinition& definition =
            pair.second;

        const AchievementProgress* progress =
            FindAchievementProgress(id);

        if (progress != nullptr && progress->Done)
        {
            continue;
        }

        if (mappedIDs.find(id) != mappedIDs.end())
        {
            continue;
        }

        if (!AchievementDefinitionMentionsCurrentMap(
            definition))
        {
            continue;
        }

        CoverageLead lead;
        lead.AchievementID = id;
        lead.Name =
            definition.Name.empty()
            ? "(Unnamed achievement)"
            : definition.Name;

        lead.Categories =
            GetAchievementCategoryNames(id);

        lead.IsRetired =
            IsRetiredAchievement(id);

        leads.push_back(std::move(lead));
    }

    std::sort(
        leads.begin(),
        leads.end(),
        [](const CoverageLead& aLeft,
            const CoverageLead& aRight)
        {
            return aLeft.Name < aRight.Name;
        }
    );

    return leads;
}

///----------------------------------------------------------------------------------------------------
/// JoinCoverageLeadCategories:
///----------------------------------------------------------------------------------------------------
std::string JoinCoverageLeadCategories(
    const std::vector<std::string>& aCategories)
{
    if (aCategories.empty())
    {
        return "Category unavailable";
    }

    std::string result;

    for (const std::string& category :
        aCategories)
    {
        if (!result.empty())
        {
            result += ", ";
        }

        result += category;
    }

    return result;
}

///----------------------------------------------------------------------------------------------------
/// CountActionableCoverageLeads:
///----------------------------------------------------------------------------------------------------
size_t CountActionableCoverageLeads(
    const std::vector<CoverageLead>& aLeads)
{
    size_t count = 0;

    for (const CoverageLead& lead : aLeads)
    {
        if (!lead.IsRetired)
        {
            ++count;
        }
    }

    return count;
}

///----------------------------------------------------------------------------------------------------
/// CountRetiredCoverageLeads:
///----------------------------------------------------------------------------------------------------
size_t CountRetiredCoverageLeads(
    const std::vector<CoverageLead>& aLeads)
{
    size_t count = 0;

    for (const CoverageLead& lead : aLeads)
    {
        if (lead.IsRetired)
        {
            ++count;
        }
    }

    return count;
}


///----------------------------------------------------------------------------------------------------
/// GetMapCoverageModeLabel:
///----------------------------------------------------------------------------------------------------
const char* GetMapCoverageModeLabel()
{
    switch (currentCoverageMode)
    {
    case MapCoverageMode::ExactCategory:
        return "Exact category match";

    case MapCoverageMode::CuratedMapping:
        return "Curated map mapping";

    case MapCoverageMode::ContextualDiscovery:
        return "Contextual / regional discovery (partial)";

    default:
        return "No coverage";
    }
}

///----------------------------------------------------------------------------------------------------
/// CountRelevantContextualIDs:
///----------------------------------------------------------------------------------------------------
size_t CountRelevantContextualIDs()
{
    size_t count = 0;

    for (int id : currentOpportunityTextFilteredIDs)
    {
        if (DoesAchievementMatchCurrentMapText(id))
        {
            ++count;
        }
    }

    return count;
}

///----------------------------------------------------------------------------------------------------
/// CountIgnoredCurrentMapIDs:
///----------------------------------------------------------------------------------------------------
size_t CountIgnoredCurrentMapIDs()
{
    size_t count = 0;

    for (int id : currentOpportunityCategoryIDs)
    {
        if (ignoredAchievementIDs.find(id) !=
            ignoredAchievementIDs.end())
        {
            ++count;
        }
    }

    return count;
}

///----------------------------------------------------------------------------------------------------
/// CountUserFilteredCurrentMapIDs:
///----------------------------------------------------------------------------------------------------
size_t CountUserFilteredCurrentMapIDs()
{
    size_t count = 0;

    for (int id : currentOpportunityCategoryIDs)
    {
        // Contextual candidates that do not actually mention the current map
        // are mapping rejections, not user-filter rejections.
        if (currentOpportunityTextFilteredIDs.find(id) !=
            currentOpportunityTextFilteredIDs.end() &&
            !DoesAchievementMatchCurrentMapText(id))
        {
            continue;
        }

        if (ignoredAchievementIDs.find(id) !=
            ignoredAchievementIDs.end())
        {
            continue;
        }

        const AchievementProgress* progress =
            FindAchievementProgress(id);

        if (progress != nullptr && progress->Done)
        {
            continue;
        }

        const OpportunityMetadata metadata =
            InferOpportunityMetadata(id);

        bool hidden = false;

        if (hideNotStartedAchievements &&
            progress == nullptr)
        {
            hidden = true;
        }

        if (hideStoryAchievements &&
            metadata.Actionability ==
            OpportunityActionability::Story)
        {
            hidden = true;
        }

        if (hideEventAchievements &&
            metadata.Actionability ==
            OpportunityActionability::Event)
        {
            hidden = true;
        }

        if (hideCollectionAchievements &&
            metadata.Actionability ==
            OpportunityActionability::Collection)
        {
            hidden = true;
        }

        if (hidden)
        {
            ++count;
        }
    }

    return count;
}

///----------------------------------------------------------------------------------------------------
/// UpdateCurrentMapOpportunityScan:
///     Matches the live map name to an achievement category.
///----------------------------------------------------------------------------------------------------
// Skip festival/daily/retired categories in normal map discovery. Seasonal
// content remains in the Holiday tab where its active dates are checked.
bool UBIsNonMapCategory(const std::string& aName)
{
    const std::string lowered = ToLowerCopy(aName);
    if (IsRetiredCategoryName(aName) ||
        (lowered.size() >= 5 && lowered.compare(0, 5, "daily") == 0) ||
        (lowered.size() >= 6 && lowered.compare(0, 6, "weekly") == 0) ||
        (lowered.size() >= 6 && lowered.compare(0, 6, "annual") == 0) ||
        lowered.find("pvp") != std::string::npos ||
        lowered.find("wvw") != std::string::npos)
    {
        return true;
    }

    for (const FestivalDescriptor& festival : FestivalDescriptors)
    {
        for (const std::string& fragment : festival.CategoryNameFragments)
        {
            if (UBContainsWholePhrase(lowered, ToLowerCopy(fragment)))
            {
                return true;
            }
        }
    }

    return false;
}

///----------------------------------------------------------------------------------------------------
/// UpdateCurrentMapOpportunityScan:
///     Preserve exact/curated mapping first, then discover map and regional
///     candidates using public category and achievement text. Never imply
///     full map coverage based only on an empty result set.
///----------------------------------------------------------------------------------------------------
void UpdateCurrentMapOpportunityScan()
{
    if (!achievementCategoriesLoaded ||
        !achievementDataLoaded ||
        currentMapName.empty() ||
        currentMapName == "Loading map name..." ||
        currentMapName.find("Map lookup failed:") == 0)
    {
        return;
    }

    if (ubVpsRescanPending && !currentMapDefinitionSyncActive)
    {
        lastOpportunityMapName.clear();
        ubVpsRescanPending = false;
    }

    if (currentMapName == lastOpportunityMapName)
    {
        return;
    }

    lastOpportunityMapName = currentMapName;
    currentOpportunityCategoryName.clear();
    currentOpportunityCategoryIDs.clear();
    currentMapOpportunityIDs.clear();
    ubVpsCurrentCandidates.clear();
    ubVpsOnlyCandidateIDs.clear();
    ubVpsMergedCount = 0;
    ubVpsRegionalCount = 0;
    currentOpportunityDirectIDs.clear();
    currentOpportunityTextFilteredIDs.clear();
    currentOpportunityMatchTerms.clear();
    currentOpportunityRegionalIDs.clear();
    currentCoverageDirectCategories.clear();
    currentCoverageContextualCategories.clear();
    currentCoverageMode = MapCoverageMode::None;
    currentAutoMapMatchCount = 0;
    currentRegionalMatchCount = 0;
    currentDiscoveryScanComplete = achievementDefinitionsLoaded;
    currentMapDefinitionsReady = false;

    const std::string mapNeedle = ToLowerCopy(currentMapName);
    const std::string regionNeedle = ToLowerCopy(currentMapRegionName);
    const bool usableRegion = regionNeedle.size() >= 5 &&
        regionNeedle != "tyria" && regionNeedle != "the mists" &&
        regionNeedle != mapNeedle;
    currentOpportunityMatchTerms.push_back(mapNeedle);

    // Build category eligibility once. Avoid searching every category for
    // every definition during a map change (can be thousands of entries).
    std::unordered_set<int> eligibleIDs;
    std::unordered_set<int> excludedIDs;
    for (const AchievementCategory& category : achievementCategories)
    {
        if (UBIsNonMapCategory(category.Name))
        {
            // Do not promote old, daily or seasonal achievements to ordinary
            // map recommendations, even if also listed in a broad category.
            excludedIDs.insert(
                category.AchievementIDs.begin(),
                category.AchievementIDs.end());
        }
        else
        {
            eligibleIDs.insert(
                category.AchievementIDs.begin(),
                category.AchievementIDs.end());
        }
    }

    std::unordered_set<int> knownIDs;
    size_t regionalCandidateFetches = 0;
    bool regionalCatalogTruncated = false;
    constexpr size_t regionalCandidateLimit = 80;

    auto addIDs = [&](const AchievementCategory& category,
        bool requiresText, bool regional)
        {
            for (int id : category.AchievementIDs)
            {
                // Keep direct map IDs intact, but bound exploratory regional
                // requests so they cannot hold up core map achievements.
                if (regional && knownIDs.count(id) == 0 &&
                    regionalCandidateFetches >= regionalCandidateLimit)
                {
                    regionalCatalogTruncated = true;
                    continue;
                }
                if (knownIDs.insert(id).second)
                {
                    currentOpportunityCategoryIDs.push_back(id);
                    if (regional)
                    {
                        ++regionalCandidateFetches;
                    }
                }
                if (regional)
                {
                    if (currentOpportunityDirectIDs.count(id) == 0 &&
                        currentOpportunityTextFilteredIDs.count(id) == 0)
                    {
                        currentOpportunityRegionalIDs.insert(id);
                        currentOpportunityTextFilteredIDs.insert(id);
                    }
                }
                else if (requiresText)
                {
                    if (currentOpportunityDirectIDs.count(id) == 0)
                    {
                        currentOpportunityRegionalIDs.erase(id);
                        currentOpportunityTextFilteredIDs.insert(id);
                    }
                }
                else
                {
                    currentOpportunityRegionalIDs.erase(id);
                    currentOpportunityTextFilteredIDs.erase(id);
                    currentOpportunityDirectIDs.insert(id);
                }
            }
        };

    // The authoritative exact category remains the strongest evidence.
    const AchievementCategory* exact = FindCategoryByName(currentMapName);
    if (exact != nullptr)
    {
        currentCoverageMode = MapCoverageMode::ExactCategory;
        currentCoverageDirectCategories.push_back(exact->Name);
        currentOpportunityCategoryName = exact->Name;
        addIDs(*exact, false, false);
    }

    // Existing curated mappings continue to take precedence where available.
    const MapCategoryAlias* alias = FindMapCategoryAliases(currentMapName);
    if (alias != nullptr)
    {
        if (currentCoverageMode == MapCoverageMode::None)
        {
            currentCoverageMode = MapCoverageMode::CuratedMapping;
            currentOpportunityCategoryName = currentMapName + " (curated)";
        }
        for (const std::string& term : alias->MatchTerms)
        {
            const std::string lowered = ToLowerCopy(term);
            if (std::find(currentOpportunityMatchTerms.begin(),
                currentOpportunityMatchTerms.end(), lowered) ==
                currentOpportunityMatchTerms.end())
            {
                currentOpportunityMatchTerms.push_back(lowered);
            }
        }
        for (const std::string& name : alias->PrimaryCategoryNames)
        {
            const AchievementCategory* category = FindCategoryByName(name);
            if (category == nullptr || UBIsNonMapCategory(category->Name))
            {
                continue;
            }
            currentCoverageDirectCategories.push_back(category->Name);
            addIDs(*category, false, false);
        }
        for (const std::string& name : alias->SecondaryCategoryNames)
        {
            const AchievementCategory* category = FindCategoryByName(name);
            if (category == nullptr || UBIsNonMapCategory(category->Name))
            {
                continue;
            }
            currentCoverageContextualCategories.push_back(category->Name);
            addIDs(*category, true, false);
        }
    }

    // Discover category names containing the *whole* current map name, rather
    // than only the exact category or the hand-maintained mapping table.
    for (const AchievementCategory& category : achievementCategories)
    {
        if (UBIsNonMapCategory(category.Name) ||
            (exact != nullptr && category.ID == exact->ID))
        {
            continue;
        }
        const std::string categoryName = ToLowerCopy(category.Name);
        if (UBContainsWholePhrase(categoryName, mapNeedle))
        {
            const size_t before = knownIDs.size();
            addIDs(category, false, false);
            currentAutoMapMatchCount += knownIDs.size() - before;
            currentCoverageDirectCategories.push_back(
                category.Name + " (auto-discovered)");
        }
        else if (usableRegion &&
            UBContainsWholePhrase(categoryName, regionNeedle))
        {
            addIDs(category, true, true);
            currentCoverageContextualCategories.push_back(
                category.Name + " (regional)");
        }
    }

    // Search account-known public definitions for explicit references to the
    // map, even when the category has an unrelated name. Only known cached
    // definitions are scanned; unknown unstarted global achievements are not
    // presented as complete or fully searched.
    for (const auto& entry : achievementDefinitions)
    {
        const int id = entry.first;
        const AchievementProgress* progress = FindAchievementProgress(id);
        if ((progress != nullptr && progress->Done) ||
            eligibleIDs.count(id) == 0 || excludedIDs.count(id) != 0)
        {
            continue;
        }
        const AchievementDefinition& definition = entry.second;
        const std::string text = ToLowerCopy(
            definition.Name + " " + definition.Description + " " +
            definition.Requirement);
        const bool mapMention = UBContainsWholePhrase(text, mapNeedle);
        const bool regionalMention = !mapMention && usableRegion &&
            UBContainsWholePhrase(
                ToLowerCopy(definition.Name + " " + definition.Requirement),
                regionNeedle);

        if (mapMention)
        {
            if (knownIDs.insert(id).second)
            {
                currentOpportunityCategoryIDs.push_back(id);
                ++currentAutoMapMatchCount;
            }
            if (currentOpportunityDirectIDs.count(id) == 0)
            {
                currentOpportunityRegionalIDs.erase(id);
                currentOpportunityTextFilteredIDs.insert(id);
            }
        }
        else if (regionalMention &&
            currentOpportunityDirectIDs.count(id) == 0 &&
            currentOpportunityTextFilteredIDs.count(id) == 0)
        {
            if (knownIDs.insert(id).second)
            {
                currentOpportunityCategoryIDs.push_back(id);
            }
            currentOpportunityTextFilteredIDs.insert(id);
            currentOpportunityRegionalIDs.insert(id);
        }
    }

    // Merge the optional session-cached VPS evidence. This never sends any
    // account data to the server; filtering remains entirely local.
    if (ubVpsIndexEnabled && MumbleLink != nullptr &&
        MumbleLink->Context.MapID == displayedMapID &&
        currentMapName != "Loading map name...")
    {
        auto indexed = ubVpsCache.find(displayedMapID);
        if (indexed != ubVpsCache.end())
        {
            for (const UBVpsCandidate& candidate : indexed->second.Candidates)
            {
                const int id = candidate.ID;
                // Apply the same normal-category guardrails as local discovery:
                // no daily, seasonal, retired, PvP, or WvW-only candidates.
                if (eligibleIDs.count(id) == 0 || excludedIDs.count(id) != 0)
                {
                    continue;
                }
                const AchievementProgress* progress = FindAchievementProgress(id);
                if (progress != nullptr && progress->Done)
                {
                    continue;
                }

                const bool localAlreadyKnows = knownIDs.count(id) != 0;
                ubVpsCurrentCandidates[id] = candidate;
                if (!localAlreadyKnows)
                {
                    knownIDs.insert(id);
                    ubVpsOnlyCandidateIDs.insert(id);
                    currentOpportunityCategoryIDs.push_back(id);
                    ++ubVpsMergedCount;
                }

                // Exact *category-name* evidence is comparable to the local
                // map category scan. Text matches are classified later from
                // their public GW2 definitions, not trusted blindly.
                if (candidate.Scope == "map" &&
                    ((candidate.Source == "category_name" &&
                        candidate.Confidence == "explicit") ||
                        candidate.Source == "reviewed"))
                {
                    currentOpportunityRegionalIDs.erase(id);
                    currentOpportunityTextFilteredIDs.erase(id);
                    currentOpportunityDirectIDs.insert(id);
                }
                else if (candidate.Scope == "regional" &&
                    currentOpportunityDirectIDs.count(id) == 0)
                {
                    currentOpportunityRegionalIDs.insert(id);
                    // No forced current-map text requirement: this is a
                    // separately labeled regional research lead.
                    currentOpportunityTextFilteredIDs.erase(id);
                    ++ubVpsRegionalCount;
                }
                else if (candidate.Scope == "map" &&
                    currentOpportunityDirectIDs.count(id) == 0)
                {
                    // UBClassifyMapLead promotes a map lead only if the
                    // fetched GW2 definition explicitly names this map.
                    currentOpportunityRegionalIDs.erase(id);
                    currentOpportunityTextFilteredIDs.erase(id);
                }
            }
            if (currentCoverageMode == MapCoverageMode::None &&
                !ubVpsCurrentCandidates.empty())
            {
                currentCoverageMode = MapCoverageMode::ContextualDiscovery;
                currentOpportunityCategoryName =
                    currentMapName + " (public VPS candidates)";
            }
        }
    }

    currentRegionalMatchCount = currentOpportunityRegionalIDs.size();

    if (regionalCatalogTruncated)
    {
        currentDiscoveryScanComplete = false;
    }

    if (currentCoverageMode == MapCoverageMode::None &&
        !currentOpportunityCategoryIDs.empty())
    {
        currentCoverageMode = MapCoverageMode::ContextualDiscovery;
        currentOpportunityCategoryName = currentMapName + " (discovered)";
    }

    if (currentOpportunityCategoryIDs.empty())
    {
        // No verified candidate is not evidence of 100% completion.
        currentMapDefinitionsReady = true;
        currentOpportunityStatus =
            achievementDefinitionsLoaded
            ? "Limited coverage: no verified map-specific candidates found. "
            "This does not mean the map is completed."
            : "Limited coverage: awaiting the public achievement catalog "
            "for further map discovery.";
        return;
    }

    if (currentOpportunityCategoryName.empty())
    {
        currentOpportunityCategoryName = currentMapName + " (discovered)";
    }

    BuildCurrentMapOpportunityList();
    StartCurrentMapDefinitionSync(currentOpportunityCategoryIDs);
}

///----------------------------------------------------------------------------------------------------
/// StartMapLookup:
///----------------------------------------------------------------------------------------------------
void StartMapLookup(uint32_t aMapID)
{
    if (aMapID == 0 || mapLookupActive)
    {
        return;
    }

    mapLookupActive = true;
    currentMapName = "Loading map name...";
    currentMapRegionName.clear();

    mapLookupFuture = std::async(
        std::launch::async,
        [aMapID]()
        {
            return LookupMapName(aMapID);
        }
    );
}

///----------------------------------------------------------------------------------------------------
/// UpdateMapLookup:
///----------------------------------------------------------------------------------------------------
void UpdateMapLookup()
{
    if (MumbleLink == nullptr)
    {
        return;
    }

    const uint32_t mapID = MumbleLink->Context.MapID;

    if (mapID != 0 && mapID != lastObservedMapID)
    {
        lastObservedMapID = mapID;

        if (!mapLookupActive)
        {
            StartMapLookup(mapID);
        }
        else
        {
            currentMapName = "Loading map name...";
        }
    }

    if (!mapLookupActive)
    {
        return;
    }

    if (mapLookupFuture.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    MapLookupResult result = mapLookupFuture.get();
    mapLookupActive = false;

    if (result.MapID == lastObservedMapID)
    {
        displayedMapID = result.MapID;

        if (result.Success)
        {
            currentMapName = result.Name;
            currentMapRegionName = result.RegionName;
        }
        else
        {
            currentMapRegionName.clear();
            currentMapName =
                "Map lookup failed: " +
                (result.Error.empty()
                    ? std::string("Unknown error.")
                    : result.Error);
        }
    }

    if (lastObservedMapID != 0 && displayedMapID != lastObservedMapID)
    {
        StartMapLookup(lastObservedMapID);
    }
}

///----------------------------------------------------------------------------------------------------
/// StartAchievementSync:
///----------------------------------------------------------------------------------------------------
void StartAchievementSyncWithKey(
    const std::string& aApiKey)
{
    if (achievementSyncActive || aApiKey.empty())
    {
        return;
    }

    std::string keyForRequest = aApiKey;

    achievementSyncActive = true;
    achievementSyncStatus = "Syncing achievement progress...";

    achievementSyncFuture = std::async(
        std::launch::async,
        [keyForRequest]() mutable
        {
            AchievementSyncResult result =
                SyncAchievements(keyForRequest);

            SecureClearString(keyForRequest);
            return result;
        }
    );

    SecureClearString(keyForRequest);
}

void StartAchievementSync()
{
    if (achievementSyncActive || !apiKeyValid)
    {
        return;
    }

    std::string keyForRequest;
    std::string loadError;

    if (!LoadEncryptedApiKey(
        keyForRequest,
        loadError))
    {
        achievementSyncStatus =
            loadError.empty()
            ? "Saved API key is unavailable. Revalidate the API key."
            : "Saved API key could not be loaded. Revalidate the API key.";

        SecureClearString(keyForRequest);
        return;
    }

    StartAchievementSyncWithKey(keyForRequest);
    SecureClearString(keyForRequest);
}

///----------------------------------------------------------------------------------------------------
/// UpdateAchievementSync:
///----------------------------------------------------------------------------------------------------
void UpdateAchievementSync()
{
    if (!achievementSyncActive)
    {
        return;
    }

    if (achievementSyncFuture.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    AchievementSyncResult result = achievementSyncFuture.get();
    achievementSyncActive = false;

    if (!result.Success)
    {
        achievementDataLoaded = false;
        achievementSyncStatus =
            result.Error.empty()
            ? "Achievement sync failed."
            : result.Error;
        return;
    }

    achievementProgress = std::move(result.Records);
    achievementRecordsLoaded = result.RecordsLoaded;
    achievementCompleted = result.Completed;
    achievementInProgress = result.InProgress;
    achievementDataLoaded = true;
    achievementSyncStatus = "Achievement progress synced successfully.";

    lastAchievementSyncCompleted =
        std::chrono::steady_clock::now();

    achievementSyncTimestampValid = true;

    // Public achievement definitions are static enough that they do not need
    // to be thrown away and re-downloaded every time account progress refreshes.
    if (!achievementDefinitionsLoaded &&
        !achievementDefinitionSyncActive)
    {
        StartAchievementDefinitionSync();
    }
    else if (!currentOpportunityCategoryIDs.empty())
    {
        // Re-evaluate completion/progress against the definitions we already
        // have, so completed items disappear from the current map immediately.
        BuildCurrentMapOpportunityList();
    }

    if (festivalDefinitionsReady)
    {
        BuildFestivalOpportunityList();
    }
}

///----------------------------------------------------------------------------------------------------
/// StartApiKeyValidation:
///----------------------------------------------------------------------------------------------------
void StartApiKeyValidationForKey(
    const std::string& aApiKey)
{
    if (apiValidationActive)
    {
        return;
    }

    if (aApiKey.empty())
    {
        apiKeyValid = false;
        apiKeyHasProgression = false;
        SecureClearString(pendingValidationKey);
        apiKeyStatus = "Enter an API key first.";
        return;
    }

    SecureClearString(pendingValidationKey);
    pendingValidationKey = aApiKey;

    std::string validationKey = aApiKey;

    apiValidationActive = true;
    apiKeyValid = false;
    apiKeyHasProgression = false;
    ++ubVaultGeneration;
    ubVaultDaily = UBVaultPeriod{};
    ubVaultWeekly = UBVaultPeriod{};
    ubVaultLatestSignature.clear();
    ubVaultLastAttemptValid = false;
    ubVaultStatus = "Waiting for API key validation...";
    apiKeyStatus = "Validating API key...";

    achievementDataLoaded = false;
    achievementRecordsLoaded = 0;
    achievementCompleted = 0;
    achievementInProgress = 0;
    achievementProgress.clear();
    achievementSyncStatus = "Waiting for API key validation.";

    achievementDefinitionsLoaded = false;
    achievementDefinitionsLoadedCount = 0;
    achievementDefinitions.clear();
    achievementDefinitionStatus =
        "Waiting for achievement progress sync.";

    achievementCategorySyncAttempted = false;
    achievementCategoriesLoaded = false;
    achievementCategories.clear();
    achievementCategoryStatus =
        "Waiting for achievement definitions.";

    festivalMatchedCategoryNames.clear();
    festivalCategoryIDs.clear();
    festivalOpportunityIDs.clear();
    festivalDefinitionsReady = false;
    festivalOpportunityScanDirty = true;

    lastOpportunityMapName.clear();
    currentOpportunityCategoryName.clear();
    currentOpportunityCategoryIDs.clear();
    currentMapOpportunityIDs.clear();
    currentMapDefinitionsReady = false;
    currentOpportunityStatus =
        "Waiting for achievement category data...";

    apiValidationFuture = std::async(
        std::launch::async,
        [validationKey]() mutable
        {
            ApiKeyValidationResult result =
                ValidateApiKey(validationKey);

            SecureClearString(validationKey);
            return result;
        }
    );

    SecureClearString(validationKey);
}

void StartApiKeyValidation()
{
    std::string apiKey = Trim(apiKeyBuffer);

    // Do not leave a validated or in-flight key sitting in the visible UI
    // buffer. The password field is cleared immediately after submission.
    SecureClearApiKeyBuffer();

    StartApiKeyValidationForKey(apiKey);
    SecureClearString(apiKey);
}

///----------------------------------------------------------------------------------------------------
/// UpdateApiKeyValidation:
///----------------------------------------------------------------------------------------------------
void UpdateApiKeyValidation()
{
    if (!apiValidationActive)
    {
        return;
    }

    if (apiValidationFuture.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        return;
    }

    ApiKeyValidationResult result = apiValidationFuture.get();
    apiValidationActive = false;

    apiKeyValid = result.Success;
    apiKeyHasProgression = result.HasProgressionPermission;

    if (result.Success)
    {
        std::string saveError;

        if (SaveEncryptedApiKey(
            pendingValidationKey,
            saveError))
        {
            apiKeyLoadedFromDisk = true;
            apiKeyStorageStatus =
                "API key saved securely for this Windows user.";
        }
        else
        {
            apiKeyLoadedFromDisk = false;
            apiKeyStorageStatus =
                "API key is valid, but could not be saved securely.";
        }

        // Never display the API key or its user-assigned key name.
        apiKeyStatus =
            "Valid API key. account + progression permissions found.";

        // Use a short-lived copy for the immediate sync. Future refreshes
        // decrypt the DPAPI-protected copy only when needed.
        StartAchievementSyncWithKey(
            pendingValidationKey
        );
        // The key stays local; these requests go to the official GW2 API.
        UBStartVaultSyncWithKey(pendingValidationKey);
    }
    else
    {
        apiKeyStatus =
            result.Error.empty()
            ? "API key validation failed."
            : result.Error;
    }

    SecureClearString(pendingValidationKey);
    SecureClearApiKeyBuffer();
}

///----------------------------------------------------------------------------------------------------
/// DllMain:
///----------------------------------------------------------------------------------------------------
BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: hSelf = hModule; break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }

    return TRUE;
}

///----------------------------------------------------------------------------------------------------
/// GetAddonDef:
///----------------------------------------------------------------------------------------------------
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    AddonDef.Signature = (uint32_t)-481027;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Unfinished Business";

    AddonDef.Version.Major = 0;
    AddonDef.Version.Minor = 2;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 0;

    AddonDef.Author = "spectre.9510";
    AddonDef.Description =
        "Finds relevant unfinished Guild Wars 2 achievements and "
        "opportunities based on where you are.";

    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;

    return &AddonDef;
}

///----------------------------------------------------------------------------------------------------
/// AddonLoad:
///----------------------------------------------------------------------------------------------------
void AddonLoad(AddonAPI_t* aApi)
{
    APIDefs = aApi;

    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);

    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void(*)(void*, void*))APIDefs->ImguiFree
    );

    NexusLink =
        (NexusLinkData_t*)APIDefs->DataLink_Get("DL_NEXUS_LINK");

    MumbleLink =
        (Mumble::Data*)APIDefs->DataLink_Get("DL_MUMBLE_LINK");

    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);

    LoadUserPreferences();

    std::string savedApiKey;
    std::string loadError;

    if (LoadEncryptedApiKey(savedApiKey, loadError))
    {
        apiKeyLoadedFromDisk = true;
        apiKeyStorageStatus =
            "Saved API key loaded securely. Validating automatically...";

        StartApiKeyValidationForKey(
            savedApiKey
        );

        SecureClearString(savedApiKey);
        SecureClearApiKeyBuffer();
    }
    else if (!loadError.empty())
    {
        apiKeyStorageStatus =
            "Saved API key could not be loaded: " +
            loadError;
    }

    APIDefs->Log(
        LOGL_DEBUG,
        "Unfinished Business",
        "Unfinished Business v0.2.0 VPS TEST loaded."
    );
}

///----------------------------------------------------------------------------------------------------
/// AddonUnload:
///----------------------------------------------------------------------------------------------------
void AddonUnload()
{
    if (ubVaultSyncActive && ubVaultFuture.valid())
    {
        ubVaultFuture.wait();
        ubVaultSyncActive = false;
    }
    if (ubVpsLookupActive && ubVpsLookupFuture.valid())
    {
        ubVpsLookupFuture.wait();
        ubVpsLookupActive = false;
    }

    if (mapLookupActive && mapLookupFuture.valid())
    {
        mapLookupFuture.wait();
        mapLookupActive = false;
    }

    if (apiValidationActive && apiValidationFuture.valid())
    {
        apiValidationFuture.wait();
        apiValidationActive = false;
    }

    if (achievementSyncActive && achievementSyncFuture.valid())
    {
        achievementSyncFuture.wait();
        achievementSyncActive = false;
    }

    if (achievementDefinitionSyncActive &&
        achievementDefinitionSyncFuture.valid())
    {
        achievementDefinitionSyncFuture.wait();
        achievementDefinitionSyncActive = false;
    }

    if (achievementCategorySyncActive &&
        achievementCategorySyncFuture.valid())
    {
        achievementCategorySyncFuture.wait();
        achievementCategorySyncActive = false;
    }

    if (currentMapDefinitionSyncActive &&
        currentMapDefinitionSyncFuture.valid())
    {
        currentMapDefinitionSyncFuture.wait();
        currentMapDefinitionSyncActive = false;
    }

    if (festivalScheduleSyncActive &&
        festivalScheduleFuture.valid())
    {
        festivalScheduleFuture.wait();
        festivalScheduleSyncActive = false;
    }

    if (festivalDefinitionSyncActive &&
        festivalDefinitionSyncFuture.valid())
    {
        festivalDefinitionSyncFuture.wait();
        festivalDefinitionSyncActive = false;
    }

    // Best-effort wipe of any remaining plaintext credential material.
    SecureClearString(pendingValidationKey);
    SecureClearApiKeyBuffer();

    APIDefs->GUI_Deregister(AddonRender);
    APIDefs->GUI_Deregister(AddonOptions);

    APIDefs->Log(
        LOGL_DEBUG,
        "Unfinished Business",
        "Unfinished Business unloaded."
    );
}

///----------------------------------------------------------------------------------------------------
/// Tabbed map presentation. Category splitting is a view-only operation:
/// achievement data, recommendation scores, ignore lists and refreshes stay intact.
///----------------------------------------------------------------------------------------------------
bool UBIsMasteryPointAchievement(int aID)
{
    const auto found = achievementDefinitions.find(aID);
    return found != achievementDefinitions.end() &&
        found->second.RewardsMastery;
}

std::vector<int> UBGetMapTabOpportunityIDs(bool aMasteryTab)
{
    std::vector<int> ids;
    ids.reserve(currentMapOpportunityIDs.size());

    for (int id : currentMapOpportunityIDs)
    {
        if (UBIsMasteryPointAchievement(id) == aMasteryTab)
        {
            ids.push_back(id);
        }
    }

    return ids;
}

size_t UBCountCharacterSpecificOpportunities(const std::vector<int>& aIDs)
{
    size_t count = 0;
    for (int id : aIDs)
    {
        if (IsCharacterAdventureGuideAchievement(id))
        {
            ++count;
        }
    }
    return count;
}

bool UBHasAccountTrackedOpportunities(const std::vector<int>& aIDs)
{
    for (int id : aIDs)
    {
        if (!IsCharacterAdventureGuideAchievement(id))
        {
            return true;
        }
    }
    return false;
}

void UBCountMapTabOpportunityGroups(
    const std::vector<int>& aIDs,
    size_t& aAlmostDone,
    size_t& aInProgress,
    size_t& aNotStarted,
    size_t& aCharacterSpecific)
{
    aAlmostDone = 0;
    aInProgress = 0;
    aNotStarted = 0;
    aCharacterSpecific = 0;

    for (int id : aIDs)
    {
        if (IsCharacterAdventureGuideAchievement(id))
        {
            ++aCharacterSpecific;
            continue;
        }

        if (FindAchievementProgress(id) == nullptr)
        {
            ++aNotStarted;
        }
        else if (GetAchievementProgressRatio(id) >= 0.75)
        {
            ++aAlmostDone;
        }
        else
        {
            ++aInProgress;
        }
    }
}

std::vector<OpportunityRecommendation> UBBuildMapTabRecommendations(
    const std::vector<int>& aIDs)
{
    std::vector<OpportunityRecommendation> recommendations;

    for (int id : aIDs)
    {
        // Character Adventure Guide has no reliable per-character API progress.
        if (!IsCharacterAdventureGuideAchievement(id))
        {
            recommendations.push_back(BuildRecommendation(id));
        }
    }

    std::sort(recommendations.begin(), recommendations.end(),
        [](const OpportunityRecommendation& a, const OpportunityRecommendation& b)
        {
            if (a.Score != b.Score)
            {
                return a.Score > b.Score;
            }

            auto aDefinition = achievementDefinitions.find(a.ID);
            auto bDefinition = achievementDefinitions.find(b.ID);
            const std::string aName =
                aDefinition != achievementDefinitions.end()
                ? aDefinition->second.Name : std::string();
            const std::string bName =
                bDefinition != achievementDefinitions.end()
                ? bDefinition->second.Name : std::string();
            return aName < bName;
        });

    if (recommendations.size() > 5)
    {
        recommendations.resize(5);
    }
    return recommendations;
}

///----------------------------------------------------------------------------------------------------
/// UBSplitMapLeads:
///     A lead belongs to exactly one group; never count regional or
///     character-specific matches as confirmed unfinished map objectives.
///----------------------------------------------------------------------------------------------------
struct UBMapLeadGroups
{
    std::vector<int> OnMap;
    std::vector<int> EventDependent;
    std::vector<int> Regional;
    std::vector<int> CharacterSpecific;
    std::vector<int> Historical;
};

UBMapLeadGroups UBSplitMapLeads(const std::vector<int>& aIDs)
{
    UBMapLeadGroups groups;
    for (int id : aIDs)
    {
        const UBAvailabilityInfo availability = UBGetAvailability(id);
        if (availability.Status == UBAvailabilityStatus::Historical)
        {
            groups.Historical.push_back(id);
            continue;
        }
        switch (UBClassifyMapLead(id))
        {
        case UBMapLeadKind::OnMap:
            if (availability.Status == UBAvailabilityStatus::EventDependent)
            {
                groups.EventDependent.push_back(id);
            }
            else
            {
                groups.OnMap.push_back(id);
            }
            break;
        case UBMapLeadKind::Regional:
            groups.Regional.push_back(id);
            break;
        case UBMapLeadKind::CharacterSpecific:
            groups.CharacterSpecific.push_back(id);
            break;
        }
    }
    return groups;
}

size_t UBCountRecordedUnfinished(const std::vector<int>& aIDs)
{
    size_t count = 0;
    for (int id : aIDs)
    {
        const AchievementProgress* progress = FindAchievementProgress(id);
        if (progress != nullptr && !progress->Done)
        {
            ++count;
        }
    }
    return count;
}

///----------------------------------------------------------------------------------------------------
/// UBRenderLeadCards:
///     Preserves the established detailed achievement cards. Only the
///     first five recommendations display initially; all entries stay
///     available in the expandable full list.
///----------------------------------------------------------------------------------------------------
void UBRenderLeadCards(const std::vector<int>& aIDs,
    const char* aGroupID, const char* aAllLabel)
{
    const std::vector<OpportunityRecommendation> top =
        UBBuildMapTabRecommendations(aIDs);

    ImGui::PushID(aGroupID);
    for (const OpportunityRecommendation& recommendation : top)
    {
        ImGui::PushID(recommendation.ID);
        RenderRecommendedOpportunity(recommendation, true);
        ImGui::PopID();
    }

    if (aIDs.size() > top.size() && ImGui::CollapsingHeader(aAllLabel))
    {
        for (int id : aIDs)
        {
            ImGui::PushID(id);
            RenderOpportunityRow(id);
            ImGui::PopID();
        }
    }
    ImGui::PopID();
}

///----------------------------------------------------------------------------------------------------
/// UBRenderMapTab:
///     On-map matches, broader regional leads, and character-only
///     candidates are separate in BOTH Achievements and Masteries tabs.
///----------------------------------------------------------------------------------------------------
void UBRenderMapTab(bool masteryTab)
{
    if (!apiKeyValid)
    {
        ImGui::TextWrapped(
            "GW2 API access is not ready. Open Nexus Options -> "
            "Unfinished Business to add or validate an API key.");
        return;
    }
    if (achievementSyncActive || achievementDefinitionSyncActive ||
        achievementCategorySyncActive || currentMapDefinitionSyncActive)
    {
        ImGui::TextDisabled("Refreshing achievement data...");
        return;
    }
    if (!achievementDataLoaded || !achievementCategoriesLoaded)
    {
        ImGui::TextDisabled("Waiting for achievement data...");
        return;
    }
    if (!currentMapDefinitionsReady)
    {
        ImGui::TextWrapped("%s", currentOpportunityStatus.c_str());
        return;
    }

    const std::vector<int> tabIDs = UBGetMapTabOpportunityIDs(masteryTab);
    const UBMapLeadGroups groups = UBSplitMapLeads(tabIDs);

    ImGui::TextColored(masteryTab ? UBSpecialColor() : UBAccentColor(),
        masteryTab ? "Mastery point discovery" : "Map achievement discovery");

    ImGui::TextDisabled("Public VPS index: %s", ubVpsStatus.c_str());
    if (ubVpsMergedCount != 0)
    {
        ImGui::TextDisabled(
            "%llu additional public candidates merged; "
            "account progress checked locally.",
            static_cast<unsigned long long>(ubVpsMergedCount));
    }
    if (MumbleLink != nullptr && ubVpsIndexEnabled)
    {
        auto vpsData = ubVpsCache.find(MumbleLink->Context.MapID);
        if (vpsData != ubVpsCache.end() && vpsData->second.Truncated)
        {
            ImGui::TextDisabled(
                "Public VPS results capped; location coverage is partial.");
        }
    }

    ImGui::TextDisabled(
        "%llu map leads | %llu event-dependent | %llu regional | %llu character-only",
        static_cast<unsigned long long>(groups.OnMap.size()),
        static_cast<unsigned long long>(groups.EventDependent.size()),
        static_cast<unsigned long long>(groups.Regional.size()),
        static_cast<unsigned long long>(groups.CharacterSpecific.size()));

    if (tabIDs.empty())
    {
        ImGui::TextWrapped(
            "No unfinished matches found in the available data. "
            "This does not prove the map is complete.");
        if (!masteryTab && currentCoverageMode == MapCoverageMode::None)
        {
            ImGui::Separator();
            RenderLimitedCoverageLeads();
        }
        return;
    }

    ImGui::Spacing();

    // Only map-specific, account-tracked entries can contribute to the main
    // progression summary. Regional and per-character leads never do.
    size_t almostDone = 0;
    size_t inProgress = 0;
    size_t noProgress = 0;
    size_t ignoredCharacterCount = 0;
    UBCountMapTabOpportunityGroups(groups.OnMap,
        almostDone, inProgress, noProgress, ignoredCharacterCount);

    UBRenderMapSummaryStats(almostDone, inProgress, noProgress, 0);
    ImGui::TextDisabled(
        "%llu map matches have recorded unfinished account progress.",
        static_cast<unsigned long long>(
            UBCountRecordedUnfinished(groups.OnMap)));
    if (noProgress > 0)
    {
        ImGui::TextDisabled(
            "No progress record does not confirm an objective is unlocked.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(UBAccentColor(),
        "On This Map (%llu)",
        static_cast<unsigned long long>(groups.OnMap.size()));
    ImGui::TextDisabled(
        "Map-specific leads, not verified unlocks. Green = likely ongoing; "
        "blue = unverified. Event and historical entries are separated.");

    if (groups.OnMap.empty())
    {
        ImGui::TextDisabled(
            "No map-specific matches found in the currently loaded data.");
    }
    else
    {
        UBRenderLeadCards(groups.OnMap,
            "##UBOnMapGroup", "All map-specific matches");
    }

    if (!groups.EventDependent.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(UBWarningColor(),
            "Event-dependent (%llu)",
            static_cast<unsigned long long>(groups.EventDependent.size()));
        ImGui::TextWrapped(
            "Related to an event; current activity and eligibility are unverified.");
        if (ImGui::CollapsingHeader("Show event-dependent leads##UBEvents"))
        {
            UBRenderLeadCards(groups.EventDependent,
                "##UBEventGroup", "All event-dependent leads");
        }
    }

    if (!groups.Regional.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(UBWarningColor(),
            "Regional Leads (%llu)",
            static_cast<unsigned long long>(groups.Regional.size()));
        ImGui::TextWrapped(
            "Related to the wider region, not necessarily this map. "
            "These do not count as verified map objectives.");
        UBRenderLeadCards(groups.Regional,
            "##UBRegionalGroup", "All regional leads");
    }

    if (!groups.CharacterSpecific.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(UBSpecialColor(),
            "Character-Specific Leads (%llu)",
            static_cast<unsigned long long>(groups.CharacterSpecific.size()));
        ImGui::TextWrapped(
            "Character Adventure Guide progress is unavailable through "
            "the account API. Check your current character in-game. "
            "These are not counted as verified unfinished tasks.");

        ImGui::PushID("##UBCharacterGroup");
        const size_t previewCount =
            std::min<size_t>(3, groups.CharacterSpecific.size());
        for (size_t index = 0; index < previewCount; ++index)
        {
            const int id = groups.CharacterSpecific[index];
            ImGui::PushID(id);
            RenderCharacterSpecificOpportunity(id);
            ImGui::PopID();
        }
        if (groups.CharacterSpecific.size() > previewCount &&
            ImGui::CollapsingHeader("Show remaining character-only leads"))
        {
            for (size_t index = previewCount;
                index < groups.CharacterSpecific.size(); ++index)
            {
                const int id = groups.CharacterSpecific[index];
                ImGui::PushID(id);
                RenderCharacterSpecificOpportunity(id);
                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }

    // Historical entries are kept for transparency, but are not part of the
    // main map recommendations or progression counters.
    if (!groups.Historical.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        const std::string historicalHeader =
            "Historical / unavailable (" +
            std::to_string(groups.Historical.size()) +
            ") - research only##UBHistorical";
        if (ImGui::CollapsingHeader(historicalHeader.c_str()))
        {
            ImGui::TextWrapped(
                "Not currently verified as obtainable. These are hidden "
                "from normal recommendations, even if their objectives name this map.");
            UBRenderLeadCards(groups.Historical,
                "##UBHistoricalGroup", "All historical leads");
        }
    }
}

///----------------------------------------------------------------------------------------------------
/// UBRenderColoredTabButton:
///     Distinct, readable category tabs with vector icons. The icons are drawn
///     with ImGui primitives so no external font or icon package is required.
///----------------------------------------------------------------------------------------------------
bool UBRenderColoredTabButton(
    const char* aText,
    const char* aId,
    const char* aTooltip,
    int aIcon,
    bool aSelected,
    const ImVec4& aAccent,
    float aWidth,
    int aBadgeCount = 0,
    bool aPulse = false)
{
    const float scale = ImGui::GetFontSize() / 13.0f;
    const float height = 34.0f * scale;

    const ImVec4 background = aSelected
        ? ImVec4(aAccent.x * 0.35f + 0.055f,
            aAccent.y * 0.35f + 0.055f,
            aAccent.z * 0.35f + 0.055f, 1.0f)
        : ImVec4(aAccent.x * 0.13f + 0.075f,
            aAccent.y * 0.13f + 0.075f,
            aAccent.z * 0.13f + 0.075f, 1.0f);
    const ImVec4 hovered = ImVec4(
        aAccent.x * 0.48f + 0.07f,
        aAccent.y * 0.48f + 0.07f,
        aAccent.z * 0.48f + 0.07f, 1.0f);
    const ImVec4 active = ImVec4(
        aAccent.x * 0.60f + 0.07f,
        aAccent.y * 0.60f + 0.07f,
        aAccent.z * 0.60f + 0.07f, 1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Button, background);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    ImGui::PushStyleColor(ImGuiCol_Border,
        ImVec4(aAccent.x, aAccent.y, aAccent.z,
            aSelected ? 0.95f : 0.45f));

    const bool clicked = ImGui::Button(aId, ImVec2(aWidth, height));
    const bool hoveredNow = ImGui::IsItemHovered();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    // New, unacknowledged objectives breathe visibly. The preview is allowed
    // even on the selected tab, so users can confirm the animation works.
    const bool previewingPulse = ImGui::GetTime() < ubVaultPulsePreviewUntil;
    const bool drawPulse = aPulse && (!aSelected || previewingPulse);
    const float pulseWave = drawPulse
        ? 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.2f)
        : 0.0f;
    if (drawPulse)
    {
        draw->AddRectFilled(
            ImVec2(min.x + 1.0f, min.y + 1.0f),
            ImVec2(max.x - 1.0f, max.y - 1.0f),
            ImGui::GetColorU32(ImVec4(aAccent.x, aAccent.y, aAccent.z,
                0.045f + 0.14f * pulseWave)), 5.0f * scale);
    }
    const ImU32 ink = ImGui::GetColorU32(aSelected
        ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
        : ImVec4(0.84f, 0.87f, 0.94f, 1.0f));
    const ImU32 iconInk = ImGui::GetColorU32(aSelected
        ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
        : aAccent);

    const float cx = min.x + 18.0f * scale;
    const float cy = (min.y + max.y) * 0.5f;
    const float line = 1.65f * scale;

    if (aIcon == 0) // Achievements: checked objective list.
    {
        draw->AddRect(
            ImVec2(cx - 7.0f * scale, cy - 8.0f * scale),
            ImVec2(cx + 7.0f * scale, cy + 8.0f * scale),
            iconInk, 2.0f * scale, ImDrawCornerFlags_All, line);
        draw->AddLine(ImVec2(cx - 4.5f * scale, cy - 2.0f * scale),
            ImVec2(cx - 2.0f * scale, cy + 0.5f * scale), iconInk, line);
        draw->AddLine(ImVec2(cx - 2.0f * scale, cy + 0.5f * scale),
            ImVec2(cx + 4.0f * scale, cy - 4.0f * scale), iconInk, line);
        draw->AddLine(ImVec2(cx - 4.0f * scale, cy + 4.0f * scale),
            ImVec2(cx + 4.0f * scale, cy + 4.0f * scale), iconInk, line);
    }
    else if (aIcon == 1) // Masteries: faceted mastery crystal.
    {
        const ImVec2 diamond[4] = {
            ImVec2(cx, cy - 9.0f * scale),
            ImVec2(cx + 8.0f * scale, cy),
            ImVec2(cx, cy + 9.0f * scale),
            ImVec2(cx - 8.0f * scale, cy)
        };
        draw->AddPolyline(diamond, 4, iconInk, true, line);
        draw->AddLine(ImVec2(cx, cy - 9.0f * scale),
            ImVec2(cx, cy + 9.0f * scale), iconInk, line);
        draw->AddLine(ImVec2(cx - 8.0f * scale, cy),
            ImVec2(cx + 8.0f * scale, cy), iconInk, line);
    }
    else if (aIcon == 3) // Dailies: small clock.
    {
        draw->AddCircle(ImVec2(cx, cy), 8.0f * scale,
            iconInk, 20, line);
        draw->AddLine(ImVec2(cx, cy),
            ImVec2(cx, cy - 5.0f * scale), iconInk, line);
        draw->AddLine(ImVec2(cx, cy),
            ImVec2(cx + 4.0f * scale, cy + 2.0f * scale), iconInk, line);
    }
    else // Holiday: wrapped festival gift.
    {
        draw->AddRect(
            ImVec2(cx - 8.0f * scale, cy - 3.0f * scale),
            ImVec2(cx + 8.0f * scale, cy + 8.0f * scale),
            iconInk, 1.0f * scale, ImDrawCornerFlags_All, line);
        draw->AddRect(
            ImVec2(cx - 9.0f * scale, cy - 6.0f * scale),
            ImVec2(cx + 9.0f * scale, cy - 2.0f * scale),
            iconInk, 1.0f * scale, ImDrawCornerFlags_All, line);
        draw->AddLine(ImVec2(cx, cy - 6.0f * scale),
            ImVec2(cx, cy + 8.0f * scale), iconInk, line);
        draw->AddCircle(ImVec2(cx - 3.0f * scale, cy - 8.5f * scale),
            3.0f * scale, iconInk, 8, line);
        draw->AddCircle(ImVec2(cx + 3.0f * scale, cy - 8.5f * scale),
            3.0f * scale, iconInk, 8, line);
    }

    const ImVec2 textSize = ImGui::CalcTextSize(aText);
    draw->AddText(
        ImVec2(min.x + 34.0f * scale,
            (min.y + max.y - textSize.y) * 0.5f), ink, aText);

    if (aSelected)
    {
        draw->AddRectFilled(
            ImVec2(min.x + 7.0f * scale, max.y - 3.5f * scale),
            ImVec2(max.x - 7.0f * scale, max.y - 1.5f * scale),
            ImGui::GetColorU32(aAccent), 1.0f * scale);
    }

    // Bright outline on unacknowledged new objectives, or while previewing.
    if (drawPulse)
    {
        draw->AddRect(ImVec2(min.x + 1.0f, min.y + 1.0f),
            ImVec2(max.x - 1.0f, max.y - 1.0f),
            ImGui::GetColorU32(ImVec4(aAccent.x, aAccent.y, aAccent.z,
                0.35f + 0.65f * pulseWave)), 5.0f * scale,
            ImDrawCornerFlags_All, (1.75f + 2.25f * pulseWave) * scale);
    }
    if (aBadgeCount > 0)
    {
        const ImVec2 center(max.x - 11.0f * scale,
            min.y + 10.0f * scale);
        draw->AddCircleFilled(center, 10.0f * scale,
            ImGui::GetColorU32(ImVec4(0.95f, 0.46f, 0.16f, 1.0f)));
        const std::string label = aBadgeCount > 9 ? "9+" :
            std::to_string(aBadgeCount);
        const ImVec2 size = ImGui::CalcTextSize(label.c_str());
        draw->AddText(ImVec2(center.x - size.x * 0.5f,
            center.y - size.y * 0.5f),
            ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f)),
            label.c_str());
    }
    if (hoveredNow)
    {
        ImGui::SetTooltip("%s", aTooltip);
    }

    return clicked;
}

///----------------------------------------------------------------------------------------------------
/// AddonRender:
///----------------------------------------------------------------------------------------------------
void AddonRender()
{
    MaybeExpireApiKeyInput();

    UpdateMapLookup();
    UBUpdatePublicVpsIndex();
    UpdateApiKeyValidation();
    UBUpdateVaultSync();
    if (apiKeyValid && !ubVaultSyncActive &&
        (!ubVaultLastAttemptValid ||
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - ubVaultLastAttempt).count() >=
            ubVaultRefreshSeconds))
    {
        UBStartVaultSync();
    }
    UpdateAchievementSync();
    UpdateAchievementDefinitionSync();
    UpdateAchievementCategorySync();
    UpdateCurrentMapDefinitionSync();

    UpdateFestivalScheduleSync();
    ExpireFestivalAfterVerifiedWindow();
    UpdateFestivalDefinitionSync();
    MaybeRefreshFestivalSchedule();

    if (achievementDataLoaded &&
        !achievementCategoriesLoaded &&
        !achievementCategorySyncActive &&
        !achievementCategorySyncAttempted)
    {
        StartAchievementCategorySync();
    }

    UpdateCurrentMapOpportunityScan();
    UpdateActiveFestivalOpportunityScan();

    if (autoRefreshAchievementProgress &&
        apiKeyValid &&
        achievementDataLoaded &&
        achievementSyncTimestampValid &&
        !achievementSyncActive)
    {
        const auto elapsedSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() -
                lastAchievementSyncCompleted
            ).count();

        if (elapsedSeconds >= achievementAutoRefreshSeconds)
        {
            StartAchievementSync();
        }
    }

    if (opportunityFiltersDirty)
    {
        if (!currentOpportunityCategoryIDs.empty())
        {
            BuildCurrentMapOpportunityList();
        }

        if (festivalDefinitionsReady)
        {
            BuildFestivalOpportunityList();
        }

        opportunityFiltersDirty = false;
    }

    ImGui::SetNextWindowSize(
        ImVec2(840.0f, 720.0f),
        ImGuiCond_FirstUseEver
    );

    ImGui::SetNextWindowSizeConstraints(
        ImVec2(580.0f, 380.0f),
        ImVec2(1200.0f, 1200.0f)
    );

    if (ImGui::Begin("Unfinished Business"))
    {
        ImGui::TextColored(
            UBAccentColor(),
            "Unfinished Business"
        );
        ImGui::SameLine();
        ImGui::TextDisabled("v0.2.0 VPS TEST");

        ImGui::Separator();

        if (MumbleLink != nullptr)
        {
            // Restore the original visual hierarchy: map name on its own line,
            // then opportunity count and a compact coverage indicator.
            ImGui::SetWindowFontScale(1.15f);
            ImGui::TextWrapped("%s", currentMapName.c_str());
            ImGui::SetWindowFontScale(1.0f);

            if (!currentOpportunityCategoryName.empty())
            {
                const UBMapLeadGroups overview =
                    UBSplitMapLeads(currentMapOpportunityIDs);
                ImGui::TextDisabled(
                    "%llu map matches (%llu with recorded progress)",
                    static_cast<unsigned long long>(overview.OnMap.size()),
                    static_cast<unsigned long long>(
                        UBCountRecordedUnfinished(overview.OnMap)));
                if (!overview.EventDependent.empty() ||
                    !overview.Regional.empty() ||
                    !overview.CharacterSpecific.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("| %llu event | %llu regional | %llu character-only",
                        static_cast<unsigned long long>(overview.EventDependent.size()),
                        static_cast<unsigned long long>(overview.Regional.size()),
                        static_cast<unsigned long long>(overview.CharacterSpecific.size()));
                }
            }

            if (achievementCategoriesLoaded)
            {
                if (!currentOpportunityCategoryName.empty())
                {
                    ImGui::SameLine();
                }
                ImGui::TextColored(GetCoverageDisplayColor(),
                    "Coverage: %s", GetCoverageDisplayLabel());
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", GetCoverageTooltip());
                }
            }

            if (achievementSyncActive)
            {
                ImGui::TextDisabled("Refreshing account progress...");
            }
        }
        else
        {
            ImGui::TextDisabled("Waiting for game context...");
        }

        ImGui::Spacing();

        // Keep the map context visible while the category content scrolls.
        ImGui::Separator();

        // Four vector-icon tabs; the Daily/Weekly tab can pulse when new
        // outstanding objectives have not been viewed this period.
        static int activeCategoryTab = 0; // 0 Achievements, 1 Masteries, 2 Dailies, 3 Holiday
        // If the user is already viewing this tab when a refresh completes,
        // treat that result as viewed instead of alerting after they leave.
        if (activeCategoryTab == 2 && !ubVaultLatestSignature.empty() &&
            ubVaultAcknowledgedSignature != ubVaultLatestSignature)
        {
            ubVaultAcknowledgedSignature = ubVaultLatestSignature;
            SaveUserPreferences();
        }
        const float tabGap = 6.0f * (ImGui::GetFontSize() / 13.0f);
        const float tabWidth = (ImGui::GetContentRegionAvail().x - tabGap * 3.0f) / 4.0f;
        const int vaultPending = UBVaultPendingCount(ubVaultDaily) +
            UBVaultPendingCount(ubVaultWeekly);
        const bool previewPulse = ImGui::GetTime() < ubVaultPulsePreviewUntil;
        const bool vaultPulse = previewPulse ||
            (ubVaultAnimateTab && vaultPending > 0 &&
                !ubVaultLatestSignature.empty() &&
                ubVaultLatestSignature != ubVaultAcknowledgedSignature);

        if (UBRenderColoredTabButton(
            "Achievements", "##UBTabAchievements",
            "Achievements available in this map", 0,
            activeCategoryTab == 0,
            ImVec4(0.34f, 0.71f, 1.0f, 1.0f), tabWidth))
        {
            activeCategoryTab = 0;
        }
        ImGui::SameLine(0.0f, tabGap);
        if (UBRenderColoredTabButton(
            "Masteries", "##UBTabMasteries",
            "Achievements rewarding mastery points", 1,
            activeCategoryTab == 1,
            ImVec4(0.75f, 0.57f, 1.0f, 1.0f), tabWidth))
        {
            activeCategoryTab = 1;
        }
        ImGui::SameLine(0.0f, tabGap);
        if (UBRenderColoredTabButton(
            tabWidth < 170.0f ? "Daily/Weekly" : "Daily + Weekly", "##UBTabVault",
            "Current Wizard's Vault daily and weekly objectives", 3,
            activeCategoryTab == 2,
            ImVec4(1.0f, 0.53f, 0.29f, 1.0f), tabWidth,
            vaultPending, vaultPulse))
        {
            activeCategoryTab = 2;
            if (!ubVaultLatestSignature.empty() &&
                ubVaultAcknowledgedSignature != ubVaultLatestSignature)
            {
                ubVaultAcknowledgedSignature = ubVaultLatestSignature;
                SaveUserPreferences();
            }
        }
        ImGui::SameLine(0.0f, tabGap);
        if (UBRenderColoredTabButton(
            "Holiday", "##UBTabHoliday",
            "Seasonal festival achievements", 2,
            activeCategoryTab == 3,
            ImVec4(1.0f, 0.69f, 0.35f, 1.0f), tabWidth))
        {
            activeCategoryTab = 3;
        }

        ImGui::Spacing();

        if (activeCategoryTab == 0)
        {
            ImGui::BeginChild("##AchievementsTabBody", ImVec2(0.0f, 0.0f), false);
            UBRenderMapTab(false);
            ImGui::EndChild();
        }
        else if (activeCategoryTab == 1)
        {
            ImGui::BeginChild("##MasteriesTabBody", ImVec2(0.0f, 0.0f), false);
            ImGui::TextDisabled(
                "Achievements in this map that reward mastery points.");
            ImGui::Spacing();
            UBRenderMapTab(true);
            ImGui::EndChild();
        }
        else if (activeCategoryTab == 2)
        {
            ImGui::BeginChild("##DailyWeeklyTabBody", ImVec2(0.0f, 0.0f), false);
            UBRenderVaultTab();
            ImGui::EndChild();
        }
        else
        {
            ImGui::BeginChild("##HolidayTabBody", ImVec2(0.0f, 0.0f), false);
            if (activeFestivalAvailable)
            {
                RenderActiveFestivalSection();
            }
            else
            {
                ImGui::TextDisabled("No active festival detected.");
                ImGui::TextWrapped("%s", festivalScheduleStatus.c_str());
            }
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

///----------------------------------------------------------------------------------------------------
/// RenderLimitedCoverageLeads:
///     Shows map-name matches as research leads only. These are intentionally
///     not promoted into the recommendation list automatically.
///----------------------------------------------------------------------------------------------------
void RenderLimitedCoverageLeads()
{
    const std::vector<CoverageLead> leads =
        BuildUnmappedCoverageLeads();

    const size_t actionableCount =
        CountActionableCoverageLeads(leads);

    if (actionableCount == 0)
    {
        ImGui::TextDisabled(
            "No obvious text-based coverage leads found for this map."
        );
        return;
    }

    ImGui::TextColored(
        UBWarningColor(),
        "Potential coverage leads: %llu",
        static_cast<unsigned long long>(
            actionableCount
            )
    );

    ImGui::TextDisabled(
        "Research only. These are not recommendations yet."
    );

    ImGui::Spacing();

    for (const CoverageLead& lead : leads)
    {
        if (lead.IsRetired)
        {
            continue;
        }

        ImGui::PushID(lead.AchievementID);

        std::string label =
            lead.Name +
            "  |  " +
            JoinCoverageLeadCategories(
                lead.Categories
            );

        if (ImGui::TreeNodeEx(
            label.c_str(),
            ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            const auto definitionIt =
                achievementDefinitions.find(
                    lead.AchievementID
                );

            if (definitionIt != achievementDefinitions.end())
            {
                const AchievementDefinition& definition =
                    definitionIt->second;

                if (!definition.Requirement.empty())
                {
                    ImGui::TextColored(
                        UBAccentColor(),
                        "Requirement:"
                    );

                    ImGui::TextWrapped(
                        "%s",
                        definition.Requirement.c_str()
                    );

                    ImGui::Spacing();
                }

                if (!definition.Description.empty())
                {
                    ImGui::TextColored(
                        UBMutedInfoColor(),
                        "Description:"
                    );

                    ImGui::TextWrapped(
                        "%s",
                        definition.Description.c_str()
                    );

                    ImGui::Spacing();
                }
            }

            if (UBColoredActionButton("Open Wiki", UBActionButtonKind::Wiki))
            {
                OpenAchievementInNexusGameWiki(
                    lead.Name.c_str()
                );
            }

            ImGui::SameLine();

            if (UBColoredActionButton("Copy name", UBActionButtonKind::CopyName))
            {
                CopyAchievementName(
                    lead.Name.c_str()
                );
            }

            ImGui::SameLine();

            ImGui::TextDisabled(
                "ID %d",
                lead.AchievementID
            );

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

///----------------------------------------------------------------------------------------------------
/// GetSecondsSinceLastAchievementSync:
///----------------------------------------------------------------------------------------------------
long long GetSecondsSinceLastAchievementSync()
{
    if (!achievementSyncTimestampValid)
    {
        return -1;
    }

    return
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() -
            lastAchievementSyncCompleted
        ).count();
}

///----------------------------------------------------------------------------------------------------
/// AddonOptions:
///----------------------------------------------------------------------------------------------------
void AddonOptions()
{
    ImGui::TextColored(
        UBAccentColor(),
        "Unfinished Business"
    );
    ImGui::TextDisabled("Version 0.1.2");
    ImGui::Separator();

    ImGui::TextColored(
        UBAccentColor(),
        "Guild Wars 2 API"
    );
    ImGui::TextWrapped(
        "Requires a GW2 API key with account and progression permissions. "
        "Validated keys are stored locally using Windows DPAPI encryption."
    );

    ImGui::Spacing();

    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::InputText(
        "##GW2ApiKeyOptions",
        apiKeyBuffer,
        sizeof(apiKeyBuffer),
        ImGuiInputTextFlags_Password |
        ImGuiInputTextFlags_NoUndoRedo))
    {
        apiKeyBufferHasInput =
            apiKeyBuffer[0] != '\0';

        apiKeyBufferLastTouched =
            std::chrono::steady_clock::now();
    }

    if (ImGui::Button(
        apiValidationActive
        ? "Validating..."
        : "Validate API Key"))
    {
        if (!apiValidationActive)
        {
            StartApiKeyValidation();
        }
    }

    ImGui::SameLine();

    if (!apiValidationActive &&
        ImGui::Button("Forget Key"))
    {
        DeleteSavedApiKey();

        SecureClearApiKeyBuffer();
        SecureClearString(pendingValidationKey);

        apiKeyValid = false;
        apiKeyHasProgression = false;
        ++ubVaultGeneration;
        ubVaultDaily = UBVaultPeriod{};
        ubVaultWeekly = UBVaultPeriod{};
        ubVaultLatestSignature.clear();
        ubVaultLastAttemptValid = false;
        ubVaultStatus = "No API key validated.";
        apiKeyStatus = "No API key validated.";

        achievementDataLoaded = false;
        achievementSyncTimestampValid = false;
        achievementRecordsLoaded = 0;
        achievementCompleted = 0;
        achievementInProgress = 0;
        achievementProgress.clear();
        achievementSyncStatus = "Not synced yet.";

        achievementDefinitionsLoaded = false;
        achievementDefinitionsLoadedCount = 0;
        achievementDefinitions.clear();
        achievementDefinitionStatus = "Not loaded yet.";

        achievementCategorySyncAttempted = false;
        achievementCategoriesLoaded = false;
        achievementCategories.clear();
        achievementCategoryStatus = "Not loaded yet.";

        festivalMatchedCategoryNames.clear();
        festivalCategoryIDs.clear();
        festivalOpportunityIDs.clear();
        festivalDefinitionsReady = false;
        festivalOpportunityScanDirty = true;

        lastOpportunityMapName.clear();
        currentOpportunityCategoryName.clear();
        currentOpportunityCategoryIDs.clear();
        currentMapOpportunityIDs.clear();
        currentOpportunityDirectIDs.clear();
        currentOpportunityTextFilteredIDs.clear();
        currentOpportunityMatchTerms.clear();
        currentOpportunityRegionalIDs.clear();
        currentAutoMapMatchCount = 0;
        currentRegionalMatchCount = 0;
        currentDiscoveryScanComplete = false;

        currentCoverageMode = MapCoverageMode::None;
        currentCoverageDirectCategories.clear();
        currentCoverageContextualCategories.clear();

        currentMapDefinitionsReady = false;
        currentOpportunityStatus =
            "Waiting for achievement category data...";
    }

    if (apiValidationActive)
    {
        ImGui::TextDisabled(
            "Forget Key is temporarily unavailable while validation is running."
        );
    }

    ImGui::TextColored(
        apiKeyValid
        ? UBSuccessColor()
        : UBWarningColor(),
        "%s",
        apiKeyStatus.c_str()
    );

    ImGui::TextColored(
        UBMutedInfoColor(),
        "%s",
        apiKeyStorageStatus.c_str()
    );

    ImGui::TextDisabled(
        "Festival detection: %s",
        festivalScheduleStatus.c_str()
    );

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(
        UBAccentColor(),
        "Recommendations"
    );

    bool preferencesChanged = false;

    preferencesChanged |=
        ImGui::Checkbox(
            "Prefer achievements already started",
            &preferStartedAchievements
        );

    preferencesChanged |=
        ImGui::Checkbox(
            "Prefer Mastery Point rewards",
            &preferMasteryRewards
        );

    preferencesChanged |=
        ImGui::Checkbox(
            "Prefer Achievement Points",
            &preferAchievementPoints
        );

    preferencesChanged |=
        ImGui::Checkbox(
            "Auto-refresh achievement progress every 5 minutes",
            &autoRefreshAchievementProgress
        );

    preferencesChanged |= ImGui::Checkbox(
        "Animate Daily + Weekly tab for outstanding objectives",
        &ubVaultAnimateTab);

    ImGui::Spacing();
    ImGui::TextColored(
        UBAccentColor(),
        "Filters"
    );

    preferencesChanged |=
        ImGui::Checkbox(
            "Hide Story-related achievements",
            &hideStoryAchievements
        );

    preferencesChanged |=
        ImGui::Checkbox(
            "Hide Event-dependent achievements",
            &hideEventAchievements
        );

    preferencesChanged |=
        ImGui::Checkbox(
            "Hide Collection achievements",
            &hideCollectionAchievements
        );

    preferencesChanged |=
        ImGui::Checkbox(
            "Hide achievements not started",
            &hideNotStartedAchievements
        );

    if (preferencesChanged)
    {
        SaveUserPreferences();
        opportunityFiltersDirty = true;
    }

    ImGui::Spacing();

    ImGui::Text(
        "Ignored achievements: %llu",
        static_cast<unsigned long long>(
            ignoredAchievementIDs.size()
            )
    );

    if (!ignoredAchievementIDs.empty())
    {
        if (ImGui::Button(
            "Restore All Ignored Achievements"))
        {
            RestoreAllIgnoredAchievements();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::CollapsingHeader(
        "Coverage & Mapping",
        ImGuiTreeNodeFlags_None))
    {
        if (ImGui::Checkbox("Use public VPS map index (optional)",
            &ubVpsIndexEnabled))
        {
            SaveUserPreferences();
            // Rebuild local recommendations with/without cached VPS data.
            if (!currentMapDefinitionSyncActive)
            {
                lastOpportunityMapName.clear();
            }
            else
            {
                ubVpsRescanPending = true;
            }
        }
        ImGui::TextWrapped(
            "VPS: %s. Only public map ID sent. No account key or progress.",
            ubVpsStatus.c_str());
        ImGui::Text("Additional public candidates: %llu",
            static_cast<unsigned long long>(ubVpsMergedCount));
        if (MumbleLink != nullptr && ubVpsIndexEnabled &&
            !ubVpsLookupActive &&
            ubVpsAttemptedMaps.count(MumbleLink->Context.MapID) != 0 &&
            ubVpsCache.count(MumbleLink->Context.MapID) == 0)
        {
            if (ImGui::Button("Retry public VPS lookup"))
            {
                ubVpsAttemptedMaps.erase(MumbleLink->Context.MapID);
            }
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Current map: %s",
            currentMapName.c_str()
        );

        ImGui::Text(
            "Coverage mode: %s",
            GetMapCoverageModeLabel()
        );

        if (!currentCoverageDirectCategories.empty())
        {
            ImGui::Text("Direct categories:");

            for (const std::string& categoryName :
                currentCoverageDirectCategories)
            {
                ImGui::BulletText(
                    "%s",
                    categoryName.c_str()
                );
            }
        }

        if (!currentCoverageContextualCategories.empty())
        {
            ImGui::Text("Contextual categories:");

            for (const std::string& categoryName :
                currentCoverageContextualCategories)
            {
                ImGui::BulletText(
                    "%s",
                    categoryName.c_str()
                );
            }
        }

        ImGui::Spacing();

        ImGui::Text(
            "Mapped achievement IDs: %llu",
            static_cast<unsigned long long>(
                currentOpportunityCategoryIDs.size()
                )
        );

        ImGui::Text(
            "Direct-source IDs: %llu",
            static_cast<unsigned long long>(
                currentOpportunityDirectIDs.size()
                )
        );

        ImGui::Text(
            "Contextual candidates: %llu",
            static_cast<unsigned long long>(
                currentOpportunityTextFilteredIDs.size()
                )
        );
        ImGui::Text(
            "Auto-discovered map IDs: %llu",
            static_cast<unsigned long long>(currentAutoMapMatchCount));
        ImGui::Text(
            "Regional candidates: %llu",
            static_cast<unsigned long long>(currentRegionalMatchCount));
        if (!currentDiscoveryScanComplete)
        {
            ImGui::TextWrapped(
                "Discovery is partial: the public achievement catalog "
                "may still be loading or regional candidates were capped. "
                "No results does not mean the map is complete.");
        }

        if (currentMapDefinitionsReady)
        {
            ImGui::Text(
                "Contextual matches: %llu",
                static_cast<unsigned long long>(
                    CountRelevantContextualIDs()
                    )
            );

            ImGui::Text(
                "Ignored on this map: %llu",
                static_cast<unsigned long long>(
                    CountIgnoredCurrentMapIDs()
                    )
            );

            ImGui::Text(
                "Hidden by filters: %llu",
                static_cast<unsigned long long>(
                    CountUserFilteredCurrentMapIDs()
                    )
            );

            ImGui::Text(
                "Visible candidate IDs (all groups): %llu",
                static_cast<unsigned long long>(
                    currentMapOpportunityIDs.size()
                    )
            );
            const UBMapLeadGroups diagnosticGroups =
                UBSplitMapLeads(currentMapOpportunityIDs);
            ImGui::Text("Map-specific matches: %llu (recorded unfinished: %llu)",
                static_cast<unsigned long long>(diagnosticGroups.OnMap.size()),
                static_cast<unsigned long long>(
                    UBCountRecordedUnfinished(diagnosticGroups.OnMap)));
            ImGui::Text("Regional leads: %llu | Character-only: %llu",
                static_cast<unsigned long long>(diagnosticGroups.Regional.size()),
                static_cast<unsigned long long>(diagnosticGroups.CharacterSpecific.size()));
        }

        ImGui::Spacing();
        ImGui::TextWrapped(
            "Status: %s",
            currentOpportunityStatus.c_str()
        );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Coverage Leads");

        const std::vector<CoverageLead>
            coverageLeads =
            BuildUnmappedCoverageLeads();

        const size_t actionableLeadCount =
            CountActionableCoverageLeads(
                coverageLeads
            );

        const size_t retiredLeadCount =
            CountRetiredCoverageLeads(
                coverageLeads
            );

        ImGui::Text(
            "Actionable unmapped leads: %llu",
            static_cast<unsigned long long>(
                actionableLeadCount
                )
        );

        ImGui::Text(
            "Retired/historical matches ignored: %llu",
            static_cast<unsigned long long>(
                retiredLeadCount
                )
        );

        if (actionableLeadCount == 0)
        {
            ImGui::TextDisabled(
                "No obvious current text-based coverage gaps found."
            );
        }
        else
        {
            ImGui::TextDisabled(
                "These are leads, not automatic matches. "
                "Review before adding them to the curated mapping."
            );

            if (ImGui::BeginChild(
                "##CoverageLeadList",
                ImVec2(0.0f, 180.0f),
                true))
            {
                for (const CoverageLead& lead :
                    coverageLeads)
                {
                    if (lead.IsRetired)
                    {
                        continue;
                    }

                    ImGui::PushID(lead.AchievementID);

                    if (ImGui::TreeNodeEx(
                        lead.Name.c_str(),
                        ImGuiTreeNodeFlags_SpanAvailWidth))
                    {
                        const auto definitionIt =
                            achievementDefinitions.find(
                                lead.AchievementID
                            );

                        ImGui::Text(
                            "Achievement ID: %d",
                            lead.AchievementID
                        );

                        ImGui::TextWrapped(
                            "Categories: %s",
                            JoinCoverageLeadCategories(
                                lead.Categories
                            ).c_str()
                        );

                        if (definitionIt !=
                            achievementDefinitions.end())
                        {
                            const AchievementDefinition&
                                definition =
                                definitionIt->second;

                            if (!definition.Requirement.empty())
                            {
                                ImGui::TextWrapped(
                                    "Requirement: %s",
                                    definition.Requirement.c_str()
                                );
                            }

                            if (!definition.Description.empty())
                            {
                                ImGui::TextWrapped(
                                    "Description: %s",
                                    definition.Description.c_str()
                                );
                            }
                        }

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndChild();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::CollapsingHeader(
        "Sync & Diagnostics",
        ImGuiTreeNodeFlags_None))
    {
        if (MumbleLink != nullptr)
        {
            ImGui::TextWrapped(
                "Current map: %s",
                currentMapName.c_str()
            );

            ImGui::Text(
                "Map ID: %u",
                MumbleLink->Context.MapID
            );
        }

        ImGui::Spacing();

        ImGui::Text(
            "Achievement access: %s",
            (apiKeyValid && apiKeyHasProgression)
            ? "ready"
            : "not ready"
        );

        ImGui::TextWrapped(
            "Progress sync: %s",
            achievementSyncStatus.c_str()
        );

        ImGui::Text(
            "Auto refresh: %s",
            autoRefreshAchievementProgress
            ? "every 5 minutes"
            : "disabled"
        );

        const long long secondsSinceSync =
            GetSecondsSinceLastAchievementSync();

        if (secondsSinceSync >= 0)
        {
            ImGui::Text(
                "Last account-progress sync: %lld seconds ago",
                secondsSinceSync
            );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Achievement Progress Probe");
        ImGui::TextDisabled(
            "Checks the exact account-progress record loaded by Unfinished Business."
        );

        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputInt(
            "Achievement ID##ProgressProbe",
            &achievementProgressProbeID
        );

        if (achievementProgressProbeID < 0)
        {
            achievementProgressProbeID = 0;
        }

        ImGui::TextWrapped(
            "Name: %s",
            GetAchievementProgressProbeName(
                achievementProgressProbeID
            ).c_str()
        );

        ImGui::TextWrapped(
            "%s",
            GetAchievementProgressProbeText(
                achievementProgressProbeID
            ).c_str()
        );

        if (apiKeyValid && !achievementSyncActive)
        {
            if (ImGui::Button("Sync Account Progress Now##Probe"))
            {
                StartAchievementSync();
            }
        }

        ImGui::Separator();

        if (achievementDataLoaded)
        {
            ImGui::Text(
                "Progress records: %llu",
                static_cast<unsigned long long>(
                    achievementRecordsLoaded
                    )
            );

            ImGui::Text(
                "Completed: %llu",
                static_cast<unsigned long long>(
                    achievementCompleted
                    )
            );

            ImGui::Text(
                "In progress: %llu",
                static_cast<unsigned long long>(
                    achievementInProgress
                    )
            );
        }

        ImGui::TextWrapped(
            "Definition sync: %s",
            achievementDefinitionStatus.c_str()
        );

        if (achievementDefinitionsLoaded)
        {
            ImGui::Text(
                "Definitions loaded: %llu",
                static_cast<unsigned long long>(
                    achievementDefinitionsLoadedCount
                    )
            );
        }

        ImGui::TextWrapped(
            "Category sync: %s",
            achievementCategoryStatus.c_str()
        );

        ImGui::TextWrapped(
            "Current-map scan: %s",
            currentOpportunityStatus.c_str()
        );

        ImGui::Spacing();

        if (apiKeyValid && !achievementSyncActive)
        {
            if (ImGui::Button("Sync Achievements Now"))
            {
                StartAchievementSync();
            }
        }

        if (achievementDefinitionsLoaded &&
            !achievementCategorySyncActive)
        {
            ImGui::SameLine();

            if (ImGui::Button("Reload Categories"))
            {
                achievementCategorySyncAttempted = false;
                achievementCategoriesLoaded = false;
                StartAchievementCategorySync();
            }
        }
    }
}
