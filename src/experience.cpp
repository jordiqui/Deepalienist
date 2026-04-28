#include "experience.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iterator>
#include <limits>
#include <list>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

#include "experience_compat.h"
#include "misc.h"
#include "position.h"
#include "search.h"
#include "uci.h"
#include "ucioption.h"

namespace Stockfish::Experience {

namespace {

struct Entry {
    Move           bestMove        = Move::none();
    Value          score           = VALUE_ZERO;
    Value          eval            = VALUE_ZERO;
    Depth          depth           = 0;
    std::uint32_t  performance     = 0;
    std::uint16_t  visits          = 0;
};

struct Settings {
    bool        enabled            = true;
    bool        readonly           = false;
    bool        bookEnabled        = false;
    int         bookWidth          = 1;
    int         bookEvalImportance = 5;
    Depth       bookMinDepth       = 4;
    int         bookMaxMoves       = 16;
    std::string file               = "experience.exp";
    std::size_t maxEntries         = 200000;
    Value       minScore           = Value(20);
};

struct Stats {
    std::size_t uniqueMoves    = 0;
    std::size_t duplicateMoves = 0;
    std::size_t totalPositions = 0;
};

Settings settings;
std::unordered_map<std::uint64_t, std::vector<Entry>> table;
std::list<std::uint64_t>                               lru;
std::unordered_map<std::uint64_t, std::list<std::uint64_t>::iterator> lruIndex;
std::mutex                                            mutex;
bool                                                  dirty                = false;
Stats                                                 stats;
std::string                                           lastStatus;
bool                                                  settingsInitialized = false;

#pragma pack(push, 1)
struct PersistedLearningMove {
    std::uint64_t key = 0;
    std::uint32_t depth = 0;
    std::int32_t  scoreCp = 0;
    std::uint32_t taggedMove = 0;
    std::uint32_t performance = 0;
};
#pragma pack(pop)

static_assert(sizeof(PersistedLearningMove) == 24, "unexpected persisted format size");
constexpr std::uint16_t ExpTag = 0x746C;

void touch_lru(std::uint64_t key) {
    auto it = lruIndex.find(key);
    if (it != lruIndex.end())
    {
        lru.splice(lru.end(), lru, it->second);
        it->second = std::prev(lru.end());
    }
    else
    {
        lru.push_back(key);
        lruIndex[key] = std::prev(lru.end());
    }
}

Entry* find_entry(std::vector<Entry>& entries, Move move) {
    auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) {
        return e.bestMove == move;
    });

    if (it == entries.end())
        return nullptr;

    return &*it;
}

[[maybe_unused]] const Entry* find_entry(const std::vector<Entry>& entries, Move move) {
    auto it = std::find_if(entries.begin(), entries.end(), [&](const Entry& e) {
        return e.bestMove == move;
    });

    if (it == entries.end())
        return nullptr;

    return &*it;
}

void merge_entry_for_load(std::uint64_t key, const Entry& entry, Stats& loadStats) {
    auto& entries  = table[key];
    bool  wasEmpty = entries.empty();
    auto* existing = find_entry(entries, entry.bestMove);

    if (existing)
    {
        ++loadStats.duplicateMoves;

        if (entry.depth > existing->depth)
        {
            *existing = entry;
        }
        else if (entry.depth == existing->depth)
        {
            existing->score = entry.score;
            existing->eval  = entry.eval;
        }

        existing->performance = std::max(existing->performance, entry.performance);
        existing->visits = std::max(existing->visits, entry.visits);
    }
    else
    {
        entries.push_back(entry);
        ++loadStats.uniqueMoves;
    }

    if (wasEmpty && !entries.empty())
        ++loadStats.totalPositions;

    touch_lru(key);
}

std::string format_status_unlocked() {
    if (!settings.enabled)
        return "Experience disabled";

    const std::string fileName = settings.file.empty() ? "<none>" : settings.file;
    const std::size_t totalMoves = stats.uniqueMoves + stats.duplicateMoves;

    double fragmentation = totalMoves ? (100.0 * static_cast<double>(stats.duplicateMoves)
                                          / static_cast<double>(totalMoves))
                                      : 0.0;

    std::ostringstream oss;
    oss << fileName << " -> Total moves: " << totalMoves << ". Total positions: "
        << stats.totalPositions << ". Duplicate moves: " << stats.duplicateMoves
        << ". Fragmentation: " << std::fixed << std::setprecision(2) << fragmentation
        << '%';

    return oss.str();
}

