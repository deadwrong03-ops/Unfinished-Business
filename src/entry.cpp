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
void SaveUserPreferences();
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
    std::string Error;
};

uint32_t lastObservedMapID = 0;
uint32_t displayedMapID = 0;
std::string currentMapName = "Waiting for map data...";

bool mapLookupActive = false;
std::future<MapLookupResult> mapLookupFuture;

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

enum class MapCoverageMode
{
    None,
    ExactCategory,
    CuratedMapping
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
        << "auto_refresh=" << (autoRefreshAchievementProgress ? 1 : 0) << "\n";

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
        result.Error = error;
        return result;
    }

    if (statusCode != 200)
    {
        result.Error =
            "Official GW2 event calendar returned HTTP " +
            std::to_string(statusCode) + ".";

        return result;
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
        "Checking Guild Wars 2 Wiki festival schedule...";

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
        festivalScheduleStatus =
            "Active festival detected from the official GW2 event calendar.";
    }
    else
    {
        festivalScheduleStatus =
            "No supported annual festival is marked active on the official GW2 event calendar.";
    }
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
    achievementDefinitionsLoadedCount = 0;
    achievementDefinitions.clear();
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

    achievementDefinitions = std::move(result.Definitions);
    achievementDefinitionsLoadedCount = result.DefinitionsLoaded;
    achievementDefinitionsLoaded = true;

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
/// GetOpportunityReason:
///----------------------------------------------------------------------------------------------------
std::string GetOpportunityReason(int aID)
{
    const AchievementProgress* progress =
        FindAchievementProgress(aID);

    const bool contextual =
        IsContextualMapOpportunity(aID);

    if (IsCharacterAdventureGuideAchievement(aID))
    {
        if (contextual)
        {
            return
                "Character Adventure Guide objective matched through a broader "
                "category because its public text references " +
                currentMapName +
                ". Progress is character-specific and is not exposed by the "
                "Guild Wars 2 API.";
        }

        return
            "Character Adventure Guide objective relevant to this map. "
            "Progress is character-specific and is not exposed by the "
            "Guild Wars 2 API.";
    }

    if (progress == nullptr)
    {
        if (contextual)
        {
            return
                "Matched through a broader achievement category because "
                "its public text explicitly references " +
                currentMapName +
                ", and there is no recorded account progress yet.";
        }

        return
            "Belongs directly to the current map's achievement category and "
            "has no recorded account progress yet.";
    }

    const double ratio =
        GetAchievementProgressRatio(aID);

    if (ratio >= 0.75)
    {
        if (contextual)
        {
            return
                "Matched through a broader achievement category because "
                "its public text explicitly references " +
                currentMapName +
                ", and it is at least 75% complete.";
        }

        return
            "Belongs directly to the current map's achievement category and "
            "is at least 75% complete.";
    }

    if (contextual)
    {
        return
            "Matched through a broader achievement category because "
            "its public text explicitly references " +
            currentMapName +
            ", and it has unfinished account progress.";
    }

    return
        "Belongs directly to the current map's achievement category and "
        "has unfinished account progress.";
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
                "available on this map"
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
    const OpportunityRecommendation& aRecommendation);

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

    RenderRecommendedOpportunity(opportunity);
}

