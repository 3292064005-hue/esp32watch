async function sendKey(key, eventType) {
  await runTrackedAction(contractRoute("inputKey"), { key, event: eventType }, `Key ${key}/${eventType}`);
}

async function sendCommand(type) {
  const knownCommands = Array.isArray(state.commandCatalog) ? state.commandCatalog.map(entry => entry?.type).filter(Boolean) : [];
  if (knownCommands.length > 0 && !knownCommands.includes(type)) {
    throw new Error(`unsupported command type: ${type}`);
  }
  await runTrackedAction(contractRoute("command"), { type }, `Command applied: ${type}`);
}

function resetActionConfig(type) {
  return RESET_ACTIONS[type] || null;
}

async function sendResetAction(type) {
  const reset = resetActionConfig(type);
  if (!reset) {
    throw new Error(`unsupported reset type: ${type}`);
  }
  const path = contractRoute(reset.routeKey);
  if (!path) {
    throw new Error(`runtime contract missing route for ${reset.routeKey}`);
  }
  await postReset(path, reset.successMessage, reset.failureMessage);
}

async function postReset(path, successMessage, failureMessage) {
  const currentToken = document.getElementById("currentAuthToken").value.trim() || loadToken();
  const result = validateApiPayload(await apiPostJson(path, { token: currentToken }, currentToken), "resetActionResponse", "reset action payload does not match runtime contract schema");
  const runtimeReload = canonicalRuntimeReload(result);
  if (path === contractRoute("resetDeviceConfig") || path === contractRoute("resetFactory")) {
    saveToken("");
    document.getElementById("currentAuthToken").value = "";
    document.getElementById("apiToken").value = "";
  }
  state.formDirty = false;
  if (runtimeReload.runtimeReloadRequested && runtimeReload.runtimeReloadRequiresReboot) {
    toast(result.message || "Reset completed; reboot required for all changes", "info");
  } else if (runtimeReload.runtimeReloadRequested && !runtimeReload.runtimeHotAppliedOk) {
    toast(result.message || failureMessage, "error");
  } else if (runtimeReload.runtimeReloadRequested && !runtimeReload.runtimeFullyEffectiveNow) {
    toast(result.message || "Reset completed; some changes are saved but not hot-applied", "info");
  } else {
    toast(successMessage, "success");
  }
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
    await runTrackedAction(contractRoute("displayOverlay"), { text, durationMs: 1500 }, "Overlay sent");
  } catch (err) {
    toast(err.message, "error");
  }
}

async function clearOverlay() {
  try {
    await runTrackedAction(contractRoute("displayOverlayClear"), {}, "Overlay cleared");
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

  const result = validateApiPayload(await apiPostJson(contractRoute("configDevice"), payload, currentToken), "deviceConfigUpdate", "device config update payload does not match runtime contract schema");
  const runtimeReload = canonicalRuntimeReload(result);
  saveToken(newToken);
  document.getElementById("currentAuthToken").value = newToken;
  state.formDirty = false;
  document.getElementById("wifiPassword").value = "";
  if (runtimeReload.runtimeReloadRequested && runtimeReload.runtimeReloadRequiresReboot) {
    toast("Configuration saved; reboot required for all changes", "info");
  } else if (runtimeReload.runtimeReloadRequested && !runtimeReload.runtimeHotAppliedOk) {
    toast(result.message || "Configuration saved, but hot runtime reload failed", "error");
  } else if (runtimeReload.runtimeReloadRequested && !runtimeReload.runtimeFullyEffectiveNow) {
    toast("Configuration saved; some changes are persisted only", "info");
  } else {
    toast("Configuration saved", "success");
  }
  await refreshState();
}

async function resetConfig() {
  await sendResetAction("resetDeviceConfig");
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
