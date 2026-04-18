#!/usr/bin/env node
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const appJs = fs.readFileSync(path.join(__dirname, '..', 'data', 'app.js'), 'utf8');
const bootstrapContract = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'data', 'contract-bootstrap.json'), 'utf8'));
const pageRoutesSource = fs.readFileSync(path.join(__dirname, '..', 'src', 'web', 'web_routes_page.cpp'), 'utf8');

const listeners = {};
const elements = new Map();
const localStore = new Map();
let lastToast = null;

function makeElement(id) {
  return {
    id,
    value: '',
    textContent: '',
    innerHTML: '',
    className: '',
    dataset: {},
    style: {},
    width: 128,
    height: 64,
    addEventListener() {},
    appendChild(child) { if (id === 'toastHost') { lastToast = child.textContent; } },
    remove() {},
    getContext() {
      return {
        fillStyle: '#000',
        fillRect() {},
        putImageData() {},
        createImageData(w, h) {
          return { data: new Uint8ClampedArray(w * h * 4) };
        },
      };
    },
  };
}

const document = {
  addEventListener(name, cb) {
    listeners[name] = cb;
  },
  querySelectorAll() {
    return [];
  },
  createElement(tag) {
    return makeElement(tag);
  },
  getElementById(id) {
    if (!elements.has(id)) {
      elements.set(id, makeElement(id));
    }
    return elements.get(id);
  },
};

