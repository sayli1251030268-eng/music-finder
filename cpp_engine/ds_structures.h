#ifndef DS_STRUCTURES_H
#define DS_STRUCTURES_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Core Song Metadata Record
// ============================================================================
struct Song {
    int song_id;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    int year;
    std::string lyrics;
    std::string mood;
    double tempo;
    double energy;
    double valence;
    double danceability;
    std::string audio_path;
};

struct ScoredSong {
    int song_id;
    double score;       // Lower = better for MinHeap (distance), or negative similarity
    double confidence;  // 0..100 display confidence
    std::string extra;

    bool operator<(const ScoredSong& other) const {
        return score < other.score;
    }
};

// ============================================================================
// Helper: String Normalization & Tokenization
// ============================================================================
inline std::string toLowerClean(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isalnum(c) || c == ' ') {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else {
            out.push_back(' ');
        }
    }
    return out;
}

inline std::vector<std::string> tokenizeWords(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cleaned = toLowerClean(text);
    std::istringstream iss(cleaned);
    std::string word;
    while (iss >> word) {
        if (!word.empty()) {
            tokens.push_back(word);
        }
    }
    return tokens;
}

// ============================================================================
// 1. CUSTOM BINARY MIN-HEAP (Priority Queue)
//    Operations: push O(log N), pop O(log N), top O(1)
//    Used for Top-K extraction in Lyrics Search, Humming DTW, & Recommendations
// ============================================================================
template <typename T>
class MinHeap {
private:
    std::vector<T> data;

    void heapifyUp(size_t idx) {
        while (idx > 0) {
            size_t parent = (idx - 1) / 2;
            if (data[idx] < data[parent]) {
                std::swap(data[idx], data[parent]);
                idx = parent;
            } else {
                break;
            }
        }
    }

    void heapifyDown(size_t idx) {
        size_t n = data.size();
        while (true) {
            size_t left = 2 * idx + 1;
            size_t right = 2 * idx + 2;
            size_t smallest = idx;

            if (left < n && data[left] < data[smallest]) {
                smallest = left;
            }
            if (right < n && data[right] < data[smallest]) {
                smallest = right;
            }
            if (smallest != idx) {
                std::swap(data[idx], data[smallest]);
                idx = smallest;
            } else {
                break;
            }
        }
    }

public:
    void push(const T& item) {
        data.push_back(item);
        heapifyUp(data.size() - 1);
    }

    T pop() {
        T topVal = data[0];
        data[0] = data.back();
        data.pop_back();
        if (!data.empty()) {
            heapifyDown(0);
        }
        return topVal;
    }

    const T& top() const { return data[0]; }
    bool empty() const { return data.empty(); }
    size_t size() const { return data.size(); }
};

// ============================================================================
// 2. CUSTOM SEPARATE-CHAINING HASH TABLE (Inverted Index: word -> song_ids)
//    Operations: insert O(1) avg, get O(1) avg
//    Polynomial rolling hash function + explicit linked-list buckets
// ============================================================================
class InvertedIndexHashTable {
private:
    struct BucketNode {
        std::string key;
        std::vector<int> song_ids;
        BucketNode* next;
        BucketNode(const std::string& k) : key(k), next(NULL) {}
    };

    static const size_t TABLE_SIZE = 257;  // Prime bucket count to minimize collisions
    BucketNode* buckets[TABLE_SIZE];
    size_t uniqueKeys;

    size_t hashFunction(const std::string& key) const {
        unsigned long hash = 5381;
        for (size_t i = 0; i < key.size(); ++i) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(key[i]);
        }
        return hash % TABLE_SIZE;
    }

public:
    InvertedIndexHashTable() : uniqueKeys(0) {
        for (size_t i = 0; i < TABLE_SIZE; ++i) {
            buckets[i] = NULL;
        }
    }

    ~InvertedIndexHashTable() {
        for (size_t i = 0; i < TABLE_SIZE; ++i) {
            BucketNode* curr = buckets[i];
            while (curr != NULL) {
                BucketNode* nxt = curr->next;
                delete curr;
                curr = nxt;
            }
        }
    }

    void insert(const std::string& word, int song_id) {
        size_t idx = hashFunction(word);
        BucketNode* curr = buckets[idx];
        while (curr != NULL) {
            if (curr->key == word) {
                // Avoid duplicate song_id for the same word
                for (size_t j = 0; j < curr->song_ids.size(); ++j) {
                    if (curr->song_ids[j] == song_id) return;
                }
                curr->song_ids.push_back(song_id);
                return;
            }
            curr = curr->next;
        }
        BucketNode* node = new BucketNode(word);
        node->song_ids.push_back(song_id);
        node->next = buckets[idx];
        buckets[idx] = node;
        uniqueKeys++;
    }

    std::vector<int> lookup(const std::string& word) const {
        size_t idx = hashFunction(word);
        BucketNode* curr = buckets[idx];
        while (curr != NULL) {
            if (curr->key == word) {
                return curr->song_ids;
            }
            curr = curr->next;
        }
        return std::vector<int>();
    }

    size_t getUniqueWordCount() const { return uniqueKeys; }
    size_t getBucketCount() const { return TABLE_SIZE; }
};

