#include <bits/stdc++.h>
using namespace std;

struct Token {
    int offset;
    int length;
    char ch;
    bool has_char;
};

static string map_with_terminator(const string &s) {
    if (s.empty()) return string(1, '\0');
    unsigned char minc = 255;
    for (unsigned char uc : s) if (uc < minc) minc = uc;
    string mapped;
    mapped.reserve(s.size() + 1);
    for (unsigned char uc : s) {
        unsigned char mapped_ch = static_cast<unsigned char>(uc - minc + 1); // >=1
        mapped.push_back(static_cast<char>(mapped_ch));
    }
    mapped.push_back('\0'); // terminator (0)
    return mapped;
}

vector<int> build_sa_mapped(const string &str) {
    int n = (int)str.size();
    vector<int> p(n), c(n);

    // sort by single character
    {
        vector<pair<unsigned char,int>> a(n);
        for (int i = 0; i < n; ++i) a[i] = { static_cast<unsigned char>(str[i]), i };
        sort(a.begin(), a.end());
        for (int i = 0; i < n; ++i) p[i] = a[i].second;
        c[p[0]] = 0;
        for (int i = 1; i < n; ++i) {
            c[p[i]] = c[p[i-1]] + (a[i].first != a[i-1].first);
        }
    }

    int k = 0;
    vector<int> pn(n), cn(n);
    while ((1 << k) < n) {
        int len = 1 << k;
        for (int i = 0; i < n; ++i)
            pn[i] = (p[i] - len + n) % n;

        // counting sort by class
        vector<int> cnt(n, 0);
        for (int x : c) cnt[x]++;
        vector<int> pos(n);
        pos[0] = 0;
        for (int i = 1; i < n; ++i) pos[i] = pos[i-1] + cnt[i-1];
        for (int x : pn) {
            int cl = c[x];
            p[pos[cl]++] = x;
        }

        // recalc classes
        cn[p[0]] = 0;
        for (int i = 1; i < n; ++i) {
            pair<int,int> prev = {c[p[i-1]], c[(p[i-1] + len) % n]};
            pair<int,int> now  = {c[p[i]],   c[(p[i]   + len) % n]};
            cn[p[i]] = cn[p[i-1]] + (now != prev);
        }
        c.swap(cn);
        ++k;
    }

    return p;
}

vector<int> build_lcp_mapped(const string &str, const vector<int> &sa) {
    int n = (int)sa.size();
    vector<int> rank(n);
    for (int i = 0; i < n; ++i) rank[sa[i]] = i;
    vector<int> lcp(n, 0);
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

vector<Token> lz77_compress_sa(const string &s) {
    vector<Token> res;
    int n = (int)s.size();
    if (n == 0) return res;

    string mapped = map_with_terminator(s);
    vector<int> sa = build_sa_mapped(mapped);
    vector<int> lcp = build_lcp_mapped(mapped, sa);
    int N = (int)sa.size(); // n + 1
    vector<int> rank(N);
    for (int i = 0; i < N; ++i) rank[sa[i]] = i;

    int pos = 0;
    while (pos < n) {
        int r = rank[pos];
        int best_len = 0;
        int best_start = 0;

        // go left from r-1 down to 0
        int minL = numeric_limits<int>::max();
        for (int i = r - 1; i >= 0; --i) {
            minL = min(minL, lcp[i+1]);
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
        minL = numeric_limits<int>::max();
        for (int i = r + 1; i < N; ++i) {
            minL = min(minL, lcp[i]);
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

string lz77_decompress(const vector<Token> &tokens) {
    string out;
    for (const Token &t : tokens) {
        if (t.offset == 0 && t.length == 0) {
            if (t.has_char) out.push_back(t.ch);
        } else {
            int start = (int)out.size() - t.offset;
            if (start < 0) throw runtime_error("Invalid token offset");
            for (int k = 0; k < t.length; ++k) out.push_back(out[start + k]);
            if (t.has_char) out.push_back(t.ch);
        }
    }
    return out;
}

string gen_random_string(size_t n, mt19937 &rng) {
    string s;
    s.resize(n);
    uniform_int_distribution<int> dist(0, 25);
    for (size_t i = 0; i < n; ++i) s[i] = char('a' + dist(rng));
    return s;
}

void run_correctness_tests() {
    vector<string> tests = {"", "a", "aaaaaa", "ababa bababab", "abcabcdabcde", "thequickbrownfoxjumpsoverthelazydog"};
    mt19937 rng(12345);
    tests.push_back(gen_random_string(1000, rng));

    cout << "==== Correctness tests ====" << endl;
    for (size_t i = 0; i < tests.size(); ++i) {
        const string &s = tests[i];
        auto toks = lz77_compress_sa(s);
        string r = lz77_decompress(toks);
        bool ok = (r == s);
        cout << "Test " << i << ": len=" << s.size() << " -> " << (ok ? "OK" : "FAIL") << "; tokens=" << toks.size() << "\n";
        if (!ok) {
            cerr << "Original:   '" << s.substr(0,200) << "'...\n";
            cerr << "Decompressed: '" << r.substr(0,200) << "'...\n";
        }
    }
}

void run_performance_tests() {
    cout << "\n==== Performance tests ====" << endl;
    vector<int> sizes = {1000, 5000, 10000, 30000, 60000, 120000, 240000, 480000};
    mt19937 rng((unsigned)chrono::steady_clock::now().time_since_epoch().count());

    cout << "n\tcompress_ms\tdecompress_ms\ttokens\n";
    for (int n : sizes) {
        string s = gen_random_string(n, rng);

        auto t1 = chrono::steady_clock::now();
        auto toks = lz77_compress_sa(s);
        auto t2 = chrono::steady_clock::now();
        string r = lz77_decompress(toks);
        auto t3 = chrono::steady_clock::now();

        long long comp_ms = chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
        long long decomp_ms = chrono::duration_cast<chrono::milliseconds>(t3 - t2).count();

        if (r != s) {
            cerr << "ERROR: mismatch at n=" << n << "\n";
        }

        cout << n << '\t' << comp_ms << '\t' << decomp_ms << '\t' << toks.size() << '\n';
    }
}

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << "LZ77 test (SA-based)." << endl;
    run_correctness_tests();
    run_performance_tests();

    cout << "\nDone." << endl;
    return 0;
}
