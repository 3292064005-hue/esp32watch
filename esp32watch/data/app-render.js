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