// ============================================================================
// 3. CUSTOM TRIE (Prefix Tree) FOR LYRIC WORD SEARCH & AUTOCOMPLETE
//    Operations: insert O(L), search O(L), autocomplete O(L + K)
// ============================================================================
class LyricTrie {
private:
    struct TrieNode {
        TrieNode* children[26];
        bool isEndOfWord;
        std::vector<int> song_ids;

        TrieNode() : isEndOfWord(false) {
            for (int i = 0; i < 26; ++i) children[i] = NULL;
        }
    };

    TrieNode* root;
    size_t nodeCount;

    void destroy(TrieNode* node) {
        if (node == NULL) return;
        for (int i = 0; i < 26; ++i) {
            if (node->children[i] != NULL) {
                destroy(node->children[i]);
            }
        }
        delete node;
    }

    void collectWords(TrieNode* node, std::string& current,
                      std::vector<std::string>& results, size_t maxResults) const {
        if (node == NULL || results.size() >= maxResults) return;
        if (node->isEndOfWord) {
            results.push_back(current);
        }
        for (int i = 0; i < 26 && results.size() < maxResults; ++i) {
            if (node->children[i] != NULL) {
                current.push_back(static_cast<char>('a' + i));
                collectWords(node->children[i], current, results, maxResults);
                current.pop_back();
            }
        }
    }

    static std::string alphaOnlyLower(const std::string& word) {
        std::string out;
        for (size_t i = 0; i < word.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(word[i]);
            if (std::isalpha(c)) {
                out.push_back(static_cast<char>(std::tolower(c)));
            }
        }
        return out;
    }

public:
    LyricTrie() : root(new TrieNode()), nodeCount(1) {}
    ~LyricTrie() { destroy(root); }

    void insert(const std::string& rawWord, int song_id) {
        std::string word = alphaOnlyLower(rawWord);
        if (word.empty()) return;
        TrieNode* curr = root;
        for (size_t i = 0; i < word.size(); ++i) {
            int idx = word[i] - 'a';
            if (idx < 0 || idx >= 26) continue;
            if (curr->children[idx] == NULL) {
                curr->children[idx] = new TrieNode();
                nodeCount++;
            }
            curr = curr->children[idx];
        }
        curr->isEndOfWord = true;
        for (size_t j = 0; j < curr->song_ids.size(); ++j) {
            if (curr->song_ids[j] == song_id) return;
        }
        curr->song_ids.push_back(song_id);
    }

    std::vector<std::string> autocomplete(const std::string& rawPrefix, size_t maxResults = 8) const {
        std::string prefix = alphaOnlyLower(rawPrefix);
        std::vector<std::string> results;
        if (prefix.empty()) return results;

        TrieNode* curr = root;
        for (size_t i = 0; i < prefix.size(); ++i) {
            int idx = prefix[i] - 'a';
            if (idx < 0 || idx >= 26 || curr->children[idx] == NULL) {
                return results;
            }
            curr = curr->children[idx];
        }
        std::string buffer = prefix;
        collectWords(curr, buffer, results, maxResults);
        return results;
    }

    size_t getNodeCount() const { return nodeCount; }
};

// ============================================================================
// 4. CUSTOM DOUBLY LINKED LIST FOR PLAYLIST QUEUE NAVIGATION
//    Operations: insertTail O(1), moveNext O(1), movePrev O(1), remove O(N)
// ============================================================================
class PlaylistDoublyLinkedList {
private:
    struct DLLNode {
        int song_id;
        std::string title;
        DLLNode* prev;
        DLLNode* next;
        DLLNode(int id, const std::string& t) : song_id(id), title(t), prev(NULL), next(NULL) {}
    };

