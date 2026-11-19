// TuneFinder Minimal Hybrid C++14 DS Frontend

const state = {
  catalog: [],
  dllIds: [],
  dllCursor: 0,
  stackIds: [1, 2, 3],
  recordedWavBlob: null,
  audioCtx: null,
  mediaStream: null,
  scriptNode: null,
  pcmBuffers: [],
  recordingSampleRate: 22050,
  timerInterval: null,
  secondsRecorded: 0,
};

const $ = (id) => document.getElementById(id);

async function initApp() {
  setupTabs();
  setupEvents();
  try {
    const res = await fetch("/api/stats");
    const data = await res.json();
    const s = data.stats;
    state.catalog = data.catalog || [];
    state.dllIds = state.catalog.map((c) => c.song_id);

    $("m-songs").textContent = s.songs;
    $("m-trie").textContent = s.trie_nodes;
    $("m-hash").textContent = s.hash_unique_words;
    $("m-graph").textContent = s.graph_edges;
    $("m-phrases").textContent = s.melody_phrases;

    populateSelectors(data.moods || []);
    renderCatalogTable();
    await refreshDLL("view");
    await refreshStack("view");
    await loadSavedPlaylists();
  } catch (err) {
    console.error("Initialization error:", err);
  }
}

function populateSelectors(moods) {
  ["hum-sample-select", "audio-sample-select", "rec-song-select", "stack-song-select"].forEach((id) => {
    $(id).innerHTML = state.catalog
      .map((s) => `<option value="${s.song_id}">${s.title} — ${s.artist}</option>`)
      .join("");
  });

  $("rec-mood-select").innerHTML =
    `<option value="All">All Moods</option>` +
    moods.map((m) => `<option value="${m}">${m}</option>`).join("");

  $("pl-checkboxes").innerHTML = state.catalog
    .map((s) => `<label><input type="checkbox" value="${s.song_id}" checked /> ${s.title}</label>`)
    .join("");

  updateSampleAudio("hum-sample-select", "hum-sample-audio");
  updateSampleAudio("audio-sample-select", "audio-sample-audio");
}

function updateSampleAudio(selectId, audioId) {
  const sid = Number($(selectId).value);
  const song = state.catalog.find((s) => s.song_id === sid);
  const el = $(audioId);
  if (song && song.audio_url) {
    el.src = song.audio_url;
    el.classList.remove("hidden");
  } else {
    el.classList.add("hidden");
  }
}

function setupTabs() {
  document.querySelectorAll(".tab-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".tab-btn").forEach((b) => b.classList.remove("active"));
      document.querySelectorAll(".tab-panel").forEach((p) => p.classList.remove("active"));
      btn.classList.add("active");
      $(btn.dataset.tab).classList.add("active");
    });
  });

  document.querySelectorAll("[data-hum-mode]").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll("[data-hum-mode]").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      const mode = btn.dataset.humMode;
      $("hum-upload-box").classList.toggle("hidden", mode !== "upload");
      $("hum-mic-box").classList.toggle("hidden", mode !== "mic");
    });
  });
}

