#ifndef BOOK_PROVIDER_H_INCLUDED
#define BOOK_PROVIDER_H_INCLUDED

#include <optional>

#include "../position.h"
#include "../ucioption.h"
#include "book_manager.h"
#ifdef USE_LIVEBOOK
#include "../livebook/LiveBookManager.h"
#endif

namespace Stockfish {

class BookProvider {
   public:
    virtual ~BookProvider() = default;
    virtual std::optional<Move> pick_move(const Position& pos, int ply_from_root) = 0;
};

class LocalBookProvider final: public BookProvider {
   public:
    LocalBookProvider(BookManager& book_manager_, const OptionsMap& options_);
    std::optional<Move> pick_move(const Position& pos, int ply_from_root) override;

   private:
    BookManager& bookManager;
    const OptionsMap&        options;
};

class LiveBookProvider final: public BookProvider {
   public:
#ifdef USE_LIVEBOOK
    LiveBookProvider(LiveBookManager& manager_, const OptionsMap& options_);
#else
    template <typename Manager>
    LiveBookProvider(Manager&, const OptionsMap& options_) :
        options(options_) {}
#endif
    std::optional<Move> pick_move(const Position& pos, int ply_from_root) override;

   private:
#ifdef USE_LIVEBOOK
    LiveBookManager& liveBookManager;
#endif
    [[maybe_unused]] const OptionsMap& options;
};

class SearchFallback final: public BookProvider {
   public:
    std::optional<Move> pick_move(const Position& pos, int ply_from_root) override;
};

#ifdef USE_LIVEBOOK
class BookProviderChain {
   public:
    BookProviderChain(BookManager& book_manager,
                      LiveBookManager&         livebook_manager,
                      const OptionsMap&        options);

    std::optional<Move> pick_move(const Position& pos, int ply_from_root);

   private:
    LocalBookProvider localBookProvider;
    LiveBookProvider  liveBookProvider;
    SearchFallback    searchFallback;
};
#endif

}  // namespace Stockfish

#endif  // BOOK_PROVIDER_H_INCLUDED
