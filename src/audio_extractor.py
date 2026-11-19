"""Audio Signal Processing Layer (Python / Librosa).

Responsibility:
- Converts raw audio (.wav, .mp3, .ogg, .m4a, .flac) into clean MIDI note sequences
  using Harmonic separation + YIN fundamental frequency tracking + median filtering.
- Computes Chroma CENS + MFCC timbre similarity for studio playing-song clips.
- All Data Structures & Algorithmic matching (Trie, Hash Table, Min-Heap, 2D DP DTW,
  Weighted Graph, Doubly Linked List, Stack) are executed in the compiled C++ engine
  (cpp_engine/ds_engine.exe).
"""

from __future__ import annotations

import pickle
from pathlib import Path

import librosa
import numpy as np
import pandas as pd
from scipy.ndimage import median_filter
from sklearn.metrics.pairwise import cosine_similarity

BASE_DIR = Path(__file__).resolve().parent.parent
AUDIO_EXTENSIONS = {".wav", ".mp3", ".flac", ".ogg", ".m4a"}
FEATURE_CACHE_FILE = BASE_DIR / "data" / "audio_features.pkl"
_AUDIO_FEATURE_CACHE: dict[str, tuple[np.ndarray, np.ndarray]] = {}


def resolve_audio_path(value: str | Path) -> Path:
    path = Path(str(value))
    return path if path.is_absolute() else BASE_DIR / path


def _load_audio_cache() -> None:
    global _AUDIO_FEATURE_CACHE
    if not _AUDIO_FEATURE_CACHE and FEATURE_CACHE_FILE.exists():
        try:
            with open(FEATURE_CACHE_FILE, "rb") as f:
                _AUDIO_FEATURE_CACHE = pickle.load(f)
        except Exception:
            _AUDIO_FEATURE_CACHE = {}


def _save_audio_cache() -> None:
    if _AUDIO_FEATURE_CACHE:
        try:
            FEATURE_CACHE_FILE.parent.mkdir(parents=True, exist_ok=True)
            with open(FEATURE_CACHE_FILE, "wb") as f:
                pickle.dump(_AUDIO_FEATURE_CACHE, f)
        except Exception:
            pass


def _extract_chroma_sequence(y: np.ndarray, sr: int = 22050) -> np.ndarray:
    if len(y) < 1024:
        return np.empty((12, 0))
    try:
        chroma = librosa.feature.chroma_cens(y=y, sr=sr, hop_length=512, n_chroma=12)
        norms = np.linalg.norm(chroma, axis=0, keepdims=True)
        norms[norms == 0] = 1.0
        return chroma / norms
    except Exception:
        return np.empty((12, 0))


def _chroma_cross_correlation(c_query: np.ndarray, c_ref: np.ndarray) -> float:
    n_q, n_r = c_query.shape[1], c_ref.shape[1]
    if n_q == 0 or n_r == 0:
        return 0.0
    if n_q > n_r:
        c_query, c_ref = c_ref, c_query
        n_q, n_r = n_r, n_q

    max_sim = 0.0
    for lag in range(0, max(1, n_r - n_q + 1), 2):
        overlap = c_ref[:, lag : lag + n_q]
        sim = float(np.mean(np.sum(c_query * overlap, axis=0)))
        if sim > max_sim:
            max_sim = sim
    return max_sim


def _extract_timbre_vector(y: np.ndarray, sr: int = 22050) -> np.ndarray:
    if len(y) < 1024:
        return np.array([])
    try:
        y_norm = librosa.util.normalize(y.astype(float))
        mfcc = librosa.feature.mfcc(y=y_norm, sr=sr, n_mfcc=20)
        mfcc_d = librosa.feature.delta(mfcc)
        contrast = librosa.feature.spectral_contrast(y=y_norm, sr=sr, n_bands=6)
        centroid = librosa.feature.spectral_centroid(y=y_norm, sr=sr)
        bandwidth = librosa.feature.spectral_bandwidth(y=y_norm, sr=sr)
        rolloff = librosa.feature.spectral_rolloff(y=y_norm, sr=sr, roll_percent=0.85)
        features = np.concatenate([
            mfcc.mean(axis=1), mfcc.std(axis=1),
            mfcc_d.mean(axis=1), mfcc_d.std(axis=1),
            contrast.mean(axis=1), contrast.std(axis=1),
            [
                centroid.mean() / 1000.0, centroid.std() / 1000.0,
                bandwidth.mean() / 1000.0, bandwidth.std() / 1000.0,
                rolloff.mean() / 1000.0, rolloff.std() / 1000.0,
            ],
        ])
        return np.nan_to_num(features)
    except Exception:
        return np.array([])


