#include "experience.h"

#include <algorithm>
#include <array>
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
#include <cmath>
#include <vector>
#include <cstdlib>
#include <system_error>

#include "experience_zobrist.h"
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
    int         bookMinVisits      = 1;
    std::string file               = "experience.exp";
    std::size_t maxEntries         = 200000;
    Value       minScore           = Value(20);
    bool        auditEnabled       = false;
};

struct Stats {
    std::size_t uniqueMoves    = 0;
    std::size_t duplicateMoves = 0;
    std::size_t totalPositions = 0;
};

struct Distribution {
    static constexpr std::size_t BucketCount = 8;
    std::array<std::uint64_t, BucketCount> buckets{};

    void add(int value) {
        const int absValue = std::abs(value);
        const std::size_t index = absValue < 25   ? 0
                                : absValue < 50   ? 1
                                : absValue < 100  ? 2
                                : absValue < 200  ? 3
                                : absValue < 400  ? 4
                                : absValue < 800  ? 5
                                : absValue < 1600 ? 6
                                                  : 7;
        ++buckets[index];
    }

    [[nodiscard]] std::string to_string(std::string_view label) const {
        static constexpr std::array<std::string_view, BucketCount> Ranges = {
          "[0,24]", "[25,49]", "[50,99]", "[100,199]", "[200,399]", "[400,799]", "[800,1599]",
          "[1600,+)"};

        std::ostringstream oss;
        oss << label << '=';
        for (std::size_t i = 0; i < BucketCount; ++i)
        {
            if (i)
                oss << ',';
            oss << Ranges[i] << ':' << buckets[i];
        }
        return oss.str();
    }
};

struct ProbeCounters {
    std::uint64_t probes = 0;
    std::uint64_t hits = 0;
    std::uint64_t filteredHits = 0;
    std::uint64_t minVisitsRejected = 0;
    std::uint64_t rootReorders = 0;
    std::uint64_t entriesWritten = 0;

    void clear() { *this = {}; }
};

struct AuditMetrics {
    std::uint64_t loadCount = 0;
    std::uint64_t saveCount = 0;
    std::uint64_t loadMicros = 0;
    std::uint64_t saveMicros = 0;
    std::uint64_t finishedGames = 0;
    std::uint64_t sessionEntriesWritten = 0;
    std::uint64_t currentGameEntriesWritten = 0;
    ProbeCounters lifetime;
    ProbeCounters sinceNewGame;
    Distribution scoreDistribution;
    Distribution depthDistribution;
    Distribution visitsDistribution;

    void clear() { *this = {}; }
};

Settings settings;
std::unordered_map<std::uint64_t, std::vector<Entry>> table;
std::list<std::uint64_t>                               lru;
std::unordered_map<std::uint64_t, std::list<std::uint64_t>::iterator> lruIndex;
std::mutex                                            mutex;
bool                                                  dirty                = false;
std::size_t                                           pendingFlush         = 0;
constexpr std::size_t                                 FlushInterval        = 64;
Stats                                                 stats;
AuditMetrics                                          auditMetrics;
std::string                                           lastStatus;
bool                                                  settingsInitialized = false;

std::uint64_t compute_key(const Position& pos) {
    using namespace Experience::Zobrist;

    std::uint64_t key = pos.key();
    key               = combine(key, std::uint64_t(pos.side_to_move()));
    key               = combine(key, std::uint64_t(pos.game_ply()));
    key               = combine(key, std::uint64_t(pos.rule50_count()));
    return key;
}

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


void record_entry_distributions(const Entry& entry) {
    if (!settings.auditEnabled)
        return;

    auditMetrics.scoreDistribution.add(int(entry.score));
    auditMetrics.depthDistribution.add(int(entry.depth));
    auditMetrics.visitsDistribution.add(int(entry.visits));
}

void reset_audit_metrics_unlocked() {
    auditMetrics.clear();
}

void reset_since_newgame_counters_unlocked() {
    if (!settings.auditEnabled)
        return;

    auditMetrics.sinceNewGame.clear();
    auditMetrics.currentGameEntriesWritten = 0;
}

void finalize_game_metrics_unlocked() {
    if (!settings.auditEnabled)
        return;

    if (auditMetrics.currentGameEntriesWritten > 0 || auditMetrics.finishedGames > 0)
        ++auditMetrics.finishedGames;

    reset_since_newgame_counters_unlocked();
}

