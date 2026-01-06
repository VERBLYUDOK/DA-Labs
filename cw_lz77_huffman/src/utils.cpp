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
#include <array>
#include <sstream>

class BitWriter {
    // std::ostream &out_;
    std::vector<unsigned char>& out_;
    unsigned char cur_;
    int bits_filled_;
    uint64_t total_bits_;
public:
    explicit BitWriter(std::vector<unsigned char>& out) : out_(out), cur_(0), bits_filled_(0), total_bits_(0) {}
    ~BitWriter() = default;
    void write_bit(int bit) {
        cur_ = static_cast<unsigned char>((cur_ << 1) | (bit & 1));
        bits_filled_++;
        total_bits_++;
        if (bits_filled_ == 8) {
            out_.push_back(static_cast<unsigned char>(cur_));
            bits_filled_ = 0;
            cur_ = 0;
        }
    }
    void write_byte(int byte) {
        for (int i = 7; i >= 0; --i) {
            write_bit((byte >> i) & 1);
        }
    }
    void write_u16(uint16_t v) {
        write_byte((v >> 8) & 0xFF);
        write_byte(v & 0xFF);
    }
    void flush() {
        if (bits_filled_ > 0) {
            unsigned char padded = static_cast<unsigned char>(cur_ << (8 - bits_filled_));
            out_.push_back(padded);
            bits_filled_ = 0;
            cur_ = 0;
        }
    }
    uint64_t total_bits() const { return total_bits_; }
};

class BitReader {
    const unsigned char* data_;
    size_t byte_size_;
    uint64_t valid_bits_;
    unsigned char cur_ = 0;
    int bits_left_ = 0;
    uint64_t pos_ = 0;

public:
    BitReader(const unsigned char* data, size_t size, uint64_t valid_bits)
        : data_(data), byte_size_(size), valid_bits_(valid_bits) {}

    int read_bit() {
        if (pos_ >= valid_bits_) return -1;
        if (bits_left_ == 0) {
            size_t byte_idx = pos_ / 8;
            if (byte_idx >= byte_size_) return -1;
            cur_ = data_[byte_idx];
            bits_left_ = 8;
        }
        int bit = (cur_ >> (bits_left_ - 1)) & 1;
        bits_left_--;
        pos_++;
        return bit;
    }

    int read_byte() {
        int b = 0;
        for (int i = 0; i < 8; ++i) {
            int bit = read_bit();
            if (bit == -1) return -1;
            b = (b << 1) | bit;
        }
        return b;
    }

    uint16_t read_u16() {
        int high = read_byte();
        int low = read_byte();
        if (high == -1 || low == -1) throw std::runtime_error("Unexpected EOF reading u16");
        return static_cast<uint16_t>((high << 8) | low);
    }
};

