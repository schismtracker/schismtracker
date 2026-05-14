#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_BUILD_DIR="${ROOT_DIR}/build-web"
BUILD_DIR="${PROJECT_BUILD_DIR}"
DIST_DIR="${PROJECT_BUILD_DIR}/dist"
CONFIGURE_SRC_DIR="${ROOT_DIR}"

if ! command -v emcc >/dev/null 2>&1; then
  echo "error: emcc not found. Activate emsdk first." >&2
  exit 1
fi

if ! command -v emconfigure >/dev/null 2>&1; then
  echo "error: emconfigure not found. Activate emsdk first." >&2
  exit 1
fi

if ! command -v emmake >/dev/null 2>&1; then
  echo "error: emmake not found. Activate emsdk first." >&2
  exit 1
fi

mkdir -p "${PROJECT_BUILD_DIR}" "${DIST_DIR}"

# Single source of truth for linker flags (pass to make so rebuilds pick up changes
# without re-running configure).
WEB_LDFLAGS="-O2 -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sASSERTIONS=1 -sSTACK_SIZE=1048576 -sEXIT_RUNTIME=0 -sINITIAL_MEMORY=67108864 -sEXPORTED_RUNTIME_METHODS=HEAPU8,ccall,FS -sFORCE_FILESYSTEM -lidbfs.js"

# Autotools may reject source paths containing spaces (unsafe srcdir). Create a
# stable, space-free source+build path under /tmp and configure through that
# path when needed.
if [[ "${ROOT_DIR}" == *" "* ]]; then
  TMP_BASE="/tmp/schismtracker-web"
  SPACELESS_LINK="${TMP_BASE}/src"
  BUILD_DIR="${TMP_BASE}/build"
  mkdir -p "${TMP_BASE}" "${BUILD_DIR}"
  rm -f "${SPACELESS_LINK}"
  ln -s "${ROOT_DIR}" "${SPACELESS_LINK}"
  CONFIGURE_SRC_DIR="${SPACELESS_LINK}"
fi

if [ ! -f "${ROOT_DIR}/configure" ]; then
  echo "info: generating configure script via autoreconf"
  (cd "${ROOT_DIR}" && autoreconf -i)
fi

# schismico*.c used to live under schism/auto/; stale build Makefiles still point
# emcc at schism/auto/schismtracker-schismico_hires.o after the move.
if [ -f "${BUILD_DIR}/Makefile" ] \
  && grep -q 'schism/auto/schismico_hires' "${BUILD_DIR}/Makefile" 2>/dev/null \
  && [ -f "${CONFIGURE_SRC_DIR}/schism/schismico_hires.c" ]; then
  echo "info: web build Makefile is stale (schismico path); removing for reconfigure" >&2
  rm -f "${BUILD_DIR}/Makefile" "${BUILD_DIR}/config.status" "${BUILD_DIR}/config.log"
fi

cd "${BUILD_DIR}"

# Out-of-tree configure refuses a srcdir that still has in-tree configure
# artifacts (config.status). That happens when ./configure was run in the
# source tree for a native build, then this script uses build-web/ or /tmp/...
# as a separate build directory — including when CONFIGURE_SRC_DIR is a
# symlink to ROOT_DIR (paths with spaces).
if [ ! -f "${BUILD_DIR}/Makefile" ] && [ -f "${ROOT_DIR}/config.status" ]; then
  echo "info: clearing native in-tree configure state so emscripten can configure out-of-tree..."
  if [ -f "${ROOT_DIR}/Makefile" ]; then
    if ! (cd "${ROOT_DIR}" && make distclean); then
      echo "warn: make distclean failed; removing config.status so configure can proceed" >&2
      rm -f "${ROOT_DIR}/config.status" "${ROOT_DIR}/config.log" "${ROOT_DIR}/libtool"
    fi
  else
    rm -f "${ROOT_DIR}/config.status" "${ROOT_DIR}/config.log" "${ROOT_DIR}/libtool"
  fi
fi

