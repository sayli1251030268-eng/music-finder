#include "ds_structures.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>

// ============================================================================
// RFC-4180 CSV Row Parser (handles quoted strings containing commas)
// ============================================================================
static std::vector<std::string> parseCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            fields.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    fields.push_back(current);
    return fields;
}

static double safeStod(const std::string& s, double fallback = 0.0) {
    if (s.empty()) return fallback;
    try {
        return std::stod(s);
    } catch (...) {
        return fallback;
    }
}

static int safeStoi(const std::string& s, int fallback = 0) {
    if (s.empty()) return fallback;
    try {
        return std::stoi(s);
    } catch (...) {
        return fallback;
    }
}

static std::vector<Song> loadCatalogCSV(const std::string& csvPath) {
    std::vector<Song> catalog;
    std::ifstream file(csvPath.c_str());
    if (!file.is_open()) {
        return catalog;
    }

    std::string headerLine;
    if (!std::getline(file, headerLine)) return catalog;
    std::vector<std::string> headers = parseCSVLine(headerLine);

    std::map<std::string, int> colIdx;
    for (size_t i = 0; i < headers.size(); ++i) {
        colIdx[headers[i]] = static_cast<int>(i);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::vector<std::string> row = parseCSVLine(line);
        if (row.size() < 3) continue;

        auto getCol = [&](const std::string& name) -> std::string {
            std::map<std::string, int>::iterator it = colIdx.find(name);
            if (it == colIdx.end()) return "";
            int idx = it->second;
            if (idx < 0 || static_cast<size_t>(idx) >= row.size()) return "";
            return row[idx];
        };

        Song s;
        s.song_id = safeStoi(getCol("song_id"), 0);
        s.title = getCol("title");
        s.artist = getCol("artist");
        s.album = getCol("album");
        s.genre = getCol("genre");
        s.year = safeStoi(getCol("year"), 2020);
        s.lyrics = getCol("lyrics");
        s.mood = getCol("mood");
        s.tempo = safeStod(getCol("tempo"), 100.0);
        s.energy = safeStod(getCol("energy"), 0.5);
        s.valence = safeStod(getCol("valence"), 0.5);
        s.danceability = safeStod(getCol("danceability"), 0.5);
        s.audio_path = getCol("audio_path");
        if (s.song_id > 0 && !s.title.empty()) {
            catalog.push_back(s);
        }
    }
    return catalog;
}

