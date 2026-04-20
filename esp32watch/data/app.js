const OLED_WIDTH = 128;
const OLED_HEIGHT = 64;
const OLED_BUFFER_SIZE = (OLED_WIDTH * OLED_HEIGHT) / 8;
const OLED_ON_RGBA = [158, 252, 136, 255];
const OLED_OFF_RGBA = [8, 16, 12, 255];
const ACTION_RESULT_TERMINAL_STATES = new Set(["APPLIED", "REJECTED", "FAILED"]);
const BOOTSTRAP_CONTRACT_PATH = "/contract-bootstrap.json";
const MINIMAL_CONTRACT = {
  routes: {
    contract: "/api/contract"
  }
};

const state = {
  snapshot: null,
  frame: null,
  config: null,
  health: null,
  meta: null,
  commandCatalog: null,
  contract: MINIMAL_CONTRACT,
  storageSemantics: null,
  polling: false,
  formDirty: false,
  lastSnapshotRevision: null,
  lastFrameRevision: null,
  lastMetaRevision: null,
  lastHealthRevision: null,
  lastConfigRevision: null
};

let oledContext = null;
let oledImageData = null;

const RESET_ACTIONS = {
  resetAppState: {
    routeKey: "resetAppState",
    successMessage: "App state reset",
    failureMessage: "App state reset completed with runtime follow-up warnings"
  },
  factoryReset: {
    routeKey: "resetFactory",
    successMessage: "Factory reset completed",
    failureMessage: "Factory reset completed, but runtime reload failed"
  },
  resetDeviceConfig: {
    routeKey: "resetDeviceConfig",
    successMessage: "Device config reset",
    failureMessage: "Device config reset, but runtime reload failed"
  }
};


function stableRevision(value) {
  if (value == null) return null;
  try {
    return JSON.stringify(value);
  } catch (_) {
    return String(value);
  }
}

function contractRoute(name) {
  return state.contract?.routes?.[name] || null;
}

function loadCachedContract() {
  try {
    const raw = window.localStorage.getItem("esp32watch.contractCache");
    if (!raw) return null;
    const parsed = JSON.parse(raw);
    return parsed?.routes ? parsed : null;
  } catch (_) {
    return null;
  }
}

function saveCachedContract(contract) {
  try {
    if (contract?.routes) {
      window.localStorage.setItem("esp32watch.contractCache", JSON.stringify(contract));
    }
  } catch (_) {
  }
}

function mergeContract(base, overrideContract) {
  if (!overrideContract?.routes) {
    return base;
  }
  return {
    ...base,
    ...overrideContract,
    routes: {
      ...(base?.routes || {}),
      ...(overrideContract.routes || {})
    },
    stateSchemas: {
      ...(base?.stateSchemas || {}),
      ...(overrideContract.stateSchemas || {})
    },
    apiSchemas: {
      ...(base?.apiSchemas || {}),
      ...(overrideContract.apiSchemas || {})
    },
    routeSchemas: {
      ...(base?.routeSchemas || {}),
      ...(overrideContract.routeSchemas || {})
    },
    releaseValidation: {
      ...(base?.releaseValidation || {}),
      ...(overrideContract.releaseValidation || {})
    }
  };
}

function contractStateSchema(viewName) {
  const schema = state.contract?.stateSchemas?.[viewName];
  return Array.isArray(schema) ? schema : null;
}

function contractApiSchema(name) {
  const schema = state.contract?.apiSchemas?.[name];
  return schema && typeof schema === "object" ? schema : null;
}

function contractRouteSchema(routeKey) {
  const schema = state.contract?.routeSchemas?.[routeKey];
  return schema && typeof schema === "object" ? schema : null;
}


function payloadHasRouteResponseFields(payload, routeKey) {
  const schema = contractRouteSchema(routeKey);
  if (!schema?.responseFields) return true;
  return schema.responseFields.every(key => payloadHasPath(payload || {}, key));
}

