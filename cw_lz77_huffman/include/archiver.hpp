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
void compress_stream_lz77(std::istream &in, std::ostream &out);

// Сжатие LZ77 + Huffman (level==9)
void compress_stream_lz77_huffman(std::istream &in, std::ostream &out);

void decompress_stream_lz77(std::istream &in, std::ostream &out);

// Читает первые 8 байт заголовка (magic "LZ77", version, level, reserved(2))
// Возвращает уровень (1 или 9). Курсор потока продвинут ПОСЛЕ заголовка
static int read_header_level(std::istream &in);

// Печать краткой информации об архиве (аналог -l)
// Для файла path == "-" читаем из stdin (будем буферизовать)
void list_archive_info(const std::string &path);

// Проверка архива (-t) - пытаемся прочитать и распаковать, ошибки означают неверный архив
void test_archive(const std::string &path);
