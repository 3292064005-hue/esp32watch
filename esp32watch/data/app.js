const OLED_WIDTH = 128;
const OLED_HEIGHT = 64;
const OLED_BUFFER_SIZE = (OLED_WIDTH * OLED_HEIGHT) / 8;
const OLED_ON_RGBA = [158, 252, 136, 255];
const OLED_OFF_RGBA = [8, 16, 12, 255];

const state = {
  snapshot: null,
  frame: null,
  polling: false
};

let oledContext = null;
let oledImageData = null;

async function api(path, options = {}) {
  const res = await fetch(path, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });

  const text = await res.text();
  let data = {};
  try {
    data = text ? JSON.parse(text) : {};
  } catch (_) {
    data = { ok: false, message: text || "invalid json" };
  }

  if (!res.ok) {
    throw new Error(data.message || `HTTP ${res.status}`);
  }

  return data;
}

async function refreshState() {
  if (state.polling) return;
  state.polling = true;

  try {
    const [snapshot, frame] = await Promise.all([
      api("/api/state"),
      api("/api/display/frame")
    ]);

    state.snapshot = snapshot;
    state.frame = frame;
    renderAll();
  } catch (err) {
    toast(err.message, "error");
  } finally {
    state.polling = false;
  }
}

function renderAll() {
  renderTopbar();
  renderTerminal();
  renderHub();
  renderDiag();
  renderStorage();
  renderOLEDFrame();
}

function splitTags(text) {
  return String(text || "")
    .trim()
    .split(/\s+/)
    .filter(Boolean);
}

function renderTopbar() {
  const s = state.snapshot;
  if (!s) return;

  const page = s.system?.page || "-";
  const timeSource = s.system?.timeSource || "-";
  const wifi = s.wifi?.connected ? "WIFI" : "OFFLINE";
  const safe = s.system?.safeMode ? "SAFE" : "OK";

  document.getElementById("deviceMeta").textContent =
    `PAGE ${page}  TIME ${timeSource}  NET ${wifi}  SYS ${safe}`;

  const tags = splitTags(s.summary?.headerTags);
  document.getElementById("tagBar").innerHTML = tags
    .map(tag => `<span class="status-tag">${tag}</span>`)
    .join("");
}

function renderTerminal() {
  const s = state.snapshot;
  if (!s) return;

  const cards = [
    {
      title: "System",
      value: `${s.system?.page ?? "-"}  ${s.terminal?.systemFace ?? "-"}`,
      meta: `BR ${s.terminal?.brightnessLabel ?? "-"}  ${s.system?.safeMode ? "SAFE" : "READY"}`
    },
    {
      title: "Activity",
      value: `${s.activity?.steps ?? 0} / ${s.activity?.goal ?? 0}`,
      meta: `GOAL ${s.terminal?.activityLabel ?? "0%"}`
    },
    {
      title: "Alarm",
      value: s.alarm?.label || "OFF",
      meta: s.alarm?.ringing ? "RINGING" : (s.alarm?.enabled ? `NEXT ${s.alarm?.nextTime}` : "NO ALARM")
    },
    {
      title: "Sensor",
      value: s.terminal?.sensorLabel || "OFFLINE",
      meta: `${s.sensor?.runtimeState ?? "-"}  CAL ${s.sensor?.calibrationProgress ?? 0}%`
    },
    {
      title: "Network",
      value: s.summary?.networkLine || "SYNC NEEDED",
      meta: s.summary?.networkSubline || "-"
    }
  ];

  document.getElementById("terminalGrid").innerHTML = cards
    .map(card => `
      <article class="terminal-card">
        <span class="terminal-label">${card.title}</span>
        <strong class="terminal-value">${card.value}</strong>
        <span class="terminal-meta">${card.meta}</span>
      </article>
    `)
    .join("");
}