function validateRoutePayload(routeKey, payload, errorMessage) {
  const routeSchema = contractRouteSchema(routeKey);
  if (!routeSchema) return payload;
  if (routeSchema.kind === "state") {
    if (!payloadHasDeclaredSections(payload, routeSchema.name) || !payloadHasRouteResponseFields(payload, routeKey)) {
      throw new Error(errorMessage || `${routeKey} payload does not match runtime contract state schema`);
    }
    return payload;
  }
  return validateApiPayload(payload, routeSchema.name, errorMessage || `${routeKey} payload does not match runtime contract schema`);
}

function validateContractCatalogCoverage() {
  const routes = state.contract?.routes || {};
  const routeSchemas = state.contract?.routeSchemas || {};
  const apiSchemas = state.contract?.apiSchemas || {};
  const stateSchemas = state.contract?.stateSchemas || {};
  Object.keys(routes).forEach(key => {
    if (!routeSchemas[key]) {
      throw new Error(`runtime contract missing route schema for ${key}`);
    }
  });
  Object.keys(routeSchemas).forEach(key => {
    if (!routes[key]) {
      throw new Error(`runtime contract route schema without route for ${key}`);
    }
    const schema = routeSchemas[key];
    if (schema.kind === "api") {
      if (!apiSchemas[schema.name]) {
        throw new Error(`runtime contract missing api schema ${schema.name} for ${key}`);
      }
    } else if (schema.kind === "state") {
      if (!stateSchemas[schema.name]) {
        throw new Error(`runtime contract missing state schema ${schema.name} for ${key}`);
      }
    } else {
      throw new Error(`runtime contract route schema ${key} has unsupported kind ${schema.kind}`);
    }
  });
  return true;
}

function validateApiPayload(payload, schemaName, errorMessage) {
  if (!payloadHasRequiredKeys(payload, schemaName) || !payloadHasDeclaredApiSections(payload, schemaName)) {
    throw new Error(errorMessage || `${schemaName} payload does not match runtime contract schema`);
  }
  return payload;
}

async function apiWithSchema(routeKey, fallbackPath, schemaName, options = {}) {
  const payload = await api(contractRoute(routeKey) || fallbackPath, options);
  return schemaName
    ? validateApiPayload(payload, schemaName, `${routeKey} payload does not match runtime contract schema`)
    : validateRoutePayload(routeKey, payload, `${routeKey} payload does not match runtime contract schema`);
}

function payloadHasPath(payload, keyPath) {
  if (!keyPath) return true;
  const parts = String(keyPath).split('.');
  let cursor = payload;
  for (const part of parts) {
    if (cursor === null || cursor === undefined || !Object.prototype.hasOwnProperty.call(cursor, part)) {
      return false;
    }
    cursor = cursor[part];
  }
  return true;
}

function payloadHasRequiredKeys(payload, schemaName) {
  const schema = contractApiSchema(schemaName);
  if (!schema?.required) return true;
  return schema.required.every(key => payloadHasPath(payload || {}, key));
}

function payloadHasDeclaredSections(payload, viewName) {
  const schema = contractStateSchema(viewName);
  if (!schema) return true;
  return schema.every(entry => Object.prototype.hasOwnProperty.call(payload || {}, entry.section));
}

function payloadHasDeclaredApiSections(payload, schemaName) {
  const schema = contractApiSchema(schemaName);
  if (!schema?.sections) return true;
  return schema.sections.every(entry => Object.prototype.hasOwnProperty.call(payload || {}, entry));
}

function canonicalActionStatus(payload) {
  return payload?.actionStatus && typeof payload.actionStatus === 'object'
    ? { ...payload.actionStatus, id: payload.actionStatus.id ?? payload.id ?? payload.actionId, type: payload.actionStatus.type ?? payload.type ?? payload.actionType, status: payload.actionStatus.status ?? payload.status }
    : { ...payload, id: payload?.id ?? payload?.actionId, type: payload?.type ?? payload?.actionType, status: payload?.status };
}

function canonicalRuntimeReload(payload) {
  return payload?.runtimeReload && typeof payload.runtimeReload === 'object' ? payload.runtimeReload : payload;
}

