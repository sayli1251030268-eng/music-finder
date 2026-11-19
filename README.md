# TuneFinder — Hybrid C++14 Data Structures + HTML/CSS/JS Edition

A **Data Structures Course Project** combining a **from-scratch C++14 Data Structures & Algorithms Engine** with a **custom HTML / CSS / JavaScript Frontend** and lightweight **Python Flask + Librosa** audio backend.

---

## Architecture Overview

```text
User Browser (HTML5 + CSS3 + Vanilla JS + Web Audio WAV Mic Recorder)
        │  REST API (JSON + Audio Upload)
        ▼
  Flask Server (server.py)
        ├── Audio DSP Only (src/audio_extractor.py)
        │     └── Extracts MIDI pitch notes via Librosa YIN + Chroma/MFCC
        │
        └── Python → C++ Bridge (src/cpp_bridge.py)
              │   Invokes compiled native binary via subprocess + JSON
              ▼
  Compiled C++14 DS Engine (cpp_engine/ds_engine.exe)
        ├── LyricTrie                → Prefix Tree (Live Autocomplete)
        ├── InvertedIndexHashTable   → Chaining Hash Table (Lyric Search)
        ├── MinHeap<ScoredSong>      → Binary Min-Heap (Top-K Extraction)
        ├── DTWMatcher               → 2D Dynamic Programming Table (Melody Match)
        ├── SongSimilarityGraph      → Weighted Adjacency-List Graph (Recommendations)
        ├── PlaylistDoublyLinkedList → Doubly Linked List (Next / Prev Queue)
        └── HistoryStack             → Linked-List LIFO Stack (Listening History)
```

---

## Core Data Structures Implemented From Scratch (`cpp_engine/ds_structures.h`)

| # | Data Structure | C++ Class | Purpose in TuneFinder | Complexity |
| :-: | :--- | :--- | :--- | :--- |
| 1 | **Trie (Prefix Tree)** | `LyricTrie` | Live lyric prefix autocomplete | `O(L + K)` |
| 2 | **Chaining Hash Table** | `InvertedIndexHashTable` | Inverted lyric word index (257 prime buckets) | `O(1)` avg |
| 3 | **Binary Min-Heap** | `MinHeap<T>` | Top-K ranking (Lyrics, Humming DTW, Graph Recs) | `O(N log K)` |
| 4 | **2D DP Matrix (DTW)** | `DTWMatcher` | Scale-normalized + chunked melody alignment | `O(n × m)` |
| 5 | **Weighted Graph** | `SongSimilarityGraph` | Adjacency list weighted by acoustic distance | `O(V log K)` |
| 6 | **Doubly Linked List** | `PlaylistDoublyLinkedList` | Bidirectional playlist cursor (`curr->prev`, `curr->next`) | `O(1)` step |
| 7 | **Linked-List Stack** | `HistoryStack` | LIFO listening history (`push`, `pop`, `peek`) | `O(1)` |

---

## How to Run

### 1. Run the HTML/CSS/JS Web App (Flask + C++14 Engine)
```powershell
cd music_finder_cpp
pip install -r requirements.txt
python server.py
```
Then open **`http://127.0.0.1:5000`** in your browser!

### 2. Recompile the C++ Engine *(already pre-compiled as `cpp_engine/ds_engine.exe`)*
```powershell
cd music_finder_cpp\cpp_engine
.\build.bat
```

### 3. Run Standalone Pure-C++ Interactive Console *(No Python or Browser needed!)*
```powershell
cd music_finder_cpp
.\cpp_engine\ds_engine.exe --csv data\songs.csv --cli
```
