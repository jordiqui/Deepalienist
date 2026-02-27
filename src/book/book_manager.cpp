#include "../uci.h"

#include <iomanip>
#include "polyglot/polyglot.h"
#include "ctg/ctg.h"
#include "book_manager.h"

namespace Stockfish {
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
    Move bookMove = Move::none();

    for (size_t i = 0; i < NumberOfBooks; ++i)
    {
        const int depth = int(options[Util::format_string("Book %d Depth", i + 1)]);
        const bool depthOk = depth == 0 || bookPly < depth;
        if (books[i] != nullptr && depthOk)
        {
            bookMove = books[i]->probe(
              pos, size_t(int(options[Util::format_string("Book %d Width", i + 1)])),
              bool(options[Util::format_string("(CTG) Book %d Only Green", i + 1)]));
            if (bookMove != Move::none())
                break;
        }
    }

    return bookMove;
}

void BookManager::show_moves(const Position& pos, const OptionsMap& options) const {
    std::cout << pos << std::endl << std::endl;
    std::cout << "FEN: " << pos.fen() << std::endl;
    std::cout << "Polyglot key: 0x" << std::hex << std::setw(16) << std::setfill('0')
              << Book::Polyglot::Polyglot_key(pos) << std::dec << std::setfill(' ') << std::endl;

    bool anyBook = false;
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

        anyBook = true;
        if (books[i] == nullptr)
        {
            std::cout << "Book " << i + 1 << ": Failed to load (" << fileName << ")" << std::endl;
            continue;
        }

        std::cout << "Book " << i + 1 << " (" << books[i]->type() << "): " << fileName
                  << std::endl;
        books[i]->show_moves(pos);

        if (!anyMove)
        {
            auto move = books[i]->probe(
              pos, size_t(int(options[Util::format_string("Book %d Width", i + 1)])),
              bool(options[Util::format_string("(CTG) Book %d Only Green", i + 1)]));
            anyMove = move != Move::none();
        }
    }

    if (!anyBook || !anyMove)
        std::cout << "No book moves" << std::endl;
}
}
