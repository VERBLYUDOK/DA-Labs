#include <iostream>
#include <string>
#include <cstdint>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cctype>

class TPatriciaTrie {
private:
    struct Node {
        int id; // используется для сериализации
        std::string key;
        std::uint64_t value;
        int bit;
        Node* children[2]; // 0 - left, 1 - right

        Node() : id(0), key(""), value(0), bit(0) {
            children[0] = this;
            children[1] = this;
        }

        Node(std::string k, std::uint64_t v, int b) : id(0), key(std::move(k)), value(v), bit(b) {
            children[0] = this;
            children[1] = this;
        }

        void Init(std::string k, std::uint64_t v, int b, Node* left, Node* right) {
            id = 0;
            key = std::move(k);
            value = v;
            bit = b;
            children[0] = left;
            children[1] = right;
        }

        ~Node() {}
    };

    Node* _header;
    int size;

    static bool KeyCompare(const std::string& key1, const std::string& key2) {
        if (&key1 == &key2) return true;
        if (key1.length() != key2.length()) return false;
        if (FirstDifferentBit(key1, key2) != key1.length() * 8) return false;
        return true;
    }

    static int BitLen(const std::string& k) {
        return k.length() * 8;
    }

    static int ByteLen(const std::string& k) {
        return k.length();
    }

    static int BitGet(const std::string& k, int bit) {
        if (bit < 0) return 0; // [-] bit = 0;
        int byteIndex = bit / 8;
        if (byteIndex >= static_cast<int>(k.length())) return 0;
        int bitIndex = 7 - (bit % 8);
        return ((k[byteIndex] >> bitIndex) & 1U);
    }

    static int FirstDifferentBit(const std::string& keya, const std::string& keyb) {
        size_t differ = 0;
        size_t lena = ByteLen(keya);
        size_t lenb = ByteLen(keyb);
        size_t minlen = std::min(lena, lenb);
        size_t maxlen = std::max(lena, lenb);
        while (differ < minlen && keya[differ] == keyb[differ]) differ++;
        differ *= 8;
        maxlen *= 8;
        while (differ < maxlen && BitGet(keya, differ) == BitGet(keyb, differ)) differ++;
        return differ;
    }

    void SetKey(Node* to, Node* from) {
        to->key = from->key;
    }

    void DestructRecursive(Node* node) {
        if (node->children[0]->bit > node->bit)
            DestructRecursive(node->children[0]);
        if (node->children[1]->bit > node->bit)
            DestructRecursive(node->children[1]);
        delete node;
    }

    void Index(Node* node, Node** nodes, int* depth) {
        node->id = *depth;
        nodes[*depth] = node;
        (*depth)++;
        if (node->children[0]->bit > node->bit)
            Index(node->children[0], nodes, depth);
        if (node->children[1]->bit > node->bit)
            Index(node->children[1], nodes, depth);
    }

public:
    TPatriciaTrie() : size(0) {
        _header = new Node("", 0, -1);
    }

    ~TPatriciaTrie() {
        DestructRecursive(_header);
    }

    bool Insert(const std::string& k, std::uint64_t d) {
        Node* prev = _header;
        Node* nxt = _header->children[0];
        while (prev->bit < nxt->bit) {
            prev = nxt;
            nxt = prev->children[BitGet(k, nxt->bit)];
        }

        if (KeyCompare(k, nxt->key))
            return false;

        int bitPrefix = FirstDifferentBit(k, nxt->key);
        prev = _header;
        nxt = _header->children[0];
        while (prev->bit < nxt->bit && nxt->bit < bitPrefix) {
            prev = nxt;
            nxt = prev->children[BitGet(k, nxt->bit)];
        }

        Node* newNode = new Node(k, d, bitPrefix);
        prev->children[BitGet(k, prev->bit)] = newNode;
        newNode->children[BitGet(k, bitPrefix)] = newNode;
        newNode->children[1 - BitGet(k, bitPrefix)] = nxt;
        this->size++;
        return true;
    }

    Node* Find(const std::string& k) {
        if (size == 0) return nullptr;

        Node* pref = _header;
        Node* ref = _header->children[0];
        while (pref->bit < ref->bit) {
            pref = ref;
            ref = pref->children[BitGet(k, pref->bit)];
        }
        if (!KeyCompare(k, ref->key))
            return nullptr;

        return ref;
    }