if [ ! -f "${BUILD_DIR}/Makefile" ]; then
  emconfigure "${CONFIGURE_SRC_DIR}/configure" \
    --host=wasm32-unknown-emscripten \
    --with-sdl2=yes \
    --with-sdl12=no \
    --with-sdl3=no \
    --with-jack=no \
    --with-alsa=no \
    --enable-threads=no \
    --enable-sdl2-linking=yes \
    --enable-x11-linking=no \
    CFLAGS="-O2" \
    LDFLAGS="${WEB_LDFLAGS}"
fi

emmake make -j"$(getconf _NPROCESSORS_ONLN || echo 4)" LDFLAGS="${WEB_LDFLAGS}"

# Link a browser target from the built executable objects by rebuilding with an
# explicit JS output name if needed. Some autotools setups emit an ELF-like
# executable name that does not include the web extension by default.
if [ -f "${BUILD_DIR}/schismtracker.js" ]; then
  cp -f "${BUILD_DIR}/schismtracker.js" "${DIST_DIR}/schismtracker.js"
  [ -f "${BUILD_DIR}/schismtracker.wasm" ] && cp -f "${BUILD_DIR}/schismtracker.wasm" "${DIST_DIR}/schismtracker.wasm"
  [ -f "${BUILD_DIR}/schismtracker.data" ] && cp -f "${BUILD_DIR}/schismtracker.data" "${DIST_DIR}/schismtracker.data"
elif [ -f "${BUILD_DIR}/schismtracker" ] && [ -f "${BUILD_DIR}/schismtracker.wasm" ]; then
  cp -f "${BUILD_DIR}/schismtracker" "${DIST_DIR}/schismtracker.js"
  cp -f "${BUILD_DIR}/schismtracker.wasm" "${DIST_DIR}/schismtracker.wasm"
  [ -f "${BUILD_DIR}/schismtracker.data" ] && cp -f "${BUILD_DIR}/schismtracker.data" "${DIST_DIR}/schismtracker.data"
elif [ -f "${BUILD_DIR}/schismtracker.html" ]; then
  cp -f "${BUILD_DIR}/schismtracker.html" "${DIST_DIR}/index.html"
  [ -f "${BUILD_DIR}/schismtracker.js" ] && cp -f "${BUILD_DIR}/schismtracker.js" "${DIST_DIR}/schismtracker.js"
  [ -f "${BUILD_DIR}/schismtracker.wasm" ] && cp -f "${BUILD_DIR}/schismtracker.wasm" "${DIST_DIR}/schismtracker.wasm"
  [ -f "${BUILD_DIR}/schismtracker.data" ] && cp -f "${BUILD_DIR}/schismtracker.data" "${DIST_DIR}/schismtracker.data"
else
  echo "warn: expected web artifacts not found in ${BUILD_DIR}" >&2
  echo "warn: inspect linker output and adjust configure/make flags for this platform." >&2
fi

