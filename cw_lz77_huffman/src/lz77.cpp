#include "lz77.hpp"
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <limits>
#include <vector>
#include <string>
#include <utility>
#include <cstring>

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

static std::vector<int> build_sa_mapped(const std::string &str) {
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

static std::vector<int> build_lcp_mapped(const std::string &str, const std::vector<int> &sa) {
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
    out.reserve(1024);
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