async function loadContract() {
  let contract = loadCachedContract() || MINIMAL_CONTRACT;
  try {
    const bootstrapRes = await fetch(BOOTSTRAP_CONTRACT_PATH, { cache: "no-store" });
    if (bootstrapRes.ok) {
      const bootstrap = await bootstrapRes.json();
      contract = mergeContract(contract, bootstrap);
    }
  } catch (_) {
  }

  try {
    const payload = await api((contract.routes?.contract || MINIMAL_CONTRACT.routes.contract), { authToken: "" });
    validateApiPayload(payload, "contractDocument", "runtime contract payload does not match runtime contract schema");
    if (payload?.contract?.routes) {
      contract = mergeContract(contract, payload.contract);
      saveCachedContract(contract);
    }
  } catch (err) {
    if (String(err?.message || err).includes("runtime contract")) {
      throw err;
    }
  }

  state.contract = contract;
  validateContractCatalogCoverage();
}

async function loadStorageSemantics() {
  state.storageSemantics = await apiWithSchema("storageSemantics", "/api/storage/semantics", "storageSemantics");
  return state.storageSemantics;
}

async function refreshCommandCatalog() {
  const payload = await apiWithSchema("actionsCatalog", "/api/actions/catalog", "actionsCatalog", {});
  const commands = payload.commandCatalog?.commands || payload.commands || [];
  state.commandCatalog = Array.isArray(commands) ? commands : [];
}

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


async function refreshState() {
  if (state.polling) return;
  state.polling = true;

  try {
    const [aggregate, frame, meta, health, configReadback] = await Promise.all([
      api(contractRoute("stateAggregate") || "/api/state/aggregate"),
      apiWithSchema("displayFrame", "/api/display/frame", "displayFrame"),
      apiWithSchema("meta", "/api/meta", "meta"),
      apiWithSchema("health", "/api/health", "health"),
      apiWithSchema("configDevice", "/api/config/device", "configDeviceReadback")
    ]);

    if (!payloadHasDeclaredSections(aggregate, "aggregate") || !payloadHasRouteResponseFields(aggregate, "stateAggregate")) {
      throw new Error("state aggregate payload does not match runtime contract schema");
    }

    try {
      const perfPayload = await api(contractRoute("statePerf") || "/api/state/perf");
      if (!payloadHasDeclaredSections(perfPayload, "perf") || !payloadHasRouteResponseFields(perfPayload, "statePerf")) {
        throw new Error("state perf payload does not match runtime contract schema");
      }
      if (aggregate && perfPayload?.perf) {
        aggregate.perf = perfPayload.perf;
      }
    } catch (_) {
      if (aggregate && typeof aggregate === "object" && !aggregate.perf) {
        aggregate.perf = { stages: [], history: [] };
      }
    }

    const nextSnapshotRevision = aggregate?.stateRevision ?? null;
    const nextFrameRevision = frame?.presentCount ?? null;
    const nextMetaRevision = stableRevision(meta);
    const nextHealthRevision = stableRevision(health);
    const nextConfigRevision = stableRevision(configReadback?.config || aggregate?.config || null);
    const renderNeeded = state.lastSnapshotRevision !== nextSnapshotRevision ||
      state.lastFrameRevision !== nextFrameRevision ||
      state.lastMetaRevision !== nextMetaRevision ||
      state.lastHealthRevision !== nextHealthRevision ||
      state.lastConfigRevision !== nextConfigRevision ||
      !state.snapshot || !state.frame;

    state.snapshot = aggregate || null;
    state.frame = frame;
    state.health = health || null;
    state.config = configReadback?.config || aggregate?.config || null;
    state.meta = meta || null;
    state.lastSnapshotRevision = nextSnapshotRevision;
    state.lastFrameRevision = nextFrameRevision;
    state.lastMetaRevision = nextMetaRevision;
    state.lastHealthRevision = nextHealthRevision;
    state.lastConfigRevision = nextConfigRevision;
    if (renderNeeded) {
      renderAll();
    }
  } catch (err) {
    toast(err.message, "error");
  } finally {
    state.polling = false;
  }
}

