#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

constexpr size_t MAX_KEY = 1000000;

struct Item {
    size_t key;
    std::string value;
};

size_t maxKey(std::vector<Item> items) {
    size_t maxK = 0;
    for (const auto& item : items) {
        maxK = std::max(maxK, item.key);
    }
    return maxK;
}

std::vector<Item> countingSort(std::vector<Item> items) {
    const size_t sizeCntVect = MAX_KEY;
    std::vector<size_t> cntVect(sizeCntVect, 0);
    // size_t* cntVect = new size_t[sizeCntVect]();

    for (const auto& item : items) {
        ++cntVect[item.key];
    }

    for (size_t i = 1; i < sizeCntVect; ++i) {
        cntVect[i] += cntVect[i - 1];
    }

    std::vector<Item> res(items.size());
    size_t i = items.size();
    while (i-- > 0) {
        size_t index = --cntVect[items[i].key];
        res[index] = std::move(items[i]);
    }

    // delete[] cntVect;

    return res;
}

int main() {
    std::vector<Item> items;
    size_t key;
    std::string value;

    while(std::cin >> key >> value) {
        items.push_back({
            .key = key,
            .value = std::move(value),
        });
    }

    items = countingSort(std::move(items));

    for (const auto& [key, value] : items) {
        std::cout << std::setfill('0') << std::setw(6) << key << '\t' << value << '\n';
    }

    return 0;
}