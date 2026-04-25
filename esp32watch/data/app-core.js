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


function contractRouteOperationSchema(routeKey, method) {
  const schema = contractRouteSchema(routeKey);
  const operations = schema?.operations || {};
  return operations[method || schema?.defaultMethod || "GET"] || schema;
}

function payloadHasRouteResponseFields(payload, routeKey, method) {
  const schema = contractRouteOperationSchema(routeKey, method);
  if (!schema?.responseFields) return true;
  return schema.responseFields.every(key => payloadHasPath(payload || {}, key));
}

function validateRoutePayload(routeKey, payload, errorMessage, method) {
  const routeSchema = contractRouteOperationSchema(routeKey, method);
  if (!routeSchema) return payload;
  if (routeSchema.kind === "state") {
    if (!payloadHasDeclaredSections(payload, routeSchema.name) || !payloadHasRouteResponseFields(payload, routeKey, method)) {
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
    const operations = schema.operations && typeof schema.operations === "object" ? schema.operations : { [schema.defaultMethod || "GET"]: schema };
    Object.entries(operations).forEach(([method, opSchema]) => {
      if (opSchema.kind === "api") {
        if (!apiSchemas[opSchema.name]) {
          throw new Error(`runtime contract missing api schema ${opSchema.name} for ${key}.${method}`);
        }
      } else if (opSchema.kind === "state") {
        if (!stateSchemas[opSchema.name]) {
          throw new Error(`runtime contract missing state schema ${opSchema.name} for ${key}.${method}`);
        }
      } else {
        throw new Error(`runtime contract route schema ${key}.${method} has unsupported kind ${opSchema.kind}`);
      }
    });
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

    if (!payloadHasDeclaredSections(aggregate, "aggregate") || !payloadHasRouteResponseFields(aggregate, "stateAggregate", "GET")) {
      throw new Error("state aggregate payload does not match runtime contract schema");
    }

    try {
      const perfPayload = await api(contractRoute("statePerf") || "/api/state/perf");
      if (!payloadHasDeclaredSections(perfPayload, "perf") || !payloadHasRouteResponseFields(perfPayload, "statePerf", "GET")) {
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