    DLLNode* head;
    DLLNode* tail;
    DLLNode* current;
    size_t count;

public:
    PlaylistDoublyLinkedList() : head(NULL), tail(NULL), current(NULL), count(0) {}

    ~PlaylistDoublyLinkedList() {
        DLLNode* node = head;
        while (node != NULL) {
            DLLNode* nxt = node->next;
            delete node;
            node = nxt;
        }
    }

    void insertTail(int song_id, const std::string& title) {
        DLLNode* node = new DLLNode(song_id, title);
        if (head == NULL) {
            head = tail = current = node;
        } else {
            tail->next = node;
            node->prev = tail;
            tail = node;
        }
        count++;
    }

    bool removeBySongId(int song_id) {
        DLLNode* node = head;
        while (node != NULL) {
            if (node->song_id == song_id) {
                if (node->prev != NULL) node->prev->next = node->next;
                else head = node->next;

                if (node->next != NULL) node->next->prev = node->prev;
                else tail = node->prev;

                if (current == node) {
                    current = (node->next != NULL) ? node->next : node->prev;
                }
                delete node;
                count--;
                return true;
            }
            node = node->next;
        }
        return false;
    }

    void setCursorIndex(int index) {
        DLLNode* node = head;
        int i = 0;
        while (node != NULL && i < index) {
            node = node->next;
            i++;
        }
        if (node != NULL) current = node;
    }

    bool moveNext() {
        if (current != NULL && current->next != NULL) {
            current = current->next;
            return true;
        }
        return false;
    }

    bool movePrev() {
        if (current != NULL && current->prev != NULL) {
            current = current->prev;
            return true;
        }
        return false;
    }

    int getCurrentSongId() const { return current != NULL ? current->song_id : -1; }
    std::string getCurrentTitle() const { return current != NULL ? current->title : ""; }
    size_t size() const { return count; }

    std::vector<std::pair<int, std::string> > toVector() const {
        std::vector<std::pair<int, std::string> > out;
        DLLNode* node = head;
        while (node != NULL) {
            out.push_back(std::make_pair(node->song_id, node->title));
            node = node->next;
        }
        return out;
    }
};

// ============================================================================
// 5. CUSTOM LINKED-LIST STACK (LIFO) FOR LISTENING HISTORY
//    Operations: push O(1), pop O(1), peek O(1)
// ============================================================================
class HistoryStack {
private:
    struct StackNode {
        int song_id;
        std::string title;
        std::string action;
        StackNode* next;
        StackNode(int id, const std::string& t, const std::string& a)
            : song_id(id), title(t), action(a), next(NULL) {}
    };

    StackNode* topNode;
    size_t count;

public:
    HistoryStack() : topNode(NULL), count(0) {}

    ~HistoryStack() {
        while (topNode != NULL) {
            StackNode* nxt = topNode->next;
            delete topNode;
            topNode = nxt;
        }
    }

    void push(int song_id, const std::string& title, const std::string& action) {
        StackNode* node = new StackNode(song_id, title, action);
        node->next = topNode;
        topNode = node;
        count++;
    }

    bool pop(int& outId, std::string& outTitle) {
        if (topNode == NULL) return false;
        outId = topNode->song_id;
        outTitle = topNode->title;
        StackNode* old = topNode;
        topNode = topNode->next;
        delete old;
        count--;
        return true;
    }

    size_t size() const { return count; }

    std::vector<std::pair<int, std::string> > toVector() const {
        std::vector<std::pair<int, std::string> > out;
        StackNode* curr = topNode;
        while (curr != NULL) {
            out.push_back(std::make_pair(curr->song_id, curr->title));
            curr = curr->next;
        }
        return out;
    }
};

// ============================================================================
// 6. CUSTOM WEIGHTED SIMILARITY GRAPH (Adjacency List) FOR RECOMMENDATIONS
//    Nodes = Songs, Edge Weight = Normalized Euclidean Acoustic Distance
//    Uses MinHeap to retrieve Top-K Nearest Neighbor Songs in O(V log K)
// ============================================================================
struct GraphEdge {
    int target_song_id;
    double weight;  // Smaller = more acoustically similar
    double fit_pct;
};

class SongSimilarityGraph {
private:
    std::vector<Song> nodes;
    std::vector<std::vector<GraphEdge> > adjList;

    int findIndex(int song_id) const {
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].song_id == song_id) return static_cast<int>(i);
        }
        return -1;
    }

