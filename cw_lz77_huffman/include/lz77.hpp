#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Token {
    int offset;
    int length;
    char ch;
    bool has_char;

    bool operator==(const Token &o) const {
        return offset==o.offset && length==o.length && has_char==o.has_char && (!has_char || ch==o.ch);
    }
};

std::vector<Token> lz77_compress_sa(const std::string &s);

std::string lz77_decompress(const std::vector<Token> &tokens);