const fetchPayloads = {
  '/contract-bootstrap.json': bootstrapContract,
  [bootstrapContract.routes.contract]: { ok: true, contract: bootstrapContract },
  [bootstrapContract.routes.storageSemantics]: {
    ok: true,
    apiVersion: bootstrapContract.apiVersion,
    objects: [{ kind: 1, name: 'appState', owner: 'app', namespace: 'watch', resetBehavior: 'RESET', durability: 'DURABLE', managedByStorageService: true, survivesPowerLoss: true }],
    storageSemantics: [{ kind: 1, name: 'appState', owner: 'app', namespace: 'watch', resetBehavior: 'RESET', durability: 'DURABLE', managedByStorageService: true, survivesPowerLoss: true }],
  },
  [bootstrapContract.routes.health]: {
    ok: true,
    wifiConnected: true,
    ip: '192.168.1.10',
    uptimeMs: 123456,
    filesystemReady: true,
    assetContractReady: true,
    healthStatus: { wifiConnected: true, ip: '192.168.1.10', uptimeMs: 123456, filesystemReady: true, assetContractReady: true },
  },
  [bootstrapContract.routes.stateAggregate]: {
    ok: true,
    apiVersion: bootstrapContract.apiVersion,
    stateVersion: bootstrapContract.stateVersion,
    wifi: { connected: true, provisioningApActive: false, provisioningApSsid: '', mode: 'STA', status: 'CONNECTED', ip: '192.168.1.10', rssi: -48 },
    system: { page: 'WATCHFACE', timeSource: 'NTP', timeConfidence: 'HIGH', appReady: true, appDegraded: false, startupOk: true, startupDegraded: false, fatalStopRequested: false, startupFailureStage: 'NONE', startupRecoveryStage: 'NONE', appInitStage: 'APP', timeValid: true, timeAuthoritative: true, timeSourceAgeMs: 10, sleeping: false, animating: false, uptimeMs: 123456 },
    config: {
      wifiConfigured: true,
      weatherConfigured: true,
      authRequired: false,
      wifiSsid: 'TestWiFi',
      timezoneId: 'Etc/UTC',
      locationName: 'Lab',
      filesystemReady: true,
      filesystemAssetsReady: true,
      filesystemStatus: 'READY',
      appStateDurability: 'DURABLE'
    },
    summary: { headerTags: 'SYNC READY', networkLine: 'SYNCED', networkSubline: 'TLS OK', diagLabel: 'OK', storageLabel: 'OK', sensorLabel: 'ONLINE' },
    weather: { valid: true, location: 'Lab', text: 'Sunny', syncStatus: 'SYNCED', tlsMode: 'STRICT', tlsVerified: true, tlsCaLoaded: true, lastHttpStatus: 200, temperatureC: 23.4, updatedAtMs: 123000 },
    activity: { steps: 1000, goal: 8000, goalPercent: 12 },
    alarm: { label: '07:30', enabled: true, ringing: false, nextIndex: 0, nextTime: '07:30' },
    music: { available: false, playing: false, state: 'IDLE', song: '' },
    sensor: { online: true, calibrated: true, staticNow: false, runtimeState: 'RUNNING', calibrationState: 'DONE', quality: 5, errorCode: 0, faultCount: 0, reinitCount: 0, calibrationProgress: 100, ax: 0, ay: 0, az: 1000, gx: 0, gy: 0, gz: 0, accelNormMg: 1000, baselineMg: 1000, motionScore: 0, lastSampleMs: 5, stepsTotal: 1000, pitchDeg: 0, rollDeg: 0 },
    storage: { backend: 'RTC_RESET_DOMAIN', backendPhase: 'READY', commitState: 'IDLE', schemaVersion: 7, flashSupported: false, flashReady: false, migrationAttempted: false, migrationOk: false, transactionActive: false, sleepFlushPending: false, appStateBackend: 'NVS_APP_STATE', deviceConfigBackend: 'NVS_A_B', appStateDurability: 'DURABLE', deviceConfigDurability: 'DURABLE', appStatePowerLossGuaranteed: true, deviceConfigPowerLossGuaranteed: true, appStateMixedDurability: false, appStateResetDomainObjectCount: 0, appStateDurableObjectCount: 1 },
    diag: { safeModeActive: false, hasLastFault: false, hasLastLog: false, bootCount: 7, previousBootCount: 6 },
    display: { presentCount: 1, txFailCount: 0 },
    terminal: { systemFace: 'DEFAULT', brightnessLabel: '2/3', activityLabel: '12%', sensorLabel: 'ONLINE', networkLabel: 'SYNCED', storageLabel: 'OK' },
    overlay: { active: false, text: '', expireAtMs: 0 },
    perf: { loopCount: 3, maxLoopMs: 12, lastCheckpoint: 'WEB', lastCheckpointResult: 'OK', actionQueueDepth: 0, actionQueueDropCount: 0, stages: [], history: [] }
  },
  [bootstrapContract.routes.stateSummary]: {
    ok: true,
    apiVersion: bootstrapContract.apiVersion,
    stateVersion: bootstrapContract.stateVersion,
    wifi: { mode: 'STA', status: 'CONNECTED', ip: '192.168.1.10', provisioningApActive: false },
    system: { page: 'WATCHFACE', timeSource: 'NTP', timeConfidence: 'HIGH', appReady: true, appDegraded: false, uptimeMs: 123456 },
    summary: { headerTags: 'SYNC READY', networkLine: 'SYNCED', diagLabel: 'OK', storageLabel: 'OK', sensorLabel: 'ONLINE' },
    weather: { syncStatus: 'SYNCED', tlsMode: 'STRICT' },
  },
  [bootstrapContract.routes.stateDetail]: {
    ok: true,
    apiVersion: bootstrapContract.apiVersion,
    stateVersion: bootstrapContract.stateVersion,
    weather: { syncStatus: 'SYNCED', tlsMode: 'STRICT', updatedAtMs: 123000 },
    activity: { steps: 1000, goal: 8000 },
    alarm: { label: '07:30', enabled: true, nextTime: '07:30' },
    music: {},
    sensor: { runtimeState: 'RUNNING', calibrationProgress: 100, quality: 5 },
    storage: { backend: 'RTC_RESET_DOMAIN', commitState: 'IDLE', transactionActive: false, appStateDurability: 'DURABLE', deviceConfigDurability: 'DURABLE' },
    diag: { safeModeActive: false, hasLastFault: false, hasLastLog: false, bootCount: 7, previousBootCount: 6 },
    display: {},
    terminal: { systemFace: 'DEFAULT', brightnessLabel: '2/3', activityLabel: '12%', sensorLabel: 'ONLINE', networkLabel: 'SYNCED' },
    overlay: {},
    perf: { loopCount: 3, maxLoopMs: 12, lastCheckpoint: 'WEB', lastCheckpointResult: 'OK', actionQueueDepth: 0, actionQueueDropCount: 0, stages: [], history: [] },
  },
  [bootstrapContract.routes.statePerf]: {
    ok: true,
    perf: { loopCount: 3, maxLoopMs: 12, lastCheckpoint: 'WEB', lastCheckpointResult: 'OK', actionQueueDepth: 0, actionQueueDropCount: 0, stages: [], history: [] },
  },
  [bootstrapContract.routes.stateRaw]: {
    ok: true,
    startupRaw: { ok: true },
    queueRaw: { depth: 0 },
    sensor: { ax: 0, ay: 0, az: 1000 },
    storage: { backend: 'RTC_RESET_DOMAIN' },
    display: { presentCount: 1 },
    perf: { loopCount: 3, maxLoopMs: 12, lastCheckpoint: 'WEB', lastCheckpointResult: 'OK', actionQueueDepth: 0, actionQueueDropCount: 0, stages: [], history: [] },
  },
  [bootstrapContract.routes.displayFrame]: {
    ok: true,
    width: 128,
    height: 64,
    presentCount: 1,
    bufferHex: '00'.repeat((128 * 64) / 8),
    displayFrame: { width: 128, height: 64, presentCount: 1, bufferHex: '00'.repeat((128 * 64) / 8) },
  },
  [bootstrapContract.routes.actionsCatalog]: {
    ok: true,
    commands: [{ type: 'storageManualFlush', webExposed: true }],
    commandCatalog: { commandCatalogVersion: bootstrapContract.commandCatalogVersion, commands: [{ type: 'storageManualFlush', webExposed: true }] },
  },
  [bootstrapContract.routes.configDevice]: {
    ok: true,
    config: {
      wifiSsid: 'TestWiFi',
      timezonePosix: 'UTC0',
      timezoneId: 'Etc/UTC',
      latitude: 0,
      longitude: 0,
      locationName: 'Lab',
      authRequired: false,
      controlLockedInProvisioningAp: false,
      provisioningSerialPasswordLogEnabled: false,
      appStateDurability: 'DURABLE',
      deviceConfigDurability: 'DURABLE',
      filesystemStatus: 'READY',
      weatherTlsMode: 'STRICT',
      weatherSyncStatus: 'SYNCED',
      ip: '192.168.1.10',
      apSsid: 'ESP32WATCH-SETUP',
    },
    deviceConfigReadback: {
      wifiConfigured: true,
      weatherConfigured: true,
      apiTokenConfigured: false,
      authRequired: false,
      wifiSsid: 'TestWiFi',
      timezonePosix: 'UTC0',
      timezoneId: 'Etc/UTC',
      latitude: 0,
      longitude: 0,
      locationName: 'Lab',
      filesystemStatus: 'READY',
      filesystemReady: true,
      filesystemAssetsReady: true,
    },
    capabilities: {
      configProvisioning: true,
      securedProvisioningAp: true,
      authToken: true,
      overlayControl: true,
      trackedMutations: true,
      stateSummary: true,
      stateDetail: true,
      statePerf: true,
      stateRaw: true,
      controlLockInProvisioningAp: true,
    },
  },
  [bootstrapContract.routes.meta]: {
    ok: true,
    apiVersion: bootstrapContract.apiVersion,
    stateVersion: bootstrapContract.stateVersion,
    storageSchemaVersion: 7,
    assetContractReady: true,
    assetContractHashVerified: true,
    assetContractVersion: bootstrapContract.assetContractVersion,
    provisioningSerialPasswordLogEnabled: false,
    versions: { apiVersion: bootstrapContract.apiVersion, stateVersion: bootstrapContract.stateVersion, storageSchemaVersion: 7, runtimeContractVersion: bootstrapContract.runtimeContractVersion, commandCatalogVersion: bootstrapContract.commandCatalogVersion },
    storageRuntime: { flashStorageSupported: false, flashStorageReady: false, storageMigrationAttempted: false, storageMigrationOk: false, appStateBackend: 'NVS_APP_STATE', deviceConfigBackend: 'NVS_A_B', appStateDurability: 'DURABLE', deviceConfigDurability: 'DURABLE', appStatePowerLossGuaranteed: true, deviceConfigPowerLossGuaranteed: true, appStateMixedDurability: false, appStateResetDomainObjectCount: 0, appStateDurableObjectCount: 1 },
    assetContract: { webControlPlaneReady: true, webConsoleReady: true, filesystemReady: true, filesystemAssetsReady: true, assetContractReady: true, assetContractHashVerified: true, assetContractVersion: bootstrapContract.assetContractVersion, assetContractHash: 123, assetContractGeneratedAt: '2026-04-14T00:00:00Z', filesystemStatus: 'READY', assetExpectedHashIndexHtml: 'A', assetActualHashIndexHtml: 'A', assetExpectedHashAppJs: 'B', assetActualHashAppJs: 'B', assetExpectedHashAppCss: 'C', assetActualHashAppCss: 'C', assetExpectedHashContractBootstrap: 'D', assetActualHashContractBootstrap: 'D' },
    runtimeEvents: { runtimeEventHandlers: 1, runtimeEventCapacity: 8, runtimeEventRegistrationRejectCount: 0, runtimeEventPublishCount: 1, runtimeEventPublishFailCount: 0, runtimeEventLastSuccessCount: 1, runtimeEventLastFailureCount: 0, runtimeEventLastCriticalFailureCount: 0, runtimeEventLast: 'CONFIG_UPDATE', runtimeEventLastFailed: 'NONE', runtimeEventLastFailedHandlerIndex: -1, handlers: [{ index: 0, name: 'cfg', priority: 0, critical: true }] },
    platformSupport: { rtcResetDomain: true, idleLightSleep: true, watchdog: false, flashJournal: true },
    deviceIdentity: { boardProfile: 'esp32watch', chipModel: 'ESP32-S3', efuseMac: 'AA:BB:CC:DD:EE:FF' },
    capabilities: { configProvisioning: true, securedProvisioningAp: true, authToken: true, overlayControl: true, trackedMutations: true, stateSummary: true, stateDetail: true, statePerf: true, stateRaw: true, controlLockInProvisioningAp: true },
    releaseValidation: { schemaVersion: bootstrapContract.releaseValidation.validationSchemaVersion, candidateBundleKind: 'candidate', verifiedBundleKind: 'verified', hostReportType: 'HOST_VALIDATION_REPORT', deviceReportType: 'DEVICE_SMOKE_REPORT' },
  },
  ['POST ' + bootstrapContract.routes.configDevice]: {
    ok: true,
    runtimeReload: { runtimeReloadRequested: true, runtimeReloadPreflightOk: true, runtimeReloadApplyAttempted: true, runtimeReloaded: true, runtimeReloadEventDispatchOk: true, runtimeReloadAuthoritativePath: true, runtimeReloadVerifyOk: true, runtimeReloadPartialSuccess: false, runtimeWifiReloadOk: true, runtimeNetworkReloadOk: true, runtimeHandlerCount: 1, runtimeHandlerSuccessCount: 1, runtimeHandlerFailureCount: 0, runtimeHandlerCriticalFailureCount: 0, runtimeReloadImpactDomains: ['WIFI'], runtimeReloadAppliedDomains: ['WIFI'], runtimeReloadFailedDomains: [], runtimeReloadFailurePhase: 'NONE', runtimeReloadFailureCode: 'NONE' },
    wifiSaved: true, networkSaved: true, tokenSaved: true, runtimeReloadRequested: true, runtimeReloaded: true, runtimeReloadAuthoritativePath: true, filesystemReady: true, filesystemAssetsReady: true, filesystemStatus: 'READY', message: 'config saved',
    deviceConfigUpdate: { wifiSaved: true, networkSaved: true, tokenSaved: true, filesystemReady: true, filesystemAssetsReady: true, filesystemStatus: 'READY' },
  },
  ['POST ' + bootstrapContract.routes.resetDeviceConfig]: {
    ok: true,
    message: 'device config reset',
    resetKind: 'DEVICE_CONFIG',
    runtimeReload: { runtimeReloadRequested: true, runtimeReloadPreflightOk: true, runtimeReloadApplyAttempted: true, runtimeReloaded: true, runtimeReloadEventDispatchOk: true, runtimeReloadAuthoritativePath: true, runtimeReloadVerifyOk: true, runtimeReloadPartialSuccess: false, runtimeWifiReloadOk: true, runtimeNetworkReloadOk: true, runtimeHandlerCount: 1, runtimeHandlerSuccessCount: 1, runtimeHandlerFailureCount: 0, runtimeHandlerCriticalFailureCount: 0, runtimeReloadImpactDomains: ['WIFI'], runtimeReloadAppliedDomains: ['WIFI'], runtimeReloadFailedDomains: [], runtimeReloadFailurePhase: 'NONE', runtimeReloadFailureCode: 'NONE' },
    appStateReset: false, deviceConfigReset: true, auditNotified: true, auditEventDispatched: true, runtimeReloadRequested: true, runtimeReloaded: true, runtimeReloadAuthoritativePath: true, filesystemReady: true, filesystemAssetsReady: true, assetContractReady: true, filesystemStatus: 'READY', hardFailure: false, completedWithReloadFailure: false, resetAction: { resetKind: 'DEVICE_CONFIG', appStateReset: false, deviceConfigReset: true, auditNotified: true, auditEventDispatched: true },
  },
  [bootstrapContract.routes.actionsStatus + '?id=1']: {
    ok: true, id: 1, type: 'COMMAND', status: 'APPLIED', actionId: 1, requestId: 1, actionType: 'COMMAND', state: 'APPLIED', acceptedAtMs: 1, startedAtMs: 2, completedAtMs: 3, message: 'accepted', commandOk: true, commandCode: 'OK', actionStatus: { id: 1, type: 'COMMAND', status: 'APPLIED', acceptedAtMs: 1, startedAtMs: 2, completedAtMs: 3, message: 'accepted', commandOk: true, commandCode: 'OK' }
  },
  [bootstrapContract.routes.actionsStatus + '?id=999']: {
    ok: true, id: 999, type: 'COMMAND', status: 'APPLIED'
  },
  [bootstrapContract.routes.actionsLatest]: {
    ok: true, id: 1, type: 'COMMAND', status: 'APPLIED', actionStatus: { id: 1, type: 'COMMAND', status: 'APPLIED' }
  },
  ['POST ' + bootstrapContract.routes.command]: { ok: true, actionId: 1, requestId: 1, actionType: 'COMMAND', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1, trackedActionAccepted: { actionId: 1, requestId: 1, actionType: 'COMMAND', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1 } },
  ['POST ' + bootstrapContract.routes.inputKey]: { ok: true, actionId: 2, requestId: 2, actionType: 'INPUT_KEY', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1, trackedActionAccepted: { actionId: 2, requestId: 2, actionType: 'INPUT_KEY', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1 } },
  ['POST ' + bootstrapContract.routes.displayOverlay]: { ok: true, actionId: 3, requestId: 3, actionType: 'DISPLAY_OVERLAY', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1, trackedActionAccepted: { actionId: 3, requestId: 3, actionType: 'DISPLAY_OVERLAY', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1 } },
  ['POST ' + bootstrapContract.routes.displayOverlayClear]: { ok: true, actionId: 4, requestId: 4, actionType: 'DISPLAY_OVERLAY_CLEAR', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1, trackedActionAccepted: { actionId: 4, requestId: 4, actionType: 'DISPLAY_OVERLAY_CLEAR', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1 } },
  ['POST ' + bootstrapContract.routes.resetAppState]: { ok: true, message: 'app state reset', resetKind: 'APP_STATE', runtimeReload: { runtimeReloadRequested: false }, resetAction: { resetKind: 'APP_STATE' } },
  ['POST ' + bootstrapContract.routes.resetFactory]: { ok: true, message: 'factory reset', resetKind: 'FACTORY', runtimeReload: { runtimeReloadRequested: true }, resetAction: { resetKind: 'FACTORY' } },
};

