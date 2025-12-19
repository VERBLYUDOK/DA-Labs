#include "lz77.hpp"
#include "utils.hpp"
#include "archiver.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>

// Поддержка ключей:
// -d  распаковка
// -1  LZ77
// -9  LZ77 + Huffman
// -c  вывод в stdout
// -k  не удалять исходный файл
// -l  list archive info
// -r  recursive (для директорий)
// -t  test archive (проверка)
// "-" stdin/stdout

int main(int argc, char** argv) {
    bool decompress = false;
    int level = 1;          // по умолчанию -1 (compress)
    bool to_stdout = false;
    bool keep = false;
    bool list_mode = false;
    bool recursive = false;
    bool test_mode = false;

    std::vector<std::string> roots;

    if (argc == 1) {
        std::cerr << "Usage: " << argv[0]
                  << " [-1|-9] [-d] [-c] [-k] [-l] [-r] [-t] [files...]\n";
        return 1;
    }

    // разбор аргументов
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-d") {
            decompress = true;
        } else if (a == "-1") {
            level = 1;
        } else if (a == "-9") {
            level = 9;
        } else if (a == "-c") {
            to_stdout = true;
        } else if (a == "-k") {
            keep = true;
        } else if (a == "-l") {
            list_mode = true;
        } else if (a == "-r") {
            recursive = true;
        } else if (a == "-t") {
            test_mode = true;
        } else {
            roots.push_back(a);
        }
    }

    // если нет файлов — работаем со stdin/stdout
    if (roots.empty()) roots.push_back("-");

    // если recursive, разворачиваем в список файлов
    std::vector<std::string> files;
    for (const auto &p : roots) {
        if (p == "-") {
            files.push_back(p);
            continue;
        }
        std::error_code ec;
        std::filesystem::file_status st = std::filesystem::status(p, ec);
        if (ec) {
            std::cerr << "Warning: cannot stat " << p << ": " << ec.message() << "\n";
            continue;
        }
        if (recursive && std::filesystem::is_directory(st)) {
            for (auto it = std::filesystem::recursive_directory_iterator(p, std::filesystem::directory_options::skip_permission_denied, ec);
                 it != std::filesystem::recursive_directory_iterator(); ++it) {
                if (ec) break;
                try {
                    if (it->is_regular_file()) {
                        files.push_back(it->path().string());
                    }
                } catch (...) {}
            }
            if (ec) {
                std::cerr << "Warning: error walking directory " << p << ": " << ec.message() << "\n";
            }
        } else {
            files.push_back(p);
        }
    }

    // основной цикл обработки
    for (const auto &path : files) {
        // LIST mode: только выводим информацию и идем дальше
        if (list_mode) {
            list_archive_info(path);
            continue;
        }

        // TEST mode: проверяем архив и идём дальше
        if (test_mode) {
            test_archive(path);
            continue;
        }

        // Обычный compress/decompress
        std::string out_path;
        if (decompress) {
            if (to_stdout || path == "-") {
                out_path = "-";
            } else {
                if (path.size() > 3 && path.substr(path.size() - 3) == ".lz") {
                    out_path = path.substr(0, path.size() - 3);
                } else {
                    out_path = path + ".out";
                }
            }
        } else {
            if (to_stdout || path == "-") {
                out_path = "-";
            } else {
                out_path = path + ".lz";
            }
        }

        {
            std::ifstream in_file;
            std::ofstream out_file;

            std::istream* in = open_input_stream(path, in_file);
            if (!in) {
                std::cerr << "Failed to open input: " << path << "\n";
                continue;
            }

            std::ostream* out = open_output_stream(out_path, out_file);
            if (!out) {
                std::cerr << "Failed to open output: " << out_path << "\n";
                continue;
            }

            try {
                if (decompress) {
                    decompress_stream_lz77(*in, *out);
                } else {
                    if (level == 9) compress_stream_lz77_huffman(*in, *out);
                    else compress_stream_lz77(*in, *out);
                }
            } catch (const std::exception &e) {
                std::cerr << (decompress ? "Decompression" : "Compression")
                          << " failed for " << path << ": " << e.what() << "\n";
                continue;
            }
        }

        if (!decompress && !to_stdout && path != "-" && !keep) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
            if (ec) {
                std::cerr << "Warning: failed to remove original file: " << ec.message() << "\n";
            }
        }
    }

    return 0;
}
