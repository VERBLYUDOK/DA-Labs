#include "lz77.hpp"
#include "utils.hpp"
#include "huffman.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <system_error>

// Сжатие LZ77 (level==1)
void compress_stream_lz77(std::istream &in, std::ostream &out) {
    std::string data = read_all(in);
    auto tokens = lz77_compress_sa(data);
    write_tokens_binary(out, tokens, false);
}

// Сжатие LZ77 + Huffman (level==9)
void compress_stream_lz77_huffman(std::istream &in, std::ostream &out) {
    std::string data = read_all(in);
    auto tokens = lz77_compress_sa(data);
    // utils::write_tokens_binary сам вызовет Huffman
    write_tokens_binary(out, tokens, true);
}

void decompress_stream_lz77(std::istream &in, std::ostream &out) {
    auto tokens = read_tokens_binary(in);
    std::string s = lz77_decompress(tokens);
    write_all(out, s);
}

// Читает первые 8 байт заголовка (magic "LZ77", version, level, reserved(2))
// Возвращает уровень (1 или 9). Курсор потока продвинут ПОСЛЕ заголовка
static int read_header_level(std::istream &in) {
    char magic[4];
    in.read(magic, 4);
    if (!in) throw std::runtime_error("Failed to read archive header (magic)");
    if (std::memcmp(magic, "LZ77", 4) != 0) throw std::runtime_error("Not an LZ77 archive");
    int version = in.get();
    if (version == EOF) throw std::runtime_error("Truncated header");
    int level = in.get();
    if (level == EOF) throw std::runtime_error("Truncated header");
    // skip reserved 2 bytes
    in.get(); in.get();
    (void)version;
    return level;
}

// Печать краткой информации об архиве (аналог -l)
// Для файла path == "-" читаем из stdin (будем буферизовать)
void list_archive_info(const std::string &path) {
    try {
        std::vector<Token> tokens;
        uint64_t compressed_size = 0;
        int level = -1;

        if (path == "-") {
            // читаем весь stdin в память (иначе нельзя переоткрыть)
            std::string raw = read_all(std::cin);
            compressed_size = static_cast<uint64_t>(raw.size());
            std::istringstream iss(raw, std::ios::binary);
            // определяем уровень
            level = read_header_level(iss);
            // вернемся в начало
            iss.clear();
            iss.seekg(0);
            tokens = read_tokens_binary(iss);
        } else {
            std::ifstream fin(path, std::ios::binary);
            if (!fin.is_open()) {
                std::cerr << "list: failed to open " << path << "\n";
                return;
            }
            // получить уровень (и оставить поток на месте)
            level = read_header_level(fin);
            fin.clear();
            fin.seekg(0);
            tokens = read_tokens_binary(fin);
            std::error_code ec;
            compressed_size = std::filesystem::file_size(path, ec);
            if (ec) compressed_size = 0;
        }

        // оценка распакованного размера: распакуем токены и возьмем длину строки
        std::string decompressed = lz77_decompress(tokens);
        uint64_t uncompressed_size = static_cast<uint64_t>(decompressed.size());

        std::cout << path << ":\n";
        std::cout << "  archive level: " << level << "\n";
        std::cout << "  token count: " << tokens.size() << "\n";
        std::cout << "  uncompressed size: " << uncompressed_size << " bytes\n";
        if (compressed_size != 0) {
            std::cout << "  compressed size: " << compressed_size << " bytes\n";
            double ratio = 100.0 * (1.0 - (double)compressed_size / (double)uncompressed_size);
            std::cout << "  compression ratio: " << ratio << "%\n";
        } else {
            std::cout << "  compressed size: (unknown for stdin)\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "list: failed for " << path << ": " << e.what() << "\n";
    }
}

// Проверка архива (-t) - пытаемся прочитать и распаковать, ошибки означают неверный архив
void test_archive(const std::string &path) {
    try {
        if (path == "-") {
            std::string raw = read_all(std::cin);
            std::istringstream iss(raw, std::ios::binary);
            auto tokens = read_tokens_binary(iss);
            // пробуем распаковать (без записи)
            std::string dec = lz77_decompress(tokens);
            (void)dec;
        } else {
            std::ifstream fin(path, std::ios::binary);
            if (!fin.is_open()) {
                std::cerr << "test: failed to open " << path << "\n";
                return;
            }
            auto tokens = read_tokens_binary(fin);
            std::string dec = lz77_decompress(tokens);
            (void)dec;
        }
        std::cout << path << ": OK\n";
    } catch (const std::exception &e) {
        std::cout << path << ": CORRUPT (" << e.what() << ")\n";
    }
}