function setupEvents() {
  // Unified Lyrics Search + Live C++ Trie Autocomplete on last typed word
  let trieTimer = null;
  $("lyrics-query").addEventListener("input", (e) => {
    clearTimeout(trieTimer);
    const parts = e.target.value.trim().split(/\s+/);
    const lastWord = parts[parts.length - 1] || "";
    if (lastWord.length < 2) {
      $("trie-chips").innerHTML = "";
      $("trie-meta").textContent = "";
      return;
    }
    trieTimer = setTimeout(() => runTrieAutocomplete(lastWord), 120);
  });
  $("lyrics-query").addEventListener("keydown", (e) => {
    if (e.key === "Enter") runLyricsSearch();
  });
  $("btn-lyrics-search").addEventListener("click", runLyricsSearch);

  // Humming
  $("hum-file-input").addEventListener("change", (e) => {
    const f = e.target.files[0];
    $("hum-file-label").textContent = f ? `${f.name} (${(f.size / 1024).toFixed(1)} KB)` : "Drop or click to choose audio";
  });
  $("btn-hum-upload").addEventListener("click", () => {
    const f = $("hum-file-input").files[0];
    if (!f) return alert("Please choose an audio file first.");
    runHummingFile(f);
  });
  $("hum-sample-select").addEventListener("change", () => updateSampleAudio("hum-sample-select", "hum-sample-audio"));
  $("btn-hum-sample").addEventListener("click", () => runHummingSample(Number($("hum-sample-select").value)));

  // Mic Recorder
  $("btn-mic-start").addEventListener("click", startMicRecording);
  $("btn-mic-stop").addEventListener("click", stopMicRecording);
  $("btn-hum-mic").addEventListener("click", () => {
    if (!state.recordedWavBlob) return;
    runHummingFile(new File([state.recordedWavBlob], "mic_humming.wav", { type: "audio/wav" }));
  });

  // Playing Audio
  $("audio-file-input").addEventListener("change", (e) => {
    const f = e.target.files[0];
    $("audio-file-label").textContent = f ? f.name : "Click to choose an audio clip";
  });
  $("btn-audio-upload").addEventListener("click", () => {
    const f = $("audio-file-input").files[0];
    if (!f) return alert("Please choose an audio file first.");
    runAudioFile(f);
  });
  $("audio-sample-select").addEventListener("change", () => updateSampleAudio("audio-sample-select", "audio-sample-audio"));
  $("btn-audio-sample").addEventListener("click", () => runAudioSample(Number($("audio-sample-select").value)));

  // Recommendations
  $("btn-recommend").addEventListener("click", runRecommendations);

  // DLL Playlist
  $("btn-dll-prev").addEventListener("click", () => refreshDLL("prev"));
  $("btn-dll-next").addEventListener("click", () => refreshDLL("next"));
  $("btn-pl-save").addEventListener("click", saveCustomPlaylist);

  // LIFO Stack
  $("btn-stack-push").addEventListener("click", async () => {
    const sid = Number($("stack-song-select").value);
    state.stackIds.push(sid);
    await fetch("/api/history/save", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ song_id: sid, action: "stack_push" }),
    });
    $("stack-toast").textContent = `Pushed Song #${sid} onto topNode`;
    await refreshStack("view");
  });
  $("btn-stack-pop").addEventListener("click", async () => {
    if (!state.stackIds.length) return;
    const res = await refreshStack("pop");
    state.stackIds.pop();
    $("stack-toast").textContent = `Popped: ${res.popped_title || "Empty"}`;
    await refreshStack("view");
  });
}

// ------------------------------------------------------------------
// Clean Minimal Result Cards
// ------------------------------------------------------------------
function renderResults(containerId, results, meta, scoreKey, isPct = true) {
  const box = $(containerId);
  let html = "";

  if (meta && meta.data_structure) {
    const ms = meta.runtime_ms !== undefined ? `${Number(meta.runtime_ms).toFixed(1)} ms` : "";
    html += `<div class="cpp-meta-bar"><span>♫ ${meta.data_structure}</span><span>${ms}</span></div>`;
  }

  if (!results || !results.length) {
    html += `<div class="card"><p class="subtitle" style="margin:0">No match found. Try a longer clip or different query.</p></div>`;
    box.innerHTML = html;
    return;
  }

  results.forEach((r, idx) => {
    const raw = r[scoreKey] ?? 0;
    const pct = isPct ? Math.min(100, Math.max(0, raw)) : Math.min(100, (raw / 4.2) * 100);
    const label = isPct ? `${Number(raw).toFixed(1)}%` : `Score ${Number(raw).toFixed(2)}`;
    const extra =
      r.dtw_distance !== undefined ? ` · DTW ${r.dtw_distance}` :
      r.edge_weight !== undefined ? ` · Weight ${r.edge_weight}` : "";

    html += `
      <div class="result-card ${idx === 0 ? "best" : ""}">
        <div class="result-top">
          <div class="result-title">#${idx + 1} ${r.title} <span class="result-artist">— ${r.artist}</span></div>
          <span class="score-pill">${label}</span>
        </div>
        <div class="result-sub">${r.genre} · ${r.mood} · ${r.year}${extra}</div>
        <div class="bar-track"><div class="bar-fill" style="width:${pct.toFixed(1)}%"></div></div>
        <div class="result-footer">
          ${r.audio_url ? `<audio controls src="${r.audio_url}"></audio>` : `<span></span>`}
          <a class="spotify-link" href="${r.spotify_url}" target="_blank" rel="noopener">Open on Spotify ↗</a>
        </div>
      </div>`;
  });
  box.innerHTML = html;
}

