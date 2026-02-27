#ifndef LEARN_H_INCLUDED
#define LEARN_H_INCLUDED

#include <unordered_map>
#include <vector>
#include "../types.h"
#include "../ucioption.h"
#include "../position.h"

namespace Stockfish {

enum class LearningMode {
    Off      = 1,
    Standard = 2,
    Self     = 3,
};

struct LearningMove {
    Stockfish::Depth depth       = 0;
    Stockfish::Value score       = Stockfish::VALUE_NONE;
    Stockfish::Move  move        = Stockfish::Move::none();
    int               performance = 100;
};

struct PersistedLearningMove {
    Stockfish::Key key{};
    LearningMove    learningMove;
};

struct QLearningMove {
    PersistedLearningMove persistedLearningMove;
    int                   materialClamp;
};

class LearningData {
    bool         isPaused;
    bool         isReadOnly;
    bool         needPersisting;
    LearningMode learningMode;
    bool         loaded;
    bool         experienceEnabled;
    Stockfish::OptionsMap* optionsRef;

    std::unordered_multimap<Stockfish::Key, LearningMove*> HT;
    std::vector<void*>                                      mainDataBuffers;
    std::vector<void*>                                      newMovesDataBuffers;
    bool                                                    load(const std::string& filename);
    void insert_or_update(PersistedLearningMove* plm, bool qLearning);

   public:
    LearningData();
    ~LearningData();

    void               pause();
    void               resume();
    [[nodiscard]] bool is_paused() const { return isPaused; };

    void quick_reset_exp();
    void set_learning_mode(Stockfish::OptionsMap& options, const std::string& lm);
    [[nodiscard]] LearningMode learning_mode() const;
    [[nodiscard]] bool         is_enabled() const { return learningMode != LearningMode::Off; }

    void               set_readonly(bool ro) { isReadOnly = ro; }
    [[nodiscard]] bool is_readonly() const { return isReadOnly; }

    void clear();
    void init(Stockfish::OptionsMap& o);
    void ensure_loaded();
    bool persist(const Stockfish::OptionsMap& o);
    [[nodiscard]] bool experience_enabled() const { return experienceEnabled; }
    [[nodiscard]] const Stockfish::OptionsMap* options_ref() const { return optionsRef; }

    void add_new_learning(Stockfish::Key key, const LearningMove& lm);

    int probeByMaxDepthAndScore(Stockfish::Key key, const LearningMove*& learningMove);
    const LearningMove*        probe_move(Stockfish::Key key, Stockfish::Move move);
    std::vector<LearningMove*> probe(Stockfish::Key key);
    static void                sortLearningMoves(std::vector<LearningMove*>& learningMoves);
    static void                show_exp(const Stockfish::Position& pos);
};

extern LearningData LD;

namespace Experience {
void flush_to_disk(const char* reason);
}

}  // namespace Stockfish

#endif  // #ifndef LEARN_H_INCLUDED
