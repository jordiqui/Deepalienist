#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <unordered_set>
#include "../misc.h"
#include "learn.h"
#include <algorithm>
#include <cstdint>
#include "../uci.h"
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

namespace Stockfish {

LearningData LD;

namespace {
int win_probability_from_score(Value score) {
    const int centered = 50 + score / 8;
    return std::clamp(centered, 0, 100);
}

LearningMode identify_learning_mode(const string& lm) {
    if (lm == "Off")
        return LearningMode::Off;

    if (lm == "Standard")
        return LearningMode::Standard;

    return LearningMode::Self;
}

std::filesystem::path experience_directory() {
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD        size = 0;

    while (true)
    {
        size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size == 0)
            break;
        if (size < buffer.size())
        {
            buffer.resize(size);
            std::filesystem::path exePath(buffer);
            return std::filesystem::absolute(exePath.parent_path());
        }
        buffer.resize(buffer.size() * 2);
    }
#endif
    return std::filesystem::current_path();
}

std::filesystem::path experience_path(const std::string& filename) {
    return experience_directory() / filename;
}

std::string current_experience_file_name(const Stockfish::OptionsMap* options) {
    if (!options)
        return "Deepalienist.exp";

    const std::string configured = static_cast<std::string>((*options)["Experience File"]);
    return configured.empty() ? "Deepalienist.exp" : configured;
}

bool atomic_rename(const std::filesystem::path& source, const std::filesystem::path& dest) {
#ifdef _WIN32
    const std::wstring sourceW = source.wstring();
    const std::wstring destW   = dest.wstring();
    return MoveFileExW(sourceW.c_str(), destW.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(source, dest, ec);
    return !ec;
#endif
}
}

bool LearningData::load(const string& filename) {
    ifstream in(filename, ios::in | ios::binary);

    //Quick exit if file is not present
    if (!in.is_open())
        return false;

    //Get experience file size
    in.seekg(0, ios::end);
    const size_t fileSize = in.tellg();

    //File size should be a multiple of 'PersistedLearningMove'
    if (fileSize % sizeof(PersistedLearningMove))
    {
        cerr << "info string The file <" << filename << "> with size <" << fileSize
             << "> is not a valid experience file" << endl;
        return false;
    }

    //Allocate buffer to read the entire file
    void* fileData = malloc(fileSize);
    if (!fileData)
    {
        cerr << "info string Failed to allocate <" << fileSize
             << "> bytes to read experience file <" << filename << ">" << endl;
        return false;
    }

    //Read the entire file
    in.seekg(0, ios::beg);  //Move read pointer to the beginning of the file
    in.read(static_cast<char*>(fileData), static_cast<std::streamsize>(fileSize));
    if (!in)
    {
        free(fileData);

        cerr << "info string Failed to read <" << fileSize << "> bytes from experience file <"
             << filename << ">" << endl;
        return false;
    }

    //Close the input data file
    in.close();

    //Save pointer to fileData to be freed later
    mainDataBuffers.push_back(fileData);

    //Loop the moves from this file
    const bool qLearning             = learningMode == LearningMode::Self;
    auto*      persistedLearningMove = static_cast<PersistedLearningMove*>(fileData);
    do
    {
        insert_or_update(persistedLearningMove, qLearning);
        ++persistedLearningMove;
    } while (reinterpret_cast<size_t>(persistedLearningMove)
             < reinterpret_cast<size_t>(fileData) + fileSize);

    return true;
}

inline bool should_update(const LearningMove existing_move, const LearningMove learning_move) {
    if (learning_move.depth > existing_move.depth)
    {
        return true;
    }

    if (learning_move.depth < existing_move.depth)
    {
        return false;
    }

    if (learning_move.score != existing_move.score)
    {
        return true;
    }

    return learning_move.performance != existing_move.performance;
}

void LearningData::insert_or_update(PersistedLearningMove* plm, bool qLearning) {
    // We search in the range of all the hash table entries with plm key
    const auto [first, second] = HT.equal_range(plm->key);

    //If the plm key belongs to a position that did not exist before in the 'LHT'
    //then, we insert this new key and LearningMove and return
    if (first == second)
    {
        //Insert new key and learningMove
        HT.insert({plm->key, &plm->learningMove});

        //Flag for persisting
        needPersisting = true;

        //Nothing else to do
        return;
    }

    //The plm key belongs to a position already existing in the LHT
    //Check if this move already exists for this position
    const auto itr = find_if(
      first, second, [&plm](const auto& p) { return p.second->move == plm->learningMove.move; });

    //If the move does not exist then insert it
    LearningMove* bestNewMoveCandidate = nullptr;
    if (itr == second)
    {
        HT.insert({plm->key, &plm->learningMove});
        bestNewMoveCandidate = &plm->learningMove;

        //Flag for persisting
        needPersisting = true;
    }
    else  //If the move exists, check if it better than the move we already have
    {
        LearningMove* existingMove = itr->second;
        if (should_update(*existingMove, plm->learningMove))
        {
            //Replace the existing move
            *existingMove = plm->learningMove;

            //Since an existing move was replaced, check the best move again
            bestNewMoveCandidate = existingMove;

            //Flag for persisting
            needPersisting = true;
        }
    }

    //Do we have a candidate for new best move?
    if (bestNewMoveCandidate != nullptr)
    {
        bool          newBestMove     = false;
        LearningMove* currentBestMove = first->second;
        if (bestNewMoveCandidate != currentBestMove)
        {
            if (qLearning)
            {
                if (bestNewMoveCandidate->score > currentBestMove->score)
                {
                    newBestMove = true;
                }
            }
            else
            {
                if ((currentBestMove->depth < bestNewMoveCandidate->depth)
                    || (currentBestMove->depth == bestNewMoveCandidate->depth
                        && currentBestMove->score <= bestNewMoveCandidate->score))
                {
                    newBestMove = true;
                }
            }
        }

        if (newBestMove)
        {
            //Boring and slow, but I can't think of an alternative at the moment
            //This is not thread safe, but it is fine since this code will never be called from multiple threads
            static LearningMove lm;

            lm                    = *bestNewMoveCandidate;
            *bestNewMoveCandidate = *currentBestMove;
            *currentBestMove      = lm;

            //Flag for persisting
            needPersisting = true;
        }
    }
}

LearningData::LearningData() :
    isPaused(false),
    isReadOnly(false),
    needPersisting(false),
    learningMode(LearningMode::Standard),
    loaded(false),
    experienceEnabled(false),
    optionsRef(nullptr) {}

LearningData::~LearningData() { clear(); }

void LearningData::clear() {
    //Clear hash table
    HT.clear();

    //Release internal data buffers
    for (void* p : mainDataBuffers)
        free(p);

    //Clear internal data buffers
    mainDataBuffers.clear();

    //Release internal new moves data buffers
    for (void* p : newMovesDataBuffers)
        free(p);

    //Clear internal new moves data buffers
    newMovesDataBuffers.clear();
}

void LearningData::init(Stockfish::OptionsMap& o) {
    OptionsMap& options = o;
    clear();
    optionsRef = &options;
    const bool readOnlyLearning      = static_cast<bool>(options["Read only learning"]);
    const bool experienceEnabledOpt  = static_cast<bool>(options["Experience Book"]);
    const bool experienceGlobalEnabled = static_cast<bool>(options["Experience"]);

    isReadOnly              = readOnlyLearning;
    loaded                  = false;
    this->experienceEnabled = experienceGlobalEnabled && experienceEnabledOpt;
    learningMode = static_cast<bool>(options["Self Q-learning"]) ? LearningMode::Self
                                                                   : LearningMode::Standard;

    ensure_loaded();
}

void LearningData::ensure_loaded() {
    if (loaded)
        return;

    if (!optionsRef)
    {
        loaded = true;
        return;
    }

    const bool readOnlyLearning      = static_cast<bool>((*optionsRef)["Read only learning"]);
    const bool experienceEnabledOpt  = static_cast<bool>((*optionsRef)["Experience Book"]);
    const bool experienceGlobalEnabled = static_cast<bool>((*optionsRef)["Experience"]);

    isReadOnly              = readOnlyLearning;
    this->experienceEnabled = experienceGlobalEnabled && experienceEnabledOpt;

    if (!experienceGlobalEnabled)
    {
        loaded = true;
        needPersisting = false;
        return;
    }

    const string experienceFile = experience_file_name();

    load(experience_path(experienceFile).string());

    vector<string> slaveFiles;

    // Just in case, check and load for "<Experience File>.tmp" which will be present if
    // previous saving operation failed (engine crashed or terminated)
    string slaveFile = experienceFile + ".tmp";
    if (load(experience_path(slaveFile).string()))
        slaveFiles.push_back(slaveFile);

    //We need to write all consolidated experience to disk
    loaded = true;

    if (!slaveFiles.empty() && optionsRef)
        persist(*optionsRef);

    //Remove slave files
    for (const string& fn : slaveFiles)
    {
        remove(experience_path(fn).string().c_str());
    }

    // Clear the 'needPersisting' flag
    needPersisting = false;
}

void LearningData::quick_reset_exp() {
    const std::string experienceFile = experience_file_name();
    std::cout << "Loading experience file: " << experienceFile << std::endl;

    std::ifstream file(experience_path(experienceFile), std::ifstream::binary | std::ifstream::ate);
    if (!file)
    {
        std::cerr << "Failed to load experience file" << std::endl;
        return;
    }

    const std::streamsize  file_size     = file.tellg();
    constexpr unsigned int entry_size    = 24;
    const unsigned int     total_entries = file_size / entry_size;

    file.close();

    std::cout << "Total entries in the file: " << total_entries << std::endl;

    if (const auto check = load(experience_path(experienceFile).string()); !check)
    {
        std::cerr << "Failed to load experience file" << std::endl;
        return;
    }

    std::cout << "Successfully loaded experience file" << std::endl;

    int entry_count = 0;
    for (auto& [key, learning_move] : HT)
    {
        entry_count++;

        const auto old_performance = learning_move->performance;
        const auto new_performance =
          win_probability_from_score(learning_move->score);

        std::cout << "Updating entry " << entry_count << "/" << total_entries << " Key " << key
                  << "Value " << learning_move->score << "Depth " << learning_move->depth
                  << ": old performance=" << static_cast<int>(old_performance)
                  << ", new performance=" << static_cast<int>(new_performance) << std::endl;

        learning_move->performance = new_performance;
    }

    needPersisting = true;
    std::cout << "Finished updating performances. Total processed entries: " << entry_count
              << std::endl;
}


void LearningData::set_learning_mode(Stockfish::OptionsMap& options, const string& lm) {
    LearningMode newLearningMode = identify_learning_mode(lm);
    if (newLearningMode == learningMode)
        return;

    init(options);
}

LearningMode LearningData::learning_mode() const { return learningMode; }


size_t LearningData::total_positions() const {
    std::unordered_set<Key> uniqueKeys;
    uniqueKeys.reserve(HT.size());
    for (const auto& [key, _] : HT)
        uniqueKeys.insert(key);

    return uniqueKeys.size();
}

std::string LearningData::experience_file_name() const {
    return current_experience_file_name(optionsRef);
}

bool LearningData::persist(const Stockfish::OptionsMap&) {
    ensure_loaded();

    if (optionsRef && !static_cast<bool>((*optionsRef)["Experience"]))
        return false;

    if (isReadOnly)
    {
        return false;
    }

    /*
        To avoid any problems when saving to experience file, we will actually do the following:
        1) Save new experience to "experience.exp.tmp"
        2) Rename "experience.exp.tmp" to "experience.exp" atomically

        This approach is fail proof so that the old file is only replaced when the new file is successfully saved!
        If, for whatever odd reason, the engine fails after step (1), we will load "experience.exp.tmp" on restart.
    */

    const auto experienceFilename     = experience_path(experience_file_name());
    auto       tempExperienceFilename = experienceFilename;
    tempExperienceFilename += ".tmp";
    const bool experienceExists = std::filesystem::exists(experienceFilename);

    if (HT.empty() || !needPersisting)
    {
        if (!experienceExists)
        {
            ofstream outputFile(tempExperienceFilename, ofstream::trunc | ofstream::binary);
            outputFile.close();
            atomic_rename(tempExperienceFilename, experienceFilename);
            return true;
        }
        return false;
    }

    ofstream              outputFile(tempExperienceFilename, ofstream::trunc | ofstream::binary);
    PersistedLearningMove persistedLearningMove;
    for (auto& kvp : HT)
    {
        persistedLearningMove.key          = kvp.first;
        persistedLearningMove.learningMove = *kvp.second;
        if (persistedLearningMove.learningMove.depth != 0)
        {
            outputFile.write(reinterpret_cast<char*>(&persistedLearningMove),
                             sizeof(persistedLearningMove));
        }
    }
    outputFile.close();

    atomic_rename(tempExperienceFilename, experienceFilename);

    //Prevent persisting again without modifications
    needPersisting = false;
    return true;
}

void LearningData::pause() { isPaused = true; }

void LearningData::resume() { isPaused = false; }

void LearningData::add_new_learning(Key key, const LearningMove& lm) {
    if (optionsRef && !static_cast<bool>((*optionsRef)["Experience"]))
        return;

    //Allocate buffer to read the entire file
    auto* newPlm = static_cast<PersistedLearningMove*>(malloc(sizeof(PersistedLearningMove)));
    if (!newPlm)
    {
        cerr << "info string Failed to allocate <" << sizeof(PersistedLearningMove)
             << "> bytes for new learning entry" << endl;
        return;
    }

    //Save pointer to fileData to be freed later
    newMovesDataBuffers.push_back(newPlm);

    //Assign
    newPlm->key          = key;
    newPlm->learningMove = lm;

    //Add to HT
    insert_or_update(newPlm, learningMode == LearningMode::Self);
}

int LearningData::probeByMaxDepthAndScore(Key key, const LearningMove*& learningMove) {
    const LearningMove* maxDepthMove = nullptr;
    int                 maxDepth     = -1;
    int                 maxScore     = -1;

    // Iterate through the range of elements with the given key
    auto range = HT.equal_range(key);
    if (range.first == range.second)
    {
        learningMove = nullptr;
        return 0;
    }
    const auto siblings = distance(range.first, range.second);
    for (auto it = range.first; it != range.second; ++it)
    {
        LearningMove* move = it->second;

        // Check if the current move has a greater depth than the maximum depth found so far
        if (move->depth > maxDepth)
        {
            maxDepth     = move->depth;
            maxScore     = move->score;
            maxDepthMove = move;
        }
        // If the current move has the same depth as the maximum depth found so far,
        // check if it has a greater score
        else if (move->depth == maxDepth && move->score > maxScore)
        {
            maxScore     = move->score;
            maxDepthMove = move;
        }
    }

    // Return the reference to the LearningMove with the maximum depth and score (or nullptr if not found)
    learningMove = maxDepthMove;

    return static_cast<int>(siblings);
}

const LearningMove* LearningData::probe_move(Key key, Move move) {
    auto range = HT.equal_range(key);

    if (range.first == range.second)
        return nullptr;

    auto itr =
      find_if(range.first, range.second, [&move](const auto& p) { return p.second->move == move; });

    if (itr == range.second)
        return nullptr;

    return itr->second;
}


void LearningData::sortLearningMoves(std::vector<LearningMove*>& learningMoves) {
    std::sort(learningMoves.begin(), learningMoves.end(), [](const LearningMove* a, const LearningMove* b) {
        if (a->performance != b->performance)
            return a->performance > b->performance;
        if (a->score != b->score)
            return a->score > b->score;
        return a->depth > b->depth;
    });
}

vector<LearningMove*> LearningData::probe(Stockfish::Key key) {
    vector<LearningMove*> result;
    auto                  range = HT.equal_range(key);
    for (auto it = range.first; it != range.second; ++it)
    {
        result.push_back(it->second);
    }

    return result;
}
void LearningData::show_exp(const Position& pos) {
    const auto* options = LD.options_ref();
    if (options && !static_cast<bool>((*options)["Experience"]))
    {
        sync_cout << "info string Experience disabled" << sync_endl;
        return;
    }

    sync_cout << pos << endl;
    cout << "Experience:";
    vector<LearningMove*> learningMoves = LD.probe(pos.key());
    if (learningMoves.empty())
    {
        cout << " No experience data found for this position" << sync_endl;
        return;
    }

    const int minDepth = options ? static_cast<int>((*options)["Experience Book Min Depth"]) : 1;
    const int maxMoves = options ? static_cast<int>((*options)["Experience Book Max Moves"]) : 100;

    learningMoves.erase(remove_if(learningMoves.begin(), learningMoves.end(),
                                  [minDepth](const LearningMove* lm) { return lm->depth < minDepth; }),
                        learningMoves.end());

    if (learningMoves.empty())
    {
        cout << " No entries satisfy current Experience Book Min Depth" << sync_endl;
        return;
    }

    sortLearningMoves(learningMoves);
    const size_t limit = min(static_cast<size_t>(maxMoves), learningMoves.size());

    cout << endl;
    for (size_t i = 0; i < limit; ++i)
    {
        const auto& move = learningMoves[i];
        const int   winProb = move->performance;
        cout << "move: " << UCIEngine::move(move->move, pos.is_chess960()) << " winprob: " << winProb
             << " score: " << move->score << " depth: " << move->depth
             << " performance: " << move->performance << endl;
    }
    cout << sync_endl;
}

namespace Experience {
std::string stats_line() {
    const auto* options = LD.options_ref();
    if (options && !static_cast<bool>((*options)["Experience"]))
        return "Experience disabled";

    const auto fileName = LD.experience_file_name();
    if (fileName.empty())
        return "No experience file loaded";

    LD.ensure_loaded();

    const auto totalMoves      = LD.total_moves();
    const auto totalPositions  = LD.total_positions();
    const auto duplicateMoves  = totalMoves > totalPositions ? totalMoves - totalPositions : 0;
    const double fragmentation = totalMoves == 0 ? 0.0 : (100.0 * duplicateMoves) / totalMoves;

    std::ostringstream oss;
    oss << fileName << " -> Total moves: " << totalMoves << ". Total positions: "
        << totalPositions << ". Duplicate moves: " << duplicateMoves << ". Fragmentation: "
        << std::fixed << std::setprecision(2) << fragmentation << "%";
    return oss.str();
}

void flush_to_disk(const char* reason) {
    const auto* options = LD.options_ref();
    if (!options)
        return;

    if (!static_cast<bool>((*options)["Experience"]))
    {
        sync_cout << "info string experience: disabled reason=" << reason << sync_endl;
        return;
    }

    if (static_cast<bool>((*options)["Read only learning"]))
        return;

    const bool wrote = LD.persist(*options);
    const auto message =
      std::string("experience: flushed reason=") + reason + " wrote=" + (wrote ? "1" : "0");
    sync_cout << "info string " << message << sync_endl;
}
}

}  // namespace Stockfish
