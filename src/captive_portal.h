#pragma once

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "nvs_config.h"

#define PORTAL_SSID "HB9IIU-Setup"

namespace CaptivePortal {

static DNSServer dnsServer;
static WebServer webServer(80);
static String    _cachedOptions;
static String    _pendingSsid;
static String    _pendingPass;
static uint32_t  _testStart = 0;

// ── WiFi scan → sorted <option> list ─────────────────────────────────────────

static String buildScanOptions() {
    String html;
    int n = WiFi.scanNetworks();
    if (n <= 0) {
        return "<option value='' disabled selected>No networks found</option>";
    }
    if (n > 30) n = 30;

    // Bubble-sort indices by RSSI descending
    int idx[30];
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (WiFi.RSSI(idx[j]) < WiFi.RSSI(idx[j + 1])) {
                int t = idx[j]; idx[j] = idx[j + 1]; idx[j + 1] = t;
            }

    for (int i = 0; i < n; i++) {
        int k = idx[i];
        bool open = WiFi.encryptionType(k) == WIFI_AUTH_OPEN;
        html += "<option value='"; html += WiFi.SSID(k); html += "'";
        if (i == 0) html += " selected";
        html += ">";
        html += WiFi.SSID(k);
        html += "  ("; html += WiFi.RSSI(k); html += " dBm";
        if (open) html += ", open";
        html += ")</option>";
    }
    WiFi.scanDelete();
    return html;
}

// ── HTML pages ────────────────────────────────────────────────────────────────

static String buildFormHtml() {
    String html = R"html(<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HB9IIU WiFi Setup</title>
<style>
  *{box-sizing:border-box}
  body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;
       display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
  .card{background:#16213e;padding:2rem;border-radius:12px;width:90%;max-width:380px}
  h2{color:#00d4ff;margin:0 0 .25rem;text-align:center}
  .sub{text-align:center;color:#666;font-size:.8rem;margin:0 0 1.5rem}
  label{font-size:.82rem;color:#aaa;display:block;margin-bottom:.3rem}
  select,input[type=text],input[type=password]{
    display:block;width:100%;padding:.7rem;margin-bottom:1.1rem;
    border-radius:6px;border:1px solid #333;background:#0f3460;
    color:#fff;font-size:.95rem}
  .divider{text-align:center;color:#444;font-size:.78rem;margin:.2rem 0 1rem}
  button{width:100%;padding:.8rem;background:#00d4ff;color:#1a1a2e;
         border:none;border-radius:6px;font-size:1rem;font-weight:bold;cursor:pointer}
  button:active{background:#00b0d0}
</style>
</head><body>
<div class="card">
  <h2>HB9IIU WiFi Setup</h2>
  <p class="sub">Select your network and enter the password</p>
  <form action="/save" method="POST">
    <label>Available networks</label>
    <select name="ssid">)html";
    html += _cachedOptions;
    html += R"html(</select>
    <label>Password</label>
    <input type="password" name="pass" placeholder="leave blank if open network">
    <button type="submit">Connect &amp; Save</button>
  </form>
</div>
</body></html>)html";
    return html;
}

static const char CHECKING_HTML[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset="UTF-8"><meta http-equiv="refresh" content="3">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Checking...</title>
<style>
  body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;
       display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;text-align:center}
  h2{font-size:2rem;color:#00d4ff;margin-bottom:1rem}
  p{font-size:1.2rem;margin:.5rem 0}
  .hint{font-size:.95rem;color:#555;margin-top:1rem}
</style>
</head><body><div>
<h2>Checking connection&hellip;</h2>
<p>Trying to reach your network.</p>
<p class="hint">This page refreshes automatically.</p>
</div></body></html>)html";

static const char ERROR_HTML[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Connection Failed</title>
<style>
  body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;
       display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;text-align:center}
  h2{font-size:2rem;color:#ff4444;margin-bottom:1rem}
  p{font-size:1.2rem}
  a{display:inline-block;margin-top:1.5rem;padding:.9rem 2.5rem;
    background:#00d4ff;color:#1a1a2e;border-radius:8px;text-decoration:none;
    font-size:1.1rem;font-weight:bold}
</style>
</head><body><div>
<h2>Connection Failed</h2>
<p>Could not connect &mdash; check the password and try again.</p>
<a href="/">Try Again</a>
</div></body></html>)html";

static const char SUCCESS_HTML[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Connected!</title>
<style>
  body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;
       display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;text-align:center}
  h2{font-size:2rem;color:#00ff88;margin-bottom:1rem}
  p{font-size:1.2rem}
</style>
</head><body><div>
<h2>Connected!</h2>
<p>Credentials saved. Device is rebooting&hellip;</p>
</div></body></html>)html";

// ── Request handlers ──────────────────────────────────────────────────────────

static void handleRoot() {
    String html = buildFormHtml();
    webServer.send(200, "text/html", html);
}

static void handleSave() {
    String ssid = webServer.arg("ssid");
    String pass = webServer.arg("pass");

    if (ssid.isEmpty()) {
        webServer.sendHeader("Location", "/");
        webServer.send(302);
        return;
    }

    Serial.printf("[portal] Testing SSID=%s\n", ssid.c_str());
    _pendingSsid = ssid;
    _pendingPass = pass;
    _testStart   = millis();

    WiFi.mode(WIFI_AP_STA);          // keep AP alive while testing STA
    WiFi.begin(ssid.c_str(), pass.c_str());

    webServer.sendHeader("Location", "/checking");
    webServer.send(302);
}

static void handleChecking() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[portal] Connected — saving credentials.");
        NVSConfig::saveWiFi(_pendingSsid.c_str(), _pendingPass.c_str());
        webServer.send_P(200, "text/html", SUCCESS_HTML);
        delay(2000);
        ESP.restart();
        return;
    }
    if (millis() - _testStart > 12000 || WiFi.status() == WL_CONNECT_FAILED) {
        Serial.println("[portal] Connection failed.");
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        webServer.send_P(200, "text/html", ERROR_HTML);
        return;
    }
    webServer.send_P(200, "text/html", CHECKING_HTML);   // still trying — auto-refresh
}

static void handleNotFound() {
    webServer.sendHeader("Location", "http://192.168.4.1/");
    webServer.send(302);
}

// ── Public API ────────────────────────────────────────────────────────────────

inline void start() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(PORTAL_SSID);   // open network

    Serial.println("[portal] Scanning networks...");
    _cachedOptions = buildScanOptions();

    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    webServer.on("/",         HTTP_GET,  handleRoot);
    webServer.on("/save",     HTTP_POST, handleSave);
    webServer.on("/checking", HTTP_GET,  handleChecking);
    webServer.onNotFound(handleNotFound);
    webServer.begin();

    Serial.printf("[portal] AP ready: %s  (open)  IP: 192.168.4.1\n", PORTAL_SSID);
}

inline void loop() {
    dnsServer.processNextRequest();
    webServer.handleClient();
}

} // namespace CaptivePortal
