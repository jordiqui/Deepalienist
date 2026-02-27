#ifndef BOOK_UTIL_H_INCLUDED
#define BOOK_UTIL_H_INCLUDED

#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>

namespace Stockfish::Util {

inline std::string format_string(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const int len = std::vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    std::string out(size_t(len), '\0');
    va_start(args, fmt);
    std::vsnprintf(out.data(), out.size() + 1, fmt, args);
    va_end(args);
    return out;
}

inline bool is_empty_filename(const std::string& name) { return name.empty(); }

inline std::string map_path(const std::string& p) { return p; }

inline bool is_same_file(const std::string& a, const std::string& b) {
    std::error_code ec;
    return std::filesystem::equivalent(a, b, ec);
}

inline uint64_t get_file_size(const std::string& p) {
    std::error_code ec;
    return std::filesystem::file_size(p, ec);
}

inline std::string format_bytes(uint64_t bytes, int) { return std::to_string(bytes) + " bytes"; }

}  // namespace Stockfish::Util

#endif