async function waitForActionResult(actionId, trackPath) {
  let lastResult = null;
  const statusBasePath = contractRoute("actionsStatus") || "/api/actions/status";
  const statusPath = trackPath || `${statusBasePath}?id=${actionId}`; // legacy shape: /api/actions/status?id=123
  for (let attempt = 0; attempt < 30; attempt += 1) {
    lastResult = validateApiPayload(await api(statusPath), "actionsStatus", "action status payload does not match runtime contract schema");
    const canonical = canonicalActionStatus(lastResult);
    if (ACTION_RESULT_TERMINAL_STATES.has(canonical.status)) {
      return { ...lastResult, ...canonical };
    }
    await delayMs(100);
  }
  throw new Error(lastResult?.message || lastResult?.actionStatus?.message || "action result timeout");
}

async function runTrackedAction(path, payload, successMessage) {
  const ack = validateApiPayload(await apiPostJson(path, payload), "trackedActionAccepted", "tracked action ack payload does not match runtime contract schema");
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
  renderPerf();
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
  const timeAuthority = s.system?.timeAuthority || "NONE";
  const wifi = s.wifi?.mode || "IDLE";
  const safe = s.diag?.safeModeActive ? "SAFE" : "OK";
  const app = s.system?.appReady
    ? (s.system?.appDegraded ? "DEGRADED" : "READY")
    : `INIT:${s.system?.appInitStage || s.system?.startupFailureStage || "-"}`;
  const apiVersion = state.meta?.apiVersion || s.apiVersion || "-";
  const expectedAssetVersion = state.contract?.assetContractVersion || state.meta?.assetContractVersion || 0;
  const observedAssetVersion = state.meta?.assetContractVersion || 0;
  const assetStatus = state.meta?.assetContractReady
    ? ((expectedAssetVersion !== 0 && observedAssetVersion === expectedAssetVersion) ? "ASSET OK" : "ASSET MISMATCH")
    : ((observedAssetVersion && expectedAssetVersion && observedAssetVersion !== expectedAssetVersion)
        ? "ASSET MISMATCH"
        : "ASSET MISSING");

  document.getElementById("deviceMeta").textContent =
    `PAGE ${page}  TIME ${timeSource}/${timeConfidence}/${timeAuthority}  NET ${wifi}  APP ${app}  SYS ${safe}  API v${apiVersion}  ${assetStatus}`;

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
    ["LOG", s.diag?.hasLastLog ? `${s.diag.lastLogName}` : "NONE"],
    ["BOOT", `${s.diag?.consecutiveIncompleteBoots ?? 0}`]
  ];

  document.getElementById("diagPanel").innerHTML = items
    .map(([k, v]) => `<div class="mini-item"><span>${k}</span><strong>${v}</strong></div>`)
    .join("");
}


function renderPerf() {
  const s = state.snapshot;
  if (!s) return;

  const perf = s.perf || {};
  const stages = Array.isArray(perf.stages) ? perf.stages : [];
  const history = Array.isArray(perf.history) ? perf.history : [];

  const perfSummary = [
    ["LOOPS", perf.loopCount ?? 0],
    ["MAX", `${perf.maxLoopMs ?? 0}ms`],
    ["WDT", `${perf.lastCheckpoint || '-'} / ${perf.lastCheckpointResult || '-'}`],
    ["QUEUE", `${perf.actionQueueDepth ?? 0} / DROP ${perf.actionQueueDropCount ?? 0}`]
  ];

  document.getElementById("perfPanel").innerHTML = perfSummary
    .map(([k, v]) => `<div class="mini-item"><span>${k}</span><strong>${v}</strong></div>`)
    .join("");

  document.getElementById("perfStagePanel").innerHTML = stages
    .map(stage => `
      <div class="perf-row">
        <div>
          <strong>${stage.name || '-'}</strong>
          <span>${stage.lastDurationMs ?? 0} / ${stage.budgetMs ?? 0} ms</span>
        </div>
        <em>OVER ${stage.overBudgetCount ?? 0} · DEF ${stage.deferredCount ?? 0}</em>
      </div>
    `)
    .join("") || '<div class="perf-empty">No stage telemetry</div>';

  document.getElementById("perfHistoryPanel").innerHTML = history
    .map(entry => `
      <div class="perf-row perf-history-row">
        <div>
          <strong>${entry.stage || '-'}</strong>
          <span>${entry.event || '-'} · ${entry.durationMs ?? 0}/${entry.budgetMs ?? 0} ms</span>
        </div>
        <em>#${entry.loopCounter ?? 0}</em>
      </div>
    `)
    .join("") || '<div class="perf-empty">No recent perf events</div>';
}