public:
    void build(const std::vector<Song>& catalog) {
        nodes = catalog;
        size_t n = nodes.size();
        adjList.assign(n, std::vector<GraphEdge>());
        if (n < 2) return;

        // Compute standard deviation of each acoustic feature for z-scale distance
        double mean[4] = {0, 0, 0, 0};
        double stdv[4] = {0, 0, 0, 0};
        for (size_t i = 0; i < n; ++i) {
            mean[0] += nodes[i].tempo;
            mean[1] += nodes[i].energy;
            mean[2] += nodes[i].valence;
            mean[3] += nodes[i].danceability;
        }
        for (int f = 0; f < 4; ++f) mean[f] /= static_cast<double>(n);

        for (size_t i = 0; i < n; ++i) {
            stdv[0] += (nodes[i].tempo - mean[0]) * (nodes[i].tempo - mean[0]);
            stdv[1] += (nodes[i].energy - mean[1]) * (nodes[i].energy - mean[1]);
            stdv[2] += (nodes[i].valence - mean[2]) * (nodes[i].valence - mean[2]);
            stdv[3] += (nodes[i].danceability - mean[3]) * (nodes[i].danceability - mean[3]);
        }
        for (int f = 0; f < 4; ++f) {
            stdv[f] = std::sqrt(stdv[f] / static_cast<double>(n));
            if (stdv[f] < 1e-6) stdv[f] = 1.0;
        }

        // Build complete weighted undirected adjacency list
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                if (i == j) continue;
                double d0 = (nodes[i].tempo - nodes[j].tempo) / stdv[0];
                double d1 = (nodes[i].energy - nodes[j].energy) / stdv[1];
                double d2 = (nodes[i].valence - nodes[j].valence) / stdv[2];
                double d3 = (nodes[i].danceability - nodes[j].danceability) / stdv[3];
                double dist = std::sqrt(d0 * d0 + d1 * d1 + d2 * d2 + d3 * d3);
                double fit = (1.0 / (1.0 + dist)) * 100.0;

                GraphEdge e;
                e.target_song_id = nodes[j].song_id;
                e.weight = dist;
                e.fit_pct = fit;
                adjList[i].push_back(e);
            }
        }
    }

    std::vector<ScoredSong> recommendTopK(int source_song_id, int top_k, const std::string& moodFilter = "All") const {
        int idx = findIndex(source_song_id);
        std::vector<ScoredSong> results;
        if (idx < 0) return results;

        std::string moodLow = toLowerClean(moodFilter);
        MinHeap<ScoredSong> heap;

        for (size_t j = 0; j < adjList[idx].size(); ++j) {
            const GraphEdge& e = adjList[idx][j];
            int targetIdx = findIndex(e.target_song_id);
            if (targetIdx < 0) continue;

            if (moodFilter != "All" && !moodFilter.empty()) {
                if (toLowerClean(nodes[targetIdx].mood) != moodLow) continue;
            }

            ScoredSong item;
            item.song_id = e.target_song_id;
            item.score = e.weight;
            item.confidence = e.fit_pct;
            heap.push(item);
        }

        while (!heap.empty() && static_cast<int>(results.size()) < top_k) {
            results.push_back(heap.pop());
        }
        return results;
    }

    std::vector<GraphEdge> getEdgesForSong(int song_id) const {
        int idx = findIndex(song_id);
        if (idx < 0) return std::vector<GraphEdge>();
        return adjList[idx];
    }

    size_t getVertexCount() const { return nodes.size(); }
    size_t getEdgeCount() const {
        size_t total = 0;
        for (size_t i = 0; i < adjList.size(); ++i) total += adjList[i].size();
        return total;
    }
};

// ============================================================================
// 7. 2D DYNAMIC PROGRAMMING DTW MELODY MATCHER (O(n * m) DP Table)
//    Exact C++ port of our verified 10/10 scale-normalized + directional DTW
//    with 22-note chunked multi-phrase consensus for full-song recordings.
// ============================================================================
class DTWMatcher {
private:
    static double computeMedian(std::vector<double> vals) {
        if (vals.empty()) return 0.0;
        std::sort(vals.begin(), vals.end());
        size_t n = vals.size();
        if (n % 2 == 1) return vals[n / 2];
        return 0.5 * (vals[n / 2 - 1] + vals[n / 2]);
    }