// ------------------------------------------------------------------
// API Calls
// ------------------------------------------------------------------
async function runTrieAutocomplete(prefix) {
  const res = await fetch(`/api/autocomplete?prefix=${encodeURIComponent(prefix)}`);
  const data = await res.json();
  const list = data.suggestions || [];
  $("trie-chips").innerHTML = list
    .map((w) => `<button class="chip" onclick="completeLastWord('${w}')">${w}</button>`)
    .join("");
  $("trie-meta").textContent = list.length
    ? `C++ Trie suggestions (${Number(data.runtime_ms || 0).toFixed(1)} ms)`
    : "";
}

window.completeLastWord = function (word) {
  const box = $("lyrics-query");
  const parts = box.value.trim().split(/\s+/);
  parts[parts.length - 1] = word;
  box.value = parts.join(" ") + " ";
  $("trie-chips").innerHTML = "";
  box.focus();
};

async function runLyricsSearch() {
  const q = $("lyrics-query").value.trim();
  if (!q) return;
  $("lyrics-results").innerHTML = `<div class="loading-msg">Searching in C++...</div>`;
  const res = await fetch("/api/lyrics", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ query: q }),
  });
  const data = await res.json();
  renderResults("lyrics-results", data.results, data.meta, "similarity", false);
}

async function runHummingFile(file) {
  $("hum-results").innerHTML = `<div class="loading-msg">Extracting MIDI pitches &amp; matching in C++...</div>`;
  $("hum-notes-wrap").classList.add("hidden");
  const fd = new FormData();
  fd.append("file", file);
  const res = await fetch("/api/hum", { method: "POST", body: fd });
  const data = await res.json();
  showExtractedNotes(data.meta);
  renderResults("hum-results", data.results, data.meta, "humming_similarity", true);
}

async function runHummingSample(songId) {
  $("hum-results").innerHTML = `<div class="loading-msg">Matching sample track in C++...</div>`;
  $("hum-notes-wrap").classList.add("hidden");
  const res = await fetch("/api/hum", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ song_id: songId }),
  });
  const data = await res.json();
  showExtractedNotes(data.meta);
  renderResults("hum-results", data.results, data.meta, "humming_similarity", true);
}

function showExtractedNotes(meta) {
  const notes = meta && meta.extracted_notes ? meta.extracted_notes : [];
  if (!notes.length) return;
  $("hum-notes-summary").textContent = `View extracted MIDI notes sent to C++ (${notes.length} notes)`;
  $("hum-notes-code").textContent = "[" + notes.slice(0, 80).join(", ") + (notes.length > 80 ? ", ..." : "") + "]";
  $("hum-notes-wrap").classList.remove("hidden");
}

async function runAudioFile(file) {
  $("audio-results").innerHTML = `<div class="loading-msg">Matching audio features...</div>`;
  const fd = new FormData();
  fd.append("file", file);
  const res = await fetch("/api/audio", { method: "POST", body: fd });
  const data = await res.json();
  renderResults("audio-results", data.results, data.meta, "audio_similarity", true);
}

async function runAudioSample(songId) {
  $("audio-results").innerHTML = `<div class="loading-msg">Matching sample track...</div>`;
  const res = await fetch("/api/audio", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ song_id: songId }),
  });
  const data = await res.json();
  renderResults("audio-results", data.results, data.meta, "audio_similarity", true);
}

