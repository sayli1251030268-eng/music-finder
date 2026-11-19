"""TuneFinder Hybrid C++ DS Server (Flask + Native C++14 Engine + HTML/CSS/JS Frontend).

Run:
    python server.py
Then open:
    http://127.0.0.1:5000
"""

from __future__ import annotations

import tempfile
import urllib.parse
from pathlib import Path

import pandas as pd
from flask import Flask, jsonify, request, send_from_directory

from src.audio_extractor import audio_similarity
from src.cpp_bridge import (
    cpp_engine_stats,
    cpp_graph_edges,
    cpp_history_stack,
    cpp_humming_match,
    cpp_lyric_search,
    cpp_playlist_step,
    cpp_recommend,
    cpp_trie_autocomplete,
)
from src.database import (
    create_playlist,
    create_tables,
    history_dataframe,
    import_dataframe,
    playlists_dataframe,
    record_history,
    songs_dataframe,
)

BASE_DIR = Path(__file__).resolve().parent
FRONTEND_DIR = BASE_DIR / "frontend"
DATA_DIR = BASE_DIR / "data"
AUDIO_DIR = DATA_DIR / "audio"
DEFAULT_CSV = DATA_DIR / "songs.csv"

app = Flask(__name__, static_folder=str(FRONTEND_DIR), static_url_path="")

create_tables()
AUDIO_DIR.mkdir(parents=True, exist_ok=True)
if songs_dataframe().empty and DEFAULT_CSV.exists():
    import_dataframe(pd.read_csv(DEFAULT_CSV))


def get_catalog() -> pd.DataFrame:
    frame = songs_dataframe()
    for col in ["song_id", "year", "tempo", "energy", "valence", "danceability"]:
        if col in frame:
            frame[col] = pd.to_numeric(frame[col], errors="coerce")
    for col, default in [("lyrics", ""), ("mood", "unknown"), ("audio_path", "")]:
        if col not in frame:
            frame[col] = default
        frame[col] = frame[col].fillna(default).astype(str)
    return frame


def spotify_url(title: str, artist: str = "") -> str:
    return f"https://open.spotify.com/search/{urllib.parse.quote(f'{title} {artist}'.strip())}"


def serialize_results(df: pd.DataFrame) -> list[dict]:
    if df.empty:
        return []
    rows = []
    for _, r in df.iterrows():
        audio_rel = str(r.get("audio_path", ""))
        audio_file = Path(audio_rel).name if audio_rel else ""
        item = {
            "song_id": int(r["song_id"]),
            "title": str(r.get("title", "")),
            "artist": str(r.get("artist", "")),
            "album": str(r.get("album", "")),
            "genre": str(r.get("genre", "")),
            "mood": str(r.get("mood", "")),
            "year": int(r["year"]) if pd.notna(r.get("year")) else 0,
            "tempo": float(r["tempo"]) if pd.notna(r.get("tempo")) else 0.0,
            "energy": float(r["energy"]) if pd.notna(r.get("energy")) else 0.0,
            "valence": float(r["valence"]) if pd.notna(r.get("valence")) else 0.0,
            "danceability": float(r["danceability"]) if pd.notna(r.get("danceability")) else 0.0,
            "audio_url": f"/audio/{audio_file}" if (audio_file and (AUDIO_DIR / audio_file).exists()) else "",
            "spotify_url": spotify_url(str(r.get("title", "")), str(r.get("artist", ""))),
        }
        for metric in ["similarity", "humming_similarity", "dtw_distance", "audio_similarity", "recommendation_score", "edge_weight"]:
            if metric in df.columns and pd.notna(r.get(metric)):
                val = float(r[metric])
                if metric == "audio_similarity" and val <= 1.0:
                    val = round(val * 100.0, 1)
                item[metric] = round(val, 4)
        rows.append(item)
    return rows


def save_temp_upload(file_storage) -> Path:
    temp_dir = Path(tempfile.gettempdir()) / "tunefinder_uploads"
    temp_dir.mkdir(parents=True, exist_ok=True)
    clean_name = Path(file_storage.filename or "recording.wav").name
    target = temp_dir / clean_name
    file_storage.save(str(target))
    return target


# ──────────────────────────────────────────────────────────────────
# Static Frontend & Audio Streaming Routes
# ──────────────────────────────────────────────────────────────────

@app.route("/")
def serve_index():
    return send_from_directory(str(FRONTEND_DIR), "index.html")


@app.route("/audio/<path:filename>")
def serve_audio(filename: str):
    return send_from_directory(str(AUDIO_DIR), filename)


# ──────────────────────────────────────────────────────────────────
# REST API Endpoints (Backed by C++14 Native DS Engine)
# ──────────────────────────────────────────────────────────────────

@app.route("/api/stats", methods=["GET"])
def api_stats():
    catalog = get_catalog()
    stats = cpp_engine_stats(catalog)
    return jsonify({
        "stats": stats,
        "catalog": serialize_results(catalog),
        "moods": sorted(catalog["mood"].dropna().unique().tolist()),
    })


@app.route("/api/autocomplete", methods=["GET"])
def api_autocomplete():
    prefix = request.args.get("prefix", "").strip()
    catalog = get_catalog()
    return jsonify(cpp_trie_autocomplete(prefix, catalog, top_k=10))