void enforce_limit() {
    if (settings.maxEntries == 0)
        return;

    while (table.size() > settings.maxEntries && !lru.empty())
    {
        auto key = lru.front();
        lru.pop_front();
        lruIndex.erase(key);

        auto it = table.find(key);
        if (it != table.end())
        {
            if (stats.uniqueMoves >= it->second.size())
                stats.uniqueMoves -= it->second.size();
            else
                stats.uniqueMoves = 0;

            if (stats.totalPositions > 0)
                --stats.totalPositions;

            table.erase(it);
        }
    }
}

void ensure_directory_exists(const std::string& file) {
    namespace fs = std::filesystem;

    if (file.empty())
        return;

    fs::path path(file);
    if (!path.has_parent_path())
        return;

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
}

bool atomic_rename(const std::filesystem::path& source, const std::filesystem::path& destination) {
#ifdef _WIN32
    const std::wstring sourcePath      = source.wstring();
    const std::wstring destinationPath = destination.wstring();
    return MoveFileExW(sourcePath.c_str(), destinationPath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(source, destination, ec);
    return !ec;
#endif
}

void save_table_unlocked(const std::string& file) {
    if (settings.readonly || file.empty())
        return;

    const std::filesystem::path destination(file);
    const std::filesystem::path temporary = destination.string() + ".tmp";

    ensure_directory_exists(destination.string());

    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out)
        return;

    for (const auto key : lru)
    {
        const auto it = table.find(key);
        if (it == table.end())
            continue;

        for (const auto& e : it->second)
        {
            if (!e.bestMove)
                continue;

            PersistedLearningMove persisted;
            persisted.key         = key;
            persisted.depth       = std::uint32_t(std::max(0, int(e.depth)));
            persisted.scoreCp     = int(e.score);
            persisted.taggedMove  = (std::uint32_t(ExpTag) << 16) | std::uint32_t(e.bestMove.raw());
            persisted.performance = std::uint32_t(e.performance);

            out.write(reinterpret_cast<const char*>(&persisted), sizeof(persisted));
        }
    }

    out.close();
    atomic_rename(temporary, destination);
}

bool load_brainlearn_binary(std::istream& in, Stats& loadStats) {
    auto startPos = in.tellg();
    in.seekg(0, std::ios::end);
    const auto fileSize = in.tellg();
    in.seekg(startPos, std::ios::beg);

    if (fileSize <= 0 || (fileSize % std::streamoff(sizeof(PersistedLearningMove))) != 0)
        return false;

    bool parsedAny = false;

    while (true)
    {
        PersistedLearningMove raw{};
        if (!in.read(reinterpret_cast<char*>(&raw), sizeof(raw)))
            break;

        Entry entry;
        const std::uint16_t tag = std::uint16_t((raw.taggedMove >> 16) & 0xFFFFU);
        if (tag != ExpTag)
        {
            in.clear();
            in.seekg(startPos, std::ios::beg);
            return false;
        }

        entry.bestMove = Move(std::uint16_t(raw.taggedMove & 0xFFFFU));
        entry.score    = Value(raw.scoreCp);
        entry.eval     = Value(raw.scoreCp);
        entry.depth    = Depth(raw.depth);
        entry.performance = raw.performance;
        entry.visits   = 1;
        merge_entry_for_load(raw.key, entry, loadStats);
        parsedAny = true;
    }

    if (!parsedAny)
    {
        in.clear();
        in.seekg(startPos, std::ios::beg);
        return false;
    }

    return true;
}

bool load_text_format(std::istream& in, Stats& loadStats) {
    bool parsedAny = false;

    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::uint64_t      key;
        unsigned           rawMove;
        int                score, eval;
        int                depth;
        unsigned           visits = 0;

        if (!(iss >> key >> rawMove >> score >> eval >> depth >> visits))
            continue;

        Entry entry;
        entry.bestMove = Move(static_cast<std::uint16_t>(rawMove));
        entry.score    = Value(score);
        entry.eval     = Value(eval);
        entry.depth    = Depth(depth);
        entry.performance = 0;
        entry.visits   = std::uint16_t(std::min<unsigned>(
          visits, std::numeric_limits<std::uint16_t>::max()));

        merge_entry_for_load(key, entry, loadStats);
        parsedAny = true;
    }

    return parsedAny;
}