function renderHub() {
  const s = state.snapshot;
  if (!s) return;

  const cards = [
    {
      title: "System",
      tag: s.summary?.headerTags || "READY",
      body: `FACE ${s.terminal?.systemFace ?? "-"}  BR ${s.terminal?.brightnessLabel ?? "-"}`
    },
    {
      title: "Activity",
      tag: s.terminal?.activityLabel || "0%",
      body: `${s.activity?.steps ?? 0} steps  /  ${(s.activity?.goal ?? 0)}`
    },
    {
      title: "Alarm",
      tag: s.alarm?.ringing ? "RING" : (s.alarm?.enabled ? "ARMED" : "OFF"),
      body: s.alarm?.enabled ? `NEXT ${s.alarm?.nextTime}` : "No scheduled alarm"
    },
    {
      title: "Sensor",
      tag: s.summary?.sensorLabel || "OFFLINE",
      body: `${s.sensor?.runtimeState ?? "-"}  Q${s.sensor?.quality ?? "-"}`
    },
    {
      title: "Network",
      tag: s.terminal?.networkLabel || "SYNC",
      body: s.summary?.networkSubline || "-"
    }
  ];

  document.getElementById("hubGrid").innerHTML = cards
    .map(card => `
      <article class="hub-tile">
        <div class="hub-tile-head">
          <span>${card.title}</span>
          <em>${card.tag}</em>
        </div>
        <p>${card.body}</p>
      </article>
    `)
    .join("");
}

function renderDiag() {
  const s = state.snapshot;
  if (!s) return;

  const items = [
    ["STATE", s.summary?.diagLabel || (s.system?.safeMode ? "SAFE" : "OK")],
    ["SAFE", s.system?.safeMode ? "ACTIVE" : "CLEAR"],
    ["FAULT", s.diag?.hasLastFault ? `${s.diag.lastFaultName}` : "NONE"],
    ["SEV", s.diag?.hasLastFault ? `${s.diag.lastFaultSeverity}` : "INFO"],
    ["LOG", s.diag?.hasLastLog ? `${s.diag.lastLogName}` : "NONE"]
  ];

  document.getElementById("diagPanel").innerHTML = items
    .map(([k, v]) => `<div class="mini-item"><span>${k}</span><strong>${v}</strong></div>`)
    .join("");
}

function renderStorage() {
  const s = state.snapshot;
  if (!s) return;

  const items = [
    ["STATE", s.summary?.storageLabel || "OK"],
    ["BACKEND", s.storage?.backend || "-"],
    ["COMMIT", s.storage?.commitState || "-"],
    ["TXN", s.storage?.transactionActive ? "ACTIVE" : "IDLE"],
    ["FLUSH", s.storage?.sleepFlushPending ? "PENDING" : "CLEAR"]
  ];

  document.getElementById("storagePanel").innerHTML = items
    .map(([k, v]) => `<div class="mini-item"><span>${k}</span><strong>${v}</strong></div>`)
    .join("");
}

function formatUptime(ms) {
  const s = Math.floor(ms / 1000);
  const m = Math.floor(s / 60);
  const h = Math.floor(m / 60);
  const d = Math.floor(h / 24);

  if (d > 0) return `${d}d ${h % 24}h`;
  if (h > 0) return `${h}h ${m % 60}m`;
  if (m > 0) return `${m}m ${s % 60}s`;
  return `${s}s`;
}

function formPost(params) {
  return {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
    body: new URLSearchParams(params)
  };
}

async function sendKey(key, eventType) {
  await api("/api/input/key", formPost({ key, event: eventType }));
  toast(`Key ${key}/${eventType}`, "success");
  await refreshState();
}

async function sendCommand(type) {
  await api("/api/command", formPost({ type }));
  toast(`Command: ${type}`, "success");
  await refreshState();
}

function renderPreview(text) {
  document.getElementById("oledPreview").textContent = text || "Draft preview will appear here.";
  document.getElementById("overlayCount").textContent = `${text.length} / 63`;
}

async function sendOverlay() {
  const text = document.getElementById("overlayText").value.trim();
  if (!text) {
    toast("Enter overlay text first", "error");
    return;
  }

  try {
    await api("/api/display/overlay", formPost({ text, durationMs: "1500" }));
    toast("Overlay sent", "success");
    await refreshState();
  } catch (err) {
    toast(err.message, "error");
  }
}

async function clearOverlay() {
  try {
    await api("/api/display/overlay/clear", formPost({}));
    toast("Overlay cleared", "success");
    await refreshState();
  } catch (err) {
    toast(err.message, "error");
  }
}