cat > "${DIST_DIR}/index.html" <<'HTML'
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Schism Tracker (WASM)</title>
    <style>
      :root { color-scheme: dark; }
      body {
        margin: 0;
        font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
        background: #111;
        color: #ddd;
      }
      header {
        padding: 7px 10px;
        border-bottom: 1px solid #2a2a2a;
        font-size: 12px;
        display: flex;
        align-items: center;
        gap: 6px;
        flex-wrap: nowrap;
        overflow-x: auto;
        white-space: nowrap;
      }
      .header-title {
        color: #d7d7d7;
        font-weight: 600;
        margin-right: 4px;
        flex: 0 0 auto;
      }
      .header-group {
        display: inline-flex;
        align-items: center;
        gap: 4px;
        padding: 2px 5px;
        border: 1px solid #2c2c2c;
        border-radius: 5px;
        background: #181818;
        flex: 0 0 auto;
      }
      #status { color: #9ad; }
      #midiStatus { color: #7c9; }
      .status-item {
        padding: 0 2px;
        white-space: nowrap;
        flex: 0 0 auto;
      }
      button {
        font: inherit;
        color: #ddd;
        background: #222;
        border: 1px solid #3a3a3a;
        border-radius: 4px;
        padding: 3px 6px;
        cursor: pointer;
      }
      select {
        font: inherit;
        color: #ddd;
        background: #222;
        border: 1px solid #3a3a3a;
        border-radius: 4px;
        padding: 3px 6px;
      }
      /* Canvas CSS size is set only in updateCanvasScale() so 640×400 logical
       * aspect is preserved (Fit = letterbox inside the viewport below header). */
      canvas {
        display: block;
        margin: 0;
        image-rendering: pixelated;
      }
    </style>
  </head>
  <body>
    <header>
      <span class="header-title">Schism Web</span>
      <div class="header-group">
        <button id="openBtn" type="button">Open</button>
        <input id="openFile" type="file" hidden />
      </div>
      <div class="header-group">
        <select id="storedModules" aria-label="Stored modules">
          <option value="">Stored</option>
        </select>
        <select id="storedAction" aria-label="Stored module action">
          <option value="load">Load</option>
          <option value="refresh">Refresh</option>
          <option value="rename">Rename</option>
          <option value="delete">Delete</option>
        </select>
        <button id="storedActionRunBtn" type="button">Go</button>
      </div>
      <div class="header-group">
        <select id="saveFormat" aria-label="Save format">
          <option value="IT">IT</option>
          <option value="S3M">S3M</option>
        </select>
        <button id="saveBtn" type="button">Save</button>
      </div>
      <div class="header-group">
        <select id="scaleMode" aria-label="Display scale">
          <option value="fit" selected>Fit</option>
          <option value="1">x1</option>
          <option value="2">x2</option>
          <option value="3">x3</option>
        </select>
      </div>
      <span id="midiStatus" class="status-item">MIDI: initializing...</span>
      <span id="status" class="status-item">loading...</span>
    </header>
    <canvas id="canvas" tabindex="1"></canvas>
    <script>
      const statusEl = document.getElementById("status");
      const openBtn = document.getElementById("openBtn");
      const storedModules = document.getElementById("storedModules");
      const storedAction = document.getElementById("storedAction");
      const storedActionRunBtn = document.getElementById("storedActionRunBtn");
      const scaleMode = document.getElementById("scaleMode");
      const saveFormat = document.getElementById("saveFormat");
      const saveBtn = document.getElementById("saveBtn");
      const openFile = document.getElementById("openFile");
      const midiStatusEl = document.getElementById("midiStatus");
      const headerEl = document.querySelector("header");
      /** True after user clicks the tracker canvas; cleared on header mousedown */
      let schismKeyboardGameFocus = false;
      const schismLogicalWidth = 640;
      const schismLogicalHeight = 400;

      function schismFsMkdirp(fs, path) {
        const parts = path.split("/").filter(Boolean);
        let acc = "";
        for (const p of parts) {
          acc += "/" + p;
          const analyzed = typeof fs.analyzePath === "function" ? fs.analyzePath(acc) : null;
          if (analyzed && analyzed.exists && analyzed.object && analyzed.object.isFolder) {
            continue;
          }
          try {
            fs.mkdir(acc);
          } catch (e) {
            const code = e && (e.errno !== undefined ? e.errno : e.code);
            const check = typeof fs.analyzePath === "function" ? fs.analyzePath(acc) : null;
            if (check && check.exists && check.object && check.object.isFolder) {
              continue;
            }
            if (code !== 17 && code !== "EEXIST" && code !== 10 && code !== "EBUSY") throw e;
          }
        }
      }

      function schismSanitizeImportName(name) {
        const base = (name || "module").replace(/[^A-Za-z0-9._-]/g, "_");
        const s = base.slice(0, 200);
        return s || "module.bin";
      }

      let schismIdbReady = false;
      let schismIdbSupported = false;
      let schismSyncPromise = null;
      let schismWebMIDIInitialized = false;
      let schismWebMIDIAccess = null;

      function schismUpdateMIDIStatus(text, isError) {
        if (!midiStatusEl) return;
        midiStatusEl.textContent = text;
        midiStatusEl.style.color = isError ? "#f99" : "#7c9";
      }

      function schismRefreshMIDIStatusCounts() {
        if (!schismWebMIDIAccess) {
          schismUpdateMIDIStatus("MIDI: unavailable", true);
          return;
        }
        let inCount = 0;
        let outCount = 0;
        if (schismWebMIDIAccess.inputs) {
          for (const _ of schismWebMIDIAccess.inputs.values()) inCount++;
        }
        if (schismWebMIDIAccess.outputs) {
          for (const _ of schismWebMIDIAccess.outputs.values()) outCount++;
        }
        schismUpdateMIDIStatus(`MIDI: in ${inCount}, out ${outCount}`, false);
      }

      function schismAttachWebMIDIInputs() {
        if (!schismWebMIDIAccess || !schismWebMIDIAccess.inputs) {
          schismRefreshMIDIStatusCounts();
          return;
        }
        for (const input of schismWebMIDIAccess.inputs.values()) {
          input.onmidimessage = (ev) => {
            try {
              const bytes = ev && ev.data ? new Uint8Array(ev.data) : null;
              if (!bytes || bytes.length === 0) return;
              if (!Module || typeof Module.ccall !== "function") return;
              Module.ccall(
                "schism_web_midi_receive",
                "number",
                ["array", "number"],
                [bytes, bytes.length]
              );
            } catch (err) {
              console.error("WebMIDI input bridge failed", err);
            }
          };
        }
        schismRefreshMIDIStatusCounts();
      }

      function schismInitWebMIDI() {
        if (schismWebMIDIInitialized) return;
        schismWebMIDIInitialized = true;
        schismUpdateMIDIStatus("MIDI: requesting access...", false);
        if (!navigator.requestMIDIAccess) {
          console.warn("WebMIDI is not supported in this browser.");
          schismUpdateMIDIStatus("MIDI: unsupported", true);
          return;
        }
        navigator.requestMIDIAccess({ sysex: false }).then(
          (access) => {
            schismWebMIDIAccess = access;
            schismAttachWebMIDIInputs();
            access.onstatechange = () => {
              schismAttachWebMIDIInputs();
            };
          },
          (err) => {
            console.warn("WebMIDI access denied/unavailable", err);
            schismUpdateMIDIStatus("MIDI: permission denied/unavailable", true);
          }
        );
      }

      function schismWebMIDISend(ptr, len) {
        try {
          if (!schismWebMIDIAccess || !schismWebMIDIAccess.outputs || !Module || !Module.HEAPU8) return;
          const end = ptr + len;
          const bytes = Uint8Array.from(Module.HEAPU8.subarray(ptr, end));
          for (const output of schismWebMIDIAccess.outputs.values()) {
            output.send(bytes);
          }
        } catch (err) {
          console.error("WebMIDI output bridge failed", err);
        }
      }

      function schismMountIdbfs() {
        const FS = Module.FS;
        if (typeof ENV === "object") {
          ENV.HOME = "/persistent";
        }
        const add =
          typeof addRunDependency === "function"
            ? addRunDependency
            : Module.addRunDependency;
        const rem =
          typeof removeRunDependency === "function"
            ? removeRunDependency
            : Module.removeRunDependency;
        if (!FS || !FS.filesystems || !FS.filesystems.IDBFS) {
          console.warn("Schism web: IDBFS not available; data will not persist.");
          return;
        }
        schismIdbSupported = true;
        add("schism_idbfs");
        try {
          FS.mkdir("/persistent");
        } catch (e) {
          /* exists */
        }
        try {
          FS.mount(FS.filesystems.IDBFS, {}, "/persistent");
        } catch (e) {
          console.error("IDBFS mount failed", e);
          rem("schism_idbfs");
          return;
        }
        FS.syncfs(true, function (err) {
          if (err) {
            console.error("IDBFS load from storage:", err);
          } else {
            schismIdbReady = true;
            try {
              schismFsMkdirp(FS, "/persistent/.config/schism");
              schismFsMkdirp(FS, "/persistent/modules");
              FS.chdir("/persistent");
            } catch (e) {
              console.warn("Unable to prepare /persistent dirs", e);
            }
          }
          rem("schism_idbfs");
        });
      }

      function schismSyncToIDB() {
        const FS = Module.FS;
        if (!schismIdbSupported || !schismIdbReady) return Promise.resolve(false);
        if (!FS || !FS.filesystems || !FS.filesystems.IDBFS) return Promise.resolve(false);
        if (schismSyncPromise) return schismSyncPromise;
        try {
          schismSyncPromise = new Promise((resolve) => {
            FS.syncfs(false, function (err) {
              if (err) {
                console.error("IDBFS save to storage:", err);
                resolve(false);
              } else {
                resolve(true);
              }
              schismSyncPromise = null;
            });
          });
          return schismSyncPromise;
        } catch (e) {
          console.error(e);
          schismSyncPromise = null;
          return Promise.resolve(false);
        }
      }

      function inferExtKind(filename) {
        const m = /\.([^.]+)$/.exec((filename || "").toLowerCase());
        const ext = m ? m[1] : "";
        switch (ext) {
          case "it": return 0;
          case "s3m": return 1;
          case "xm": return 2;
          case "mod": return 3;
          case "mtm": return 4;
          case "stm": return 5;
          case "669": return 6;
          case "ult": return 7;
          default: return 8;
        }
      }

      function listStoredModules() {
        if (!storedModules) return [];
        if (!Module.FS || typeof Module.FS.readdir !== "function") return [];
        try {
          schismFsMkdirp(Module.FS, "/persistent/modules");
          const entries = Module.FS.readdir("/persistent/modules")
            .filter((name) => name !== "." && name !== "..")
            .map((name) => {
              let mtime = 0;
              try {
                const st = Module.FS.stat(`/persistent/modules/${name}`);
                if (st && st.mtime) {
                  mtime = st.mtime instanceof Date ? st.mtime.getTime() : Number(st.mtime) || 0;
                }
              } catch (e) {
                /* ignore stat failures and keep default mtime */
              }
              return { name, mtime };
            })
            .sort((a, b) => {
              if (b.mtime !== a.mtime) return b.mtime - a.mtime;
              return a.name.localeCompare(b.name, "en");
            })
            .map((x) => x.name);
          return entries;
        } catch (e) {
          console.warn("Unable to read /persistent/modules", e);
          return [];
        }
      }

      function refreshStoredModulesList() {
        if (!storedModules) return;
        const prev = storedModules.value;
        storedModules.innerHTML = "";
        const placeholder = document.createElement("option");
        placeholder.value = "";
        placeholder.textContent = "(stored modules)";
        storedModules.appendChild(placeholder);
        const entries = listStoredModules();
        for (const name of entries) {
          const option = document.createElement("option");
          option.value = name;
          option.textContent = name;
          storedModules.appendChild(option);
        }
        if (prev && entries.includes(prev)) {
          storedModules.value = prev;
        } else {
          storedModules.value = "";
        }
      }

      function requestOpenFromPath(vfsPath, displayName) {
        try {
          const shownName = displayName && displayName.length > 0
            ? displayName
            : vfsPath.replace(/^.*\//, "");
          const rc = Module.ccall(
            "schism_web_open_vfs_path",
            "number",
            ["string", "string"],
            [vfsPath, shownName]
          );
          if (rc !== 0) {
            statusEl.textContent = `load request failed (${rc})`;
            return;
          }
          statusEl.textContent = `loading: ${shownName}`;
          const startedAt = Date.now();
          const poll = () => {
            const r = Module._schism_web_open_result_get();
            if (r === 1) {
              statusEl.textContent = `loaded: ${shownName}`;
              schismSyncToIDB();
              refreshStoredModulesList();
              return;
            }
            if (r < 0) {
              statusEl.textContent = `load failed: ${shownName}`;
              return;
            }
            if (Date.now() - startedAt > 8000) {
              statusEl.textContent = `load timeout: ${shownName}`;
              return;
            }
            setTimeout(poll, 30);
          };
          poll();
        } catch (err) {
          console.error("web open path bridge error", err);
          statusEl.textContent = `open bridge error: ${err && err.message ? err.message : err}`;
        }
      }

      async function requestOpenFromFile(file) {
        try {
          if (!file) return;
          const buffer = new Uint8Array(await file.arrayBuffer());
          const extKind = inferExtKind(file.name);
          const extMap = ["it", "s3m", "xm", "mod", "mtm", "stm", "669", "ult", "bin"];
          const ext = extKind >= 0 && extKind < extMap.length ? extMap[extKind] : "bin";
          /* Write via MemFS from JS; passing a huge Uint8Array through ccall("array")
             can overflow or trip WASM bounds checks. String path + FS is one copy only. */
          if (!Module.FS || typeof Module.FS.writeFile !== "function") {
            statusEl.textContent = "open bridge error: FS API not available (rebuild with FS exported)";
            return;
          }
          const safe = schismSanitizeImportName(file.name);
          let vfsPath;
          try {
            schismFsMkdirp(Module.FS, "/persistent/modules");
            vfsPath = `/persistent/modules/${safe}`;
            Module.FS.writeFile(vfsPath, buffer);
            await schismSyncToIDB();
          } catch (writeErr) {
            console.warn("persistent FS write failed, using /tmp", writeErr);
            try {
              vfsPath = `/tmp/${safe}`;
              Module.FS.writeFile(vfsPath, buffer);
            } catch (e2) {
              console.error("FS.writeFile failed", e2);
              statusEl.textContent = `open bridge error: ${e2 && e2.message ? e2.message : e2}`;
              return;
            }
          }
          const displayName =
            file.name && String(file.name).length > 0
              ? file.name
              : vfsPath.replace(/^.*\//, "");
          requestOpenFromPath(vfsPath, displayName);
        } catch (err) {
          console.error("web open js bridge error", err);
          statusEl.textContent = `open bridge error: ${err && err.message ? err.message : err}`;
        }
      }

      function downloadBytes(bytes, filename) {
        const blob = new Blob([bytes], { type: "application/octet-stream" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
      }

      function sanitizeSaveName(name) {
        const cleaned = (name || "").trim().replace(/[^A-Za-z0-9._-]/g, "_");
        if (!cleaned) return "schism-export.it";
        if (/\.[^.]+$/.test(cleaned)) return cleaned;
        return `${cleaned}.it`;
      }

      function extensionForFormat(fmt) {
        switch ((fmt || "").toUpperCase()) {
          case "S3M": return "s3m";
          case "IT":
          default: return "it";
        }
      }

      function updateCanvasScale() {
        const canvas = Module.canvas;
        if (!canvas) return;
        const baseW = schismLogicalWidth;
        const baseH = schismLogicalHeight;
        const mode = scaleMode ? scaleMode.value : "fit";
        canvas.style.display = "block";
        canvas.style.margin = "0";
        if (mode === "fit") {
          const headerH = headerEl ? Math.ceil(headerEl.getBoundingClientRect().height) : 42;
          const availW = window.innerWidth;
          const availH = Math.max(120, window.innerHeight - headerH);
          const sx = availW / baseW;
          const sy = availH / baseH;
          const s = Math.min(sx, sy);
          const w = Math.max(1, Math.floor(baseW * s));
          const h = Math.max(1, Math.floor(baseH * s));
          canvas.style.width = `${w}px`;
          canvas.style.height = `${h}px`;
          return;
        }
        const mult = Number(mode);
        if (!Number.isFinite(mult) || mult <= 0) return;
        canvas.style.width = `${Math.round(baseW * mult)}px`;
        canvas.style.height = `${Math.round(baseH * mult)}px`;
      }

      async function runStoredAction() {
        const action = storedAction && storedAction.value ? storedAction.value : "load";
        const selected = storedModules && storedModules.value ? storedModules.value : "";
        if (action === "refresh") {
          refreshStoredModulesList();
          statusEl.textContent = "stored modules refreshed";
          return;
        }
        if (action === "load") {
          if (!selected) {
            statusEl.textContent = "select a stored module first";
            return;
          }
          requestOpenFromPath(`/persistent/modules/${selected}`, selected);
          return;
        }
        if (action === "rename") {
          if (!selected) {
            statusEl.textContent = "select a stored module to rename";
            return;
          }
          if (!Module.FS || typeof Module.FS.rename !== "function") {
            statusEl.textContent = "rename failed: FS API unavailable";
            return;
          }
          const inputName = window.prompt("New file name", selected);
          if (inputName === null) {
            statusEl.textContent = "rename canceled";
            return;
          }
          const next = schismSanitizeImportName(inputName);
          if (!next) {
            statusEl.textContent = "rename failed: invalid name";
            return;
          }
          if (next === selected) {
            statusEl.textContent = "rename skipped: same name";
            return;
          }
          try {
            const dst = `/persistent/modules/${next}`;
            if (Module.FS.analyzePath(dst).exists) {
              statusEl.textContent = `rename failed: '${next}' already exists`;
              return;
            }
            Module.FS.rename(`/persistent/modules/${selected}`, dst);
            await schismSyncToIDB();
            refreshStoredModulesList();
            storedModules.value = next;
            statusEl.textContent = `renamed: ${selected} -> ${next}`;
          } catch (err) {
            console.error("rename stored module failed", err);
            statusEl.textContent = `rename failed: ${err && err.message ? err.message : err}`;
          }
          return;
        }
        if (action === "delete") {
          if (!selected) {
            statusEl.textContent = "select a stored module to delete";
            return;
          }
          if (!Module.FS || typeof Module.FS.unlink !== "function") {
            statusEl.textContent = "delete failed: FS API unavailable";
            return;
          }
          const ok = window.confirm(`Delete stored module '${selected}'?`);
          if (!ok) {
            statusEl.textContent = "delete canceled";
            return;
          }
          try {
            Module.FS.unlink(`/persistent/modules/${selected}`);
            await schismSyncToIDB();
            refreshStoredModulesList();
            statusEl.textContent = `deleted: ${selected}`;
          } catch (err) {
            console.error("delete stored module failed", err);
            statusEl.textContent = `delete failed: ${err && err.message ? err.message : err}`;
          }
        }
      }

      window.Module = {
        preRun: [schismMountIdbfs],
        canvas: document.getElementById("canvas"),
        schismInitWebMIDI,
        schismWebMIDISend,
        setStatus(text) {
          statusEl.textContent = text;
        },
        onRuntimeInitialized() {
          statusEl.textContent = "ready (click canvas to focus)";
          /* Browsers often bind F1–F12 (help, devtools, etc.). When the tracker
           * canvas is focused, reserve those keys for SDL/WASM. */
          if (Module.canvas) {
            Module.canvas.style.outline = "none";
            Module.canvas.addEventListener("mousedown", () => {
              schismKeyboardGameFocus = true;
              try {
                Module.canvas.focus();
              } catch (_e) {
                /* ignore */
              }
            });
          }
          if (headerEl) {
            headerEl.addEventListener("mousedown", () => {
              schismKeyboardGameFocus = false;
            });
          }
          let schismWebCfgComboTs = 0;
          function schismWebIsPhysicalF1(ev) {
            /* Chrome often reserves Ctrl+F1; key may be "" or "Unidentified" but code stays F1 */
            return (
              ev.key === "F1" ||
              ev.code === "F1" ||
              ev.code === "Help"
            );
          }
          function schismWebFunctionKeyNumber(ev) {
            if (ev.key && ev.key.length >= 2 && ev.key[0] === "F") {
              const n = parseInt(ev.key.slice(1), 10);
              if (n >= 1 && n <= 12) return n;
            }
            if (ev.code && ev.code.length >= 2 && ev.code[0] === "F") {
              const n = parseInt(ev.code.slice(1), 10);
              if (n >= 1 && n <= 12) return n;
            }
            return 0;
          }
          function schismWebMaybeOpenConfigPages(ev) {
            if (!Module || !Module.canvas || typeof Module.ccall !== "function") return;
            let captureActive = 0;
            try {
              captureActive = Module.ccall("schism_web_shortcut_capture_active", "number", [], []);
            } catch (_err) {
              captureActive = 0;
            }
            if (headerEl && ev.target && typeof ev.target.closest === "function" && ev.target.closest("header")) {
              return;
            }
            const useGameKeys =
              schismKeyboardGameFocus || document.activeElement === Module.canvas;
            if (!useGameKeys) return;
            if (!schismWebIsPhysicalF1(ev)) return;
            if (!ev.ctrlKey || ev.altKey || ev.metaKey) return;
            if (captureActive) {
              ev.preventDefault();
              return;
            }
            if (ev.type === "keydown" && ev.repeat) return;
            /* keydown may never fire for Ctrl+F1; keyup on F1 usually does — debounce both */
            const t = Date.now();
            if (t - schismWebCfgComboTs < 420) return;
            schismWebCfgComboTs = t;
            ev.preventDefault();
            try {
              if (ev.shiftKey) {
                Module.ccall("schism_web_open_shortcut_config", null, [], []);
              } else {
                Module.ccall("schism_web_open_system_config", null, [], []);
              }
            } catch (err) {
              console.error("schism web config shortcut", err);
            }
          }
          document.addEventListener("keydown", function (ev) {
            if (!Module || !Module.canvas) return;
            schismWebMaybeOpenConfigPages(ev);
            if (headerEl && ev.target && typeof ev.target.closest === "function" && ev.target.closest("header")) {
              return;
            }
            const useGameKeys =
              schismKeyboardGameFocus || document.activeElement === Module.canvas;
            if (!useGameKeys) return;
            const fn = schismWebFunctionKeyNumber(ev);
            if (fn >= 1 && fn <= 12) {
              ev.preventDefault();
            }
          }, true);
          document.addEventListener("keyup", schismWebMaybeOpenConfigPages, true);

          window.addEventListener("beforeunload", schismSyncToIDB);
          document.addEventListener("visibilitychange", function () {
            if (document.visibilityState === "hidden") schismSyncToIDB();
          });
          setInterval(schismSyncToIDB, 120000);
          refreshStoredModulesList();
          updateCanvasScale();
          if (scaleMode) {
            scaleMode.onchange = () => updateCanvasScale();
          }
          window.addEventListener("resize", updateCanvasScale);

          openBtn.onclick = () => openFile.click();
          openFile.onchange = async () => {
            const file = openFile.files && openFile.files[0];
            if (!file) return;
            await requestOpenFromFile(file);
            openFile.value = "";
          };
          if (storedActionRunBtn) {
            storedActionRunBtn.onclick = () => {
              runStoredAction();
            };
          }

          const dropTarget = Module.canvas;
          const stop = (ev) => {
            ev.preventDefault();
            ev.stopPropagation();
          };
          ["dragenter", "dragover", "dragleave", "drop"].forEach((name) => {
            dropTarget.addEventListener(name, stop);
          });
          dropTarget.addEventListener("drop", async (ev) => {
            const file = ev.dataTransfer && ev.dataTransfer.files && ev.dataTransfer.files[0];
            if (file) await requestOpenFromFile(file);
          });

          saveBtn.onclick = () => {
            if (typeof Module._schism_web_save_now !== "function") {
              statusEl.textContent = "web save is not exported in this build";
              return;
            }
            try {
              const fmt = (saveFormat && saveFormat.value) ? saveFormat.value : "IT";
              statusEl.textContent = "saving...";
              const rc = Module.ccall("schism_web_save_now", "number", ["string"], [fmt]);
              if (rc !== 0) {
                statusEl.textContent = `save failed (${rc})`;
                Module._schism_web_export_clear();
                return;
              }

              const size = Module._schism_web_export_size();
              if (size <= 0) {
                statusEl.textContent = "save produced empty file";
                Module._schism_web_export_clear();
                return;
              }

              const ptrRaw = Module._schism_web_export_ptr();
              const ptr = (typeof ptrRaw === "bigint") ? Number(ptrRaw) : Number(ptrRaw);
              if (!Number.isFinite(ptr) || ptr <= 0) {
                statusEl.textContent = "save bridge bad pointer";
                Module._schism_web_export_clear();
                return;
              }
              const bytes = Uint8Array.from(Module.HEAPU8.subarray(ptr, ptr + size));
              const defaultName = `schism-export.${extensionForFormat(fmt)}`;
              const inputName = window.prompt("Save file name", defaultName);
              if (inputName === null) {
                Module._schism_web_export_clear();
                statusEl.textContent = "save canceled";
                return;
              }
              const outName = sanitizeSaveName(inputName).replace(/\.(it|s3m)$/i, `.${extensionForFormat(fmt)}`);
              downloadBytes(bytes, outName);
              Module._schism_web_export_clear();
              statusEl.textContent = `saved: ${outName}`;
            } catch (err) {
              console.error("web save js bridge error", err);
              statusEl.textContent = `save bridge error: ${err && err.message ? err.message : err}`;
            }
          };
        }
      };
    </script>
    <script src="./schismtracker.js"></script>
  </body>
</html>
HTML

echo "done: web artifacts available in ${DIST_DIR}"
