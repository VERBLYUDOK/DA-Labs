#include <iostream>
#include <string>
#include <vector>
#include <tuple>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <limits>
#include <random>

struct Token {
    int offset;
    int length;
    char ch;
    bool has_char;
    bool operator==(const Token &o) const {
        return offset==o.offset && length==o.length && has_char==o.has_char && (!has_char || ch==o.ch);
    }
};

std::vector<Token> lz77_compress_brute(const std::string &s) {
    int n = static_cast<int>(s.size());
    int pos = 0;
    std::vector<Token> result;

    while (pos < n) {
        int best_len = 0;
        int best_offset = 0;

        for (int start = 0; start < pos; ++start) {
            int len = 0;
            while (pos + len < n && s[start + len] == s[pos + len]) {
                ++len;
            }
            if (len > best_len) {
                best_len = len;
                best_offset = pos - start;
            } else if (len == best_len && len > 0) {
                int off = pos - start;
                if (best_offset == 0 || off < best_offset) best_offset = off;
            }
        }

        if (best_len == 0) {
            result.push_back({0, 0, s[pos], true});
            pos += 1;
        } else {
            if (pos + best_len < n) {
                result.push_back({best_offset, best_len, s[pos + best_len], true});
                pos += best_len + 1;
            } else {
                result.push_back({best_offset, best_len, '\0', false});
                pos += best_len;
            }
        }
    }

    return result;
}

static std::string map_with_terminator(const std::string &s) {
    if (s.empty()) return std::string(1, '\0');
    unsigned char minc = 255;
    for (unsigned char uc : s) if (uc < minc) minc = uc;
    std::string mapped;
    mapped.reserve(s.size() + 1);
    for (unsigned char uc : s) {
        unsigned char mapped_ch = static_cast<unsigned char>(uc - minc + 1); // >=1
        mapped.push_back(static_cast<char>(mapped_ch));
    }
    mapped.push_back('\0'); // terminator
    return mapped;
}

std::vector<int> build_sa_mapped(const std::string &str) {
    int n = (int)str.size();
    std::vector<int> p(n), c(n);

    // sort by single character
    {
        std::vector<std::pair<unsigned char,int>> a(n);
        for (int i = 0; i < n; ++i) a[i] = { static_cast<unsigned char>(str[i]), i };
        std::sort(a.begin(), a.end());
        for (int i = 0; i < n; ++i) p[i] = a[i].second;
        c[p[0]] = 0;
        for (int i = 1; i < n; ++i) {
            c[p[i]] = c[p[i-1]] + (a[i].first != a[i-1].first);
        }
    }

    int k = 0;
    std::vector<int> pn(n), cn(n);
    while ((1 << k) < n) {
        int len = 1 << k;
        for (int i = 0; i < n; ++i)
            pn[i] = (p[i] - len + n) % n;

        // counting sort by class
        std::vector<int> cnt(n, 0);
        for (int x : c) cnt[x]++;
        std::vector<int> pos(n);
        pos[0] = 0;
        for (int i = 1; i < n; ++i) pos[i] = pos[i-1] + cnt[i-1];
        for (int x : pn) {
            int cl = c[x];
            p[pos[cl]++] = x;
        }

        // recalc classes
        cn[p[0]] = 0;
        for (int i = 1; i < n; ++i) {
            std::pair<int,int> prev = {c[p[i-1]], c[(p[i-1] + len) % n]};
            std::pair<int,int> now  = {c[p[i]],   c[(p[i]   + len) % n]};
            cn[p[i]] = cn[p[i-1]] + (now != prev);
        }
        c.swap(cn);
        ++k;
    }

    return p;
}

std::vector<int> build_lcp_mapped(const std::string &str, const std::vector<int> &sa) {
    int n = (int)sa.size();
    std::vector<int> rank(n);
    for (int i = 0; i < n; ++i) rank[sa[i]] = i;
    std::vector<int> lcp(n, 0);
    int h = 0;
    for (int i = 0; i < n; ++i) {
        int r = rank[i];
        if (r == 0) { h = 0; lcp[r] = 0; continue; }
        int j = sa[r - 1];
        while (i + h < n && j + h < n && str[i + h] == str[j + h]) ++h;
        lcp[r] = h;
        if (h > 0) --h;
    }
    return lcp;
}

std::vector<Token> lz77_compress_sa(const std::string &s) {
    std::vector<Token> res;
    int n = (int)s.size();
    if (n == 0) return res;

    std::string mapped = map_with_terminator(s);
    std::vector<int> sa = build_sa_mapped(mapped);
    std::vector<int> lcp = build_lcp_mapped(mapped, sa);
    int N = (int)sa.size(); // n + 1
    std::vector<int> rank(N);
    for (int i = 0; i < N; ++i) rank[sa[i]] = i;

    int pos = 0;
    while (pos < n) {
        int r = rank[pos];
        int best_len = 0;
        int best_start = 0;

        // go left from r-1 down to 0
        int minL = std::numeric_limits<int>::max();
        for (int i = r - 1; i >= 0; --i) {
            minL = std::min(minL, lcp[i+1]);
            if (minL > n - pos) minL = n - pos;
            if (minL < best_len) break;
            int j = sa[i];
            if (j < pos) {
                // разрешаем перекрытие: НЕ ограничиваем длину pos-j
                int allowed = minL;
                if (allowed > best_len) {
                    best_len = allowed;
                    best_start = j;
                } else if (allowed == best_len && best_len > 0) {
                    int off = pos - j;
                    int cur_off = pos - best_start;
                    if (off < cur_off) {
                        best_start = j;
                    }
                }
            }
        }

        // go right from r+1 to N-1
        minL = std::numeric_limits<int>::max();
        for (int i = r + 1; i < N; ++i) {
            minL = std::min(minL, lcp[i]);
            if (minL > n - pos) minL = n - pos;
            if (minL < best_len) break;
            int j = sa[i];
            if (j < pos) {
                int allowed = minL;
                if (allowed > best_len) {
                    best_len = allowed;
                    best_start = j;
                } else if (allowed == best_len && best_len > 0) {
                    int off = pos - j;
                    int cur_off = pos - best_start;
                    if (off < cur_off) {
                        best_start = j;
                    }
                }
            }
        }

        if (best_len == 0) {
            res.push_back({0,0,s[pos],true});
            pos += 1;
        } else {
            int offset = pos - best_start;
            if (pos + best_len < n) {
                res.push_back({offset, best_len, s[pos + best_len], true});
                pos += best_len + 1;
            } else {
                res.push_back({offset, best_len, '\0', false});
                pos += best_len;
            }
        }
    }

    return res;
}