std::string format_audit_summary_unlocked() {
    if (!settings.auditEnabled)
        return "Experience audit disabled (default behavior unchanged)";

    const auto avgLoad = auditMetrics.loadCount
      ? static_cast<double>(auditMetrics.loadMicros) / static_cast<double>(auditMetrics.loadCount)
      : 0.0;
    const auto avgSave = auditMetrics.saveCount
      ? static_cast<double>(auditMetrics.saveMicros) / static_cast<double>(auditMetrics.saveCount)
      : 0.0;
    const auto lifetimeProbeHitRate = auditMetrics.lifetime.probes
      ? (100.0 * static_cast<double>(auditMetrics.lifetime.hits)
         / static_cast<double>(auditMetrics.lifetime.probes))
      : 0.0;
    const auto lifetimeFilteredHitRate = auditMetrics.lifetime.probes
      ? (100.0 * static_cast<double>(auditMetrics.lifetime.filteredHits)
         / static_cast<double>(auditMetrics.lifetime.probes))
      : 0.0;
    const auto newgameProbeHitRate = auditMetrics.sinceNewGame.probes
      ? (100.0 * static_cast<double>(auditMetrics.sinceNewGame.hits)
         / static_cast<double>(auditMetrics.sinceNewGame.probes))
      : 0.0;
    const auto newgameFilteredHitRate = auditMetrics.sinceNewGame.probes
      ? (100.0 * static_cast<double>(auditMetrics.sinceNewGame.filteredHits)
         / static_cast<double>(auditMetrics.sinceNewGame.probes))
      : 0.0;
    const auto entriesPerGame = auditMetrics.finishedGames
      ? static_cast<double>(auditMetrics.sessionEntriesWritten)
          / static_cast<double>(auditMetrics.finishedGames)
      : static_cast<double>(auditMetrics.sessionEntriesWritten);

    std::ostringstream oss;
    oss << "Experience audit: timing scope=lifetime_since_enable_or_reload load_count="
        << auditMetrics.loadCount << " load_total_us=" << auditMetrics.loadMicros
        << " load_avg_us=" << std::fixed << std::setprecision(1) << avgLoad
        << " save_count=" << auditMetrics.saveCount << " save_total_us="
        << auditMetrics.saveMicros << " save_avg_us=" << avgSave << "\n"
        << "Experience audit: counters lifetime probes=" << auditMetrics.lifetime.probes
        << " hits=" << auditMetrics.lifetime.hits << " filtered_hits="
        << auditMetrics.lifetime.filteredHits << " min_visits_rejected="
        << auditMetrics.lifetime.minVisitsRejected << " root_reorders="
        << auditMetrics.lifetime.rootReorders << " entries_written="
        << auditMetrics.sessionEntriesWritten << " hit_rate=" << lifetimeProbeHitRate
        << "% filtered_hit_rate=" << lifetimeFilteredHitRate << "% finished_games="
        << auditMetrics.finishedGames << " entries_per_game=" << entriesPerGame << "\n"
        << "Experience audit: counters since_newgame probes=" << auditMetrics.sinceNewGame.probes
        << " hits=" << auditMetrics.sinceNewGame.hits << " filtered_hits="
        << auditMetrics.sinceNewGame.filteredHits << " min_visits_rejected="
        << auditMetrics.sinceNewGame.minVisitsRejected << " root_reorders="
        << auditMetrics.sinceNewGame.rootReorders << " entries_written="
        << auditMetrics.currentGameEntriesWritten << " hit_rate=" << newgameProbeHitRate
        << "% filtered_hit_rate=" << newgameFilteredHitRate
        << "% reset=ucinewgame_or_enable_disable_reload\n"
        << "Experience audit: reset_note lifetime counters reset when audit is enabled, disabled, or the experience file is reloaded; since_newgame counters reset on ucinewgame/reset and on those same lifecycle events.\n"
        << auditMetrics.scoreDistribution.to_string("score_abs_cp") << "\n"
        << auditMetrics.depthDistribution.to_string("depth") << "\n"
        << auditMetrics.visitsDistribution.to_string("visits");
    return oss.str();
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

void save_table_unlocked(const std::string& file) {
    if (settings.readonly || file.empty())
        return;

    ensure_directory_exists(file);

    const auto saveStart = std::chrono::steady_clock::now();

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out)
        return;

    out << "# Revolution experience format v1\n";
    for (const auto key : lru)
    {
        const auto it = table.find(key);
        if (it == table.end())
            continue;

        for (const auto& e : it->second)
        {
            if (!e.bestMove)
                continue;

            out << key << ' ' << unsigned(e.bestMove.raw()) << ' ' << int(e.score) << ' '
                << int(e.eval) << ' ' << int(e.depth) << ' ' << unsigned(e.visits) << '\n';
        }
    }

    if (settings.auditEnabled)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - saveStart);
        ++auditMetrics.saveCount;
        auditMetrics.saveMicros += static_cast<std::uint64_t>(elapsed.count());
    }
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
    const auto loadStart = std::chrono::steady_clock::now();

    table.clear();
    lru.clear();
    lruIndex.clear();
    dirty        = false;
    pendingFlush = 0;
    stats        = {};
    lastStatus.clear();
    reset_audit_metrics_unlocked();
    reset_since_newgame_counters_unlocked();

    if (file.empty())
        return;

    std::ifstream in(file, std::ios::binary);
    if (!in)
        return;

    Stats loadStats;
    bool  loaded = false;

    if (load_hypnos_binary(in, loadStats))
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
                                               Entry entry{move, score, eval, depth,
                                                           std::uint16_t(std::min<std::uint32_t>(
                                                             visits,
                                                             std::numeric_limits<std::uint16_t>::max()))};
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
    for (const auto& [loadedKey, loadedEntries] : table)
        for (const auto& entry : loadedEntries)
            record_entry_distributions(entry);
    enforce_limit();
    stats.totalPositions = table.size();
    if (settings.auditEnabled)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - loadStart);
        ++auditMetrics.loadCount;
        auditMetrics.loadMicros += static_cast<std::uint64_t>(elapsed.count());
    }
    lastStatus           = format_status_unlocked();
}

