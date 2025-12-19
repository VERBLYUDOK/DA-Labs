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
}
static uint16_t read_u16(std::istream &in) {
    char buf[2];
    in.read(buf, 2);
    if (!in) throw std::runtime_error("Unexpected EOF reading u16");
    return static_cast<uint16_t>((static_cast<unsigned char>(buf[0])) |
                                 (static_cast<unsigned char>(buf[1]) << 8));
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
    if (!in) throw std::runtime_error("Unexpected EOF reading u32");
    return (static_cast<uint32_t>(static_cast<unsigned char>(buf[0]))      ) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[1])) << 8 ) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[2])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(buf[3])) << 24);
}
static void write_u64(std::ostream &out, uint64_t v) {
    char buf[8];
    for (int i = 0; i < 8; ++i) buf[i] = static_cast<char>((v >> (8*i)) & 0xFF);
    out.write(buf, 8);
}
static uint64_t read_u64(std::istream &in) {
    char buf[8];
    in.read(buf, 8);
    if (!in) throw std::runtime_error("Unexpected EOF reading u64");
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(static_cast<unsigned char>(buf[i])) << (8*i));
    return v;
}

class BitWriter {
    std::ostream &out_;
    unsigned char cur_;
    int bits_filled_;
public:
    explicit BitWriter(std::ostream &out) : out_(out), cur_(0), bits_filled_(0) {}
    ~BitWriter() { flush(); }
    void write_bit(int bit) {
        cur_ = static_cast<unsigned char>((cur_ << 1) | (bit & 1));
        bits_filled_++;
        if (bits_filled_ == 8) {
            out_.put(static_cast<char>(cur_));
            bits_filled_ = 0;
            cur_ = 0;
        }
    }
    
    void write_bits(uint32_t value, int count) {
        for (int i = count - 1; i >= 0; --i) {
            int b = (value >> i) & 1;
            write_bit(b);
        }
    }
    void flush() {
        if (bits_filled_ > 0) {
            unsigned char padded = static_cast<unsigned char>(cur_ << (8 - bits_filled_));
            out_.put(static_cast<char>(padded));
            bits_filled_ = 0;
            cur_ = 0;
        }
    }
};

