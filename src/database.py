from __future__ import annotations

import sqlite3
from pathlib import Path
from typing import Iterable

import pandas as pd

BASE_DIR = Path(__file__).resolve().parent.parent
DATABASE_PATH = BASE_DIR / "data" / "music.db"


def get_connection() -> sqlite3.Connection:
    DATABASE_PATH.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(DATABASE_PATH)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = OFF")
    return connection


def create_tables() -> None:
    with get_connection() as connection:
        connection.executescript(
            """
            CREATE TABLE IF NOT EXISTS songs (
                song_id INTEGER PRIMARY KEY,
                title TEXT NOT NULL,
                artist TEXT NOT NULL,
                album TEXT,
                genre TEXT,
                year INTEGER,
                lyrics TEXT,
                mood TEXT,
                tempo REAL,
                energy REAL,
                valence REAL,
                danceability REAL,
                audio_path TEXT
            );
            CREATE TABLE IF NOT EXISTS users (
                user_id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                email TEXT UNIQUE
            );
            CREATE TABLE IF NOT EXISTS listening_history (
                history_id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                song_id INTEGER NOT NULL,
                action TEXT NOT NULL,
                played_at TEXT DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (user_id) REFERENCES users(user_id),
                FOREIGN KEY (song_id) REFERENCES songs(song_id)
            );
            CREATE TABLE IF NOT EXISTS playlists (
                playlist_id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                playlist_name TEXT NOT NULL,
                mood TEXT,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (user_id) REFERENCES users(user_id)
            );
            CREATE TABLE IF NOT EXISTS playlist_songs (
                playlist_id INTEGER NOT NULL,
                song_id INTEGER NOT NULL,
                position INTEGER DEFAULT 0,
                PRIMARY KEY (playlist_id, song_id),
                FOREIGN KEY (playlist_id) REFERENCES playlists(playlist_id) ON DELETE CASCADE,
                FOREIGN KEY (song_id) REFERENCES songs(song_id) ON DELETE CASCADE
            );
            """
        )


def songs_dataframe() -> pd.DataFrame:
    create_tables()
    with get_connection() as connection:
        df = pd.read_sql_query("SELECT * FROM songs ORDER BY title", connection)
        if "id" in df.columns and "song_id" not in df.columns:
            df["song_id"] = df["id"]
        return df


def import_dataframe(frame: pd.DataFrame) -> int:
    required = {"song_id", "title", "artist"}
    missing = required - set(frame.columns)
    if missing:
        raise ValueError(f"Missing required columns: {', '.join(sorted(missing))}")
    columns = ["song_id", "title", "artist", "album", "genre", "year", "lyrics", "mood", "tempo", "energy", "valence", "danceability", "audio_path"]
    clean = frame.copy()
    for column in columns:
        if column not in clean:
            clean[column] = ""
    clean = clean[columns].where(pd.notna(clean[columns]), None)
    create_tables()
    with get_connection() as connection:
        connection.executemany(
            """INSERT OR REPLACE INTO songs
            (song_id,title,artist,album,genre,year,lyrics,mood,tempo,energy,valence,danceability,audio_path)
            VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)""",
            [tuple(row) for row in clean.itertuples(index=False, name=None)],
        )
    return len(clean)


def record_history(song_id: int, action: str = "identified", user_id: int | None = None) -> None:
    create_tables()
    with get_connection() as connection:
        connection.execute("INSERT INTO listening_history (user_id, song_id, action) VALUES (?, ?, ?)", (user_id, int(song_id), action))


def history_dataframe(limit: int = 50) -> pd.DataFrame:
    create_tables()
    with get_connection() as connection:
        for sql in [
            """SELECT h.played_at, h.action, s.title, s.artist
               FROM listening_history h JOIN songs s ON (s.song_id=h.song_id)
               ORDER BY h.history_id DESC LIMIT ?""",
            """SELECT h.played_at, h.action, s.title, s.artist
               FROM listening_history h JOIN songs s ON (s.id=h.song_id)
               ORDER BY h.history_id DESC LIMIT ?""",
            """SELECT h.played_at, h.action, s.title, s.artist
               FROM history h JOIN songs s ON (s.id=h.song_id)
               ORDER BY h.id DESC LIMIT ?""",
        ]:
            try:
                return pd.read_sql_query(sql, connection, params=(limit,))
            except Exception:
                continue
        return pd.DataFrame()


def create_playlist(name: str, song_ids: Iterable[int], mood: str = "") -> int:
    create_tables()
    song_ids = [int(song_id) for song_id in song_ids]
    with get_connection() as connection:
        try:
            cursor = connection.execute("INSERT OR REPLACE INTO playlists (playlist_name, mood) VALUES (?, ?)", (name, mood))
        except sqlite3.OperationalError:
            cursor = connection.execute("INSERT OR REPLACE INTO playlists (name, description) VALUES (?, ?)", (name, mood))
        playlist_id = cursor.lastrowid
        connection.executemany(
            "INSERT INTO playlist_songs (playlist_id, song_id, position) VALUES (?, ?, ?)",
            [(playlist_id, song_id, position) for position, song_id in enumerate(song_ids)],
        )
    return int(playlist_id)


def playlists_dataframe() -> pd.DataFrame:
    create_tables()
    with get_connection() as connection:
        try:
            return pd.read_sql_query(
                """SELECT p.playlist_id, p.playlist_name, p.mood, p.created_at,
                          COUNT(ps.song_id) AS song_count
                   FROM playlists p LEFT JOIN playlist_songs ps ON ps.playlist_id=p.playlist_id
                   GROUP BY p.playlist_id ORDER BY p.playlist_id DESC""", connection
            )
        except Exception:
            return pd.read_sql_query(
                """SELECT p.id AS playlist_id, p.name AS playlist_name, p.description AS mood, p.created_at,
                          COUNT(ps.song_id) AS song_count
                   FROM playlists p LEFT JOIN playlist_songs ps ON ps.playlist_id=p.id
                   GROUP BY p.id ORDER BY p.id DESC""", connection
            )


def playlist_songs_dataframe(playlist_id: int) -> pd.DataFrame:
    with get_connection() as connection:
        return pd.read_sql_query(
            """SELECT s.title, s.artist, s.genre, s.mood
               FROM playlist_songs ps JOIN songs s ON s.song_id=ps.song_id
               WHERE ps.playlist_id=? ORDER BY ps.position""", connection, params=(int(playlist_id),)
        )