    static std::vector<double> cleanOctaveGlitches(const std::vector<double>& arr) {
        if (arr.size() < 3) return arr;
        std::vector<double> out = arr;
        for (size_t i = 0; i < out.size(); ++i) {
            double left = (i > 0) ? out[i - 1] : out[i + 1];
            double right = (i + 1 < out.size()) ? out[i + 1] : out[i - 1];
            double localRef = 0.5 * (left + right);
            if (std::fabs(out[i] - localRef) >= 6.0) {
                out[i] = localRef;
            }
        }
        return out;
    }

    static std::vector<double> subtractMedian(const std::vector<double>& arr) {
        double med = computeMedian(arr);
        std::vector<double> out(arr.size());
        for (size_t i = 0; i < arr.size(); ++i) out[i] = arr[i] - med;
        return out;
    }

    static std::vector<double> zscorePitch(const std::vector<double>& arr) {
        if (arr.empty()) return arr;
        double mean = 0.0;
        for (size_t i = 0; i < arr.size(); ++i) mean += arr[i];
        mean /= static_cast<double>(arr.size());

        double var = 0.0;
        for (size_t i = 0; i < arr.size(); ++i) {
            double d = arr[i] - mean;
            var += d * d;
        }
        double stdv = std::sqrt(var / static_cast<double>(arr.size()));
        std::vector<double> out(arr.size());
        for (size_t i = 0; i < arr.size(); ++i) {
            out[i] = (stdv < 0.5) ? (arr[i] - mean) : ((arr[i] - mean) / stdv);
        }
        return out;
    }

    static std::vector<double> melodicDirection(const std::vector<double>& arr) {
        if (arr.size() < 2) return std::vector<double>();
        std::vector<double> out(arr.size() - 1);
        for (size_t i = 0; i + 1 < arr.size(); ++i) {
            out[i] = std::tanh((arr[i + 1] - arr[i]) / 1.5);
        }
        return out;
    }

    static double stdDev(const std::vector<double>& arr) {
        if (arr.empty()) return 0.0;
        double mean = 0.0;
        for (size_t i = 0; i < arr.size(); ++i) mean += arr[i];
        mean /= static_cast<double>(arr.size());
        double var = 0.0;
        for (size_t i = 0; i < arr.size(); ++i) {
            double d = arr[i] - mean;
            var += d * d;
        }
        return std::sqrt(var / static_cast<double>(arr.size()));
    }

    // Core 2D Dynamic Programming Table for Dynamic Time Warping: O(n * m)
    static double dtw2D(const std::vector<double>& q, const std::vector<double>& r, double sigma) {
        size_t n = q.size();
        size_t m = r.size();
        if (n == 0 || m == 0) return 1.0;

        const double INF = 1e18;
        const double invTwoSigSq = 1.0 / (2.0 * sigma * sigma);
        std::vector<std::vector<double> > dp(n + 1, std::vector<double>(m + 1, INF));
        dp[0][0] = 0.0;

        for (size_t i = 1; i <= n; ++i) {
            for (size_t j = 1; j <= m; ++j) {
                double diff = q[i - 1] - r[j - 1];
                double cost = 1.0 - std::exp(-(diff * diff) * invTwoSigSq);
                double bestPrev = dp[i - 1][j];
                if (dp[i][j - 1] < bestPrev) bestPrev = dp[i][j - 1];
                if (dp[i - 1][j - 1] < bestPrev) bestPrev = dp[i - 1][j - 1];
                dp[i][j] = cost + bestPrev;
            }
        }
        return dp[n][m] / static_cast<double>(std::max(n, m));
    }