    bool Erase(const std::string& k) {
        Node* grandParent = nullptr;
        Node* parent = _header;
        Node* del = _header->children[0];

        while (parent->bit < del->bit) {
            grandParent = parent;
            parent = del;
            del = del->children[BitGet(k, del->bit)];
        }

        if (!KeyCompare(k, del->key))
            return false;

        if (del != parent) {
            SetKey(del, parent);
            del->value = parent->value;
        }

        if (parent->children[0]->bit > parent->bit || parent->children[1]->bit > parent->bit) {
            if (parent != del) {
                Node* parentOfParent = parent;
                Node* tmp = parent->children[BitGet(parent->key, parent->bit)];
                std::string keyCopy = parent->key;
                while (parentOfParent->bit < tmp->bit) {
                    parentOfParent = tmp;
                    tmp = parentOfParent->children[BitGet(keyCopy, parentOfParent->bit)];
                }

                if (!KeyCompare(keyCopy, tmp->key)) {
                    std::cerr << "ERROR: logical error during Erase (incorrect generated trie?)" << std::endl;
                    return false;
                }

                parentOfParent->children[BitGet(keyCopy, parentOfParent->bit)] = del;
            }

            if (grandParent != parent)
                grandParent->children[BitGet(k, grandParent->bit)] = parent->children[1 - BitGet(k, parent->bit)];
        } else {
            if (grandParent != parent) {
                grandParent->children[BitGet(k, grandParent->bit)] =
                    (parent->children[0] == parent) ?
                        (parent->children[1] == parent) ? grandParent : parent->children[1] :
                        parent->children[0];
            }
        }
        this->size--;
        delete parent;
        return true;
    }

    bool Save(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) throw std::runtime_error("cannot open file for writing");

        file.write(reinterpret_cast<const char*>(&size), sizeof(int));
        Node** nodes = new Node*[size + 1];
        int index = 0;
        Index(_header, nodes, &index);
        for (int i = 0; i < size + 1; ++i) {
            Node* node = nodes[i];
            file.write(reinterpret_cast<const char*>(&node->value), sizeof(std::uint64_t));
            file.write(reinterpret_cast<const char*>(&node->bit), sizeof(int));
            int len = ByteLen(node->key);
            file.write(reinterpret_cast<const char*>(&len), sizeof(int));
            file.write(node->key.data(), len);
            file.write(reinterpret_cast<const char*>(&node->children[0]->id), sizeof(int));
            file.write(reinterpret_cast<const char*>(&node->children[1]->id), sizeof(int));
        }
        delete[] nodes;
        if (file.fail()) {
            file.close();
            return false;
        }
        file.close();
        return true;
    }

    bool Load(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) throw std::runtime_error("cannot open file for reading");

        int newSize;
        file.read(reinterpret_cast<char*>(&newSize), sizeof(int));
        this->size = newSize;
        if (!newSize) {
            file.close();
            return true;
        }

        Node** nodes = new Node*[newSize + 1];
        nodes[0] = this->_header;
        for (int i = 1; i < newSize + 1; ++i) {
            nodes[i] = new Node();
        }

        for (int i = 0; i < newSize + 1; ++i) {
            std::uint64_t value;
            int bit, len;
            file.read(reinterpret_cast<char*>(&value), sizeof(std::uint64_t));
            file.read(reinterpret_cast<char*>(&bit), sizeof(int));
            file.read(reinterpret_cast<char*>(&len), sizeof(int));
            std::string key(len, '\0');
            file.read(&key[0], len);
            int indLeft, indRight;
            file.read(reinterpret_cast<char*>(&indLeft), sizeof(int));
            file.read(reinterpret_cast<char*>(&indRight), sizeof(int));
            nodes[i]->Init(key, value, bit, nodes[indLeft], nodes[indRight]);
        }

        file.peek();
        if (file.fail() || !file.eof()) {
            for (int i = 0; i < newSize + 1; i++)
                delete nodes[i];
            delete[] nodes;
            file.close();
            return false;
        }
        delete[] nodes;
        file.close();
        return true;
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);
    std::cin.tie(nullptr);

    TPatriciaTrie dict;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            if (line[0] == '+') {
                std::istringstream iss(line.substr(1));
                std::string w; std::uint64_t v;
                iss >> w >> v;
                for (char& c : w) c = std::tolower(static_cast<unsigned char>(c));
                std::cout << (dict.Insert(w, v) ? "OK\n" : "Exist\n");
            }
            else if (line[0] == '-') {
                std::istringstream iss(line.substr(1));
                std::string w;
                iss >> w;
                for (char& c : w) c = std::tolower(static_cast<unsigned char>(c));
                std::cout << (dict.Erase(w) ? "OK\n" : "NoSuchWord\n");
            }
            else if (line[0] == '!') {
                std::istringstream iss(line.substr(1));
                std::string cmd, path;
                iss >> cmd >> path;
                if (cmd == "Save") {
                    dict.Save(path);
                    std::cout << "OK\n";
                } else if (cmd == "Load") {
                    dict.Load(path);
                    std::cout << "OK\n";
                } else {
                    std::cout << "ERROR: unknown command\n";
                }
            }
            else {
                for (char& c : line) c = std::tolower(static_cast<unsigned char>(c));
                auto node = dict.Find(line);
                if (node)
                    std::cout << "OK: " << node->value << "\n";
                else
                    std::cout << "NoSuchWord\n";
            }
        }
        catch (const std::exception& e) {
            std::cout << "ERROR:" << e.what() << "\n";
        }
    }
    return 0;
}