bool load_hypnos_binary(std::istream& in, Stats& loadStats) {
    constexpr std::string_view SignaturePrefix = "SugaR Experience version";

    auto startPos = in.tellg();

    std::string header;
    if (!std::getline(in, header))
    {
        in.clear();
        in.seekg(startPos, std::ios::beg);
        return false;
    }

    if (!header.empty() && header.back() == '\r')
        header.pop_back();

    if (header.compare(0, SignaturePrefix.size(), SignaturePrefix) != 0)
    {
        in.clear();
        in.seekg(startPos, std::ios::beg);
        return false;
    }

    int version = 0;
    std::istringstream versionStream(header.substr(SignaturePrefix.size()));
    versionStream >> version;
    if (version <= 0)
        version = 1;

    struct RawEntryV1 {
        std::uint64_t key;
        std::uint32_t move;
        std::int32_t  value;
        std::int32_t  depth;
        std::uint8_t  padding[4];
    };

    struct RawEntryV2 {
        std::uint64_t key;
        std::uint32_t move;
        std::int32_t  value;
        std::int32_t  depth;
        std::uint16_t count;
        std::uint8_t  padding[2];
    };

    bool parsedAny = false;

    while (true)
    {
        if (version >= 2)
        {
            RawEntryV2 raw{};
            if (!in.read(reinterpret_cast<char*>(&raw), sizeof(raw)))
                break;

            Entry entry;
            entry.bestMove = Move(static_cast<std::uint16_t>(raw.move));
            entry.score    = Value(raw.value);
            entry.eval     = Value(raw.value);
            entry.depth    = Depth(raw.depth);
            entry.performance = 0;
            entry.visits   = std::uint16_t(raw.count);

            merge_entry_for_load(raw.key, entry, loadStats);
            parsedAny = true;
        }
        else
        {
            RawEntryV1 raw{};
            if (!in.read(reinterpret_cast<char*>(&raw), sizeof(raw)))
                break;

            Entry entry;
            entry.bestMove = Move(static_cast<std::uint16_t>(raw.move));
            entry.score    = Value(raw.value);
            entry.eval     = Value(raw.value);
            entry.depth    = Depth(raw.depth);
            entry.performance = 0;
            entry.visits   = 1;

            merge_entry_for_load(raw.key, entry, loadStats);
            parsedAny = true;
        }
    }

    if (!parsedAny)
    {
        in.clear();
        in.seekg(startPos, std::ios::beg);
        return false;
    }

    return true;
}

void load_table_unlocked(const std::string& file) {
    table.clear();
    lru.clear();
    lruIndex.clear();
    dirty        = false;
    stats        = {};
    lastStatus.clear();

    if (file.empty())
        return;

    std::ifstream in(file, std::ios::binary);
    if (!in)
        return;

    Stats loadStats;
    bool  loaded = false;

    if (load_brainlearn_binary(in, loadStats))
        loaded = true;
    else if (load_hypnos_binary(in, loadStats))
        loaded = true;
    else
    {
        in.clear();
        in.seekg(0, std::ios::beg);

        if (load_text_format(in, loadStats))
            loaded = true;
        else
        {
            in.clear();
            in.seekg(0, std::ios::beg);

            if (Compat::load_legacy_binary(in,
                                           [&](std::uint64_t key,
                                               Move          move,
                                               Value         score,
                                               Value         eval,
                                               Depth         depth,
                                               std::uint16_t visits) {
                                               Entry entry;
                                               entry.bestMove    = move;
                                               entry.score       = score;
                                               entry.eval        = eval;
                                               entry.depth       = depth;
                                               entry.performance = 0;
                                               entry.visits      = std::uint16_t(std::min<std::uint32_t>(
                                                 visits,
                                                 std::numeric_limits<std::uint16_t>::max()));
                                               merge_entry_for_load(key, entry, loadStats);
                                           }))
                loaded = true;
        }
    }

    if (!loaded)
    {
        table.clear();
        lru.clear();
        lruIndex.clear();
        stats     = {};
        lastStatus = format_status_unlocked();
        return;
    }

    stats = loadStats;
    enforce_limit();
    stats.totalPositions = table.size();
    lastStatus           = format_status_unlocked();
}

void flush_unlocked() {
    if (!dirty)
        return;

    if (settings.readonly)
    {
        dirty        = false;
        return;
    }

    save_table_unlocked(settings.file);
    dirty        = false;
}

}  // namespace

