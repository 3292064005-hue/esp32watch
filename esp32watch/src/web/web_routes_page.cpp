#include "web/web_routes_page.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

static String web_fallback_index_html(void)
{
    String html;
    html.reserve(1024);
    html += "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>ESP32 Watch Console</title>";
    html += "<style>body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0;display:grid;place-items:center;min-height:100vh;padding:24px}";
    html += ".card{max-width:560px;background:#1c1c1c;border:1px solid #333;border-radius:16px;padding:24px;line-height:1.6}";
    html += "code{background:#222;padding:2px 6px;border-radius:6px}</style></head><body>";
    html += "<div class=\"card\"><h1>ESP32 Watch Console</h1>";
    html += "<p>LittleFS 里没有找到 <code>/index.html</code>。Web 服务已启动，但静态资源尚未写入分区。</p>";
    html += "<p>请执行 <code>pio run -e esp32s3_n16r8 -t uploadfs</code> 上传 <code>data/</code> 目录，";
    html += "或者先用这个兜底页确认设备在线。</p>";
    html += "<p>如果你刚刷过机，这通常只是文件系统还没上传，不一定是损坏。</p></div></body></html>";
    return html;
}

void web_register_page_routes(AsyncWebServer &server)
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) {
            AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html; charset=utf-8");
            if (response != nullptr) {
                response->addHeader("Cache-Control", "no-store, max-age=0");
                response->addHeader("Pragma", "no-cache");
                request->send(response);
            } else {
                request->send(500, "text/plain", "Failed to load index.html");
            }
        } else {
            Serial.println("[WEB] index.html not found in LittleFS, serving fallback page");
            AsyncWebServerResponse *response = request->beginResponse(200, "text/html; charset=utf-8", web_fallback_index_html());
            if (response != nullptr) {
                response->addHeader("Cache-Control", "no-store, max-age=0");
                response->addHeader("Pragma", "no-cache");
                request->send(response);
            } else {
                request->send(200, "text/html; charset=utf-8", web_fallback_index_html());
            }
        }
    });

    server.serveStatic("/app.css", LittleFS, "/app.css")
        .setCacheControl("no-store, max-age=0");

    server.serveStatic("/app.js", LittleFS, "/app.js")
        .setCacheControl("no-store, max-age=0");

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not Found");
    });
}
