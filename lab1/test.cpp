#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>

constexpr size_t MAX_KEY = 1000000;

struct Item {
    size_t key;
    std::string value;
};

// Функция сравнения для std::sort
bool compareItems(const Item& a, const Item& b) {
    return a.key < b.key;
}

std::vector<Item> countingSort(std::vector<Item> items) {
    std::vector<size_t> cntVect(MAX_KEY, 0);

    for (const auto& item : items) {
        ++cntVect[item.key];
    }

    for (size_t i = 1; i < MAX_KEY; ++i) {
        cntVect[i] += cntVect[i - 1];
    }

    std::vector<Item> res(items.size());
    size_t i = items.size();
    while (i-- > 0) {
        size_t index = --cntVect[items[i].key];
        res[index] = std::move(items[i]);
    }

    return res;
}

int main() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(0, 999999);
    std::uniform_int_distribution<> len_dist(1, 2048);

    std::vector<size_t> sizes = {1000, 10000, 50000, 100000, 500000, 1000000, 5000000};
    for (size_t size : sizes) {
        // Генерация данных
        std::vector<Item> items;
        items.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            std::string value(len_dist(gen), 'x');
            items.push_back({static_cast<size_t>(key_dist(gen)), value});
        }

        // Копия для std::sort
        std::vector<Item> itemsCopy = items;

        // Измерение времени для std::sort
        auto startStdSort = std::chrono::high_resolution_clock::now();
        std::sort(itemsCopy.begin(), itemsCopy.end(), compareItems);
        auto endStdSort = std::chrono::high_resolution_clock::now();
        auto durationStdSort = std::chrono::duration_cast<std::chrono::milliseconds>(endStdSort - startStdSort);

        // Измерение времени для countingSort
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<Item> sorted = countingSort(std::move(items));
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Оценка памяти
        size_t memoryCountingSort = items.capacity() * sizeof(Item) + MAX_KEY * sizeof(size_t) + sorted.capacity() * sizeof(Item);
        size_t memoryStdSort = itemsCopy.capacity() * sizeof(Item);

        // Вывод результатов
        std::cout << "Size: " << size
                  << " | Time for countingSort: " << duration.count() << "ms"
                  << " | Time for std::sort: " << durationStdSort.count() << "ms"
                  << " | Memory for countingSort: " << memoryCountingSort / 1024.0 / 1024.0 << "MB"
                  << " | Memory for std::sort: " << memoryStdSort / 1024.0 / 1024.0 << "MB\n";
    }

    return 0;
}