def _get_audio_features(path: Path) -> tuple[np.ndarray, np.ndarray]:
    _load_audio_cache()
    key = path.name
    if key in _AUDIO_FEATURE_CACHE:
        return _AUDIO_FEATURE_CACHE[key]
    y, sr = librosa.load(path, sr=22050, mono=True)
    c = _extract_chroma_sequence(y, sr)
    t = _extract_timbre_vector(y, sr)
    _AUDIO_FEATURE_CACHE[key] = (c, t)
    _save_audio_cache()
    return c, t


def audio_similarity(query_path: str | Path, catalog: pd.DataFrame, top_k: int = 5) -> pd.DataFrame:
    """Matches playing-song audio clips against catalog tracks via Chroma CENS + MFCC timbre."""
    query_audio, query_sr = librosa.load(query_path, sr=22050, mono=True)
    if len(query_audio) < 1024:
        return pd.DataFrame()

    q_chroma = _extract_chroma_sequence(query_audio, query_sr)
    q_timbre = _extract_timbre_vector(query_audio, query_sr)

    rows = []
    for _, song in catalog.iterrows():
        audio_path = resolve_audio_path(song.get("audio_path", ""))
        if not str(song.get("audio_path", "")).strip() or not audio_path.exists():
            continue
        try:
            r_chroma, r_timbre = _get_audio_features(audio_path)
            s_harmonic = _chroma_cross_correlation(q_chroma, r_chroma)
            s_timbre = float(cosine_similarity([q_timbre], [r_timbre])[0, 0]) if (q_timbre.size and r_timbre.size) else s_harmonic
            combined = max(0.0, min(1.0, 0.65 * s_harmonic + 0.35 * s_timbre))
            rows.append((int(song["song_id"]), combined))
        except Exception:
            continue

    if not rows:
        return pd.DataFrame()

    result = catalog.merge(pd.DataFrame(rows, columns=["song_id", "audio_similarity"]), on="song_id", how="inner")
    return result.sort_values("audio_similarity", ascending=False).drop_duplicates(subset=["song_id"]).head(top_k)


def extract_midi_notes(audio_path: str | Path, sr: int = 22050) -> list[int]:
    """Extracts discrete MIDI note sequence from a hummed/sung recording for the C++ DTW engine."""
    try:
        y, sr = librosa.load(audio_path, sr=sr, mono=True)
        if len(y) < 2048:
            return []
        y = librosa.util.normalize(y.astype(float))
        hop_length = 512
        y_harm = librosa.effects.harmonic(y, margin=2.0)

        f0 = librosa.yin(
            y_harm,
            fmin=librosa.note_to_hz("C2"),
            fmax=librosa.note_to_hz("A5"),
            sr=sr,
            hop_length=hop_length,
            trough_threshold=0.25,
        )
        rms = librosa.feature.rms(y=y_harm, hop_length=hop_length)[0]
        rms_thresh = max(0.015, float(np.max(rms) * 0.10))
        valid = (rms > rms_thresh) & (f0 > 75.0) & (f0 < 700.0)
        if int(np.sum(valid)) < 5:
            return []

        midi = np.where(valid, 12.0 * np.log2(f0 / 440.0) + 69.0, np.nan)
        smooth_midi = np.copy(midi)
        smooth_midi[valid] = median_filter(midi[valid], size=7)

        notes: list[int] = []
        curr: list[float] = []
        for i, is_v in enumerate(valid):
            if not is_v:
                if len(curr) >= 4:
                    notes.append(int(round(float(np.median(curr)))))
                curr = []
                continue
            p_val = float(smooth_midi[i])
            if not curr:
                curr.append(p_val)
            elif abs(p_val - float(np.median(curr))) >= 0.85:
                if len(curr) >= 4:
                    notes.append(int(round(float(np.median(curr)))))
                curr = [p_val]
            else:
                curr.append(p_val)

        if len(curr) >= 4:
            notes.append(int(round(float(np.median(curr)))))

        if len(notes) < 3:
            v_idx = np.where(valid)[0]
            if len(v_idx) >= 6:
                sub = smooth_midi[v_idx[::5]]
                sub = sub[~np.isnan(sub)]
                if len(sub) >= 3:
                    return [int(round(float(x))) for x in sub]
            return []

        return notes
    except Exception:
        return []
