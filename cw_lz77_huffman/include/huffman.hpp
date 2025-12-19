#pragma once
#include <vector>
#include <cstdint>
#include <istream>
#include <ostream>

namespace huffman {

// Кодирует последовательность символов (uint32_t) в битовый поток в `out`.
// Формат (внутренний):
//  - uint32_t unique_count
//  - для i in [0..unique_count): uint32_t symbol, uint16_t code_length
//  - затем битовый поток с кодами (MSB-first per code).
// Важно: caller должен сам записать внешний контейнер/заголовок если нужно.
void encode_symbols(const std::vector<uint32_t>& symbols, std::ostream& out);

// Декодирует тот же формат и возвращает массив символов.
std::vector<uint32_t> decode_symbols(std::istream& in);

}
