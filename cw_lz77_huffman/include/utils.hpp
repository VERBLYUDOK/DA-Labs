#pragma once
#include "lz77.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <istream>
#include <ostream>
#include <fstream>

std::string read_all(std::istream &in);
void write_all(std::ostream &out, const std::string &s);

// Сериализация/десериализация токенов
// Если use_huffman == true, то записывается level==9 и Huffman-блок
// В противном случае - старый бинарный фиксированный блок (level==1)
void write_tokens_binary(std::ostream &out, const std::vector<Token> &tokens, bool use_huffman = false);
std::vector<Token> read_tokens_binary(std::istream &in);

// Открытие потоков (поддержка "-" = stdin/stdout)
std::istream* open_input_stream(const std::string &path, std::ifstream &file_holder);
std::ostream* open_output_stream(const std::string &path, std::ofstream &file_holder);