namespace {
    std::array<uint32_t, 256> make_crc32_table() {
        std::array<uint32_t, 256> table{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c >> 1) ^ ((c & 1) ? 0xEDB88320u : 0);
            }
            table[i] = c;
        }
        return table;
    }
}

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc) {
    static const auto table = make_crc32_table();
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// Удобная обёртка от std::string
uint32_t crc32(const std::string& s) {
    return crc32(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

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
static std::vector<uint32_t> tokens_to_symbols(const std::vector<Token> &tokens) {
    std::vector<uint32_t> out;
    out.reserve(tokens.size() * 2);

    for (const Token &t : tokens) {
        if (t.offset == 0 && t.length == 0) {
            // Литерал как один символ: 256 + ch (чтобы отличать от offset)
            out.push_back(256u + static_cast<uint32_t>(static_cast<unsigned char>(t.ch)));
        } else {
            // Матч: offset, length, has_char, опционально 256 + ch
            out.push_back(static_cast<uint32_t>(static_cast<uint16_t>(t.offset)));
            out.push_back(static_cast<uint32_t>(static_cast<uint16_t>(t.length)));
            out.push_back(t.has_char ? 1u : 0u);
            if (t.has_char) {
                out.push_back(256u + static_cast<uint32_t>(static_cast<unsigned char>(t.ch)));
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
        uint32_t val = symbols[i++];

        if (val >= 256 && val < 512) {
            // Литерал
            Token t{0, 0, static_cast<char>(val - 256), true};
            out.push_back(t);
        } else {
            // Матч
            uint32_t offset = val;
            if (i >= symbols.size()) throw std::runtime_error("Malformed: missing length");
            uint32_t length = symbols[i++];
            if (i >= symbols.size()) throw std::runtime_error("Malformed: missing has_char");
            uint32_t has = symbols[i++];

            Token t;
            t.offset = static_cast<int>(offset);
            t.length = static_cast<int>(length);
            t.has_char = (has != 0);
            if (t.has_char) {
                if (i >= symbols.size()) throw std::runtime_error("Malformed: missing char");
                uint32_t ch_val = symbols[i++];
                if (ch_val < 256 || ch_val >= 512) throw std::runtime_error("Malformed: invalid char");
                t.ch = static_cast<char>(ch_val - 256);
            } else {
                t.ch = '\0';
            }
            out.push_back(t);
        }
    }
    return out;
}

// write tokens -> binary archive. If use_huffman==true, output level=9 with Huffman block
void write_tokens_binary(std::ostream &out, const std::vector<Token> &tokens, bool use_huffman, uint32_t data_crc) {
    // main header "LZ77" + version + level + reserved(2)
    out.write("LZ77", 4);
    char version = 1;
    char level = use_huffman ? 9 : 1;
    char reserved[2] = {0,0};
    out.put(version);
    out.put(level);
    // CRC32
    for (int i = 0; i < 4; ++i) {
        out.put(static_cast<char>(data_crc >> (i * 8)));
    }
    out.write(reserved, 2);

    // Общий бит-пакетный формат для токенов
    std::vector<unsigned char> packed_bytes;
    packed_bytes.reserve(tokens.size() * 4);  // оценка

    BitWriter bw(packed_bytes);

    // Записываем количество токенов как 64 бита
    for (int i = 63; i >= 0; --i) {
        bw.write_bit(static_cast<int>((tokens.size() >> i) & 1));
    }

    // Записываем токены
    for (const Token &t : tokens) {
        if (t.offset == 0 && t.length == 0) {
            bw.write_bit(0);  // флаг: литерал
            bw.write_byte(static_cast<unsigned char>(t.ch));
        } else {
            bw.write_bit(1);  // флаг: матч
            bw.write_u16(static_cast<uint16_t>(t.offset));
            bw.write_u16(static_cast<uint16_t>(t.length));
            bw.write_bit(t.has_char ? 1 : 0);
            if (t.has_char) {
                bw.write_byte(static_cast<unsigned char>(t.ch));
            }
        }
    }

    bw.flush();

    if (!use_huffman) {
        // Режим -1: пишем valid_bits и packed_bytes напрямую
        write_u64(out, bw.total_bits());
        out.write(reinterpret_cast<const char*>(packed_bytes.data()), packed_bytes.size());
    } else {
        write_u64(out, bw.total_bits());
        std::vector<uint32_t> symbols;
        symbols.reserve(packed_bytes.size());
        for (unsigned char b : packed_bytes) {
            symbols.push_back(static_cast<uint32_t>(b));
        }

        huffman::encode_symbols(symbols, out);

    }
}

// read tokens: autodetect level
std::pair<std::vector<Token>, uint32_t> read_tokens_binary(std::istream &in) {
    char magic[4];
    in.read(magic, 4);
    if (!in || std::memcmp(magic, "LZ77", 4) != 0) throw std::runtime_error("Invalid header");

    int version = in.get();
    int level = in.get();
    uint32_t stored_crc = 0;
    for (int i = 0; i < 4; ++i) {
        int c = in.get();
        if (c == EOF) throw std::runtime_error("Truncated header");
        stored_crc |= static_cast<uint32_t>(c) << (i * 8);
    }
    char reserved[2];
    in.read(reserved, 2);

    uint64_t valid_bits = read_u64(in);

    std::vector<unsigned char> packed_bytes;
    if (level == 1) {
        // Читаем raw биты
        uint64_t byte_count = (valid_bits + 7) / 8;
        packed_bytes.resize(byte_count);
        in.read(reinterpret_cast<char*>(packed_bytes.data()), byte_count);
        if (!in) throw std::runtime_error("Failed to read packed bytes");
    } else if (level == 9) {
        auto symbols = huffman::decode_symbols(in);

        packed_bytes.reserve(symbols.size());
        for (uint32_t s : symbols) {
            if (s > 255) throw std::runtime_error("Invalid byte in Huffman stream");
            packed_bytes.push_back(static_cast<unsigned char>(s));
        }
        // uint64_t valid_bits = read_u64(in);
    } else {
        throw std::runtime_error("Unsupported level");
    }

    // Теперь читаем токены из packed_bytes
    BitReader br(packed_bytes.data(), packed_bytes.size(), valid_bits);

    uint64_t count = 0;
    for (int i = 0; i < 64; ++i) {
        int bit = br.read_bit();
        if (bit == -1) throw std::runtime_error("EOF reading token count");
        count = (count << 1) | bit;
    }

    std::vector<Token> tokens;
    tokens.reserve(static_cast<size_t>(count));

    for (uint64_t i = 0; i < count; ++i) {
        int flag = br.read_bit();
        if (flag == -1) throw std::runtime_error("EOF reading token flag");

        Token t;
        if (flag == 0) {
            // Литерал
            int c = br.read_byte();
            if (c == -1) throw std::runtime_error("EOF reading literal char");
            t.offset = 0;
            t.length = 0;
            t.has_char = true;
            t.ch = static_cast<char>(c);
        } else {
            // Матч
            uint16_t off = br.read_u16();
            uint16_t len = br.read_u16();
            int has = br.read_bit();
            if (has == -1) throw std::runtime_error("EOF reading has_char");

            t.offset = static_cast<int>(off);
            t.length = static_cast<int>(len);
            t.has_char = (has != 0);
            if (t.has_char) {
                int c = br.read_byte();
                if (c == -1) throw std::runtime_error("EOF reading match char");
                t.ch = static_cast<char>(c);
            } else {
                t.ch = '\0';
            }
        }
        tokens.push_back(t);
    }

    return {tokens, stored_crc};
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
