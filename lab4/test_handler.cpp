#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cstdlib>

std::string generate_random_word(int max_length) {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> len_dist(1, max_length);
    static std::uniform_int_distribution<> char_dist('a', 'z');

    int length = len_dist(gen);
    std::string word;
    for (int i = 0; i < length; ++i) {
        word += static_cast<char>(char_dist(gen));
    }
    return word;
}

void generate_test_file(const std::string& filename, int num_words, int words_per_line) {
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "Cannot open file " << filename << "\n";
        return;
    }

    std::string pattern = "cat dog";
    out << pattern << "\n";

    int words_written = 0;
    while (words_written < num_words) {
        int words_in_line = std::min(words_per_line, num_words - words_written);
        for (int i = 0; i < words_in_line; ++i) {
            if (words_written == num_words / 2 && i == 0) {
                out << "cat dog";
            } else {
                out << generate_random_word(16);
            }
            if (i < words_in_line - 1) out << " ";
            words_written++;
        }
        out << "\n";
    }
    out << "\n";
    out.close();
}

double run_test(const std::string& input_file) {
    auto start = std::chrono::high_resolution_clock::now();

    // Исправляем вызов для Windows: убираем "./"
    std::string command = "main.exe < " + input_file + " > output.txt";
    std::system(command.c_str());

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    return duration.count();
}

int main() {
    // Увеличиваем размеры тестов
    std::vector<int> test_sizes = {1000, 100000, 1000000, 10000000}; // 10^3, 10^5, 10^6, 10^7 слов
    int words_per_line = 10;

    for (int size : test_sizes) {
        std::string filename = "test_" + std::to_string(size) + ".txt";
        std::cout << "Generating test file with " << size << " words...\n";
        generate_test_file(filename, size, words_per_line);

        double time = run_test(filename);
        std::cout << "Test with " << size << " words took " << time << " seconds\n";
    }

    return 0;
}