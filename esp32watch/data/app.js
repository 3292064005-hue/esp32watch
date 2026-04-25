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
