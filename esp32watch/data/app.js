const OLED_WIDTH = 128;
const OLED_HEIGHT = 64;
const OLED_BUFFER_SIZE = (OLED_WIDTH * OLED_HEIGHT) / 8;
const OLED_ON_RGBA = [158, 252, 136, 255];
const OLED_OFF_RGBA = [8, 16, 12, 255];
const ACTION_RESULT_TERMINAL_STATES = new Set(["APPLIED", "REJECTED", "FAILED"]);

const state = {
  snapshot: null,
  frame: null,
  config: null,
  meta: null,
  polling: false,
  formDirty: false
};

let oledContext = null;
let oledImageData = null;

function loadToken() {
  return window.localStorage.getItem("esp32watch.apiToken") || "";
}

function saveToken(token) {
  if (token) {
    window.localStorage.setItem("esp32watch.apiToken", token);
  } else {
    window.localStorage.removeItem("esp32watch.apiToken");
  }
}

async function api(path, options = {}) {
  const { authToken: overrideToken, headers: extraHeaders, ...fetchOptions } = options;
  const authToken = overrideToken !== undefined ? overrideToken : loadToken();
  const res = await fetch(path, {
    ...fetchOptions,
    headers: {
      "Content-Type": "application/json",
      ...(authToken ? { "X-Auth-Token": authToken } : {}),
      ...(extraHeaders || {})
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

function apiPostJson(path, payload, authToken) {
  return api(path, {
    method: "POST",
    body: JSON.stringify(payload || {}),
    authToken
  });
}

function delayMs(ms) {
  return new Promise(resolve => window.setTimeout(resolve, ms));
}

function sanitizeNumber(value, digits = 6) {
  const number = Number(value);
  if (!Number.isFinite(number)) return "0";
  return number.toFixed(digits);
}

function mergeStatePayloads(summary, detail) {
  return {
    ok: Boolean(summary?.ok && detail?.ok),
    apiVersion: summary?.apiVersion || detail?.apiVersion || 0,
    stateVersion: summary?.stateVersion || detail?.stateVersion || 0,
    wifi: summary?.wifi || {},
    system: summary?.system || {},
    summary: summary?.summary || {},
    weather: {
      ...(summary?.weather || {}),
      ...(detail?.weather || {})
    },
    activity: detail?.activity || {},
    alarm: detail?.alarm || {},
    music: detail?.music || {},
    sensor: detail?.sensor || {},
    storage: detail?.storage || {},
    diag: detail?.diag || {},
    display: detail?.display || {},
    terminal: detail?.terminal || {},
    overlay: detail?.overlay || {}
  };
}

async function refreshState() {
  if (state.polling) return;
  state.polling = true;

  try {
    const [summary, detail, frame, config, meta] = await Promise.all([
      api("/api/state/summary"),
      api("/api/state/detail"),
      api("/api/display/frame"),
      api("/api/config/device"),
      api("/api/meta")
    ]);

    state.snapshot = mergeStatePayloads(summary, detail);
    state.frame = frame;
    state.config = config.config || null;
    if (state.snapshot && state.config) {
      state.snapshot.config = state.config;
    }
    state.meta = meta || null;
    renderAll();
  } catch (err) {
    toast(err.message, "error");
  } finally {
    state.polling = false;
  }
}

async function waitForActionResult(actionId, trackPath) {
  let lastResult = null;
  const statusPath = trackPath || `/api/actions/status?id=${actionId}`;
  for (let attempt = 0; attempt < 30; attempt += 1) {
    lastResult = await api(statusPath);
    if (ACTION_RESULT_TERMINAL_STATES.has(lastResult.status)) {
      return lastResult;
    }
    await delayMs(100);
  }
  throw new Error(lastResult?.message || "action result timeout");
}

async function runTrackedAction(path, payload, successMessage) {
  const ack = await apiPostJson(path, payload);
  const actionId = ack.actionId || ack.requestId;
  if (!actionId) {
    throw new Error("missing action id");
  }

  const result = await waitForActionResult(actionId, ack.trackPath);
  if (result.status !== "APPLIED") {
    throw new Error(result.message || `${ack.actionType || "action"} failed`);
  }

  toast(successMessage || result.message || "Action applied", "success");
  await refreshState();
}

function renderAll() {
  renderTopbar();
  renderTerminal();
  renderHub();
  renderDiag();
  renderStorage();
  renderConfig();
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
  const timeConfidence = s.system?.timeConfidence || "NONE";
  const wifi = s.wifi?.mode || "IDLE";
  const safe = s.diag?.safeModeActive ? "SAFE" : "OK";
  const app = s.system?.appReady
    ? (s.system?.appDegraded ? "DEGRADED" : "READY")
    : `INIT:${s.system?.appInitStage || s.system?.startupFailureStage || "-"}`;
  const apiVersion = state.meta?.apiVersion || s.apiVersion || "-";

  document.getElementById("deviceMeta").textContent =
    `PAGE ${page}  TIME ${timeSource}/${timeConfidence}  NET ${wifi}  APP ${app}  SYS ${safe}  API v${apiVersion}`;

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
      meta: `APP ${s.system?.appReady ? "READY" : s.system?.appInitStage ?? "INIT"}  BR ${s.terminal?.brightnessLabel ?? "-"}`
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
      meta: `${s.weather?.syncStatus || "-"}  TLS ${s.weather?.tlsMode || "-"}`
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
      body: `${s.activity?.steps ?? 0} steps / ${s.activity?.goal ?? 0}`
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
      body: s.wifi?.provisioningApActive
        ? `AP ${s.wifi?.provisioningApSsid || "-"}`
        : `${s.weather?.syncStatus || "-"}  TLS ${s.weather?.tlsMode || "-"}`
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
    ["STATE", s.summary?.diagLabel || (s.diag?.safeModeActive ? "SAFE" : "OK")],
    ["SAFE", s.diag?.safeModeActive ? "ACTIVE" : "CLEAR"],
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
    ["FS", state.config?.filesystemStatus || "-"]
  ];

  document.getElementById("storagePanel").innerHTML = items
    .map(([k, v]) => `<div class="mini-item"><span>${k}</span><strong>${v}</strong></div>`)
    .join("");
}

function populateConfigForm() {
  if (!state.config || state.formDirty) return;
  document.getElementById("wifiSsid").value = state.config.wifiSsid || "";
  document.getElementById("wifiPassword").value = "";
  document.getElementById("timezonePosix").value = state.config.timezonePosix || "UTC0";
  document.getElementById("timezoneId").value = state.config.timezoneId || "Etc/UTC";
  document.getElementById("latitude").value = state.config.latitude ?? 0;
  document.getElementById("longitude").value = state.config.longitude ?? 0;
  document.getElementById("locationName").value = state.config.locationName || "UNSET";
  document.getElementById("currentAuthToken").value = loadToken();
  document.getElementById("apiToken").value = loadToken();
}

function renderConfig() {
  const snapshot = state.snapshot;
  const config = state.config;
  if (!snapshot || !config) return;

  populateConfigForm();
  document.getElementById("wifiProvisionState").textContent = snapshot.wifi?.provisioningApActive
    ? `AP ${snapshot.wifi?.provisioningApSsid || "ACTIVE"}`
    : `${snapshot.wifi?.mode || "IDLE"} / ${snapshot.wifi?.status || "-"}`;
  document.getElementById("configHint").textContent = snapshot.wifi?.provisioningApActive
    ? `Provisioning active on ${config.apSsid || snapshot.wifi?.provisioningApSsid || "device AP"}. FS ${config.filesystemStatus || "-"}.`
    : `SSID ${config.wifiSsid || "UNSET"}  LOC ${config.locationName || "UNSET"}  IP ${config.ip || snapshot.wifi?.ip || "-"}  FS ${config.filesystemStatus || "-"}`;
  document.getElementById("authHint").textContent = config.authRequired
    ? "Auth: token required for writes"
    : (config.controlLockedInProvisioningAp ? "Auth: control locked until token or STA" : "Auth: open");
  document.getElementById("provisioningHint").textContent = snapshot.wifi?.provisioningApActive
    ? `Provisioning AP password is shown on the device serial log during AP startup.`
    : `API v${state.meta?.apiVersion || state.snapshot?.apiVersion || "-"}  State v${state.meta?.stateVersion || state.snapshot?.stateVersion || "-"}  TLS ${config.weatherTlsMode || "-"}  SYNC ${config.weatherSyncStatus || "-"}`;
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

async function sendKey(key, eventType) {
  await runTrackedAction("/api/input/key", { key, event: eventType }, `Key ${key}/${eventType}`);
}

async function sendCommand(type) {
  await runTrackedAction("/api/command", { type }, `Command applied: ${type}`);
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
    await runTrackedAction("/api/display/overlay", { text, durationMs: 1500 }, "Overlay sent");
  } catch (err) {
    toast(err.message, "error");
  }
}

async function clearOverlay() {
  try {
    await runTrackedAction("/api/display/overlay/clear", {}, "Overlay cleared");
  } catch (err) {
    toast(err.message, "error");
  }
}

async function saveConfig() {
  const currentToken = document.getElementById("currentAuthToken").value.trim() || loadToken();
  const newToken = document.getElementById("apiToken").value.trim();
  const payload = {
    ssid: document.getElementById("wifiSsid").value.trim(),
    password: document.getElementById("wifiPassword").value,
    timezonePosix: document.getElementById("timezonePosix").value.trim(),
    timezoneId: document.getElementById("timezoneId").value.trim(),
    latitude: Number(sanitizeNumber(document.getElementById("latitude").value)),
    longitude: Number(sanitizeNumber(document.getElementById("longitude").value)),
    locationName: document.getElementById("locationName").value.trim(),
    apiToken: newToken,
    token: currentToken
  };

  await apiPostJson("/api/config/device", payload, currentToken);
  saveToken(newToken);
  document.getElementById("currentAuthToken").value = newToken;
  state.formDirty = false;
  document.getElementById("wifiPassword").value = "";
  toast("Configuration saved", "success");
  await refreshState();
}

async function resetConfig() {
  const currentToken = document.getElementById("currentAuthToken").value.trim() || loadToken();
  await apiPostJson("/api/config/device/reset", { token: currentToken }, currentToken);
  saveToken("");
  document.getElementById("currentAuthToken").value = "";
  document.getElementById("apiToken").value = "";
  state.formDirty = false;
  toast("Configuration reset", "success");
  await refreshState();
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
  document.getElementById("saveConfigBtn").addEventListener("click", () => {
    saveConfig().catch(err => toast(err.message, "error"));
  });
  document.getElementById("resetConfigBtn").addEventListener("click", () => {
    resetConfig().catch(err => toast(err.message, "error"));
  });

  document.getElementById("overlayText").addEventListener("input", e => {
    renderPreview(e.target.value);
  });

  ["wifiSsid", "wifiPassword", "timezonePosix", "timezoneId", "latitude", "longitude", "locationName", "currentAuthToken", "apiToken"].forEach(id => {
    document.getElementById(id).addEventListener("input", () => {
      state.formDirty = true;
    });
  });

  document.getElementById("currentAuthToken").value = loadToken();
  document.getElementById("apiToken").value = loadToken();
  renderPreview("");
  refreshState().catch(err => toast(err.message, "error"));
});
