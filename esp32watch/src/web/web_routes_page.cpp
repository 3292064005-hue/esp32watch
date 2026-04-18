#include "web/web_routes_page.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "web/web_server.h"
#include "web/web_contract_json.h"

static String web_fallback_index_html(void)
{
    String html;
    String fs_status = web_server_filesystem_status();
    html.reserve(1400);
    html += "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>ESP32 Watch Console</title>";
    html += "<style>body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0;display:grid;place-items:center;min-height:100vh;padding:24px}";
    html += ".card{max-width:640px;background:#1c1c1c;border:1px solid #333;border-radius:16px;padding:24px;line-height:1.6}";
    html += "code{background:#222;padding:2px 6px;border-radius:6px}.badge{display:inline-block;background:#222;border:1px solid #444;border-radius:999px;padding:4px 10px;margin-bottom:12px}</style></head><body>";
    html += "<div class=\"card\"><div class=\"badge\">Fallback Console</div><h1>ESP32 Watch Console</h1>";
    html += "<p>富控制台资源当前不可用，设备仍保留最小控制面与 API。你现在看到的是固件内置兜底页，而不是 LittleFS 中的正式控制台。</p>";
    html += "<p>当前文件系统状态：<code>";
    html += fs_status;
    html += "</code></p>";
    html += "<p>常见原因包括：静态资源未上传、资产契约缺失/版本不匹配、或者文件系统未成功挂载。</p>";
    html += "<p>可执行 <code>pio run -e &lt;your-env&gt; -t uploadfs</code> 上传 <code>data/</code> 目录，然后刷新页面复核。</p>";
    html += "</div></body></html>";
    return html;
}

static String web_fallback_contract_bootstrap_json(void)
{
    String json = "{";
    web_contract_append_json(json);
    json += "}";
    return json;
}

static void send_console_asset_or_fallback(AsyncWebServerRequest *request,
                                           const char *path,
                                           const char *content_type,
                                           const char *fallback_body)
{
    if (request == nullptr || path == nullptr || content_type == nullptr || fallback_body == nullptr) {
        return;
    }

    if (web_server_console_ready() && LittleFS.exists(path)) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, content_type);
        if (response != nullptr) {
            response->addHeader("Cache-Control", "no-store, max-age=0");
            response->addHeader("Pragma", "no-cache");
            request->send(response);
            return;
        }
        request->send(500, "text/plain", "Failed to load console asset");
        return;
    }

    AsyncWebServerResponse *response = request->beginResponse(503, content_type, fallback_body);
    if (response != nullptr) {
        response->addHeader("Cache-Control", "no-store, max-age=0");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("X-Web-Console-State", web_server_filesystem_status());
        request->send(response);
    } else {
        request->send(503, content_type, fallback_body);
    }
}

static void send_console_asset_or_fallback(AsyncWebServerRequest *request,
                                           const char *path,
                                           const char *content_type,
                                           const String &fallback_body)
{
    send_console_asset_or_fallback(request, path, content_type, fallback_body.c_str());
}

void web_register_page_routes(AsyncWebServer &server)
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (web_server_console_ready() && LittleFS.exists("/index.html")) {
            AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html; charset=utf-8");
            if (response != nullptr) {
                response->addHeader("Cache-Control", "no-store, max-age=0");
                response->addHeader("Pragma", "no-cache");
                request->send(response);
            } else {
                request->send(500, "text/plain", "Failed to load index.html");
            }
            return;
        }

        Serial.printf("[WEB] rich console unavailable, serving fallback page status=%s\n", web_server_filesystem_status());
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html; charset=utf-8", web_fallback_index_html());
        if (response != nullptr) {
            response->addHeader("Cache-Control", "no-store, max-age=0");
            response->addHeader("Pragma", "no-cache");
            response->addHeader("X-Web-Console-State", web_server_filesystem_status());
            request->send(response);
        } else {
            request->send(200, "text/html; charset=utf-8", web_fallback_index_html());
        }
    });

    server.on("/app.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        send_console_asset_or_fallback(request,
                                       "/app.css",
                                       "text/css; charset=utf-8",
                                       "/* rich console unavailable: fallback console only */");
    });

    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        send_console_asset_or_fallback(request,
                                       "/app.js",
                                       "application/javascript",
                                       "console.log(\"rich console unavailable: fallback console only\");");
    });

    server.on("/contract-bootstrap.json", HTTP_GET, [](AsyncWebServerRequest *request) {
        send_console_asset_or_fallback(request,
                                       "/contract-bootstrap.json",
                                       "application/json; charset=utf-8",
                                       web_fallback_contract_bootstrap_json().c_str());
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not Found");
    });
}
