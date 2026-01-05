#include "utils.hpp"
#include "huffman.hpp"

#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdint>
#include <algorithm>
#include <iterator>

static void write_u16(std::ostream &out, uint16_t v) {
    char buf[2];
    buf[0] = static_cast<char>(v & 0xFF);
    buf[1] = static_cast<char>((v >> 8) & 0xFF);
    out.write(buf, 2);
}

static uint16_t read_u16(std::istream &in) {
    char buf[2];
    in.read(buf, 2);
    if (!in) throw std::runtime_error("Unexpected EOF while reading u16");
    return static_cast<uint16_t>(static_cast<unsigned char>(buf[0])) |
           (static_cast<uint16_t>(static_cast<unsigned char>(buf[1])) << 8);
}
static void write_u32(std::ostream &out, uint32_t v) {
    char buf[4];
    buf[0] = static_cast<char>(v & 0xFF);
    buf[1] = static_cast<char>((v >> 8) & 0xFF);
    buf[2] = static_cast<char>((v >> 16) & 0xFF);
    buf[3] = static_cast<char>((v >> 24) & 0xFF);
    out.write(buf, 4);
}
static uint32_t read_u32(std::istream &in) {
    char buf[4];
    in.read(buf, 4);
    if (!in) throw std::runtime_error("Unexpected EOF while reading u32");
    uint32_t v = (static_cast<unsigned char>(buf[0])      ) |
                 (static_cast<unsigned char>(buf[1]) << 8 ) |
                 (static_cast<unsigned char>(buf[2]) << 16) |
                 (static_cast<unsigned char>(buf[3]) << 24);
    return v;
}
static void write_u64(std::ostream &out, uint64_t v) {
    char buf[8];
    for (int i = 0; i < 8; ++i) buf[i] = static_cast<char>((v >> (8*i)) & 0xFF);
    out.write(buf, 8);
}
static uint64_t read_u64(std::istream &in) {
    char buf[8];
    in.read(buf, 8);
    if (!in) throw std::runtime_error("Unexpected EOF while reading u64");
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(static_cast<unsigned char>(buf[i])) << (8*i));
    return v;
}

std::string read_all(std::istream &in) {
    std::string s;
    std::istreambuf_iterator<char> it(in), end;
    s.assign(it, end);
    return s;
}

void write_all(std::ostream &out, const std::string &s) {
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
}

// Helper: convert tokens -> symbols (uint32_t)
// Format:
//  - literal token (offset==0 && length==0) -> push (uint32_t) (unsigned char)ch
//  - match token -> push MARKER (UINT32_MAX), then offset (u32), length (u32), has_char (0/1), if has_char push char (u32)
static std::vector<uint32_t> tokens_to_symbols(const std::vector<Token> &tokens) {
    std::vector<uint32_t> out;
    out.reserve(tokens.size() * 3);

    for (const Token &t : tokens) {
        if (t.offset == 0 && t.length == 0) {
            out.push_back(0u); // Маркер
            out.push_back(static_cast<uint32_t>(static_cast<unsigned char>(t.ch)));
        } else {
            // Матч: offset и length как uint16_t
            out.push_back(static_cast<uint32_t>(static_cast<uint16_t>(t.offset)));
            out.push_back(static_cast<uint32_t>(static_cast<uint16_t>(t.length)));
            out.push_back(t.has_char ? 1u : 0u);
            if (t.has_char) {
                out.push_back(static_cast<uint32_t>(static_cast<unsigned char>(t.ch)));
            }
        }
    }
    return out;
}

// inverse: symbols -> tokens. Assumes symbols are well-formed
static std::vector<Token> symbols_to_tokens(const std::vector<uint32_t> &symbols) {
    std::vector<Token> out;
    out.reserve(symbols.size() / 2);
    size_t i = 0;
    while (i < symbols.size()) {
        uint32_t off_val = symbols[i++];

        if (off_val == 0) {
            // Литерал: за 0 идёт символ
            if (i >= symbols.size()) throw std::runtime_error("Malformed: missing char after offset=0");
            uint32_t ch_val = symbols[i++];
            Token t{0, 0, static_cast<char>(ch_val & 0xFF), true};
            out.push_back(t);
        } else {
            // Матч: off_val = offset
            if (i + 1 >= symbols.size()) throw std::runtime_error("Malformed: missing length");
            uint32_t len_val = symbols[i++];
            if (i >= symbols.size()) throw std::runtime_error("Malformed: missing has_char");
            uint32_t has = symbols[i++];

            Token t;
            t.offset = static_cast<int>(static_cast<uint16_t>(off_val));
            t.length = static_cast<int>(static_cast<uint16_t>(len_val));
            t.has_char = (has != 0);
            if (t.has_char) {
                if (i >= symbols.size()) throw std::runtime_error("Malformed: missing char");
                uint32_t ch_val = symbols[i++];
                t.ch = static_cast<char>(ch_val & 0xFF);
            } else {
                t.ch = '\0';
            }
            out.push_back(t);
        }
    }
    return out;
}