std::string lz77_decompress(const std::vector<Token> &tokens) {
    std::string out;
    for (const Token &t : tokens) {
        if (t.offset == 0 && t.length == 0) {
            if (t.has_char) out.push_back(t.ch);
        } else {
            int start = (int)out.size() - t.offset;
            if (start < 0) throw std::runtime_error("Invalid token offset");
            for (int k = 0; k < t.length; ++k) out.push_back(out[start + k]);
            if (t.has_char) out.push_back(t.ch);
        }
    }
    return out;
}

void trim(std::string &s) {
    size_t a = 0; while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size(); while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    s = s.substr(a, b-a);
}

std::vector<Token> parse_tokens_from_stdin_lines() {
    std::vector<Token> tokens;
    while (true) {
        int off, len;
        if (!(std::cin >> off)) break;
        if (!(std::cin >> len)) break;

        int sep = std::cin.get();
        if (sep == EOF) { tokens.push_back({off,len,'\0',false}); break; }
        if (sep == '\n') { tokens.push_back({off,len,'\0',false}); continue; }

        int next = std::cin.peek();
        if (next == EOF || next == '\n') {
            if (next == '\n') std::cin.get();
            tokens.push_back({off,len,'\0',false});
            continue;
        }

        char ch = static_cast<char>(std::cin.get());
        int c;
        while ((c = std::cin.get()) != EOF && c != '\n') {}
        tokens.push_back({off,len,ch,true});
    }
    return tokens;
}

std::vector<std::string> tokens_to_lines(const std::vector<Token> &toks) {
    std::vector<std::string> out;
    for (const auto &t : toks) {
        std::ostringstream oss;
        if (t.has_char) oss << t.offset << ' ' << t.length << ' ' << t.ch;
        else oss << t.offset << ' ' << t.length;
        out.push_back(oss.str());
    }
    return out;
}

void run_debug_tests() {
    std::mt19937_64 rng(123456);
    std::uniform_int_distribution<int> len_dist(0, 30);
    std::string alphabet = "ab ";

    auto rand_string = [&](int L) {
        std::string s;
        s.resize(L);
        for (int i = 0; i < L; ++i) s[i] = alphabet[rng() % alphabet.size()];
        return s;
    };

    // fixed tests
    std::vector<std::string> fixed = {
        "abracadabra",
        "abracadabra abracadabra",
        "aaaaaaa",
        "ababa",
        "abcabcabcabc",
        "aba aba",
        "ab    ab    ",
        "    ",
        "ab\nab",
        ""
    };

    for (auto &t : fixed) {
        auto a = lz77_compress_brute(t);
        auto b = lz77_compress_sa(t);
        if (a != b) {
            std::cerr << "Mismatch on fixed test: \"" << t << "\"\n";
            auto la = tokens_to_lines(a);
            auto lb = tokens_to_lines(b);
            std::cerr << "BRUTE:\n"; for (auto &x: la) std::cerr << x << "\n";
            std::cerr << "SA:\n"; for (auto &x: lb) std::cerr << x << "\n";
            return;
        }
    }

    // random tests
    for (int iter = 0; iter < 1000; ++iter) {
        int L = len_dist(rng);
        std::string s = rand_string(L);
        auto a = lz77_compress_brute(s);
        auto b = lz77_compress_sa(s);
        if (a != b) {
            std::cerr << "Mismatch on random test (iter " << iter << "), string length " << L << ":\n";
            std::cerr << '"' << s << "\"\n";
            auto la = tokens_to_lines(a);
            auto lb = tokens_to_lines(b);
            std::cerr << "BRUTE:\n"; for (auto &x: la) std::cerr << x << "\n";
            std::cerr << "SA:\n"; for (auto &x: lb) std::cerr << x << "\n";
            return;
        }
    }

    std::cerr << "All debug tests passed\n";
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string cmd;
    if (!std::getline(std::cin, cmd)) return 0;
    trim(cmd);

    if (cmd == "debug") {
        run_debug_tests();
        return 0;
    }

    if (cmd == "compress") {
        std::string text;
        if (!std::getline(std::cin, text)) text = "";
        auto tokens = lz77_compress_sa(text);
        for (const Token &t : tokens) {
            if (t.has_char) std::cout << t.offset << ' ' << t.length << ' ' << t.ch << '\n';
            else std::cout << t.offset << ' ' << t.length << '\n';
        }
    } else if (cmd == "decompress") {
        auto tokens = parse_tokens_from_stdin_lines();
        std::cout << lz77_decompress(tokens) << '\n';
    }

    return 0;
}