async function runRecommendations() {
  const songId = Number($("rec-song-select").value);
  const mood = $("rec-mood-select").value;
  $("rec-results").innerHTML = `<div class="loading-msg">Traversing C++ Weighted Graph...</div>`;
  const res = await fetch("/api/recommend", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ song_id: songId, mood }),
  });
  const data = await res.json();
  renderResults("rec-results", data.results, data.meta, "recommendation_score", true);

  const srcSong = state.catalog.find((s) => s.song_id === songId);
  const edges = data.edges || [];
  if (edges.length) {
    $("rec-graph-wrap").classList.remove("hidden");
    $("rec-edges-grid").innerHTML = edges
      .map(
        (e) => `
        <div class="edge-item">
          <div><strong>${srcSong ? srcSong.title : songId} → ${e.target_title}</strong></div>
          <div class="meta-line">Weight: <code>${Number(e.weight).toFixed(3)}</code> · Fit ${Number(e.fit_pct).toFixed(0)}%</div>
        </div>`
      )
      .join("");
  }
}

// ------------------------------------------------------------------
// DLL & Stack
// ------------------------------------------------------------------
async function refreshDLL(action) {
  const res = await fetch("/api/playlist", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ ids: state.dllIds, cursor: state.dllCursor, action }),
  });
  const data = await res.json();
  if (action === "next" && data.moved) state.dllCursor = Math.min(state.dllIds.length - 1, state.dllCursor + 1);
  if (action === "prev" && data.moved) state.dllCursor = Math.max(0, state.dllCursor - 1);

  $("dll-current").innerHTML = `Now Playing (<code>curr</code>): <strong>${data.current_title}</strong>`;
  const nodes = (data.nodes || [])
    .map((n, i) => `<span class="dll-node ${i === state.dllCursor ? "active" : ""}">${n.title}</span>`)
    .join(`<span class="dll-sep">⇄</span>`);
  $("dll-visual").innerHTML = `<span class="dll-sep">NULL ⇄</span>${nodes}<span class="dll-sep">⇄ NULL</span>`;
}

async function saveCustomPlaylist() {
  const name = $("pl-name").value.trim();
  const mood = $("pl-mood").value.trim();
  const ids = Array.from(document.querySelectorAll("#pl-checkboxes input:checked")).map((c) => Number(c.value));
  if (!name || !ids.length) return alert("Enter a playlist name and select at least one song.");
  const res = await fetch("/api/playlist/save", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name, mood, ids }),
  });
  const data = await res.json();
  if (data.error) return alert(data.error);
  $("pl-name").value = "";
  $("pl-mood").value = "";
  renderPlaylistsList(data.playlists || []);
}

async function loadSavedPlaylists() {
  const res = await fetch("/api/playlists");
  const data = await res.json();
  renderPlaylistsList(data.playlists || []);
}