// ============================================================================
// Complete Full-Song Vocal Melodies (5-Song Catalog: Yellow, Perfect,
// Shape of You, Bad Guy, Blinding Lights)
// ============================================================================
static std::map<int, std::vector<std::vector<double> > > buildVocalMelodyBank() {
    std::map<int, std::vector<std::vector<double> > > bank;

    // 1: Yellow (Coldplay - B Major Full Song: Verse 1, Chorus, Verse 2, Bridge, Outro)
    std::vector<std::vector<double> > yellow;
    yellow.push_back({75, 75, 73, 75, 75, 75, 73, 75, 78, 73, 73, 71, 73, 78, 71, 68, 71, 68, 71, 71, 68, 66});
    yellow.push_back({75, 75, 73, 75, 75, 75, 73, 75, 78, 73});
    yellow.push_back({73, 71, 73, 78, 71, 68, 71, 68, 71, 71, 68, 66});
    yellow.push_back({75, 75, 73, 75, 75, 75, 73, 75, 76, 73, 73, 71, 73, 75, 71, 68, 71, 68, 71, 71, 68, 66});
    yellow.push_back({75, 75, 73, 75, 73, 75, 73, 75, 76, 73, 71, 73, 71, 73, 75, 71, 68, 71, 68, 71, 68, 66});
    yellow.push_back({66, 75, 73, 75, 73, 75, 73, 75, 78, 73});
    yellow.push_back({71, 73, 71, 73, 78, 71, 68, 71, 68, 71, 71, 68, 66});
    yellow.push_back({66, 75, 73, 75, 73, 75, 73, 75, 78, 73, 71, 73, 71, 73, 78, 71, 68, 71, 68, 71, 71, 68, 66});
    yellow.push_back({73, 75, 75, 73, 75, 78, 73, 73, 71, 73, 71, 73, 78, 71, 68, 71, 71, 68, 66});
    yellow.push_back({71, 78, 75, 71, 73, 75, 78, 75, 73, 71});
    yellow.push_back({71, 75, 78, 75, 73, 71, 75, 73, 71, 70, 71, 75, 73, 71});
    yellow.push_back({71, 78, 75, 71, 73, 75, 76, 75, 73, 71, 71, 75, 76, 75, 73, 71, 75, 73, 71, 70, 71, 76, 73, 71, 75, 71, 71, 70, 68});
    yellow.push_back({75, 73, 71, 73, 75, 75, 73, 71, 68, 66, 68, 71, 71, 70, 68, 66});
    yellow.push_back({71, 73, 75, 78, 75, 73, 71, 68, 66, 71, 73, 75, 78, 75, 73, 71});
    yellow.push_back({75, 78, 75, 75, 73, 75, 78, 73, 75, 75, 73, 75, 78, 73, 71, 73, 71, 73, 78, 71});
    yellow.push_back({57, 57, 55, 56, 57, 55, 54, 56, 56, 55, 54, 56, 54, 56, 58, 59, 53, 53, 55, 53, 55, 54, 56, 54});
    bank[1] = yellow;

    // 2: Perfect (Ed Sheeran - Ab Major Full Song: Verse 1, Pre-Chorus, Chorus, Verse 2, Bridge)
    std::vector<std::vector<double> > perfect;
    perfect.push_back({72, 70, 68, 68, 68, 67, 65, 63, 63, 65, 67, 68, 67, 65});
    perfect.push_back({63, 65, 68, 68, 70, 68, 68, 70, 72, 70, 68, 68, 68, 70, 72, 70, 68});
    perfect.push_back({63, 65, 68, 70, 68, 67, 68, 68, 70, 72, 70, 68, 67, 65, 68, 70, 68});
    perfect.push_back({68, 70, 72, 73, 70, 68, 67, 65, 68, 70, 72, 70, 68, 67, 65});
    perfect.push_back({68, 70, 72, 73, 72, 70, 68, 67, 65, 63, 65, 67, 68, 70, 68});
    perfect.push_back({68, 68, 67, 65, 63, 63, 65, 67, 68, 70, 72, 70, 68});
    perfect.push_back({72, 72, 70, 68, 68, 67, 65, 63, 65, 67, 68, 70, 68, 67});
    perfect.push_back({68, 70, 72, 70, 68, 68, 67, 68, 70, 72, 70, 68});
    perfect.push_back({68, 70, 72, 73, 75, 73, 72, 70, 68, 67, 68, 70, 72, 70, 68});
    perfect.push_back({63, 63, 60, 58, 59, 60, 59, 60, 59, 60, 60, 58, 56, 58, 60, 58});
    bank[2] = perfect;

    // 3: Shape of You (Ed Sheeran - C# Minor Full Song: Marimba Riff, Verse 1, Pre-Chorus, Chorus, Bridge)
    std::vector<std::vector<double> > shape;
    shape.push_back({61, 64, 61, 61, 64, 66, 64, 61, 64, 61, 61, 64, 66, 64});
    shape.push_back({64, 66, 68, 66, 64, 64, 66, 64, 64, 66, 68, 66, 64, 64, 66});
    shape.push_back({64, 64, 66, 68, 66, 64, 64, 66, 66, 64, 66, 68, 66, 64, 66});
    shape.push_back({64, 66, 68, 71, 68, 66, 64, 66, 64, 64, 66, 68, 66, 64, 66});
    shape.push_back({61, 61, 61, 61, 64, 66, 68, 61, 64, 66, 66, 64, 66, 68, 66, 64, 66});
    shape.push_back({61, 61, 64, 64, 66, 66, 68, 68, 64, 66, 68, 66, 64, 66});
    shape.push_back({61, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 66, 68, 68});
    shape.push_back({71, 68, 68, 68, 68, 68, 66, 66, 66, 66, 66, 66, 66, 64, 61});
    shape.push_back({64, 66, 68, 68, 68, 68, 68, 71, 68, 68, 66, 64, 64, 66, 68, 66, 64});
    shape.push_back({73, 73, 73, 71, 73, 68, 66, 64, 66, 68, 66, 64, 61, 64, 68, 66, 64, 66});
    shape.push_back({71, 66, 68, 66, 64, 64, 61, 64, 66, 68, 66, 64, 66, 64, 61});
    shape.push_back({61, 64, 66, 68, 66, 64, 61, 61, 64, 66, 68, 66, 64, 61});
    shape.push_back({59, 58, 58, 57, 57, 57, 57, 57, 56, 54, 54, 54, 56, 58, 58, 58, 58, 58});
    shape.push_back({61, 58, 55, 57, 58, 58, 56, 54, 58, 60, 58, 56, 57, 57, 57, 58, 56, 55, 54, 54, 54, 55, 54, 53, 54});
    bank[3] = shape;

    // 4: Bad Guy (Billie Eilish - G Minor Full Song: Bass Riff, Verse 1/2, Chorus, Bridge, Outro)
    std::vector<std::vector<double> > badguy;
    badguy.push_back({55, 55, 58, 55, 60, 58, 55, 53, 55, 55, 58, 55, 60, 58, 55, 53});
    badguy.push_back({58, 58, 58, 58, 58, 58, 55, 60, 58, 58, 58, 58, 58, 58, 55, 60});
    badguy.push_back({58, 58, 58, 58, 58, 58, 55, 62, 57, 57, 57, 57, 55, 54});
    badguy.push_back({58, 58, 58, 58, 58, 57, 60, 58, 58, 58, 58, 57, 58, 56, 60, 57});
    badguy.push_back({58, 58, 60, 62, 55, 58, 60, 60, 58, 62, 55, 58, 60, 60, 58, 62, 55});
    badguy.push_back({58, 60, 63, 63, 58, 60, 60, 58, 63, 63, 58, 60, 60, 58, 63, 63, 60, 58, 57, 55, 54});
    badguy.push_back({58, 58, 57, 55, 58, 58, 57, 55, 54, 55});
    bank[4] = badguy;

    // 5: Blinding Lights (The Weeknd - F Minor Full Song: 80s Synth Hook, Verse 1/2, Pre-Chorus, Chorus)
    std::vector<std::vector<double> > blinding;
    blinding.push_back({65, 65, 63, 65, 67, 60, 63, 65, 65, 63, 65, 67, 60, 63});
    blinding.push_back({72, 70, 72, 70, 72, 72, 70, 72, 70, 74, 70, 70, 72, 67});
    blinding.push_back({72, 70, 72, 70, 74, 70, 70, 72, 67, 70, 72, 67, 65});
    blinding.push_back({60, 62, 58, 58, 60, 57, 60, 59, 60, 58, 59, 57, 58, 59, 56, 56});
    blinding.push_back({72, 75, 77, 79, 72, 75, 77, 72, 75, 77, 79, 77});
    blinding.push_back({72, 75, 70, 67, 65, 67, 72, 75, 77, 74, 72, 70});
    blinding.push_back({70, 77, 77, 77, 79, 77, 75, 75, 72, 75, 72});
    blinding.push_back({75, 70, 67, 65, 75, 70, 67, 65, 75, 77, 77});
    blinding.push_back({70, 77, 77, 77, 79, 77, 75, 75, 72, 75, 72, 75, 70, 67, 65, 75, 70, 67, 65, 75, 77, 77});
    bank[5] = blinding;

    return bank;
}