function renderStorage() {
  const s = state.snapshot;
  if (!s) return;

  const semantics = Array.isArray(state.storageSemantics?.objects) ? state.storageSemantics.objects : [];
  const appObjects = semantics.filter(obj => ["APP_SETTINGS", "APP_ALARMS", "SENSOR_CAL", "GAME_STATS"].includes(obj?.name));
  const semanticsRows = appObjects.length > 0
    ? appObjects.map(obj => `<div class="perf-row"><div><strong>${obj.name}</strong><span>${obj.owner || '-'} · ${obj.namespace || '-'}</span></div><em>${obj.durability || '-'} / ${obj.resetBehavior || '-'}</em></div>`).join("")
    : '<div class="perf-empty">Storage semantics unavailable</div>';

  const items = [
    ["STATE", s.summary?.storageLabel || "OK"],
    ["BACKEND", s.storage?.backend || "-"],
    ["APP", s.storage?.appStateBackend || "-"],
    ["COMMIT", s.storage?.commitState || "-"],
    ["TXN", s.storage?.transactionActive ? "ACTIVE" : "IDLE"],
    ["DUR", s.storage?.appStateDurability || state.config?.appStateDurability || "-"],
    ["PWR", s.storage?.appStatePowerLossGuaranteed ? "GUARANTEED" : "PARTIAL"],
    ["CFG", "AUTHORITY / RELOAD"],
    ["FS", state.config?.filesystemStatus || "-"]
  ];

  document.getElementById("storagePanel").innerHTML =
    items.map(([k, v]) => `<div class="mini-item"><span>${k}</span><strong>${v}</strong></div>`).join("") +
    `<div class="perf-stage-panel">${semanticsRows}</div>`;
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
    : `SSID ${config.wifiSsid || "UNSET"}  LOC ${config.locationName || "UNSET"}  IP ${config.ip || snapshot.wifi?.ip || "-"}  FS ${config.filesystemStatus || "-"}  APP ${config.appStateDurability || snapshot.storage?.appStateDurability || "-"}`;
  document.getElementById("authHint").textContent = config.authRequired
    ? "Auth: token required for writes"
    : (config.controlLockedInProvisioningAp ? "Auth: control locked until token or STA" : "Auth: open");
  document.getElementById("provisioningHint").textContent = snapshot.wifi?.provisioningApActive
    ? (state.meta?.provisioningSerialPasswordLogEnabled
        ? `Provisioning AP password is shown on the device serial log during AP startup.`
        : `Provisioning AP password serial logging is disabled in this build.`)
    : `API v${state.contract?.apiVersion || state.meta?.apiVersion || state.snapshot?.apiVersion || "-"}  State v${state.contract?.stateVersion || state.meta?.stateVersion || state.snapshot?.stateVersion || "-"}  Asset v${state.meta?.assetContractVersion || "-"}/${state.contract?.assetContractVersion || "-"}  TLS ${config.weatherTlsMode || "-"}  SYNC ${config.weatherSyncStatus || "-"}  CFG ${snapshot.storage?.deviceConfigDurability || "DURABLE"}`;
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
  if (runtimeReload.runtimeReloadRequested && !runtimeReload.runtimeReloaded) {
    toast(result.message || failureMessage, "error");
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
  if (runtimeReload.runtimeReloadRequested && !runtimeReload.runtimeReloaded) {
    toast(result.message || "Configuration saved, but runtime reload failed", "error");
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
      const reset = resetActionConfig(cmd);
      const runner = reset ? sendResetAction(cmd) : sendCommand(cmd);
      Promise.resolve(runner).catch(err => toast(err.message, "error"));
    });
  });

  loadContract().then(() => loadStorageSemantics()).catch(err => toast(err.message, "error")).finally(() => {
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
  refreshCommandCatalog().catch(err => toast(err.message, "error"));
  refreshState().catch(err => toast(err.message, "error"));
  window.setInterval(() => {
    refreshState().catch(err => toast(err.message, "error"));
  }, 2000);
  });
});