void flush_unlocked() {
    if (!dirty)
        return;

    if (settings.readonly)
    {
        dirty        = false;
        pendingFlush = 0;
        return;
    }

    save_table_unlocked(settings.file);
    dirty        = false;
    pendingFlush = 0;
}

}  // namespace

std::optional<std::string> update_settings(const OptionsMap& options) {
    Settings newSettings = settings;

    if (options.count("Experience Enabled"))
        newSettings.enabled = bool(int(options["Experience Enabled"]));
    else
        newSettings.enabled = true;

    if (options.count("Experience File"))
        newSettings.file = std::string(options["Experience File"]);
    else if (newSettings.file.empty())
        newSettings.file = "experience.exp";

    if (options.count("Experience Readonly"))
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

    if (options.count("Experience Book Min Visits"))
        newSettings.bookMinVisits = std::max(1, int(options["Experience Book Min Visits"]));
    else
        newSettings.bookMinVisits = 1;

    newSettings.minScore = Value(newSettings.bookEvalImportance * 4);

    if (options.count("Experience Audit Enabled"))
        newSettings.auditEnabled = bool(int(options["Experience Audit Enabled"]));
    else
        newSettings.auditEnabled = false;

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
        pendingFlush = 0;
        stats        = {};
        lastStatus   = format_status_unlocked();
        reset_audit_metrics_unlocked();
        reset_since_newgame_counters_unlocked();
        settingsInitialized = true;
        return lastStatus;
    }

    if (!settings.auditEnabled)
        reset_audit_metrics_unlocked();

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

    if (settings.auditEnabled)
    {
        ++auditMetrics.lifetime.probes;
        ++auditMetrics.sinceNewGame.probes;
    }

    const auto key = compute_key(pos);
    auto       it  = table.find(key);
    if (it == table.end())
        return;

    touch_lru(key);
    if (settings.auditEnabled)
    {
        ++auditMetrics.lifetime.hits;
        ++auditMetrics.sinceNewGame.hits;
    }

    const auto& entries = it->second;

    std::vector<const Entry*> candidates;
    candidates.reserve(entries.size());

    for (const auto& entry : entries)
    {
        if (!entry.bestMove)
            continue;

        if (entry.depth < settings.bookMinDepth)
            continue;

        if (entry.visits < settings.bookMinVisits)
        {
            if (settings.auditEnabled)
            {
                ++auditMetrics.lifetime.minVisitsRejected;
                ++auditMetrics.sinceNewGame.minVisitsRejected;
            }
            continue;
        }

        if (std::abs(entry.score) < settings.minScore)
            continue;

        candidates.push_back(&entry);
    }

    if (candidates.empty())
        return;

    if (settings.auditEnabled)
    {
        ++auditMetrics.lifetime.filteredHits;
        ++auditMetrics.sinceNewGame.filteredHits;
    }

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
    {
        std::rotate(rootMoves.begin(), bestIt, std::next(bestIt));
        if (settings.auditEnabled)
        {
            ++auditMetrics.lifetime.rootReorders;
            ++auditMetrics.sinceNewGame.rootReorders;
        }
    }

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

    if (searchedDepth < settings.bookMinDepth)
        return;


    if (settings.readonly)
        return;

    const auto key = compute_key(pos);
    auto&       entries = table[key];
    const bool  wasEmpty = entries.empty();

    const Move move = rootMoves.front().pv[0];

    Entry* existing = find_entry(entries, move);
    if (existing)
    {
        existing->score = bestScore;
        existing->eval  = evalScore;
        existing->depth = std::max(existing->depth, searchedDepth);
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
        entry.visits   = 1;

        entries.push_back(entry);
        ++stats.uniqueMoves;
        record_entry_distributions(entry);
    }

    if (wasEmpty && !entries.empty())
        ++stats.totalPositions;

    touch_lru(key);
    enforce_limit();
    stats.totalPositions = table.size();

    if (settings.auditEnabled)
    {
        ++auditMetrics.sessionEntriesWritten;
        ++auditMetrics.currentGameEntriesWritten;
        ++auditMetrics.lifetime.entriesWritten;
        ++auditMetrics.sinceNewGame.entriesWritten;
        if (existing)
            record_entry_distributions(*existing);
    }

    dirty = true;
    if (++pendingFlush >= FlushInterval)
        flush_unlocked();

    lastStatus = format_status_unlocked();
}

void new_game() {
    std::scoped_lock lock(mutex);
    finalize_game_metrics_unlocked();
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

std::string audit_summary() {
    std::scoped_lock lock(mutex);
    return format_audit_summary_unlocked();
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