async function fetchStub(url, options = {}) {
  const method = String(options.method || 'GET').toUpperCase();
  const payload = fetchPayloads[method + ' ' + url] ?? fetchPayloads[url];
  if (payload === undefined) {
    return {
      ok: false,
      status: 404,
      text: async () => JSON.stringify({ ok: false, message: `unmocked ${url}` }),
      json: async () => ({ ok: false, message: `unmocked ${url}` }),
    };
  }
  return {
    ok: true,
    status: 200,
    text: async () => JSON.stringify(payload),
    json: async () => payload,
  };
}

const windowObj = {
  localStorage: {
    getItem(key) { return localStore.has(key) ? localStore.get(key) : null; },
    setItem(key, value) { localStore.set(key, String(value)); },
    removeItem(key) { localStore.delete(key); },
  },
  setTimeout(cb) { cb(); return 1; },
  setInterval() { return 1; },
};

const context = {
  console,
  document,
  window: windowObj,
  fetch: fetchStub,
  setTimeout: windowObj.setTimeout,
  setInterval: windowObj.setInterval,
  Uint8ClampedArray,
  Promise,
};
context.globalThis = context;
windowObj.document = document;
windowObj.fetch = fetchStub;

function clonePayload(payload) {
  return JSON.parse(JSON.stringify(payload));
}

