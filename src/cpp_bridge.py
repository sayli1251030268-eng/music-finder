"""Python <-> C++ Bridge for TuneFinder Hybrid Data Structures Engine.

Every core Data Structures operation (Trie Autocomplete, Hash Table Inverted Index,
2D DP DTW Melody Alignment, Min-Heap Top-K Ranking, Weighted Similarity Graph,
Doubly Linked List Playlist Navigation, and LIFO History Stack) executes inside
the compiled native C++ binary: cpp_engine/ds_engine.exe.
"""

from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd

from src.audio_extractor import audio_similarity, extract_midi_notes

BASE_DIR = Path(__file__).resolve().parent.parent
CPP_EXE = BASE_DIR / "cpp_engine" / "ds_engine.exe"
DEFAULT_CSV = BASE_DIR / "data" / "songs.csv"


def _write_temp_catalog(catalog: pd.DataFrame) -> Path:
    """Exports active SQLite catalog to a temp CSV so C++ always sees live additions."""
    tmp = Path(tempfile.gettempdir()) / "tunefinder_cpp_catalog.csv"
    catalog.to_csv(tmp, index=False)
    return tmp


def _run_cpp(args: list[str], catalog: pd.DataFrame | None = None) -> dict[str, Any]:
    """Executes cpp_engine/ds_engine.exe and parses its JSON response."""
    if not CPP_EXE.exists():
        raise FileNotFoundError(
            f"Compiled C++ engine not found at {CPP_EXE}. Run cpp_engine/build.bat first."
        )

    csv_path = _write_temp_catalog(catalog) if (catalog is not None and not catalog.empty) else DEFAULT_CSV
    cmd = [str(CPP_EXE), "--csv", str(csv_path)] + args
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        raise RuntimeError(f"C++ Engine Error (code {proc.returncode}): {proc.stderr or proc.stdout}")
    return json.loads(proc.stdout.strip())


def cpp_engine_stats(catalog: pd.DataFrame | None = None) -> dict[str, Any]:
    """Returns live metrics from all compiled C++ Data Structures."""
    return _run_cpp(["--mode", "stats"], catalog)


def cpp_trie_autocomplete(prefix: str, catalog: pd.DataFrame, top_k: int = 8) -> dict[str, Any]:
    """Queries the C++ Prefix Trie for lyric word completions in O(L + K)."""
    if not prefix.strip():
        return {"data_structure": "Trie (Prefix Tree)", "runtime_ms": 0.0, "suggestions": []}
    return _run_cpp(["--mode", "autocomplete", "--query", prefix.strip(), "--topk", str(top_k)], catalog)


def cpp_lyric_search(query: str, catalog: pd.DataFrame, top_k: int = 5) -> tuple[pd.DataFrame, dict[str, Any]]:
    """Runs Lyrics Search inside C++ using Custom Hash Table Inverted Index + Binary Min-Heap."""
    if not query.strip() or catalog.empty:
        return pd.DataFrame(), {}
    payload = _run_cpp(["--mode", "lyrics", "--query", query.strip(), "--topk", str(top_k)], catalog)
    items = payload.get("results", [])
    if not items:
        return pd.DataFrame(), payload

    scores_df = pd.DataFrame(items)
    merged = catalog.merge(scores_df, on="song_id", how="inner")
    merged = merged.sort_values("similarity", ascending=False).drop_duplicates(subset=["song_id"]).head(top_k)
    return merged, payload


def cpp_humming_match(
    query_path: str | Path,
    catalog: pd.DataFrame,
    top_k: int = 5,
) -> tuple[pd.DataFrame, dict[str, Any]]:
    """Extracts MIDI notes via Python librosa YIN, then runs 2D DP DTW + Min-Heap in C++."""
    query_p = Path(str(query_path))

    # 1. Studio track acoustic check (if user uploads an exact studio catalog track)
    try:
        audio_matches = audio_similarity(query_p, catalog, top_k=top_k)
        if not audio_matches.empty:
            top_score = float(audio_matches.iloc[0].get("audio_similarity", 0.0))
            if top_score >= 0.94:
                res = audio_matches.copy()
                res["dtw_distance"] = np.round(1.0 - res["audio_similarity"], 3)
                res["humming_similarity"] = np.round(res["audio_similarity"] * 100.0, 1)
                meta = {
                    "data_structure": "Acoustic Bypass (Studio Track Detected)",
                    "runtime_ms": 0.0,
                    "extracted_notes": [],
                }
                return res.head(top_k), meta
    except Exception:
        pass

    # 2. Extract MIDI notes from hummed/sung recording (Python DSP)
    notes = extract_midi_notes(query_p)
    if len(notes) < 3:
        return pd.DataFrame(), {"error": "Too few pitched notes extracted.", "extracted_notes": notes}

    # 3. Execute 2D DP DTW Matrix + Min-Heap Top-K Ranking natively in C++
    notes_csv = ",".join(str(n) for n in notes)
    payload = _run_cpp(["--mode", "hum", "--notes", notes_csv, "--topk", str(top_k)], catalog)
    payload["extracted_notes"] = notes

    items = payload.get("results", [])
    if not items:
        return pd.DataFrame(), payload

    scores_df = pd.DataFrame(items)
    merged = catalog.merge(scores_df, on="song_id", how="inner")
    merged = merged.sort_values("dtw_distance", ascending=True).drop_duplicates(subset=["song_id"]).head(top_k)
    return merged, payload


def cpp_recommend(
    song_id: int,
    catalog: pd.DataFrame,
    top_k: int = 5,
    mood: str = "All",
) -> tuple[pd.DataFrame, dict[str, Any]]:
    """Queries the C++ Weighted Song Similarity Graph + Binary Min-Heap."""
    payload = _run_cpp(
        ["--mode", "recommend", "--song_id", str(int(song_id)), "--mood", mood or "All", "--topk", str(top_k)],
        catalog,
    )
    items = payload.get("results", [])
    if not items:
        return pd.DataFrame(), payload

    scores_df = pd.DataFrame(items)
    merged = catalog.merge(scores_df, on="song_id", how="inner")
    merged = merged.sort_values("edge_weight", ascending=True).drop_duplicates(subset=["song_id"]).head(top_k)
    return merged, payload


def cpp_graph_edges(song_id: int, catalog: pd.DataFrame) -> dict[str, Any]:
    """Fetches adjacency-list edges & weights for a song from the C++ Weighted Graph."""
    return _run_cpp(["--mode", "graph_edges", "--song_id", str(int(song_id))], catalog)


def cpp_playlist_step(
    song_ids: list[int],
    cursor_idx: int,
    action: str,
    catalog: pd.DataFrame,
    remove_id: int = 0,
) -> dict[str, Any]:
    """Executes Doubly Linked List playlist navigation (next/prev/remove) in C++."""
    ids_csv = ",".join(str(int(i)) for i in song_ids)
    args = [
        "--mode", "playlist_demo",
        "--ids", ids_csv,
        "--cursor", str(int(cursor_idx)),
        "--action", action,
    ]
    if remove_id:
        args.extend(["--song_id", str(int(remove_id))])
    return _run_cpp(args, catalog)


def cpp_history_stack(song_ids: list[int], action: str, catalog: pd.DataFrame) -> dict[str, Any]:
    """Executes LIFO Linked-List Stack history operation (view/pop) in C++."""
    ids_csv = ",".join(str(int(i)) for i in song_ids)
    return _run_cpp(["--mode", "stack_demo", "--ids", ids_csv, "--action", action], catalog)