class BitReader {
    std::istream &in_;
    unsigned char cur_;
    int bits_left_;
public:
    explicit BitReader(std::istream &in) : in_(in), cur_(0), bits_left_(0) {}
    int read_bit() {
        if (bits_left_ == 0) {
            int c = in_.get();
            if (c == EOF) return -1;
            cur_ = static_cast<unsigned char>(c);
            bits_left_ = 8;
        }
        int bit = (cur_ >> (bits_left_ - 1)) & 1;
        bits_left_--;
        return bit;
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

static void collect_code_lengths(Node* root, std::unordered_map<int,int>& out_lengths, int depth=0) {
    if (!root) return;
    if (root->symbol != -1) {
        out_lengths[root->symbol] = depth > 0 ? depth : 1;
        return;
    }
    collect_code_lengths(root->left, out_lengths, depth + 1);
    collect_code_lengths(root->right, out_lengths, depth + 1);
}

static void free_tree(Node* n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    delete n;
}

namespace huffman {

void encode_symbols(const std::vector<uint32_t>& symbols, std::ostream& out) {
    // count frequencies
    std::unordered_map<uint32_t, uint64_t> freq;
    freq.reserve(1024);
    for (uint32_t s : symbols) freq[s]++;

    // build nodes
    std::priority_queue<Node*, std::vector<Node*>, NodeCmp> pq;
    for (const auto &kv : freq) {
        // symbol fits into int? We assume uint32_t values are within 0..2^31-1; cast to int could overflow for >2^31-1
        // To be safe, we will remap symbols to indices if needed. BUT for simplicity, assume symbols fit signed int range.
        // To be robust: we will store mapping from uint32_t -> int_index below.
    }

    std::vector<uint32_t> symbols_list;
    symbols_list.reserve(freq.size());
    for (auto &kv : freq) symbols_list.push_back(kv.first);
    std::sort(symbols_list.begin(), symbols_list.end());

    std::unordered_map<uint32_t,int> sym_to_idx;
    sym_to_idx.reserve(symbols_list.size());
    for (size_t i = 0; i < symbols_list.size(); ++i) {
        sym_to_idx[symbols_list[i]] = static_cast<int>(i);
        Node* nd = new Node(freq[symbols_list[i]], static_cast<int>(i));
        pq.push(nd);
    }

    if (pq.empty()) {
        // nothing to encode: write empty table and return
        write_u32(out, 0); // unique_count = 0
        write_u64(out, 0); // symbols count = 0
        return;
    }

    while (pq.size() > 1) {
        Node* a = pq.top(); pq.pop();
        Node* b = pq.top(); pq.pop();
        Node* c = new Node(a->freq + b->freq, a, b);
        pq.push(c);
    }
    Node* root = pq.top();

    std::unordered_map<int,int> idx_lengths;
    collect_code_lengths(root, idx_lengths, 0);

    struct SymLen { uint32_t symbol; uint16_t len; int idx; };
    std::vector<SymLen> symlens;
    symlens.reserve(symbols_list.size());
    uint16_t max_len = 0;
    for (size_t i = 0; i < symbols_list.size(); ++i) {
        int idx = static_cast<int>(i);
        int l = 1;
        auto it = idx_lengths.find(idx);
        if (it != idx_lengths.end()) l = it->second;
        if (l <= 0) l = 1;
        if (l > SHRT_MAX) throw std::runtime_error("Code length too large");
        symlens.push_back({ symbols_list[i], static_cast<uint16_t>(l), idx });
        if (l > max_len) max_len = static_cast<uint16_t>(l);
    }

    std::vector<int> bl_count(max_len + 1);
    for (auto &sl : symlens) bl_count[sl.len]++;

    // canonical codes: compute next_code for each length
    std::vector<uint32_t> next_code(max_len + 1);
    uint32_t code = 0;
    for (uint16_t bits = 1; bits <= max_len; ++bits) {
        code = (code + (bits > 1 ? static_cast<uint32_t>(bl_count[bits-1]) : 0)) << 1;
        next_code[bits] = code;
    }

    // sort symbols by (len, symbol) as canonical requires consistent ordering
    std::sort(symlens.begin(), symlens.end(), [](const SymLen& a, const SymLen& b){
        if (a.len != b.len) return a.len < b.len;
        return a.symbol < b.symbol;
    });

    // assign canonical codes (store code values)
    struct CodeInfo { uint32_t symbol; uint32_t code; uint16_t len; };
    std::vector<CodeInfo> codes;
    codes.reserve(symlens.size());
    for (auto &sl : symlens) {
        uint16_t L = sl.len;
        uint32_t c = next_code[L]++;
        codes.push_back({ sl.symbol, c, L });
    }

    // Now write Huffman table:
    // unique_count (u32)
    write_u32(out, static_cast<uint32_t>(codes.size()));
    // For each: symbol (u32) + code_length (u16)
    for (auto &ci : codes) {
        write_u32(out, ci.symbol);
        write_u16(out, ci.len);
    }

    // Write total count of symbols encoded (u64)
    write_u64(out, static_cast<uint64_t>(symbols.size()));

    // Build map symbol -> (code,len) for fast encoding
    std::unordered_map<uint32_t, std::pair<uint32_t,uint16_t>> sym_to_code;
    sym_to_code.reserve(codes.size());
    for (auto &ci : codes) sym_to_code[ci.symbol] = { ci.code, ci.len };

    // Encode stream
    BitWriter bw(out);
    for (uint32_t s : symbols) {
        auto it = sym_to_code.find(s);
        if (it == sym_to_code.end()) throw std::runtime_error("Symbol not in codebook");
        uint32_t codev = it->second.first;
        uint16_t len = it->second.second;
        // write 'len' bits of codev, MSB-first
        // Note: canonical codes assigned as integer where code fits in 'len' bits with MSB being highest bit.
        // We write bits MSB-first by shifting.
        bw.write_bits(codev, len);
    }
    bw.flush();

    free_tree(root);
}

std::vector<uint32_t> decode_symbols(std::istream& in) {
    // read table
    uint32_t unique_count = read_u32(in);
    if (unique_count == 0) {
        uint64_t zero = read_u64(in);
        (void)zero;
        return {};
    }
    struct SL { uint32_t symbol; uint16_t len; };
    std::vector<SL> table;
    table.reserve(unique_count);
    uint16_t max_len = 0;
    for (uint32_t i = 0; i < unique_count; ++i) {
        uint32_t sym = read_u32(in);
        uint16_t len = read_u16(in);
        table.push_back({sym, len});
        if (len > max_len) max_len = len;
    }
    uint64_t total_symbols = read_u64(in);

    // Build canonical codes: sort by (len, symbol)
    std::sort(table.begin(), table.end(), [](const SL& a, const SL& b){
        if (a.len != b.len) return a.len < b.len;
        return a.symbol < b.symbol;
    });

    std::vector<uint32_t> bl_count(max_len + 1);
    for (auto &e : table) bl_count[e.len]++;

    std::vector<uint32_t> next_code(max_len + 1);
    uint32_t code = 0;
    for (uint16_t bits = 1; bits <= max_len; ++bits) {
        code = (code + (bits > 1 ? bl_count[bits-1] : 0)) << 1;
        next_code[bits] = code;
    }

    struct CI { uint32_t code; uint16_t len; uint32_t symbol; };
    std::vector<CI> codes;
    codes.reserve(table.size());
    for (auto &e : table) {
        uint16_t L = e.len;
        uint32_t c = next_code[L]++;
        codes.push_back({c, L, e.symbol});
    }

    std::vector<std::unordered_map<uint32_t,uint32_t>> code_to_sym(max_len + 1);
    for (auto &ci : codes) {
        code_to_sym[ci.len][ci.code] = ci.symbol;
    }

    BitReader br(in);
    std::vector<uint32_t> out;
    out.reserve(static_cast<size_t>(std::min<uint64_t>(total_symbols, 1024)));
    uint32_t cur_code = 0;
    uint16_t cur_len = 0;

    for (uint64_t i = 0; i < total_symbols; ++i) {
        // read until we find a match
        while (true) {
            int b = br.read_bit();
            if (b == -1) throw std::runtime_error("Unexpected EOF in bitstream");
            cur_code = (cur_code << 1) | static_cast<uint32_t>(b);
            cur_len++;
            if (cur_len <= max_len) {
                auto it = code_to_sym[cur_len].find(cur_code);
                if (it != code_to_sym[cur_len].end()) {
                    out.push_back(it->second);
                    // reset
                    cur_code = 0;
                    cur_len = 0;
                    break;
                }
            }
            if (cur_len > max_len) throw std::runtime_error("Decoded code longer than max_len");
        }
    }

    return out;
}

}
