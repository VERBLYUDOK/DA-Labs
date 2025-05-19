#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>

struct TAnswer { int strPos, wordPos; };

static std::vector<int> computeZ(const std::string& s) {
    int n = s.size();
    std::vector<int> z(n, 0);
    int l = 0, r = 0;
    for (int i = 1; i < n; ++i) {
        if (i <= r) z[i] = std::min(z[i - l], r - i + 1);
        while (i + z[i] < n && s[z[i]] == s[i + z[i]]) {
            ++z[i];
        }
        if (i + z[i] - 1 > r) {
            l = i; r = i + z[i] - 1;
        }
    }
    return z;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string line;
    if (!std::getline(std::cin, line)) return 0;
    std::istringstream pss(line);
    std::vector<std::string> patTokens;
    std::string w;
    while (pss >> w) {
        std::transform(w.begin(), w.end(), w.begin(),
                       [](char c){ return std::tolower(static_cast<unsigned char>(c)); });
        if (w.size() <= 16) patTokens.push_back(std::move(w));
    }
    if (patTokens.empty()) return 0;

    std::string pattern;
    for (size_t i = 0; i < patTokens.size(); ++i) {
        if (i) pattern.push_back(' ');
        pattern += patTokens[i];
    }
    int P = pattern.size();

    struct Token { std::string word; int line, idx; };
    std::vector<Token> tokens;
    tokens.reserve(1024);

    int lineNo = 0;
    while (std::getline(std::cin, line)) {
        ++lineNo;
        std::istringstream tss(line);
        int idx = 0;
        while (tss >> w) {
            std::transform(w.begin(), w.end(), w.begin(),
                           [](char c){ return std::tolower(static_cast<unsigned char>(c)); });
            if (w.size() <= 16) {
                ++idx;
                tokens.push_back({std::move(w), lineNo, idx});
            }
        }
    }

    std::string text;
    text.reserve(tokens.size() * 8);
    std::vector<std::pair<int,int>> charMap;
    charMap.reserve(tokens.size() * 8);

    bool first = true;
    for (auto &tk : tokens) {
        if (!first) {
            text.push_back(' ');
            charMap.emplace_back(tk.line, 0);
        }
        first = false;
        for (char c : tk.word) {
            text.push_back(c);
            charMap.emplace_back(tk.line, tk.idx);
        }
    }

    std::string S;
    S.reserve(P + 1 + text.size());
    S += pattern;
    S.push_back('\x01');
    S += text;

    auto Z = computeZ(S);

    std::vector<TAnswer> ans;
    int N = S.size();
    for (int i = P + 1; i < N; ++i) {
        if (Z[i] == P) {
            int pos = i - P - 1;
            if (pos >= 0 && pos < (int)charMap.size()) {
                auto [ln, wd] = charMap[pos];
                if (wd > 0) {
                    ans.push_back({ln, wd});
                }
            }
        }
    }

    for (auto &a : ans) {
        std::cout << a.strPos << ", " << a.wordPos << "\n";
    }
    return 0;
}