///----------------------------------------------------------------------------------------------------
/// RenderRecommendedOpportunity:
///----------------------------------------------------------------------------------------------------
void RenderRecommendedOpportunity(
    const OpportunityRecommendation& aRecommendation)
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

    std::string progressText;

    if (IsCharacterAdventureGuideAchievement(
        aRecommendation.ID))
    {
        progressText = "Progress in-game only";
    }
    else if (progress == nullptr)
    {
        progressText = "Not started";
    }
    else if (progress->Current >= 0 && progress->Max > 0)
    {
        const double ratio =
            GetAchievementProgressRatio(
                aRecommendation.ID
            );

        const int percent =
            static_cast<int>((ratio * 100.0) + 0.5);

        progressText =
            std::to_string(progress->Current) +
            "/" +
            std::to_string(progress->Max) +
            " (" +
            std::to_string(percent) +
            "%)";
    }
    else
    {
        progressText = "Started";
    }

    std::string primaryReason;

    if (!aRecommendation.Reasons.empty())
    {
        primaryReason = aRecommendation.Reasons.front();
    }
    else if (IsCharacterAdventureGuideAchievement(
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

    std::string label =
        std::string(name) +
        "  |  " +
        progressText;

    if (!primaryReason.empty())
    {
        label +=
            "  |  " +
            primaryReason;
    }

    const std::string actionabilityTag =
        GetActionabilityLabel(
            aRecommendation.ID
        );

    if (!actionabilityTag.empty() &&
        actionabilityTag != "General")
    {
        label +=
            "  |  [" +
            actionabilityTag +
            "]";
    }

    label +=
        "##recommended_" +
        std::to_string(aRecommendation.ID);

    ImGui::PushStyleColor(
        ImGuiCol_Text,
        GetOpportunityDisplayColor(
            aRecommendation.ID
        )
    );

    const bool recommendationOpen =
        ImGui::TreeNodeEx(
            label.c_str(),
            ImGuiTreeNodeFlags_SpanAvailWidth
        );

    ImGui::PopStyleColor();

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
            IsContextualMapOpportunity(
                aRecommendation.ID
            )
            ? "Contextual map match"
            : "Direct map category"
        );

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

        if (ImGui::SmallButton("Open Wiki"))
        {
            OpenAchievementInNexusGameWiki(name);
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("Copy name"))
        {
            CopyAchievementName(name);
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("Copy + Hero"))
        {
            CopyAchievementNameAndOpenHero(name);
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("Ignore this achievement"))
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

    std::string label = name;

    if (IsCharacterAdventureGuideAchievement(aID))
    {
        label += "  |  Progress in-game only";
    }
    else if (progress == nullptr)
    {
        label += "  |  Not started";
    }
    else if (progress->Current >= 0 && progress->Max > 0)
    {
        const double ratio =
            GetAchievementProgressRatio(aID);

        const int percent =
            static_cast<int>((ratio * 100.0) + 0.5);

        label +=
            "  |  " +
            std::to_string(progress->Current) +
            "/" +
            std::to_string(progress->Max) +
            " (" +
            std::to_string(percent) +
            "%)";
    }
    else
    {
        label += "  |  Started";
    }

    if (definition != nullptr)
    {
        const std::string rewardSummary =
            BuildRewardSummary(aID, *definition);

        if (!rewardSummary.empty())
        {
            label += "  |  " + rewardSummary;
        }
    }

    const std::string actionabilityTag =
        GetActionabilityLabel(aID);

    if (!actionabilityTag.empty() &&
        actionabilityTag != "General")
    {
        label +=
            "  |  [" +
            actionabilityTag +
            "]";
    }

    label += "##achievement_" + std::to_string(aID);

    ImGui::PushStyleColor(
        ImGuiCol_Text,
        GetOpportunityDisplayColor(aID)
    );

    const bool opportunityOpen =
        ImGui::TreeNodeEx(
            label.c_str(),
            ImGuiTreeNodeFlags_SpanAvailWidth
        );

    ImGui::PopStyleColor();

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
            IsContextualMapOpportunity(aID)
            ? "Contextual map match"
            : "Direct map category"
        );

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
                "Progress: Not started"
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

        if (ImGui::SmallButton("Open Wiki"))
        {
            OpenAchievementInNexusGameWiki(name);
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("Copy name"))
        {
            CopyAchievementName(name);
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("Copy + Hero"))
        {
            CopyAchievementNameAndOpenHero(name);
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("Ignore this achievement"))
        {
            IgnoreAchievement(aID);
        }

        ImGui::Text("Achievement ID: %d", aID);

        ImGui::Unindent();
        ImGui::TreePop();
    }
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
/// DoesAchievementMatchCurrentMapText:
///     Used only for achievements pulled from broad cross-map categories.
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

    for (const std::string& term :
        currentOpportunityMatchTerms)
    {
        if (!term.empty() &&
            combined.find(term) != std::string::npos)
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

    const std::vector<int> idsCopy = aIDs;

    currentMapDefinitionSyncActive = true;
    currentMapDefinitionsReady = false;
    currentOpportunityStatus =
        "Loading current-map achievement details...";

    currentMapDefinitionSyncFuture = std::async(
        std::launch::async,
        [idsCopy]()
        {
            return FetchAchievementDefinitionsForIDs(idsCopy);
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

    if (!result.Success)
    {
        currentMapDefinitionsReady = false;
        currentOpportunityStatus =
            result.Error.empty()
            ? "Current-map definition sync failed."
            : result.Error;
        return;
    }

    for (auto& pair : result.Definitions)
    {
        achievementDefinitions[pair.first] =
            std::move(pair.second);
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
        "Detected from the official Guild Wars 2 event calendar."
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

            if (index + 1 <
                recommendations.size())
            {
                ImGui::Separator();
            }
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

                if (index + 1 <
                    festivalOpportunityIDs.size())
                {
                    ImGui::Separator();
                }
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

    if (currentMapName == lastOpportunityMapName)
    {
        return;
    }

    lastOpportunityMapName = currentMapName;
    currentOpportunityCategoryName.clear();
    currentOpportunityCategoryIDs.clear();
    currentMapOpportunityIDs.clear();
    currentOpportunityDirectIDs.clear();
    currentOpportunityTextFilteredIDs.clear();
    currentOpportunityMatchTerms.clear();

    currentCoverageMode = MapCoverageMode::None;
    currentCoverageDirectCategories.clear();
    currentCoverageContextualCategories.clear();

    currentMapDefinitionsReady = false;

    const AchievementCategory* matchedCategory =
        FindCategoryByName(currentMapName);

    if (matchedCategory != nullptr)
    {
        currentCoverageMode =
            MapCoverageMode::ExactCategory;

        currentCoverageDirectCategories.push_back(
            matchedCategory->Name
        );

        currentOpportunityCategoryName =
            matchedCategory->Name;

        currentOpportunityCategoryIDs =
            matchedCategory->AchievementIDs;

        for (int id : matchedCategory->AchievementIDs)
        {
            currentOpportunityDirectIDs.insert(id);
        }
    }
    else
    {
        const MapCategoryAlias* alias =
            FindMapCategoryAliases(currentMapName);

        if (alias == nullptr)
        {
            currentOpportunityStatus =
                "No achievement mapping exists for this map yet.";
            return;
        }

        currentCoverageMode =
            MapCoverageMode::CuratedMapping;

        std::vector<std::string> matchedPrimaryNames;
        std::vector<std::string> matchedSecondaryNames;

        currentOpportunityMatchTerms =
            alias->MatchTerms;

        for (const std::string& categoryName :
            alias->PrimaryCategoryNames)
        {
            const AchievementCategory* category =
                FindCategoryByName(categoryName);

            if (category == nullptr)
            {
                continue;
            }

            matchedPrimaryNames.push_back(category->Name);
            currentCoverageDirectCategories.push_back(
                category->Name
            );

            for (int id : category->AchievementIDs)
            {
                currentOpportunityDirectIDs.insert(id);

                // Direct source wins if an ID also appears in a broad category.
                currentOpportunityTextFilteredIDs.erase(id);
            }

            AppendUniqueAchievementIDs(
                currentOpportunityCategoryIDs,
                category->AchievementIDs
            );
        }

        for (const std::string& categoryName :
            alias->SecondaryCategoryNames)
        {
            const AchievementCategory* category =
                FindCategoryByName(categoryName);

            if (category == nullptr)
            {
                continue;
            }

            matchedSecondaryNames.push_back(category->Name);
            currentCoverageContextualCategories.push_back(
                category->Name
            );

            for (int id : category->AchievementIDs)
            {
                if (currentOpportunityDirectIDs.find(id) ==
                    currentOpportunityDirectIDs.end())
                {
                    currentOpportunityTextFilteredIDs.insert(id);
                }
            }

            AppendUniqueAchievementIDs(
                currentOpportunityCategoryIDs,
                category->AchievementIDs
            );
        }

        if (currentOpportunityCategoryIDs.empty())
        {
            currentOpportunityStatus =
                "A curated map mapping exists, but none of its "
                "achievement categories are currently available.";
            return;
        }

        currentOpportunityCategoryName =
            currentMapName + " (curated)";

        currentOpportunityStatus =
            "Using curated achievement mapping.";

        if (!matchedPrimaryNames.empty())
        {
            currentOpportunityStatus += " Direct: ";

            for (size_t i = 0;
                i < matchedPrimaryNames.size();
                ++i)
            {
                if (i > 0)
                {
                    currentOpportunityStatus += ", ";
                }

                currentOpportunityStatus +=
                    matchedPrimaryNames[i];
            }
        }

        if (!matchedSecondaryNames.empty())
        {
            currentOpportunityStatus += " Contextual: ";

            for (size_t i = 0;
                i < matchedSecondaryNames.size();
                ++i)
            {
                if (i > 0)
                {
                    currentOpportunityStatus += ", ";
                }

                currentOpportunityStatus +=
                    matchedSecondaryNames[i];
            }
        }
    }

    if (currentOpportunityCategoryIDs.empty())
    {
        currentOpportunityStatus =
            "This map mapping contains no achievement IDs.";
        return;
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
        }
        else
        {
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
    AddonDef.Version.Minor = 1;
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
        "Unfinished Business v0.1.1 loaded."
    );
}

///----------------------------------------------------------------------------------------------------
/// AddonUnload:
///----------------------------------------------------------------------------------------------------
void AddonUnload()
{
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
/// AddonRender:
///----------------------------------------------------------------------------------------------------
void AddonRender()
{
    MaybeExpireApiKeyInput();

    UpdateMapLookup();
    UpdateApiKeyValidation();
    UpdateAchievementSync();
    UpdateAchievementDefinitionSync();
    UpdateAchievementCategorySync();
    UpdateCurrentMapDefinitionSync();

    UpdateFestivalScheduleSync();
    UpdateFestivalDefinitionSync();
    MaybeRefreshFestivalSchedule();

    if (achievementDefinitionsLoaded &&
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
        ImVec2(620.0f, 540.0f),
        ImGuiCond_FirstUseEver
    );

    ImGui::SetNextWindowSizeConstraints(
        ImVec2(460.0f, 340.0f),
        ImVec2(1200.0f, 1200.0f)
    );

    if (ImGui::Begin("Unfinished Business"))
    {
        ImGui::TextColored(
            UBAccentColor(),
            "Unfinished Business"
        );
        ImGui::SameLine();
        ImGui::TextDisabled("v0.1.1");

        ImGui::Separator();

        if (MumbleLink != nullptr)
        {
            ImGui::TextWrapped(
                "%s",
                currentMapName.c_str()
            );

            if (!currentOpportunityCategoryName.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled(
                    " | %llu opportunities",
                    static_cast<unsigned long long>(
                        currentMapOpportunityIDs.size()
                        )
                );
            }

            if (achievementCategoriesLoaded)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("|");

                ImGui::SameLine();
                ImGui::TextColored(
                    GetCoverageDisplayColor(),
                    "Coverage: %s",
                    GetCoverageDisplayLabel()
                );

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "%s",
                        GetCoverageTooltip()
                    );
                }
            }

            if (achievementSyncActive)
            {
                ImGui::SameLine();
                ImGui::TextDisabled(" | refreshing...");
            }
        }
        else
        {
            ImGui::TextDisabled("Waiting for game context...");
        }

        ImGui::Spacing();

        if (!activeFestivalAvailable)
        {
            ImGui::TextDisabled(
                "Festival detection: %s",
                festivalScheduleStatus.c_str()
            );
        }

        RenderActiveFestivalSection();

        if (!apiKeyValid)
        {
            ImGui::TextWrapped(
                "GW2 API access is not ready. Open Nexus Options -> "
                "Unfinished Business to add or validate an API key."
            );
        }
        else if (achievementSyncActive ||
            achievementDefinitionSyncActive ||
            achievementCategorySyncActive ||
            currentMapDefinitionSyncActive)
        {
            ImGui::TextDisabled("Refreshing achievement data...");
        }
        else if (!achievementDataLoaded)
        {
            ImGui::TextDisabled("Waiting for achievement data...");
        }
        else if (!achievementCategoriesLoaded)
        {
            ImGui::TextWrapped(
                "%s",
                achievementCategoryStatus.c_str()
            );
        }
        else if (currentOpportunityCategoryName.empty())
        {
            if (activeFestivalAvailable &&
                currentOpportunityStatus ==
                "No achievement mapping exists for this map yet.")
            {
                ImGui::TextWrapped(
                    "No permanent map-specific achievement mapping "
                    "exists for this map yet."
                );
            }
            else
            {
                ImGui::TextWrapped(
                    "%s",
                    currentOpportunityStatus.c_str()
                );
            }

            if (currentCoverageMode == MapCoverageMode::None &&
                currentMapDefinitionsReady)
            {
                ImGui::Spacing();
                ImGui::Separator();
                RenderLimitedCoverageLeads();
            }
        }
        else if (currentMapDefinitionsReady)
        {
            size_t almostDoneCount = 0;
            size_t inProgressCount = 0;
            size_t notStartedCount = 0;
            size_t characterSpecificCount = 0;

            CountCurrentMapOpportunityGroups(
                almostDoneCount,
                inProgressCount,
                notStartedCount,
                characterSpecificCount
            );

            ImGui::TextColored(
                UBSuccessColor(),
                "Almost done: %llu",
                static_cast<unsigned long long>(almostDoneCount)
            );

            ImGui::SameLine();
            ImGui::TextDisabled("|");

            ImGui::SameLine();
            ImGui::TextColored(
                UBWarningColor(),
                "In progress: %llu",
                static_cast<unsigned long long>(inProgressCount)
            );

            ImGui::SameLine();
            ImGui::TextDisabled("|");

            ImGui::SameLine();
            ImGui::TextColored(
                UBDangerColor(),
                "Not started: %llu",
                static_cast<unsigned long long>(notStartedCount)
            );

            if (characterSpecificCount > 0)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("|");

                ImGui::SameLine();
                ImGui::TextColored(
                    UBSpecialColor(),
                    "Character-specific: %llu",
                    static_cast<unsigned long long>(
                        characterSpecificCount
                        )
                );
            }

            ImGui::Spacing();
            ImGui::Separator();

            const size_t characterSpecificTopCount =
                CountCharacterSpecificOpportunities();

            const bool hasAccountTracked =
                HasAccountTrackedOpportunities();

            if (!hasAccountTracked &&
                characterSpecificTopCount > 0)
            {
                ImGui::TextColored(
                    UBSpecialColor(),
                    "Character-specific opportunities in %s",
                    currentOpportunityCategoryName.c_str()
                );

                ImGui::TextDisabled(
                    "Exact progress is only visible in the in-game "
                    "Achievements panel."
                );

                ImGui::Spacing();

                for (int id : currentMapOpportunityIDs)
                {
                    if (!IsCharacterAdventureGuideAchievement(id))
                    {
                        continue;
                    }

                    OpportunityRecommendation opportunity =
                        BuildRecommendation(id);

                    ImGui::PushID(id);
                    RenderRecommendedOpportunity(opportunity);
                    ImGui::PopID();
                }
            }
            else
            {
                ImGui::TextColored(
                    UBAccentColor(),
                    "Recommended in %s",
                    currentOpportunityCategoryName.c_str()
                );

                ImGui::Spacing();

                const std::vector<OpportunityRecommendation>
                    recommendations =
                    BuildCurrentMapRecommendations();

                if (recommendations.empty())
                {
                    ImGui::TextDisabled(
                        "No recommendations available."
                    );
                }
                else
                {
                    for (size_t recommendationIndex = 0;
                        recommendationIndex < recommendations.size();
                        ++recommendationIndex)
                    {
                        const OpportunityRecommendation&
                            recommendation =
                            recommendations[recommendationIndex];

                        ImGui::PushID(
                            static_cast<int>(
                                recommendationIndex
                                )
                        );

                        RenderRecommendedOpportunity(
                            recommendation
                        );

                        ImGui::PopID();

                        if (recommendationIndex + 1 <
                            recommendations.size())
                        {
                            ImGui::Separator();
                        }
                    }
                }
            }

            if (hasAccountTracked &&
                characterSpecificTopCount > 0)
            {
                ImGui::Spacing();
                ImGui::Separator();

                ImGui::TextColored(
                    UBSpecialColor(),
                    "Character-specific opportunities"
                );

                ImGui::TextDisabled(
                    "Exact progress is only visible in the in-game "
                    "Achievements panel."
                );

                ImGui::Spacing();

                for (int id : currentMapOpportunityIDs)
                {
                    if (!IsCharacterAdventureGuideAchievement(id))
                    {
                        continue;
                    }

                    ImGui::PushID(id);
                    RenderCharacterSpecificOpportunity(id);
                    ImGui::PopID();
                }
            }

            ImGui::Spacing();

            // If every visible opportunity is character-specific, the top
            // section already shows the complete list. Do not duplicate it.
            const bool showAllMapOpportunities =
                hasAccountTracked;

            if (showAllMapOpportunities)
            {
                ImGui::Separator();
            }

            if (showAllMapOpportunities &&
                ImGui::CollapsingHeader(
                    "Account-tracked opportunities",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginChild(
                    "##CurrentMapOpportunityList",
                    ImVec2(0.0f, 300.0f),
                    true))
                {
                    if (almostDoneCount > 0)
                    {
                        ImGui::TextColored(
                            UBSuccessColor(),
                            "Almost Done"
                        );

                        for (int id : currentMapOpportunityIDs)
                        {
                            const double ratio =
                                GetAchievementProgressRatio(id);

                            if (ratio < 0.75)
                            {
                                continue;
                            }

                            RenderOpportunityRow(id);
                        }

                        ImGui::Spacing();
                    }

                    if (inProgressCount > 0)
                    {
                        ImGui::TextColored(
                            UBWarningColor(),
                            "In Progress"
                        );

                        for (int id : currentMapOpportunityIDs)
                        {
                            const AchievementProgress* progress =
                                FindAchievementProgress(id);

                            if (progress == nullptr)
                            {
                                continue;
                            }

                            const double ratio =
                                GetAchievementProgressRatio(id);

                            if (ratio >= 0.75)
                            {
                                continue;
                            }

                            RenderOpportunityRow(id);
                        }

                        ImGui::Spacing();
                    }

                    if (notStartedCount > 0)
                    {
                        ImGui::TextColored(
                            UBDangerColor(),
                            "Not Started"
                        );

                        for (int id : currentMapOpportunityIDs)
                        {
                            if (IsCharacterAdventureGuideAchievement(id))
                            {
                                continue;
                            }

                            if (FindAchievementProgress(id) != nullptr)
                            {
                                continue;
                            }

                            RenderOpportunityRow(id);
                        }

                        ImGui::Spacing();
                    }

                    // Character-specific objectives are shown in their own
                    // dedicated section above and are intentionally not repeated here.

                    if (currentMapOpportunityIDs.empty())
                    {
                        ImGui::TextDisabled(
                            "No map opportunities found "
                            "in this map category."
                        );
                    }
                }

                ImGui::EndChild();
            }
        }
        else
        {
            ImGui::TextDisabled(
                "Loading current-map achievement details..."
            );
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

            if (ImGui::SmallButton("Open Wiki"))
            {
                OpenAchievementInNexusGameWiki(
                    lead.Name.c_str()
                );
            }

            ImGui::SameLine();

            if (ImGui::SmallButton("Copy name"))
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
    ImGui::TextDisabled("Version 0.1.1");
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
                "Visible unfinished: %llu",
                static_cast<unsigned long long>(
                    currentMapOpportunityIDs.size()
                    )
            );
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
