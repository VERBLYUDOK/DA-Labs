#include "huffman.hpp"
#include "lz77.hpp"

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <climits>

static void write_u16(std::ostream &out, uint16_t v) {
    char buf[2];
    buf[0] = static_cast<char>(v & 0xFF);
    buf[1] = static_cast<char>((v >> 8) & 0xFF);
    out.write(buf, 2);
    if (!out) throw std::runtime_error("Write failed: u16");
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
    if (!out) throw std::runtime_error("Write failed: u32");
}

static uint32_t read_u32(std::istream &in) {
    char buf[4];
    in.read(buf, 4);
    if (!in) throw std::runtime_error("Unexpected EOF while reading u32");
    return (static_cast<uint32_t>(static_cast<unsigned char>(buf[0]))) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[1])) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[2])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[3])) << 24);
}

static void write_u64(std::ostream &out, uint64_t v) {
    char buf[8];
    for (int i = 0; i < 8; ++i) buf[i] = static_cast<char>((v >> (8*i)) & 0xFF);
    out.write(buf, 8);
    if (!out) throw std::runtime_error("Write failed: u64");
}

static uint64_t read_u64(std::istream &in) {
    char buf[8];
    in.read(buf, 8);
    if (!in) throw std::runtime_error("Unexpected EOF while reading u64");
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(static_cast<unsigned char>(buf[i])) << (8*i));
    return v;
}

// BitWriter/BitReader — полностью согласованы с utils.cpp
class BitWriter {
    std::vector<unsigned char>& out_;
    unsigned char cur_;
    int bits_filled_;
    uint64_t total_bits_ = 0;

public:
    explicit BitWriter(std::vector<unsigned char>& out) : out_(out), cur_(0), bits_filled_(0) {}

    void write_bit(int bit) {
        cur_ = static_cast<unsigned char>((cur_ << 1) | (bit & 1));
        bits_filled_++;
        total_bits_++;
        if (bits_filled_ == 8) {
            out_.push_back(cur_);
            bits_filled_ = 0;
            cur_ = 0;
        }
    }

    void write_bits(uint32_t value, int count) {
        if (count < 0 || count > 32) throw std::runtime_error("Invalid bit count in write_bits");
        for (int i = count - 1; i >= 0; --i) {
            write_bit((value >> i) & 1);
        }
    }

    void write_byte(unsigned char b) {
        write_bits(b, 8);
    }