std::optional<std::string> update_settings(const OptionsMap& options) {
    Settings newSettings = settings;

    if (options.count("Experience"))
        newSettings.enabled = bool(int(options["Experience"]));
    else if (options.count("Experience Enabled"))
        newSettings.enabled = bool(int(options["Experience Enabled"]));
    else
        newSettings.enabled = false;

    if (options.count("ExperienceFile"))
        newSettings.file = std::string(options["ExperienceFile"]);
    else if (options.count("Experience File"))
        newSettings.file = std::string(options["Experience File"]);
    else if (newSettings.file.empty())
        newSettings.file = "experience.exp";

    if (options.count("Read only learning"))
        newSettings.readonly = bool(int(options["Read only learning"]));
    else if (options.count("Experience Readonly"))
        newSettings.readonly = bool(int(options["Experience Readonly"]));
    else
        newSettings.readonly = false;

    if (options.count("Experience Book"))
        newSettings.bookEnabled = bool(int(options["Experience Book"]));
    else
        newSettings.bookEnabled = false;

    if (options.count("Experience Book Width"))
        newSettings.bookWidth = std::max(1, int(options["Experience Book Width"]));
    else
        newSettings.bookWidth = 1;

    if (options.count("Experience Book Eval Importance"))
        newSettings.bookEvalImportance =
          std::clamp(int(options["Experience Book Eval Importance"]), 0, 10);
    else
        newSettings.bookEvalImportance = 5;

    if (options.count("Experience Book Min Depth"))
        newSettings.bookMinDepth = Depth(std::max(0, int(options["Experience Book Min Depth"])));
    else
        newSettings.bookMinDepth = Depth(4);

    if (options.count("Experience Book Max Moves"))
        newSettings.bookMaxMoves = std::max(1, int(options["Experience Book Max Moves"]));
    else
        newSettings.bookMaxMoves = 16;

    newSettings.minScore = Value(newSettings.bookEvalImportance * 4);

    std::scoped_lock lock(mutex);

    const bool wasEnabled         = settings.enabled;
    const bool fileChanged        = settings.file != newSettings.file;
    const bool readonlyActivated  = !settings.readonly && newSettings.readonly;

    if (settings.enabled && dirty && !settings.readonly
        && (!newSettings.enabled || fileChanged || readonlyActivated))
        save_table_unlocked(settings.file);

    settings = std::move(newSettings);

    if (!settings.enabled)
    {
        table.clear();
        lru.clear();
        lruIndex.clear();
        dirty        = false;
        stats        = {};
        lastStatus   = format_status_unlocked();
        settingsInitialized = true;
        return lastStatus;
    }

    const bool shouldLoad = !settingsInitialized || fileChanged
                             || (!wasEnabled && settings.enabled);

    if (shouldLoad)
        load_table_unlocked(settings.file);
    else
        lastStatus = format_status_unlocked();

    settingsInitialized = true;
    return lastStatus;
}

bool is_enabled() {
    std::scoped_lock lock(mutex);
    return settings.enabled;
}

void on_new_position(const Position& pos, std::vector<Search::RootMove>& rootMoves) {
    std::scoped_lock lock(mutex);

    if (!settings.enabled || !settings.bookEnabled || rootMoves.empty())
        return;

    const int maxPliesFromBook = settings.bookMaxMoves > 0 ? settings.bookMaxMoves * 2 : 0;
    if (maxPliesFromBook != 0 && pos.game_ply() >= maxPliesFromBook)
        return;

    const auto key = std::uint64_t(pos.key());
    auto       it  = table.find(key);
    if (it == table.end())
        return;

    touch_lru(key);

    const auto& entries = it->second;

    std::vector<const Entry*> candidates;
    candidates.reserve(entries.size());

    for (const auto& entry : entries)
    {
        if (!entry.bestMove)
            continue;

        if (entry.depth < settings.bookMinDepth)
            continue;

        if (std::abs(entry.score) < settings.minScore)
            continue;

        candidates.push_back(&entry);
    }

    if (candidates.empty())
        return;

    const auto quality = [&](const Entry* entry) {
        const int evalImportance   = settings.bookEvalImportance;
        const int countImportance  = 10 - evalImportance;
        const long long evalScore  = static_cast<long long>(entry->eval) * evalImportance;
        const long long countScore = static_cast<long long>(entry->visits) * countImportance * 16LL;
        const long long depthScore = static_cast<long long>(entry->depth) * 8LL;

        return evalScore + countScore + depthScore;
    };

    std::stable_sort(candidates.begin(), candidates.end(), [&](const Entry* lhs, const Entry* rhs) {
        return quality(lhs) > quality(rhs);
    });

    const std::size_t width = std::min<std::size_t>(settings.bookWidth, candidates.size());
    const Entry*      best  = candidates.front();

    auto bestIt = std::find_if(rootMoves.begin(), rootMoves.end(), [&](const Search::RootMove& rm) {
        return !rm.pv.empty() && rm.pv[0] == best->bestMove;
    });

    if (bestIt == rootMoves.end())
        return;

    if (bestIt != rootMoves.begin())
        std::rotate(rootMoves.begin(), bestIt, std::next(bestIt));

    auto apply_entry = [](const Entry& entry, Search::RootMove& rm) {
        rm.score            = std::max(rm.score, entry.score);
        rm.previousScore    = std::max(rm.previousScore, entry.score);
        rm.averageScore     = entry.score;
        rm.meanSquaredScore = entry.score * entry.score;
        rm.selDepth         = std::max<int>(rm.selDepth, entry.depth);
    };

    apply_entry(*best, rootMoves.front());

    for (std::size_t i = 1; i < width; ++i)
    {
        const Entry* candidate = candidates[i];
        auto         rmIt      = std::find_if(rootMoves.begin(), rootMoves.end(),
                                       [&](const Search::RootMove& rm) {
                                           return !rm.pv.empty() && rm.pv[0] == candidate->bestMove;
                                       });

        if (rmIt != rootMoves.end())
            apply_entry(*candidate, *rmIt);
    }
}