// ============================================================================
// JSON Escaping Helper
// ============================================================================
static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "";
        else out.push_back(c);
    }
    return out;
}

static std::vector<double> parseDoubleList(const std::string& csv) {
    std::vector<double> out;
    std::istringstream iss(csv);
    std::string token;
    while (std::getline(iss, token, ',')) {
        if (!token.empty()) out.push_back(safeStod(token, 0.0));
    }
    return out;
}

static std::vector<int> parseIntList(const std::string& csv) {
    std::vector<int> out;
    std::istringstream iss(csv);
    std::string token;
    while (std::getline(iss, token, ',')) {
        if (!token.empty()) out.push_back(safeStoi(token, 0));
    }
    return out;
}

// ============================================================================
// Lyrics Search powered by Custom Hash Table Inverted Index + Trie + MinHeap
// ============================================================================
static std::vector<ScoredSong> searchLyricsDS(
    const std::string& query,
    const std::vector<Song>& catalog,
    const InvertedIndexHashTable& hashIndex,
    int topK
) {
    std::vector<std::string> qWords = tokenizeWords(query);
    std::string qClean = toLowerClean(query);
    if (qWords.empty()) return std::vector<ScoredSong>();

    // Deduplicate query words
    std::set<std::string> uniqueQ(qWords.begin(), qWords.end());

    // Step 1: O(1) per-word lookup in Custom Inverted Index Hash Table
    std::map<int, int> wordHitCounts;
    for (std::set<std::string>::iterator it = uniqueQ.begin(); it != uniqueQ.end(); ++it) {
        std::vector<int> hits = hashIndex.lookup(*it);
        for (size_t j = 0; j < hits.size(); ++j) {
            wordHitCounts[hits[j]]++;
        }
    }

    // Step 2: Rank candidate songs using Custom Binary MinHeap (negated score for max-first extraction)
    MinHeap<ScoredSong> heap;
    for (size_t i = 0; i < catalog.size(); ++i) {
        const Song& s = catalog[i];
        std::string lyrLow = toLowerClean(s.lyrics);
        std::string titleLow = toLowerClean(s.title);
        std::string artistLow = toLowerClean(s.artist);

        double sim = 0.0;
        if (!qClean.empty() && lyrLow.find(qClean) != std::string::npos) {
            sim += 2.0 + (static_cast<double>(qClean.size()) / std::max(1.0, static_cast<double>(lyrLow.size())));
        } else if (!qClean.empty() && (titleLow.find(qClean) != std::string::npos || artistLow.find(qClean) != std::string::npos)) {
            sim += 1.5;
        }

        int hits = wordHitCounts[s.song_id];
        if (hits > 0) {
            sim += 1.2 * (static_cast<double>(hits) / static_cast<double>(uniqueQ.size()));
        }

        if (sim > 0.0) {
            ScoredSong item;
            item.song_id = s.song_id;
            item.score = -sim;  // MinHeap pops smallest -> highest similarity first
            item.confidence = sim;
            heap.push(item);
        }
    }

    std::vector<ScoredSong> out;
    while (!heap.empty() && static_cast<int>(out.size()) < topK) {
        out.push_back(heap.pop());
    }
    return out;
}

