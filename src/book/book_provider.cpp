#include "book_provider.h"

namespace Stockfish {

LocalBookProvider::LocalBookProvider(BookManager& book_manager_,
                                     const OptionsMap&        options_) :
    bookManager(book_manager_),
    options(options_) {}

std::optional<Move> LocalBookProvider::pick_move(const Position& pos, const int ply_from_root) {
    if (!options.count("CTG/BIN Book 1 File"))
        return std::nullopt;

    const Move move = bookManager.probe(pos, options, ply_from_root);
    if (move == Move::none())
        return std::nullopt;

    return move;
}

#ifdef USE_LIVEBOOK
LiveBookProvider::LiveBookProvider(LiveBookManager& manager_, const OptionsMap& options_) :
    liveBookManager(manager_),
    options(options_) {}

std::optional<Move> LiveBookProvider::pick_move(const Position& pos, const int ply_from_root) {
    (void) options;
    return liveBookManager.pick_live_move(pos, pos.side_to_move(), ply_from_root, nullptr);
}
#else
std::optional<Move> LiveBookProvider::pick_move(const Position&, const int) { return std::nullopt; }
#endif

std::optional<Move> SearchFallback::pick_move(const Position&, const int) { return std::nullopt; }

#ifdef USE_LIVEBOOK
BookProviderChain::BookProviderChain(BookManager& book_manager,
                                     LiveBookManager&         livebook_manager,
                                     const OptionsMap&        options) :
    localBookProvider(book_manager, options),
    liveBookProvider(livebook_manager, options) {}

std::optional<Move> BookProviderChain::pick_move(const Position& pos, const int ply_from_root) {
    if (auto move = localBookProvider.pick_move(pos, ply_from_root))
        return move;
    if (auto move = liveBookProvider.pick_move(pos, ply_from_root))
        return move;
    return searchFallback.pick_move(pos, ply_from_root);
}
#endif

}  // namespace Stockfish