void on_search_complete(const Position& pos,
                        const std::vector<Search::RootMove>& rootMoves,
                        Value                                 bestScore,
                        Value                                 evalScore,
                        Depth                                 searchedDepth,
                        const Search::LimitsType&) {
    std::scoped_lock lock(mutex);

    if (!settings.enabled || rootMoves.empty())
        return;

    if (rootMoves.front().pv.empty())
        return;

    if (settings.readonly)
        return;

    const auto key = std::uint64_t(pos.key());
    auto&       entries = table[key];
    const bool  wasEmpty = entries.empty();

    const Move move = rootMoves.front().pv[0];

    Entry* existing = find_entry(entries, move);
    if (existing)
    {
        existing->score = bestScore;
        existing->eval  = evalScore;
        existing->depth = std::max(existing->depth, searchedDepth);
        existing->performance = std::max<std::uint32_t>(existing->performance, std::uint32_t(pos.game_ply()));
        if (existing->visits < std::numeric_limits<std::uint16_t>::max())
            ++existing->visits;
    }
    else
    {
        Entry entry;
        entry.bestMove = move;
        entry.score    = bestScore;
        entry.eval     = evalScore;
        entry.depth    = searchedDepth;
        entry.performance = std::uint32_t(pos.game_ply());
        entry.visits   = 1;

        entries.push_back(entry);
        ++stats.uniqueMoves;
    }

    if (wasEmpty && !entries.empty())
        ++stats.totalPositions;

    touch_lru(key);
    enforce_limit();
    stats.totalPositions = table.size();

    dirty = true;
    flush_unlocked();

    lastStatus = format_status_unlocked();
}

void new_game() {
    std::scoped_lock lock(mutex);
    flush_unlocked();
}

void flush() {
    std::scoped_lock lock(mutex);
    flush_unlocked();
}

std::string status_summary() {
    std::scoped_lock lock(mutex);

    if (lastStatus.empty())
        lastStatus = format_status_unlocked();

    return lastStatus;
}

}  // namespace Stockfish::Experience

namespace Stockfish::Experience::Compat {

namespace {

// Very small helper struct mirroring the legacy format. The values and layout
// are inferred from the historical Stockfish self-learning project where the
// entries were stored in packed binary form using 32 bit little endian fields.
struct LegacyEntry {
    std::uint64_t key;
    std::uint16_t move;
    std::int16_t  score;
    std::int16_t  eval;
    std::int16_t  depth;
    std::uint16_t visits;
};

constexpr char LegacyMagic[] = {'D', 'A', 'L', 'N'};

bool read_legacy_entry(std::istream& in, LegacyEntry& entry) {
    return static_cast<bool>(in.read(reinterpret_cast<char*>(&entry), sizeof(LegacyEntry)));
}

}  // namespace

bool load_legacy_binary(std::istream& input, const EntryCallback& callback) {
    if (!callback)
        return false;

    // Peek the first bytes to detect a known binary header. If the header does
    // not match, bail out early so that the caller can attempt other formats.
    char header[sizeof(LegacyMagic)] = {};
    auto pos                         = input.tellg();

    if (!input.read(header, sizeof(header)))
    {
        input.clear();
        input.seekg(pos, std::ios::beg);
        return false;
    }

    if (!std::equal(std::begin(header), std::end(header), std::begin(LegacyMagic)))
    {
        input.clear();
        input.seekg(pos, std::ios::beg);
        return false;
    }

    // Consume the remaining newline after the magic if present.
    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    while (true)
    {
        LegacyEntry raw{};
        if (!read_legacy_entry(input, raw))
            break;

        callback(raw.key,
                 Move(raw.move),
                 Value(raw.score),
                 Value(raw.eval),
                 Depth(raw.depth),
                 raw.visits);
    }

    return true;
}

}  // namespace Stockfish::Experience::Compat