// ============================================================================
// Standalone Interactive C++ Console Menu (No Python required!)
// ============================================================================
static void runInteractiveCLI(
    const std::vector<Song>& catalog,
    const InvertedIndexHashTable& hashIndex,
    const LyricTrie& trie,
    const SongSimilarityGraph& graph,
    const std::map<int, std::vector<std::vector<double> > >& melodyBank
) {
    PlaylistDoublyLinkedList playlist;
    HistoryStack history;
    for (size_t i = 0; i < catalog.size(); ++i) {
        playlist.insertTail(catalog[i].song_id, catalog[i].title);
    }

    while (true) {
        std::cout << "\n=========================================================\n";
        std::cout << "  TuneFinder C++ Data Structures Engine (Standalone CLI)\n";
        std::cout << "=========================================================\n";
        std::cout << "  1. Lyrics Search (Hash Table Inverted Index + MinHeap)\n";
        std::cout << "  2. Lyric Autocomplete (Prefix Trie)\n";
        std::cout << "  3. Humming Melody Match (2D DP DTW Matrix + MinHeap)\n";
        std::cout << "  4. Song Recommendations (Weighted Similarity Graph)\n";
        std::cout << "  5. Playlist Navigation (Doubly Linked List)\n";
        std::cout << "  6. Listening History (LIFO Stack)\n";
        std::cout << "  7. Inspect All Data Structures Stats\n";
        std::cout << "  0. Exit\n";
        std::cout << "Select option: ";

        int choice = -1;
        if (!(std::cin >> choice)) break;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 0) break;

        if (choice == 1) {
            std::cout << "Enter lyric phrase: ";
            std::string q;
            std::getline(std::cin, q);
            std::vector<ScoredSong> res = searchLyricsDS(q, catalog, hashIndex, 5);
            if (res.empty()) {
                std::cout << "No matches found.\n";
            } else {
                for (size_t i = 0; i < res.size(); ++i) {
                    for (size_t c = 0; c < catalog.size(); ++c) {
                        if (catalog[c].song_id == res[i].song_id) {
                            std::cout << "  #" << (i + 1) << " " << catalog[c].title << " - "
                                      << catalog[c].artist << " (Score: " << std::fixed << std::setprecision(2)
                                      << res[i].confidence << ")\n";
                            if (i == 0) history.push(catalog[c].song_id, catalog[c].title, "lyrics_search");
                        }
                    }
                }
            }
        } else if (choice == 2) {
            std::cout << "Enter word prefix (e.g. 'yel', 'dan', 'bli'): ";
            std::string pref;
            std::getline(std::cin, pref);
            std::vector<std::string> words = trie.autocomplete(pref, 10);
            std::cout << "Trie completions (" << words.size() << "): ";
            for (size_t i = 0; i < words.size(); ++i) {
                std::cout << words[i] << (i + 1 < words.size() ? ", " : "\n");
            }
        } else if (choice == 3) {
            std::cout << "Enter comma-separated MIDI notes (e.g. 62,62,60,62,62,64,59,57): ";
            std::string noteStr;
            std::getline(std::cin, noteStr);
            std::vector<double> qNotes = parseDoubleList(noteStr);
            MinHeap<ScoredSong> heap;
            for (size_t i = 0; i < catalog.size(); ++i) {
                int sid = catalog[i].song_id;
                std::map<int, std::vector<std::vector<double> > >::const_iterator it = melodyBank.find(sid);
                if (it == melodyBank.end()) continue;
                double d = DTWMatcher::scoreFullQuery(qNotes, it->second);
                ScoredSong item;
                item.song_id = sid;
                item.score = d;
                item.confidence = DTWMatcher::distanceToConfidence(d);
                heap.push(item);
            }
            int rank = 1;
            while (!heap.empty()) {
                ScoredSong top = heap.pop();
                for (size_t c = 0; c < catalog.size(); ++c) {
                    if (catalog[c].song_id == top.song_id) {
                        std::cout << "  #" << rank++ << " " << catalog[c].title << " (DTW="
                                  << std::fixed << std::setprecision(3) << top.score
                                  << ", Conf=" << std::setprecision(1) << top.confidence << "%)\n";
                    }
                }
            }
        } else if (choice == 4) {
            std::cout << "Enter Song ID (1=Yellow, 2=Perfect, 3=Shape of You, 4=Bad Guy, 5=Blinding Lights): ";
            int sid;
            std::cin >> sid;
            std::vector<ScoredSong> recs = graph.recommendTopK(sid, 4, "All");
            for (size_t i = 0; i < recs.size(); ++i) {
                for (size_t c = 0; c < catalog.size(); ++c) {
                    if (catalog[c].song_id == recs[i].song_id) {
                        std::cout << "  #" << (i + 1) << " " << catalog[c].title
                                  << " | Edge Weight=" << std::fixed << std::setprecision(3) << recs[i].score
                                  << " | Fit=" << std::setprecision(1) << recs[i].confidence << "%\n";
                    }
                }
            }
        } else if (choice == 5) {
            std::cout << "Current Playing (DLL Cursor): [" << playlist.getCurrentSongId() << "] "
                      << playlist.getCurrentTitle() << "\n";
            std::cout << "Action (n=Next, p=Prev): ";
            char act;
            std::cin >> act;
            if (act == 'n') playlist.moveNext();
            else if (act == 'p') playlist.movePrev();
            std::cout << "Now Playing: [" << playlist.getCurrentSongId() << "] "
                      << playlist.getCurrentTitle() << "\n";
        } else if (choice == 6) {
            std::vector<std::pair<int, std::string> > items = history.toVector();
            std::cout << "History Stack (Top -> Bottom, size=" << items.size() << "):\n";
            for (size_t i = 0; i < items.size(); ++i) {
                std::cout << "  [" << items[i].first << "] " << items[i].second << "\n";
            }
        } else if (choice == 7) {
            std::cout << "  Catalog Songs           : " << catalog.size() << "\n";
            std::cout << "  Trie Nodes              : " << trie.getNodeCount() << "\n";
            std::cout << "  Hash Table Unique Words : " << hashIndex.getUniqueWordCount()
                      << " (Buckets: " << hashIndex.getBucketCount() << ")\n";
            std::cout << "  Similarity Graph        : " << graph.getVertexCount()
                      << " vertices, " << graph.getEdgeCount() << " directed edges\n";
        }
    }
}