// write tokens -> binary archive. If use_huffman==true, output level=9 with Huffman block
void write_tokens_binary(std::ostream &out, const std::vector<Token> &tokens, bool use_huffman) {
    // main header "LZ77" + version + level + reserved(2)
    out.write("LZ77", 4);
    char version = 1;
    char level = use_huffman ? 9 : 1;
    char reserved[2] = {0,0};
    out.put(version);
    out.put(level);
    out.write(reserved, 2);

    if (!use_huffman) {
        write_u64(out, static_cast<uint64_t>(tokens.size()));
        for (const Token &t : tokens) {
            write_u16(out, static_cast<uint16_t>(t.offset));  // 0 для литерала, > 0 для матча
            if (t.offset == 0) {
                out.put(t.ch);
            } else {
                // Матч
                write_u16(out, static_cast<uint16_t>(t.length));
                out.put(t.has_char ? 1 : 0);
                if (t.has_char) out.put(t.ch);
            }
        }
    } else {
        // Huffman mode: convert tokens -> symbols, then call huffman::encode_symbols
        auto symbols = tokens_to_symbols(tokens);
        // We will write the Huffman block directly. huffman::encode_symbols writes:
        // [unique_count][(symbol,code_len)*][symbols_bitstream],
        // but we also need to store total symbols count for decoding; encode_symbols writes that
        huffman::encode_symbols(symbols, out);
    }
}

// read tokens: autodetect level
std::vector<Token> read_tokens_binary(std::istream &in) {
    char magic[4];
    in.read(magic, 4);
    if (!in) throw std::runtime_error("Failed to read archive header");
    if (std::memcmp(magic, "LZ77", 4) != 0) throw std::runtime_error("Not an LZ77 archive");

    int version = in.get();
    int level = in.get();
    char reserved[2];
    in.read(reserved, 2);
    (void)version;
    (void)reserved;

    if (level == 1) {
        uint64_t count = read_u64(in);
        std::vector<Token> tokens;
        tokens.reserve(static_cast<size_t>(count));

        for (uint64_t i = 0; i < count; ++i) {
            uint16_t off = read_u16(in);

            Token t;
            t.offset = static_cast<int>(off);

            if (t.offset == 0) {
                // Литерал
                int c = in.get();
                if (c == EOF) throw std::runtime_error("Unexpected EOF reading literal char");
                t.length = 0;
                t.has_char = true;
                t.ch = static_cast<char>(c);
            } else {
                // Матч
                uint16_t len = read_u16(in);
                int has = in.get();
                if (has == EOF) throw std::runtime_error("Unexpected EOF reading has_char");

                t.length = static_cast<int>(len);
                t.has_char = (has != 0);
                if (t.has_char) {
                    int c = in.get();
                    if (c == EOF) throw std::runtime_error("Unexpected EOF reading match char");
                    t.ch = static_cast<char>(c);
                } else {
                    t.ch = '\0';
                }
            }
            tokens.push_back(t);
        }
        return tokens;
    } else if (level == 9) {
        // Huffman-coded symbol stream
        auto symbols = huffman::decode_symbols(in);
        auto tokens = symbols_to_tokens(symbols);
        return tokens;
    } else {
        throw std::runtime_error("Unsupported archive level");
    }
}

// open input: if path == "-" return &std::cin (caller must NOT delete), else open ifstream
std::istream* open_input_stream(const std::string &path, std::ifstream &file_holder) {
    if (path == "-") return &std::cin;
    file_holder.open(path, std::ios::binary);
    if (!file_holder.is_open()) return nullptr;
    return &file_holder;
}

std::ostream* open_output_stream(const std::string &path, std::ofstream &file_holder) {
    if (path == "-") return &std::cout;
    file_holder.open(path, std::ios::binary);
    if (!file_holder.is_open()) return nullptr;
    return &file_holder;
}