function makeInvalidRoutePayload(routeKey, payload) {
  const invalid = clonePayload(payload);
  const routeSchema = bootstrapContract.routeSchemas[routeKey];
  if (!routeSchema) {
    return invalid;
  }
  if (routeSchema.kind === 'state') {
    const schema = bootstrapContract.stateSchemas[routeSchema.name] || [];
    const firstSection = schema[0]?.section;
    if (firstSection) {
      delete invalid[firstSection];
    }
    return invalid;
  }
  const schema = bootstrapContract.apiSchemas[routeSchema.name] || {};
  const firstRequired = (schema.required || [])[0];
  if (firstRequired && Object.prototype.hasOwnProperty.call(invalid, firstRequired)) {
    delete invalid[firstRequired];
    return invalid;
  }
  const firstSection = (schema.sections || [])[0];
  if (firstSection && Object.prototype.hasOwnProperty.call(invalid, firstSection)) {
    delete invalid[firstSection];
  }
  return invalid;
}

(async () => {
  vm.runInNewContext(`${appJs}\n;globalThis.__smokeState = state;`, context, { filename: 'app.js' });
  if (typeof listeners.DOMContentLoaded !== 'function') {
    throw new Error('DOMContentLoaded listener missing');
  }
  listeners.DOMContentLoaded();
  await new Promise(resolve => setImmediate(resolve));
  await new Promise(resolve => setImmediate(resolve));
  if (typeof context.refreshState === 'function') {
    await context.refreshState();
  }
  const state = context.__smokeState;
  if (!pageRoutesSource.includes('server.on("/contract-bootstrap.json"')) {
    throw new Error('page routes missing /contract-bootstrap.json asset route');
  }
  if (bootstrapContract.routes.stateAggregate !== '/api/state/aggregate') {
    throw new Error('canonical aggregate route drifted');
  }
  if (!state || !state.snapshot || !state.meta || !state.contract?.routes?.meta) {
    throw new Error(`runtime state not populated after DOMContentLoaded: ${lastToast || 'no toast'}`);
  }
  const routePayloads = {
    contract: fetchPayloads[bootstrapContract.routes.contract],
    health: fetchPayloads[bootstrapContract.routes.health],
    meta: fetchPayloads[bootstrapContract.routes.meta],
    actionsCatalog: fetchPayloads[bootstrapContract.routes.actionsCatalog],
    actionsLatest: fetchPayloads[bootstrapContract.routes.actionsLatest],
    actionsStatus: fetchPayloads[bootstrapContract.routes.actionsStatus + '?id=1'],
    stateSummary: fetchPayloads[bootstrapContract.routes.stateSummary],
    stateDetail: fetchPayloads[bootstrapContract.routes.stateDetail],
    statePerf: fetchPayloads[bootstrapContract.routes.statePerf],
    stateRaw: fetchPayloads[bootstrapContract.routes.stateRaw],
    stateAggregate: fetchPayloads[bootstrapContract.routes.stateAggregate],
    displayFrame: fetchPayloads[bootstrapContract.routes.displayFrame],
    displayOverlay: fetchPayloads['POST ' + bootstrapContract.routes.displayOverlay],
    displayOverlayClear: fetchPayloads['POST ' + bootstrapContract.routes.displayOverlayClear],
    configDevice: fetchPayloads[bootstrapContract.routes.configDevice],
    inputKey: fetchPayloads['POST ' + bootstrapContract.routes.inputKey],
    command: fetchPayloads['POST ' + bootstrapContract.routes.command],
    resetAppState: fetchPayloads['POST ' + bootstrapContract.routes.resetAppState],
    resetDeviceConfig: fetchPayloads['POST ' + bootstrapContract.routes.resetDeviceConfig],
    resetFactory: fetchPayloads['POST ' + bootstrapContract.routes.resetFactory],
    storageSemantics: fetchPayloads[bootstrapContract.routes.storageSemantics],
  };
  context.validateContractCatalogCoverage();
  for (const routeKey of Object.keys(bootstrapContract.routeSchemas)) {
    if (!(routeKey in routePayloads)) {
      throw new Error(`missing smoke payload for route schema ${routeKey}`);
    }
    context.validateRoutePayload(routeKey, routePayloads[routeKey], `${routeKey} payload does not match runtime contract schema`);
  }
  for (const routeKey of Object.keys(bootstrapContract.routeSchemas)) {
    const invalidPayload = makeInvalidRoutePayload(routeKey, routePayloads[routeKey]);
    try {
      context.validateRoutePayload(routeKey, invalidPayload, `${routeKey} payload does not match runtime contract schema`);
      throw new Error(`expected invalid ${routeKey} schema rejection`);
    } catch (err) {
      if (!String(err.message || err).includes(`${routeKey} payload does not match runtime contract schema`)) {
        throw err;
      }
    }
  }
  if (!document.getElementById('deviceMeta').textContent.includes('ASSET OK')) {
    throw new Error('topbar asset status did not render');
  }
  const originalCatalog = fetchPayloads[bootstrapContract.routes.actionsCatalog];
  fetchPayloads[bootstrapContract.routes.actionsCatalog] = { ok: true };
  try {
    await context.refreshCommandCatalog();
    throw new Error('expected invalid actions catalog schema rejection');
  } catch (err) {
    if (!String(err.message || err).includes('actionsCatalog payload does not match runtime contract schema')) {
      throw err;
    }
  }
  fetchPayloads[bootstrapContract.routes.actionsCatalog] = originalCatalog;
  await context.refreshCommandCatalog();
  const originalHealth = fetchPayloads[bootstrapContract.routes.health];
  fetchPayloads[bootstrapContract.routes.health] = { ok: true, ip: '192.168.1.10', uptimeMs: 123456, filesystemReady: true, assetContractReady: true };
  lastToast = null;
  await context.refreshState();
  if (!String(lastToast || '').includes('health payload does not match runtime contract schema')) {
    throw new Error('expected invalid health schema rejection');
  }
  fetchPayloads[bootstrapContract.routes.health] = originalHealth;

  const originalDisplay = fetchPayloads[bootstrapContract.routes.displayFrame];
  fetchPayloads[bootstrapContract.routes.displayFrame] = { ok: true, width: 128, height: 64, presentCount: 1 };
  lastToast = null;
  await context.refreshState();
  if (!String(lastToast || '').includes('displayFrame payload does not match runtime contract schema')) {
    throw new Error('expected invalid display frame schema rejection');
  }
  fetchPayloads[bootstrapContract.routes.displayFrame] = originalDisplay;

  const originalConfigReadback = fetchPayloads[bootstrapContract.routes.configDevice];
  fetchPayloads[bootstrapContract.routes.configDevice] = { ok: true, config: {} };
  lastToast = null;
  await context.refreshState();
  if (!String(lastToast || '').includes('configDevice payload does not match runtime contract schema')) {
    throw new Error('expected invalid config readback schema rejection');
  }
  fetchPayloads[bootstrapContract.routes.configDevice] = originalConfigReadback;
  await context.loadStorageSemantics();
  const originalContractPayload = fetchPayloads[bootstrapContract.routes.contract];
  fetchPayloads[bootstrapContract.routes.contract] = { ok: true };
  try {
    await context.loadContract();
    throw new Error('expected invalid contract schema rejection');
  } catch (err) {
    if (!String(err.message || err).includes('runtime contract payload does not match runtime contract schema')) {
      throw err;
    }
  }
  fetchPayloads[bootstrapContract.routes.contract] = originalContractPayload;
  await context.loadContract();

  const originalStorageSemantics = fetchPayloads[bootstrapContract.routes.storageSemantics];
  fetchPayloads[bootstrapContract.routes.storageSemantics] = { ok: true, apiVersion: bootstrapContract.apiVersion, objects: [] };
  try {
    await context.loadStorageSemantics();
    throw new Error('expected invalid storage semantics schema rejection');
  } catch (err) {
    if (!String(err.message || err).includes('storageSemantics payload does not match runtime contract schema')) {
      throw err;
    }
  }
  fetchPayloads[bootstrapContract.routes.storageSemantics] = originalStorageSemantics;
  await context.loadStorageSemantics();

  fetchPayloads['POST ' + bootstrapContract.routes.command] = { ok: true, actionId: 1, requestId: 1, actionType: 'COMMAND', trackPath: bootstrapContract.routes.actionsStatus + '?id=1' };
  try {
    await context.sendCommand('storageManualFlush');
    throw new Error('expected invalid tracked action ack schema rejection');
  } catch (err) {
    if (!String(err.message || err).includes('tracked action ack payload does not match runtime contract schema')) {
      throw err;
    }
  }
  fetchPayloads['POST ' + bootstrapContract.routes.command] = { ok: true, actionId: 1, requestId: 1, actionType: 'COMMAND', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1, trackedActionAccepted: { actionId: 1, requestId: 1, actionType: 'COMMAND', trackPath: bootstrapContract.routes.actionsStatus + '?id=1', queueDepth: 1 } };
  try {
    await context.waitForActionResult(999, bootstrapContract.routes.actionsStatus + '?id=999');
    throw new Error('expected invalid action status schema rejection');
  } catch (err) {
    if (!String(err.message || err).includes('action status payload does not match runtime contract schema')) {
      throw err;
    }
  }
  fetchPayloads['POST ' + bootstrapContract.routes.configDevice] = { ok: true, runtimeReload: { runtimeReloadRequested: true } };
  try {
    await context.saveConfig();
    throw new Error('expected invalid config schema rejection');
  } catch (err) {
    if (!String(err.message || err).includes('device config update payload does not match runtime contract schema')) {
      throw err;
    }
  }
  fetchPayloads['POST ' + bootstrapContract.routes.configDevice] = {
    ok: true,
    runtimeReload: { runtimeReloadRequested: true, runtimeReloaded: true, runtimeReloadAuthoritativePath: true },
    wifiSaved: true, networkSaved: true, tokenSaved: true, runtimeReloadRequested: true, runtimeReloaded: true, runtimeReloadAuthoritativePath: true, filesystemReady: true, filesystemAssetsReady: true, filesystemStatus: 'READY', message: 'config saved',
    deviceConfigUpdate: { wifiSaved: true, networkSaved: true, tokenSaved: true, filesystemReady: true, filesystemAssetsReady: true, filesystemStatus: 'READY' },
  };
  fetchPayloads['POST ' + bootstrapContract.routes.resetDeviceConfig] = { ok: true, message: 'device config reset', resetKind: 'DEVICE_CONFIG', runtimeReload: { runtimeReloadRequested: true } };
  try {
    await context.postReset(bootstrapContract.routes.resetDeviceConfig, 'x', 'y');
    throw new Error('expected invalid reset schema rejection');
  } catch (err) {
    if (!String(err.message || err).includes('reset action payload does not match runtime contract schema')) {
      throw err;
    }
  }
  console.log('[OK] web runtime smoke passed');
})().catch((err) => {
  console.error(err && err.stack ? err.stack : String(err));
  process.exit(1);
});