// ============================================================================
// Main Entrypoint: JSON Bridge Mode (for Streamlit) + Standalone C++ CLI Mode
// ============================================================================
int main(int argc, char* argv[]) {
    std::string csvPath = "../data/songs.csv";
    std::string mode = "stats";
    std::string query = "";
    std::string notesStr = "";
    std::string idsStr = "";
    std::string mood = "All";
    std::string action = "view";
    int songId = 1;
    int topK = 5;
    int cursor = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cli") {
            mode = "cli";
        } else if (arg == "--csv" && i + 1 < argc) {
            csvPath = argv[++i];
        } else if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "--query" && i + 1 < argc) {
            query = argv[++i];
        } else if (arg == "--notes" && i + 1 < argc) {
            notesStr = argv[++i];
        } else if (arg == "--ids" && i + 1 < argc) {
            idsStr = argv[++i];
        } else if (arg == "--mood" && i + 1 < argc) {
            mood = argv[++i];
        } else if (arg == "--action" && i + 1 < argc) {
            action = argv[++i];
        } else if (arg == "--song_id" && i + 1 < argc) {
            songId = safeStoi(argv[++i], 1);
        } else if (arg == "--topk" && i + 1 < argc) {
            topK = safeStoi(argv[++i], 5);
        } else if (arg == "--cursor" && i + 1 < argc) {
            cursor = safeStoi(argv[++i], 0);
        }
    }

    auto tStart = std::chrono::high_resolution_clock::now();

    std::vector<Song> catalog = loadCatalogCSV(csvPath);
    if (catalog.empty()) {
        // Fallback path if run from project root
        catalog = loadCatalogCSV("data/songs.csv");
    }

    // Build all core C++ Data Structures from scratch
    InvertedIndexHashTable hashIndex;
    LyricTrie trie;
    SongSimilarityGraph graph;

    for (size_t i = 0; i < catalog.size(); ++i) {
        std::vector<std::string> words = tokenizeWords(catalog[i].lyrics + " " + catalog[i].title + " " + catalog[i].artist);
        for (size_t w = 0; w < words.size(); ++w) {
            hashIndex.insert(words[w], catalog[i].song_id);
            trie.insert(words[w], catalog[i].song_id);
        }
    }
    graph.build(catalog);
    std::map<int, std::vector<std::vector<double> > > melodyBank = buildVocalMelodyBank();

    if (mode == "cli") {
        runInteractiveCLI(catalog, hashIndex, trie, graph, melodyBank);
        return 0;
    }

    if (mode == "stats") {
        auto tEnd = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        size_t totalHooks = 0;
        for (std::map<int, std::vector<std::vector<double> > >::iterator it = melodyBank.begin(); it != melodyBank.end(); ++it) {
            totalHooks += it->second.size();
        }
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "{"
                  << "\"engine\":\"C++14 Native DS Engine\","
                  << "\"songs\":" << catalog.size() << ","
                  << "\"trie_nodes\":" << trie.getNodeCount() << ","
                  << "\"hash_unique_words\":" << hashIndex.getUniqueWordCount() << ","
                  << "\"hash_buckets\":" << hashIndex.getBucketCount() << ","
                  << "\"graph_vertices\":" << graph.getVertexCount() << ","
                  << "\"graph_edges\":" << graph.getEdgeCount() << ","
                  << "\"melody_phrases\":" << totalHooks << ","
                  << "\"runtime_ms\":" << ms
                  << "}\n";
        return 0;
    }

    if (mode == "autocomplete") {
        std::vector<std::string> words = trie.autocomplete(query, static_cast<size_t>(topK));
        auto tEnd = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        std::cout << "{\"data_structure\":\"Trie (Prefix Tree)\",\"runtime_ms\":" << std::fixed << std::setprecision(3) << ms << ",\"suggestions\":[";
        for (size_t i = 0; i < words.size(); ++i) {
            std::cout << "\"" << jsonEscape(words[i]) << "\"" << (i + 1 < words.size() ? "," : "");
        }
        std::cout << "]}\n";
        return 0;
    }

    if (mode == "lyrics") {
        std::vector<ScoredSong> res = searchLyricsDS(query, catalog, hashIndex, topK);
        auto tEnd = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        std::cout << "{\"data_structure\":\"Hash Table Inverted Index + Min-Heap\",\"runtime_ms\":" << std::fixed << std::setprecision(3) << ms << ",\"results\":[";
        for (size_t i = 0; i < res.size(); ++i) {
            std::cout << "{\"song_id\":" << res[i].song_id << ",\"similarity\":" << std::setprecision(4) << res[i].confidence << "}"
                      << (i + 1 < res.size() ? "," : "");
        }
        std::cout << "]}\n";
        return 0;
    }

    if (mode == "hum") {
        std::vector<double> qNotes = parseDoubleList(notesStr);
        MinHeap<ScoredSong> heap;
        for (size_t i = 0; i < catalog.size(); ++i) {
            int sid = catalog[i].song_id;
            std::map<int, std::vector<std::vector<double> > >::const_iterator it = melodyBank.find(sid);
            if (it == melodyBank.end()) continue;
            double d = DTWMatcher::scoreFullQuery(qNotes, it->second);
            ScoredSong item;
            item.song_id = sid;
            item.score = d;
            item.confidence = DTWMatcher::distanceToConfidence(d);
            heap.push(item);
        }

        std::vector<ScoredSong> results;
        while (!heap.empty() && static_cast<int>(results.size()) < topK) {
            results.push_back(heap.pop());
        }
        auto tEnd = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        std::cout << "{\"data_structure\":\"2D DP DTW Matrix + Min-Heap\",\"note_count\":" << qNotes.size()
                  << ",\"runtime_ms\":" << std::fixed << std::setprecision(3) << ms << ",\"results\":[";
        for (size_t i = 0; i < results.size(); ++i) {
            std::cout << "{\"song_id\":" << results[i].song_id
                      << ",\"dtw_distance\":" << std::setprecision(4) << results[i].score
                      << ",\"humming_similarity\":" << std::setprecision(2) << results[i].confidence << "}"
                      << (i + 1 < results.size() ? "," : "");
        }
        std::cout << "]}\n";
        return 0;
    }

    if (mode == "recommend") {
        std::vector<ScoredSong> res = graph.recommendTopK(songId, topK, mood);
        auto tEnd = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        std::cout << "{\"data_structure\":\"Weighted Adjacency-List Graph + Min-Heap\",\"runtime_ms\":" << std::fixed << std::setprecision(3) << ms << ",\"results\":[";
        for (size_t i = 0; i < res.size(); ++i) {
            std::cout << "{\"song_id\":" << res[i].song_id
                      << ",\"edge_weight\":" << std::setprecision(4) << res[i].score
                      << ",\"recommendation_score\":" << std::setprecision(2) << res[i].confidence << "}"
                      << (i + 1 < res.size() ? "," : "");
        }
        std::cout << "]}\n";
        return 0;
    }

    if (mode == "graph_edges") {
        std::vector<GraphEdge> edges = graph.getEdgesForSong(songId);
        std::cout << "{\"source_song_id\":" << songId << ",\"edges\":[";
        for (size_t i = 0; i < edges.size(); ++i) {
            std::cout << "{\"target_song_id\":" << edges[i].target_song_id
                      << ",\"weight\":" << std::fixed << std::setprecision(4) << edges[i].weight
                      << ",\"fit_pct\":" << std::setprecision(1) << edges[i].fit_pct << "}"
                      << (i + 1 < edges.size() ? "," : "");
        }
        std::cout << "]}\n";
        return 0;
    }

    if (mode == "playlist_demo") {
        std::vector<int> ids = parseIntList(idsStr);
        PlaylistDoublyLinkedList dll;
        for (size_t i = 0; i < ids.size(); ++i) {
            std::string t = "Song #" + std::to_string(ids[i]);
            for (size_t c = 0; c < catalog.size(); ++c) {
                if (catalog[c].song_id == ids[i]) { t = catalog[c].title; break; }
            }
            dll.insertTail(ids[i], t);
        }
        dll.setCursorIndex(cursor);
        bool moved = false;
        if (action == "next") moved = dll.moveNext();
        else if (action == "prev") moved = dll.movePrev();
        else if (action == "remove") moved = dll.removeBySongId(songId);

        std::vector<std::pair<int, std::string> > nodes = dll.toVector();
        std::cout << "{\"data_structure\":\"Doubly Linked List\",\"moved\":" << (moved ? "true" : "false")
                  << ",\"current_song_id\":" << dll.getCurrentSongId()
                  << ",\"current_title\":\"" << jsonEscape(dll.getCurrentTitle()) << "\",\"nodes\":[";
        for (size_t i = 0; i < nodes.size(); ++i) {
            std::cout << "{\"song_id\":" << nodes[i].first << ",\"title\":\"" << jsonEscape(nodes[i].second) << "\"}"
                      << (i + 1 < nodes.size() ? "," : "");
        }
        std::cout << "]}\n";
        return 0;
    }

    if (mode == "stack_demo") {
        std::vector<int> ids = parseIntList(idsStr);
        HistoryStack st;
        for (size_t i = 0; i < ids.size(); ++i) {
            std::string t = "Song #" + std::to_string(ids[i]);
            for (size_t c = 0; c < catalog.size(); ++c) {
                if (catalog[c].song_id == ids[i]) { t = catalog[c].title; break; }
            }
            st.push(ids[i], t, "played");
        }
        int poppedId = -1;
        std::string poppedTitle = "";
        if (action == "pop") {
            st.pop(poppedId, poppedTitle);
        }
        std::vector<std::pair<int, std::string> > items = st.toVector();
        std::cout << "{\"data_structure\":\"LIFO Linked-List Stack\",\"popped_id\":" << poppedId
                  << ",\"popped_title\":\"" << jsonEscape(poppedTitle) << "\",\"items\":[";
        for (size_t i = 0; i < items.size(); ++i) {
            std::cout << "{\"song_id\":" << items[i].first << ",\"title\":\"" << jsonEscape(items[i].second) << "\"}"
                      << (i + 1 < items.size() ? "," : "");
        }
        std::cout << "]}\n";
        return 0;
    }

    std::cout << "{\"error\":\"Unknown mode\"}\n";
    return 1;
}
