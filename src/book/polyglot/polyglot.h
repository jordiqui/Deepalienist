#ifndef POLYGLOT_BOOK_H_INCLUDED
#define POLYGLOT_BOOK_H_INCLUDED

#include "../book.h"

namespace Stockfish {
namespace Book {
namespace Polyglot {
struct PolyglotBookMove;
Key Polyglot_key(const Position& pos);
class PolyglotBook: public Book {
   private:
    std::string    filename;
    unsigned char* bookData;
    size_t         bookDataLength;

   private:
    unsigned char* data() const;
    size_t         data_size() const;
    bool           has_data() const;
    size_t         total_entries() const;

    size_t find_first_pos(Key key) const;
    void   get_moves(const Position& pos, std::vector<PolyglotBookMove>& bookMoves) const;

   public:
    PolyglotBook();
    virtual ~PolyglotBook();

    PolyglotBook(const PolyglotBook&)            = delete;
    PolyglotBook& operator=(const PolyglotBook&) = delete;

    virtual std::string type() const;

    virtual void close();
    virtual bool open(const std::string& f);

    virtual Move probe(const Position& pos, size_t width, bool onlyGreen) const;
    virtual std::vector<CandidateMove> candidates(const Position& pos, bool onlyGreen) const;

    void show_moves(const Position& pos) const;
};
}  // namespace Polyglot
}  // namespace Book
}  // namespace Stockfish
#endif  // #ifndef POLYGLOT_BOOK_H_INCLUDED