@app.route("/api/lyrics", methods=["POST"])
def api_lyrics():
    body = request.get_json(silent=True) or {}
    query = str(body.get("query", "")).strip()
    catalog = get_catalog()
    df, meta = cpp_lyric_search(query, catalog, top_k=5)
    return jsonify({"results": serialize_results(df), "meta": meta})


@app.route("/api/hum", methods=["POST"])
def api_hum():
    catalog = get_catalog()
    if "file" in request.files:
        temp_path = save_temp_upload(request.files["file"])
        df, meta = cpp_humming_match(temp_path, catalog, top_k=5)
        return jsonify({"results": serialize_results(df), "meta": meta})

    body = request.get_json(silent=True) or {}
    song_id = body.get("song_id")
    if song_id is not None:
        match = catalog[catalog["song_id"] == int(song_id)]
        if match.empty:
            return jsonify({"error": "Song not found"}), 404
        track_path = BASE_DIR / str(match.iloc[0]["audio_path"])
        df, meta = cpp_humming_match(track_path, catalog, top_k=5)
        return jsonify({"results": serialize_results(df), "meta": meta})

    return jsonify({"error": "Upload an audio file or provide a song_id."}), 400


@app.route("/api/audio", methods=["POST"])
def api_audio():
    catalog = get_catalog()
    if "file" in request.files:
        temp_path = save_temp_upload(request.files["file"])
        df = audio_similarity(temp_path, catalog, top_k=5)
        return jsonify({"results": serialize_results(df), "meta": {"data_structure": "Chroma CENS + MFCC Cosine Similarity"}})

    body = request.get_json(silent=True) or {}
    song_id = body.get("song_id")
    if song_id is not None:
        match = catalog[catalog["song_id"] == int(song_id)]
        if match.empty:
            return jsonify({"error": "Song not found"}), 404
        track_path = BASE_DIR / str(match.iloc[0]["audio_path"])
        df = audio_similarity(track_path, catalog, top_k=5)
        return jsonify({"results": serialize_results(df), "meta": {"data_structure": "Chroma CENS + MFCC Cosine Similarity"}})

    return jsonify({"error": "Upload an audio file or provide a song_id."}), 400


@app.route("/api/recommend", methods=["POST"])
def api_recommend():
    body = request.get_json(silent=True) or {}
    song_id = int(body.get("song_id", 1))
    mood = str(body.get("mood", "All"))
    catalog = get_catalog()
    df, meta = cpp_recommend(song_id, catalog, top_k=5, mood=mood)
    edges_payload = cpp_graph_edges(song_id, catalog)
    id_to_title = dict(zip(catalog["song_id"], catalog["title"]))
    edges = [
        {
            "target_song_id": e["target_song_id"],
            "target_title": id_to_title.get(e["target_song_id"], f"Song #{e['target_song_id']}"),
            "weight": e["weight"],
            "fit_pct": e["fit_pct"],
        }
        for e in edges_payload.get("edges", [])
    ]
    edges.sort(key=lambda x: x["weight"])
    return jsonify({"results": serialize_results(df), "meta": meta, "edges": edges})


@app.route("/api/playlist", methods=["POST"])
def api_playlist():
    body = request.get_json(silent=True) or {}
    ids = [int(x) for x in body.get("ids", [1, 2, 3, 4, 5])]
    cursor = int(body.get("cursor", 0))
    action = str(body.get("action", "view"))
    remove_id = int(body.get("remove_id", 0))
    catalog = get_catalog()
    res = cpp_playlist_step(ids, cursor, action, catalog, remove_id=remove_id)
    return jsonify(res)


@app.route("/api/playlist/save", methods=["POST"])
def api_playlist_save():
    body = request.get_json(silent=True) or {}
    name = str(body.get("name", "")).strip()
    mood = str(body.get("mood", "")).strip()
    ids = [int(x) for x in body.get("ids", [])]
    if not name or not ids:
        return jsonify({"error": "Playlist name and at least one song are required."}), 400
    pid = create_playlist(name, ids, mood)
    return jsonify({"playlist_id": pid, "playlists": playlists_dataframe().to_dict(orient="records")})


@app.route("/api/playlists", methods=["GET"])
def api_playlists():
    return jsonify({"playlists": playlists_dataframe().to_dict(orient="records")})


@app.route("/api/stack", methods=["POST"])
def api_stack():
    body = request.get_json(silent=True) or {}
    ids = [int(x) for x in body.get("ids", [])]
    action = str(body.get("action", "view"))
    catalog = get_catalog()
    return jsonify(cpp_history_stack(ids, action, catalog))


@app.route("/api/history/save", methods=["POST"])
def api_history_save():
    body = request.get_json(silent=True) or {}
    song_id = int(body.get("song_id", 0))
    action = str(body.get("action", "identified"))
    if song_id <= 0:
        return jsonify({"error": "Invalid song_id"}), 400
    record_history(song_id, action=action)
    return jsonify({"ok": True, "history": history_dataframe(20).to_dict(orient="records")})


if __name__ == "__main__":
    print("=" * 64)
    print("  TuneFinder Hybrid C++ DS Server running at:")
    print("  http://127.0.0.1:5000")
    print("=" * 64)
    app.run(host="127.0.0.1", port=5000, debug=False)