function getOledContext() {
  if (oledContext) {
    return oledContext;
  }

  const canvas = document.getElementById("oledCanvas");
  if (!canvas) {
    return null;
  }

  oledContext = canvas.getContext("2d", { alpha: false });
  if (!oledContext) {
    return null;
  }

  oledContext.imageSmoothingEnabled = false;
  oledImageData = oledContext.createImageData(OLED_WIDTH, OLED_HEIGHT);
  return oledContext;
}

function hexNibble(ch) {
  const code = ch.charCodeAt(0);
  if (code >= 48 && code <= 57) return code - 48;
  if (code >= 65 && code <= 70) return code - 55;
  if (code >= 97 && code <= 102) return code - 87;
  return -1;
}

function decodeFrameBuffer(hex) {
  if (!hex || hex.length !== OLED_BUFFER_SIZE * 2) {
    return null;
  }

  const bytes = new Uint8Array(OLED_BUFFER_SIZE);
  for (let i = 0; i < OLED_BUFFER_SIZE; i += 1) {
    const hi = hexNibble(hex[i * 2]);
    const lo = hexNibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      return null;
    }
    bytes[i] = (hi << 4) | lo;
  }
  return bytes;
}

function renderOLEDFrame() {
  const frame = state.frame;
  const ctx = getOledContext();
  const statusEl = document.getElementById("oledFrameState");
  if (!ctx || !statusEl) {
    return;
  }

  const bytes = decodeFrameBuffer(frame?.bufferHex || "");
  if (!bytes || !oledImageData) {
    ctx.fillStyle = "#08100c";
    ctx.fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT);
    statusEl.textContent = "Frame unavailable";
    return;
  }

  const pixels = oledImageData.data;
  for (let y = 0; y < OLED_HEIGHT; y += 1) {
    const page = y >> 3;
    const mask = 1 << (y & 7);

    for (let x = 0; x < OLED_WIDTH; x += 1) {
      const index = x + page * OLED_WIDTH;
      const rgbaIndex = (y * OLED_WIDTH + x) * 4;
      const color = (bytes[index] & mask) !== 0 ? OLED_ON_RGBA : OLED_OFF_RGBA;
      pixels[rgbaIndex] = color[0];
      pixels[rgbaIndex + 1] = color[1];
      pixels[rgbaIndex + 2] = color[2];
      pixels[rgbaIndex + 3] = color[3];
    }
  }

  ctx.putImageData(oledImageData, 0, 0);
  statusEl.textContent = `Frame ${frame.presentCount ?? "-"}  UPTIME ${formatUptime(state.snapshot?.system?.uptimeMs ?? 0)}`;
}

function toast(message, type = "info") {
  const host = document.getElementById("toastHost");
  const el = document.createElement("div");
  el.className = `toast toast-${type}`;
  el.textContent = message;
  host.appendChild(el);

  setTimeout(() => {
    el.remove();
  }, 2200);
}

document.addEventListener("DOMContentLoaded", () => {
  document.querySelectorAll(".key").forEach(btn => {
    btn.addEventListener("click", () => {
      const key = btn.dataset.key;
      const eventType = document.getElementById("keyEventType").value;
      sendKey(key, eventType).catch(err => toast(err.message, "error"));
    });
  });

  document.querySelectorAll("[data-command]").forEach(btn => {
    btn.addEventListener("click", () => {
      const cmd = btn.dataset.command;
      sendCommand(cmd).catch(err => toast(err.message, "error"));
    });
  });

  document.getElementById("refreshBtn").addEventListener("click", () => {
    refreshState().catch(err => toast(err.message, "error"));
  });

  document.getElementById("previewBtn").addEventListener("click", () => {
    const text = document.getElementById("overlayText").value;
    renderPreview(text);
  });

  document.getElementById("sendOverlayBtn").addEventListener("click", sendOverlay);
  document.getElementById("clearOverlayBtn").addEventListener("click", clearOverlay);

  document.getElementById("overlayText").addEventListener("input", e => {
    renderPreview(e.target.value);
  });

  renderPreview("");
  setInterval(() => {
    refreshState().catch(err => toast(err.message, "error"));
  }, 1000);
  refreshState().catch(err => toast(err.message, "error"));
});