    static double pairDistance(const std::vector<double>& qSub,
                               const std::vector<double>& rMed,
                               const std::vector<double>& rZ,
                               const std::vector<double>& rDir) {
        std::vector<double> qMed = subtractMedian(qSub);
        std::vector<double> qZ = zscorePitch(qSub);
        std::vector<double> qDir = melodicDirection(qSub);

        double dRaw = dtw2D(qMed, rMed, 1.4);
        double dZ = dtw2D(qZ, rZ, 0.75);
        double dDir = dtw2D(qDir, rDir, 0.55);
        return 0.30 * dRaw + 0.40 * dZ + 0.30 * dDir;
    }

public:
    static double matchQueryToHook(const std::vector<double>& qRaw, const std::vector<double>& rArr) {
        std::vector<double> qArr = cleanOctaveGlitches(qRaw);
        size_t n = qArr.size();
        size_t m = rArr.size();
        if (n < 2 || m < 2) return 1.0;

        std::vector<double> rMed = subtractMedian(rArr);
        std::vector<double> rZ = zscorePitch(rArr);
        std::vector<double> rDir = melodicDirection(rArr);

        double best = 1.0;
        if (std::abs(static_cast<int>(n) - static_cast<int>(m)) <= 4) {
            double d = pairDistance(qArr, rMed, rZ, rDir);
            if (d < best) best = d;
        }

        if (n > m) {
            size_t step = (n <= 45) ? 1 : 2;
            size_t wins[3] = {
                static_cast<size_t>(std::max(4, static_cast<int>(m) - 2)),
                m,
                std::min(n, m + 3)
            };
            for (int wi = 0; wi < 3; ++wi) {
                size_t w = wins[wi];
                if (w > n || w < 2) continue;
                for (size_t s = 0; s + w <= n; s += step) {
                    std::vector<double> sub(qArr.begin() + s, qArr.begin() + s + w);
                    double d = pairDistance(sub, rMed, rZ, rDir);
                    if (d < best) best = d;
                }
            }
        } else if (n < m) {
            size_t step = (m <= 45) ? 1 : 2;
            size_t wins[3] = {
                static_cast<size_t>(std::max(4, static_cast<int>(n) - 2)),
                n,
                std::min(m, n + 3)
            };
            for (int wi = 0; wi < 3; ++wi) {
                size_t w = wins[wi];
                if (w > m || w < 2) continue;
                for (size_t s = 0; s + w <= m; s += step) {
                    std::vector<double> subR(rArr.begin() + s, rArr.begin() + s + w);
                    double d = pairDistance(qArr, subtractMedian(subR), zscorePitch(subR), melodicDirection(subR));
                    if (d < best) best = d;
                }
            }
        }
        return best;
    }

    static double scoreChunkAgainstHooks(const std::vector<double>& qChunk,
                                         const std::vector<std::vector<double> >& hooks,
                                         bool isChunked) {
        double chunkBest = 1.0;
        for (size_t i = 0; i < hooks.size(); ++i) {
            const std::vector<double>& h = hooks[i];
            double d = matchQueryToHook(qChunk, h);
            double minLen = static_cast<double>(std::min(qChunk.size(), h.size()));
            double maxLen = static_cast<double>(std::max(qChunk.size(), h.size()));
            double lenRatio = (maxLen > 0.0) ? (minLen / maxLen) : 0.0;

            double penalty = 0.0;
            if (isChunked) {
                penalty = 0.08 * (1.0 - lenRatio);
            } else if (lenRatio < 0.45) {
                penalty = 0.25 * (1.0 - std::pow(lenRatio, 0.45));
            }
            if (h.size() <= 8 && stdDev(h) < 1.8) {
                penalty += 0.12;
            }
            double sc = d + penalty;
            if (sc < chunkBest) chunkBest = sc;
        }
        return chunkBest;
    }

    static double scoreFullQuery(const std::vector<double>& qPitches,
                                 const std::vector<std::vector<double> >& hooks) {
        if (qPitches.size() > 60) {
            std::vector<double> chunkDists;
            for (size_t i = 0; i + 12 < qPitches.size(); i += 16) {
                size_t endIdx = std::min(qPitches.size(), i + 22);
                std::vector<double> chunk(qPitches.begin() + i, qPitches.begin() + endIdx);
                chunkDists.push_back(scoreChunkAgainstHooks(chunk, hooks, true));
            }
            std::sort(chunkDists.begin(), chunkDists.end());
            size_t k = std::max(static_cast<size_t>(2),
                                static_cast<size_t>(std::ceil(chunkDists.size() * 0.55)));
            if (k > chunkDists.size()) k = chunkDists.size();
            double sum = 0.0;
            for (size_t i = 0; i < k; ++i) sum += chunkDists[i];
            return (k > 0) ? (sum / static_cast<double>(k)) : 1.0;
        }
        return scoreChunkAgainstHooks(qPitches, hooks, false);
    }

    static double distanceToConfidence(double bestD) {
        if (bestD <= 0.14) {
            return std::max(90.0, std::min(99.0, 99.0 - bestD * 64.0));
        } else if (bestD <= 0.24) {
            return std::max(68.0, std::min(90.0, 90.0 - (bestD - 0.14) * 220.0));
        } else if (bestD <= 0.38) {
            return std::max(30.0, std::min(68.0, 68.0 - (bestD - 0.24) * 271.4));
        }
        return std::max(5.0, std::min(30.0, 30.0 * std::exp(-4.0 * (bestD - 0.38))));
    }
};

#endif // DS_STRUCTURES_H
