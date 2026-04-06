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
  renderStatus();
  renderDiag();
  renderOLEDFrame();
}

function renderTopbar() {
  const s = state.snapshot;
  if (!s) return;

  const ip = s.wifi?.ip || "-";
  const page = s.system?.page || "-";
  const timeSource = s.system?.timeSource || "-";
  const safeMode = s.system?.safeMode ? "SAFE MODE" : "NORMAL";

  document.getElementById("deviceMeta").textContent =
    `IP ${ip} | PAGE ${page} | TIME ${timeSource} | ${safeMode}`;
}

function renderStatus() {
  const s = state.snapshot;
  if (!s) return;

  const items = [
    ["WiFi", s.wifi?.connected ? "Connected" : "Offline"],
    ["RSSI", `${s.wifi?.rssi ?? "-"} dBm`],
    ["Page", s.system?.page ?? "-"],
    ["Time Source", s.system?.timeSource ?? "-"],
    ["Uptime", formatUptime(s.system?.uptimeMs ?? 0)],
    ["Steps", String(s.activity?.steps ?? 0)],
    ["Goal", `${s.activity?.goal ?? 0} (${s.activity?.goalPercent ?? 0}%)`],
    ["Sensor", s.sensor?.online ? `${s.sensor?.runtimeState ?? "Online"} q${s.sensor?.quality ?? "-"}` : "Offline"],
    ["Accel Raw", `${s.sensor?.ax ?? "-"} ${s.sensor?.ay ?? "-"} ${s.sensor?.az ?? "-"}`],
    ["Gyro Raw", `${s.sensor?.gx ?? "-"} ${s.sensor?.gy ?? "-"} ${s.sensor?.gz ?? "-"}`],
    ["Pose Deg", `P${s.sensor?.pitchDeg ?? "-"} R${s.sensor?.rollDeg ?? "-"}`],
    ["Motion", `${s.sensor?.motionScore ?? "-"} / steps ${s.sensor?.stepsTotal ?? 0}`],
    ["Storage", s.storage?.backend ?? "-"],
    ["Weather", s.weather?.valid ? `${s.weather?.location ?? "-"} ${s.weather?.temperatureC ?? "-"}C ${s.weather?.text ?? ""}` : "Waiting"],
    ["Presents", String(s.display?.presentCount ?? 0)],
    ["TX Fail", String(s.display?.txFailCount ?? 0)]
  ];

  document.getElementById("statusGrid").innerHTML = items
    .map(([k, v]) => `<div class="kv"><span>${k}</span><strong>${v}</strong></div>`)
    .join("");
}

function renderDiag() {
  const s = state.snapshot;
  if (!s) return;

  const diag = s.diag || {};
  const items = [];

  if (diag.hasLastFault) {
    items.push({
      label: "Last Fault",
      value: `${diag.lastFaultName} (${diag.lastFaultSeverity})`
    });
  }

  if (diag.hasLastLog) {
    items.push({
      label: "Last Log",
      value: `${diag.lastLogName}: ${diag.lastLogValue}`
    });
  }

  items.push({
    label: "Storage State",
    value: s.storage?.commitState ?? "?"
  });

  items.push({
    label: "Overlay",
    value: s.overlay?.active ? "ACTIVE" : "IDLE"
  });

  items.push({
    label: "Safe Mode",
    value: s.system?.safeMode ? "ACTIVE" : "OK"
  });

  document.getElementById("diagPanel").innerHTML = items
    .map(item => `<div class="diag-item"><span class="diag-label">${item.label}</span><span class="diag-value">${item.value}</span></div>`)
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
  statusEl.textContent = `Frame ${frame.presentCount ?? "-"}`;
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
