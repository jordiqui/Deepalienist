#include "../uci.h"
#include "util.h"

#include <algorithm>
#include <iomanip>
#include <random>
#include "polyglot/polyglot.h"
#include "ctg/ctg.h"
#include "book_manager.h"

namespace Stockfish {
namespace {
bool depth_ok(const OptionsMap& options, size_t i, int bookPly) {
    const int depth = int(options[Util::format_string("Book %d Depth", i + 1)]);
    return bookPly <= depth;
}

bool is_type(const Book::Book* book, const char* expected) {
    return book != nullptr && book->type() == expected;
}

std::mt19937_64 make_engine(const Position& pos) {
    std::seed_seq seeds = {uint32_t(pos.key()), uint32_t(pos.key() >> 32)};
    return std::mt19937_64(seeds);
}

Move select_from_candidates(const Position& pos,
                            std::vector<Book::Book::CandidateMove>& candidates,
                            size_t width) {
    if (candidates.empty())
        return Move::none();

    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.weight > b.weight;
    });

    while (candidates.size() > width)
        candidates.pop_back();

    size_t selected = 0;
    if (candidates.size() > 1)
    {
        auto randomEngine = make_engine(pos);
        std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
        selected = dist(randomEngine);
    }

    return candidates[selected].move;
}
}

BookManager::BookManager() {
    for (int i = 0; i < NumberOfBooks; ++i)
        books[i] = nullptr;
}

BookManager::~BookManager() {
    for (int i = 0; i < NumberOfBooks; ++i)
        delete books[i];
}

void BookManager::init(const OptionsMap& options) {
    for (size_t i = 0; i < NumberOfBooks; ++i)
        init(i, options);
}

void BookManager::init(int index, const OptionsMap& options) {
    assert(index < NumberOfBooks);

    //Close previous book if any
    delete books[index];
    books[index] = nullptr;

    std::string filename =
      std::string(options[Util::format_string("CTG/BIN Book %d File", index + 1)]);

    //Load new book
    if (Util::is_empty_filename(filename))
        return;

    //Create book object for the given book type
    std::string fn   = Util::map_path(filename);
    Book::Book* book = Book::Book::create_book(fn);
    if (book == nullptr)
    {
        sync_cout << "info string Unknown book type: " << filename << sync_endl;
        return;
    }

    //Open/Initialize the book
    if (!book->open(fn))
    {
        delete book;
        return;
    }

    books[index] = book;
}

Move BookManager::probe(const Position& pos, const OptionsMap& options, int bookPly) const {
    std::vector<Book::Book::CandidateMove> merged;
    size_t                                 maxWidth = 1;

    const auto gather_for_type = [&](const char* typeName, bool onlyCtgGreen) {
        merged.clear();
        maxWidth = 1;

        for (size_t i = 0; i < NumberOfBooks; ++i)
        {
            if (!depth_ok(options, i, bookPly) || !is_type(books[i], typeName))
                continue;

            const size_t width = size_t(int(options[Util::format_string("Book %d Width", i + 1)]));
            const bool onlyGreen = onlyCtgGreen
              && bool(options[Util::format_string("(CTG) Book %d Only Green", i + 1)]);
            auto local = books[i]->candidates(pos, onlyGreen);
            while (local.size() > width)
                local.pop_back();
            merged.insert(merged.end(), local.begin(), local.end());
            maxWidth = std::max(maxWidth, width);
        }

        return select_from_candidates(pos, merged, maxWidth);
    };

    if (Move m = gather_for_type("bin", false); m != Move::none())
        return m;

    if (Move m = gather_for_type("ctg", true); m != Move::none())
        return m;

    return Move::none();
}

void BookManager::show_moves(const Position& pos, const OptionsMap& options) const {
    std::cout << pos << std::endl << std::endl;
    std::cout << "FEN: " << pos.fen() << std::endl;
    std::cout << "Polyglot key: 0x" << std::hex << std::setw(16) << std::setfill('0')
              << Book::Polyglot::Polyglot_key(pos) << std::dec << std::setfill(' ') << std::endl;

    bool anyMove = false;

    for (size_t i = 0; i < NumberOfBooks; ++i)
    {
        const auto optionName = Util::format_string("CTG/BIN Book %d File", i + 1);
        const auto fileName   = std::string(options[optionName]);

        if (Util::is_empty_filename(fileName))
        {
            std::cout << "Book " << i + 1 << ": Disabled" << std::endl;
            continue;
        }

        if (books[i] == nullptr)
        {
            std::cout << "Book " << i + 1 << ": Failed to load (" << fileName << ")" << std::endl;
            continue;
        }

        std::cout << "Book " << i + 1 << " (" << books[i]->type() << "): " << fileName
                  << std::endl;
        books[i]->show_moves(pos);

        const bool onlyGreen = bool(options[Util::format_string("(CTG) Book %d Only Green", i + 1)]);
        anyMove = anyMove || !books[i]->candidates(pos, onlyGreen).empty();
    }

    if (!anyMove)
        std::cout << "no book moves" << std::endl;
}
}