    void write_u16(uint16_t v) {
        write_bits(v, 16);
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

struct Node {
    uint64_t freq;
    int symbol;
    Node* left;
    Node* right;
    Node(uint64_t f, int s) : freq(f), symbol(s), left(nullptr), right(nullptr) {}
    Node(uint64_t f, Node* l, Node* r) : freq(f), symbol(-1), left(l), right(r) {}
};

struct NodeCmp {
    bool operator()(const Node* a, const Node* b) const {
        if (a->freq != b->freq) return a->freq > b->freq;
        return a->symbol > b->symbol;
    }
};

static void collect_code_lengths(Node* root, std::unordered_map<int,int>& out_lengths) {
    if (!root) return;
    std::vector<std::pair<Node*, int>> stack;
    stack.emplace_back(root, 0);
    while (!stack.empty()) {
        auto [node, depth] = stack.back();
        stack.pop_back();
        if (node->symbol != -1) {
            out_lengths[node->symbol] = depth > 0 ? depth : 1;
            continue;
        }
        if (node->right) stack.emplace_back(node->right, depth + 1);
        if (node->left) stack.emplace_back(node->left, depth + 1);
    }
}

static void free_tree(Node* root) {
    if (!root) return;
    std::vector<Node*> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        if (node->left) stack.push_back(node->left);
        if (node->right) stack.push_back(node->right);
        delete node;
    }
}

namespace huffman {

void encode_symbols(const std::vector<uint32_t>& symbols, std::ostream& out) {
    // std::cout << "[DEBUG] Entering encode_symbols, symbols.size() = " << symbols.size() << std::endl;

    if (symbols.empty()) {
        write_u32(out, 0);
        write_u64(out, 0);
        return;
    }

    std::unordered_map<uint32_t, uint64_t> freq;
    freq.reserve(std::min(symbols.size(), size_t(1024)));
    for (uint32_t s : symbols) ++freq[s];

    std::vector<uint32_t> symbols_list;
    symbols_list.reserve(freq.size());
    for (const auto& kv : freq) symbols_list.push_back(kv.first);
    std::sort(symbols_list.begin(), symbols_list.end());

    std::unordered_map<uint32_t, int> sym_to_idx;
    sym_to_idx.reserve(symbols_list.size());
    std::priority_queue<Node*, std::vector<Node*>, NodeCmp> pq;
    for (size_t i = 0; i < symbols_list.size(); ++i) {
        uint32_t sym = symbols_list[i];
        sym_to_idx[sym] = static_cast<int>(i);
        pq.push(new Node(freq[sym], static_cast<int>(i)));
    }

    // std::cout << "[DEBUG] Built priority queue, size = " << pq.size() << std::endl;

    if (freq.size() == 1) {
        // Специальный случай: один уникальный символ
        uint32_t sym = symbols_list[0];
        uint64_t total = symbols.size();

        write_u32(out, 1);
        write_u32(out, sym);
        write_u16(out, 1);  // length = 1
        write_u64(out, total);

        std::vector<unsigned char> bitstream;
        BitWriter bw(bitstream);
        for (uint64_t i = 0; i < total; ++i) {
            bw.write_bit(0);
        }
        bw.flush();

        write_u64(out, bw.total_bits());
        out.write(reinterpret_cast<const char*>(bitstream.data()), bitstream.size());
        return;
    }

    while (pq.size() > 1) {
        Node* a = pq.top(); pq.pop();
        Node* b = pq.top(); pq.pop();
        pq.push(new Node(a->freq + b->freq, a, b));
    }
    Node* root = pq.top();
    pq.pop();

    // std::cout << "[DEBUG] Tree built" << std::endl;

    std::unordered_map<int, int> idx_lengths;
    collect_code_lengths(root, idx_lengths);

    uint16_t max_len = 0;
    std::vector<std::tuple<uint32_t, uint16_t, int>> symlens;
    symlens.reserve(symbols_list.size());
    for (size_t i = 0; i < symbols_list.size(); ++i) {
        int idx = static_cast<int>(i);
        int l = idx_lengths.count(idx) ? idx_lengths[idx] : 1;
        if (l <= 0 || l > 32) throw std::runtime_error("Invalid code length");
        symlens.emplace_back(symbols_list[i], static_cast<uint16_t>(l), idx);
        if (l > max_len) max_len = static_cast<uint16_t>(l);
    }

    // std::cout << "[DEBUG] unique symbols = " << freq.size() << ", max_len = " << max_len << std::endl;

    std::vector<int> bl_count(max_len + 1, 0);
    for (const auto& [sym, len, idx] : symlens) ++bl_count[len];

    std::vector<uint32_t> next_code(max_len + 1, 0);
    uint32_t code = 0;
    for (uint16_t bits = 1; bits <= max_len; ++bits) {
        code = (code + (bits > 1 ? bl_count[bits-1] : 0)) << 1;
        next_code[bits] = code;
    }

    std::sort(symlens.begin(), symlens.end(), [](const auto& a, const auto& b) {
        if (std::get<1>(a) != std::get<1>(b)) return std::get<1>(a) < std::get<1>(b);
        return std::get<0>(a) < std::get<0>(b);
    });

    struct CodeInfo { uint32_t symbol; uint32_t code; uint16_t len; };
    std::vector<CodeInfo> codes;
    codes.reserve(symlens.size());
    for (const auto& [sym, len, idx] : symlens) {
        uint32_t c = next_code[len]++;
        codes.push_back({sym, c, len});
    }

    // std::cout << "[DEBUG] Codes assigned" << std::endl;

    write_u32(out, static_cast<uint32_t>(codes.size()));
    for (const auto& ci : codes) {
        write_u32(out, ci.symbol);
        write_u16(out, ci.len);
    }
    write_u64(out, static_cast<uint64_t>(symbols.size()));

    std::unordered_map<uint32_t, std::pair<uint32_t, uint16_t>> sym_to_code;
    sym_to_code.reserve(codes.size());
    for (const auto& ci : codes) {
        sym_to_code[ci.symbol] = {ci.code, ci.len};
    }

    // std::cout << "[DEBUG] Starting bitstream encoding" << std::endl;

    std::vector<unsigned char> bitstream;
    BitWriter bw(bitstream);

    for (uint32_t s : symbols) {
        auto it = sym_to_code.find(s);
        if (it == sym_to_code.end()) throw std::runtime_error("Symbol not in codebook");
        bw.write_bits(it->second.first, it->second.second);
    }
    bw.flush();

    // std::cout << "[DEBUG] Bitstream encoded, size = " << bitstream.size() << " bytes" << std::endl;

    uint64_t valid_bits = bw.total_bits();
    write_u64(out, valid_bits);

    // Записываем битстрим
    out.write(reinterpret_cast<const char*>(bitstream.data()), bitstream.size());

    free_tree(root);
    // std::cout << "[DEBUG] Huffman encoding completed" << std::endl;
}

std::vector<uint32_t> decode_symbols(std::istream& in) {
    uint32_t unique_count = read_u32(in);
    if (unique_count == 0) {
        read_u64(in);
        return {};
    }

    struct SL { uint32_t symbol; uint16_t len; };
    std::vector<SL> table(unique_count);
    uint16_t max_len = 0;
    for (uint32_t i = 0; i < unique_count; ++i) {
        table[i].symbol = read_u32(in);
        table[i].len = read_u16(in);
        if (table[i].len == 0) throw std::runtime_error("Zero code length");
        if (table[i].len > max_len) max_len = table[i].len;
    }

    if (max_len > 32) throw std::runtime_error("Max code length too large");

    uint64_t total_symbols = read_u64(in);

    std::sort(table.begin(), table.end(), [](const SL& a, const SL& b) {
        if (a.len != b.len) return a.len < b.len;
        return a.symbol < b.symbol;
    });

    std::vector<uint32_t> bl_count(max_len + 1, 0);
    for (const auto& e : table) ++bl_count[e.len];

    std::vector<uint32_t> next_code(max_len + 1, 0);
    uint32_t code = 0;
    for (uint16_t bits = 1; bits <= max_len; ++bits) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    struct CodeInfo { uint32_t symbol; uint32_t code; uint16_t len; };
    std::vector<CodeInfo> codes;
    codes.reserve(table.size());
    for (const auto& e : table) {
        uint32_t c = next_code[e.len]++;
        codes.push_back({e.symbol, c, e.len});
    }

    std::vector<std::unordered_map<uint32_t, uint32_t>> code_to_sym(max_len + 1);
    for (const auto& ci : codes) {
        auto& map = code_to_sym[ci.len];
        if (map.count(ci.code)) throw std::runtime_error("Duplicate canonical code");
        map[ci.code] = ci.symbol;
    }

    uint64_t valid_bits = read_u64(in);

    // вычисляем, сколько байт нужно считать
    uint64_t byte_count = (valid_bits + 7) / 8;

    std::vector<unsigned char> bitstream;
    if (byte_count > 0) {
        bitstream.resize(static_cast<size_t>(byte_count));
        in.read(reinterpret_cast<char*>(bitstream.data()), static_cast<std::streamsize>(byte_count));
        if (!in) throw std::runtime_error("Failed to read huffman bitstream bytes");
    }

    BitReader br(bitstream.data(), bitstream.size(), valid_bits);

    std::vector<uint32_t> out;
    out.reserve(total_symbols);

    uint32_t cur_code = 0;
    uint16_t cur_len = 0;

    for (uint64_t i = 0; i < total_symbols; ++i) {
        while (true) {
            int b = br.read_bit();
            if (b == -1) throw std::runtime_error("Unexpected EOF in bitstream");
            cur_code = (cur_code << 1) | static_cast<uint32_t>(b);
            ++cur_len;

            if (cur_len <= max_len) {
                const auto& map = code_to_sym[cur_len];
                auto it = map.find(cur_code);
                if (it != map.end()) {
                    out.push_back(it->second);
                    cur_code = 0;
                    cur_len = 0;
                    break;
                }
            } else {
                throw std::runtime_error("Code longer than max_len");
            }
        }
    }

    return out;
}

} // namespace huffman