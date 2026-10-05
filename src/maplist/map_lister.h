#ifndef _INCLUDE_RTV_MAP_LISTER_H_
#define _INCLUDE_RTV_MAP_LISTER_H_

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <utility>
#include <vector>

struct MapEntry
{
	std::string displayName; // Full display name, e.g. "kz_grotto (T3, Linear)"
	std::string mapName;     // Clean name for changelevel, e.g. "kz_grotto"
	std::string workshopId;  // Workshop ID if present, else empty
	bool isWorkshop = false;
	// Added by a lookup for a map outside the approved pool (kept across pool refreshes).
	bool dynamic = false;
	// CS2KZ nub_tier per course (1-10), in course order. One entry per course that
	// has a tier for that mode. Empty = unknown.
	std::vector<int> classicTiers;
	std::vector<int> vanillaTiers;
};

class MapLister
{
public:
	// The pool is every approved map on the CS2KZ API, fetched async. There is no local list.
	// Fetches the full approved list and swaps it in on the game thread.
	// Entries added by AddDynamicMap survive the swap unless the new pool has them.
	// onDone(count) runs on the game thread, count is -1 when the fetch failed or one is already running.
	void RefreshAsync(std::function<void(int)> onDone = nullptr);

	// True when no pool is loaded yet or the loaded one is older than kRefreshIntervalSeconds, and no fetch is running.
	bool NeedsRefresh() const;

	static constexpr int kRefreshIntervalSeconds = 1200;

	// Dynamically add a map (from API lookup / off-pool nomination).
	// Does not write to disk. Returns pointer to the entry.
	const MapEntry *AddDynamicMap(const MapEntry &entry);

	const std::vector<MapEntry> &GetMaps() const
	{
		return m_maps;
	}

	// Find exact match by display name or map name (case-insensitive).
	const MapEntry *FindExact(const std::string &name) const;

	// Find exact match by workshop ID.
	const MapEntry *FindByWorkshopId(const std::string &workshopId) const;

	// Find all maps whose display/map name contains the query string.
	std::vector<const MapEntry *> FindMatching(const std::string &query) const;

	// Resolve a user input string to a single map (exact first, then partial).
	// Returns nullptr if no match or multiple matches (caller should show menu).
	// outMatches is populated with all partial matches when nullptr is returned.
	const MapEntry *Resolve(const std::string &input, std::vector<const MapEntry *> *outMatches) const;

	bool IsLoaded() const
	{
		return m_loaded;
	}

	// Player-facing label for a map: displayName (or mapName) plus the CS2KZ tier
	// annotation ("CKZ: x VNL: x") when DisplayKzTiers is enabled.
	// When colorize is true the tier values are wrapped in chat color codes.
	// resetColor is the color restored after each colored tier value, so trailing
	// text keeps the surrounding row color (e.g. "\x08" grey for disabled rows).
	std::string GetDisplayLabel(const MapEntry &e, bool colorize = true, const char *resetColor = "\x01") const;

	// Async API lookups (run on background thread; callback on same thread).
	// DO NOT call game engine APIs from the callback - set a flag and handle on next GameFrame tick.

	// Look up a map by workshop ID via CS2KZ API, then Steam fallback.
	// callback(entry) where entry.mapName is empty on failure.
	void LookupByWorkshopIdAsync(const std::string &workshopId, std::function<void(MapEntry)> callback) const;

	// Look up a map by name via CS2KZ API.
	void LookupByNameAsync(const std::string &name, std::function<void(MapEntry)> callback) const;

	// Validate all workshop maps in the pool via Steam API.
	// Dead maps are reported to server console and optionally Discord webhook.
	void ValidateMapsAsync() const;

private:
	std::vector<MapEntry> m_maps;
	bool m_loaded = false;
	std::chrono::steady_clock::time_point m_lastRefresh;

	// True while a pool fetch is in progress. Set on the game thread, cleared on the game thread
	// once the result is merged, atomic because the fetch itself runs on a worker.
	std::atomic<bool> m_refreshInFlight {false};

	// Replace the pool with a fresh fetch, keeping dynamic entries it does not contain.
	void ApplyPool(std::vector<MapEntry> fresh);

	// Paginate the CS2KZ approved-maps endpoint, parsing tiers into each entry.
	// onComplete is invoked on a BACKGROUND thread with the full list.
	static void FetchAllApprovedMapsAsync(std::function<void(std::vector<MapEntry>)> onComplete);

	// Build a MapEntry from a CS2KZ API JSON map object string fragment.
	// Returns false if parsing failed.
	static bool ParseCS2KZMapJson(const std::string &jsonObj, MapEntry &out);
};

extern MapLister g_MapLister;

// Sorts map menus by mmu::MapNameLess, which ChatMenuDef::mapList's page labels match.
void SortMapsByName(std::vector<const MapEntry *> &maps);

#endif // _INCLUDE_RTV_MAP_LISTER_H_