function renderPlaylistsList(playlists) {
  if (!playlists.length) {
    $("pl-saved-list").innerHTML = `<p class="meta-line">No saved playlists yet.</p>`;
    return;
  }
  $("pl-saved-list").innerHTML = `
    <table class="clean-table">
      <thead><tr><th>ID</th><th>Name</th><th>Mood</th></tr></thead>
      <tbody>${playlists.map((p) => `<tr><td>#${p.playlist_id}</td><td>${p.playlist_name}</td><td>${p.mood || "—"}</td></tr>`).join("")}</tbody>
    </table>`;
}

async function refreshStack(action) {
  const res = await fetch("/api/stack", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ ids: state.stackIds, action }),
  });
  const data = await res.json();
  const items = data.items || [];
  $("stack-visual").innerHTML = items.length
    ? items
        .map(
          (it, idx) => `
          <div class="stack-item ${idx === 0 ? "top" : ""}">
            <span>${it.title}</span>
            ${idx === 0 ? `<span class="top-tag">TOP</span>` : `<span class="meta-line" style="margin:0">#${idx + 1}</span>`}
          </div>`
        )
        .join("")
    : `<div class="stack-item"><span class="meta-line" style="margin:0">Empty Stack (topNode == NULL)</span></div>`;
  return data;
}

function renderCatalogTable() {
  $("catalog-table-wrap").innerHTML = `
    <table class="clean-table">
      <thead><tr><th>Title</th><th>Artist</th><th>Mood</th><th>BPM</th><th>Energy</th><th>Spotify</th></tr></thead>
      <tbody>
        ${state.catalog
          .map(
            (s) => `
            <tr>
              <td><strong>${s.title}</strong></td>
              <td>${s.artist}</td>
              <td>${s.mood}</td>
              <td>${s.tempo}</td>
              <td>${s.energy}</td>
              <td><a class="spotify-link" href="${s.spotify_url}" target="_blank" rel="noopener">Open ↗</a></td>
            </tr>`
          )
          .join("")}
      </tbody>
    </table>`;
}

// ------------------------------------------------------------------
// Browser Microphone -> Pure 16-bit PCM WAV Encoder
// ------------------------------------------------------------------
async function startMicRecording() {
  try {
    state.mediaStream = await navigator.mediaDevices.getUserMedia({ audio: true });
    state.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    state.recordingSampleRate = state.audioCtx.sampleRate;
    const source = state.audioCtx.createMediaStreamSource(state.mediaStream);
    state.scriptNode = state.audioCtx.createScriptProcessor(4096, 1, 1);
    state.pcmBuffers = [];

    state.scriptNode.onaudioprocess = (ev) => {
      state.pcmBuffers.push(new Float32Array(ev.inputBuffer.getChannelData(0)));
    };
    source.connect(state.scriptNode);
    state.scriptNode.connect(state.audioCtx.destination);

    state.secondsRecorded = 0;
    $("mic-timer").textContent = "00:00";
    state.timerInterval = setInterval(() => {
      state.secondsRecorded++;
      const mm = String(Math.floor(state.secondsRecorded / 60)).padStart(2, "0");
      const ss = String(state.secondsRecorded % 60).padStart(2, "0");
      $("mic-timer").textContent = `${mm}:${ss}`;
    }, 1000);

    $("btn-mic-start").disabled = true;
    $("btn-mic-stop").disabled = false;
    $("btn-hum-mic").disabled = true;
    $("mic-wave").classList.remove("paused");
  } catch (err) {
    alert("Microphone unavailable: " + err.message);
  }
}

function stopMicRecording() {
  clearInterval(state.timerInterval);
  $("mic-wave").classList.add("paused");
  if (state.scriptNode) state.scriptNode.disconnect();
  if (state.mediaStream) state.mediaStream.getTracks().forEach((t) => t.stop());
  if (state.audioCtx) state.audioCtx.close();

  const totalLen = state.pcmBuffers.reduce((acc, b) => acc + b.length, 0);
  const merged = new Float32Array(totalLen);
  let offset = 0;
  for (const b of state.pcmBuffers) {
    merged.set(b, offset);
    offset += b.length;
  }

  state.recordedWavBlob = encodeWAV(merged, state.recordingSampleRate);
  const preview = $("mic-preview");
  preview.src = URL.createObjectURL(state.recordedWavBlob);
  preview.classList.remove("hidden");

  $("btn-mic-start").disabled = false;
  $("btn-mic-stop").disabled = true;
  $("btn-hum-mic").disabled = false;
}

function encodeWAV(samples, sampleRate) {
  const buffer = new ArrayBuffer(44 + samples.length * 2);
  const view = new DataView(buffer);
  const writeStr = (off, str) => {
    for (let i = 0; i < str.length; i++) view.setUint8(off + i, str.charCodeAt(i));
  };
  writeStr(0, "RIFF");
  view.setUint32(4, 36 + samples.length * 2, true);
  writeStr(8, "WAVE");
  writeStr(12, "fmt ");
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, 1, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * 2, true);
  view.setUint16(32, 2, true);
  view.setUint16(34, 16, true);
  writeStr(36, "data");
  view.setUint32(40, samples.length * 2, true);

  let pos = 44;
  for (let i = 0; i < samples.length; i++, pos += 2) {
    const s = Math.max(-1, Math.min(1, samples[i]));
    view.setInt16(pos, s < 0 ? s * 0x8000 : s * 0x7fff, true);
  }
  return new Blob([view], { type: "audio/wav" });
}

document.addEventListener("DOMContentLoaded", initApp);
