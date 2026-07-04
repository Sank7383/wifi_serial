#ifndef WEBSERVER_HANDLERS_H
#define WEBSERVER_HANDLERS_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "util.h"
#include "webpage.h"
#include "websocket_handler.h"
#include "espnow_handler.h"

ESP8266WebServer server(80);

// Implemented in the main sketch.
void applyBaudRateLive(uint32_t baud);
void requestReboot(uint32_t delayMs);

inline bool requireAuth() {
  if (cfg.authDisabled) return true; // explicitly opted into open access
  if (!server.authenticate(cfg.authUser, cfg.authPass)) {
    server.requestAuthentication(BASIC_AUTH, "ESP WiFi-Serial", "Sign in required");
    return false;
  }
  return true;
}

inline void handleRoot() {
  if (!requireAuth()) return;
  server.send_P(200, "text/html", INDEX_HTML);
}

inline void handleGetConfig() {
  if (!requireAuth()) return;
  JsonDocument doc;
  doc["deviceName"] = cfg.deviceName;
  doc["wifiOpMode"] = cfg.wifiOpMode;
  doc["staSsid"] = cfg.staSsid;
  doc["apSsid"] = cfg.apSsid;
  doc["useStaticIp"] = cfg.useStaticIp;
  doc["ip"] = cfg.ip;
  doc["gateway"] = cfg.gateway;
  doc["subnet"] = cfg.subnet;
  doc["dns"] = cfg.dns;
  doc["baudRate"] = cfg.baudRate;
  doc["authUser"] = cfg.authUser;
  doc["authDisabled"] = cfg.authDisabled;
  doc["espnowEnabled"] = cfg.espnowEnabled;
  doc["espnowMode"] = cfg.espnowMode;
  doc["espnowPeerMac"] = cfg.espnowPeerMac;
  doc["espnowChannel"] = cfg.espnowChannel;
  doc["serialToWs"] = cfg.serialToWs;
  doc["wsToSerial"] = cfg.wsToSerial;
  doc["serialToEspnow"] = cfg.serialToEspnow;
  doc["espnowToSerial"] = cfg.espnowToSerial;
  doc["espnowToWs"] = cfg.espnowToWs;
  // Note: staPass / apPass / authPass are intentionally never returned.
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

inline void handlePostConfig() {
  if (!requireAuth()) return;
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "missing body"); return; }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "text/plain", "invalid json");
    return;
  }

  bool wifiChanged = false;

  // -- device name --
  const char *deviceName = doc["deviceName"] | cfg.deviceName;
  if (strlen(deviceName) > 0 && strlen(deviceName) < sizeof(cfg.deviceName)) {
    strlcpy(cfg.deviceName, deviceName, sizeof(cfg.deviceName));
  }

  // -- wifi mode --
  uint8_t wifiOpMode = doc["wifiOpMode"] | cfg.wifiOpMode;
  if (wifiOpMode > WIFI_OPMODE_AP_STA) { server.send(400, "text/plain", "invalid wifiOpMode"); return; }
  if (wifiOpMode != cfg.wifiOpMode) { wifiChanged = true; }
  cfg.wifiOpMode = wifiOpMode;

  // -- STA --
  String staSsid = doc["staSsid"] | String(cfg.staSsid);
  if (staSsid.length() >= sizeof(cfg.staSsid)) { server.send(400, "text/plain", "ssid too long"); return; }
  if (staSsid != String(cfg.staSsid)) wifiChanged = true;
  strlcpy(cfg.staSsid, staSsid.c_str(), sizeof(cfg.staSsid));

  if (doc["staPass"].is<const char*>() && strlen(doc["staPass"].as<const char*>()) > 0) {
    String p = doc["staPass"].as<String>();
    if (p.length() < 8 || p.length() >= sizeof(cfg.staPass)) {
      server.send(400, "text/plain", "sta password must be 8-64 chars");
      return;
    }
    strlcpy(cfg.staPass, p.c_str(), sizeof(cfg.staPass));
    wifiChanged = true;
  }

  // -- AP --
  String apSsid = doc["apSsid"] | String(cfg.apSsid);
  if (apSsid.length() == 0 || apSsid.length() >= sizeof(cfg.apSsid)) {
    server.send(400, "text/plain", "invalid AP ssid"); return;
  }
  if (apSsid != String(cfg.apSsid)) wifiChanged = true;
  strlcpy(cfg.apSsid, apSsid.c_str(), sizeof(cfg.apSsid));

  if (doc["apPass"].is<const char*>() && strlen(doc["apPass"].as<const char*>()) > 0) {
    String p = doc["apPass"].as<String>();
    if (p.length() < 8 || p.length() >= sizeof(cfg.apPass)) {
      server.send(400, "text/plain", "AP password must be 8-64 chars");
      return;
    }
    strlcpy(cfg.apPass, p.c_str(), sizeof(cfg.apPass));
    wifiChanged = true;
  }

  // -- static IP --
  bool useStaticIp = doc["useStaticIp"] | cfg.useStaticIp;
  String ip = doc["ip"] | String(cfg.ip);
  String gw = doc["gateway"] | String(cfg.gateway);
  String sn = doc["subnet"] | String(cfg.subnet);
  String dns = doc["dns"] | String(cfg.dns);
  if (!isValidIpOrEmpty(ip) || !isValidIpOrEmpty(gw) || !isValidIpOrEmpty(sn) || !isValidIpOrEmpty(dns)) {
    server.send(400, "text/plain", "invalid IP address"); return;
  }
  if (useStaticIp != cfg.useStaticIp || ip != String(cfg.ip) || gw != String(cfg.gateway) || sn != String(cfg.subnet)) {
    wifiChanged = true;
  }
  cfg.useStaticIp = useStaticIp;
  strlcpy(cfg.ip, ip.c_str(), sizeof(cfg.ip));
  strlcpy(cfg.gateway, gw.c_str(), sizeof(cfg.gateway));
  strlcpy(cfg.subnet, sn.c_str(), sizeof(cfg.subnet));
  strlcpy(cfg.dns, dns.c_str(), sizeof(cfg.dns));

  // -- serial --
  uint32_t baud = doc["baudRate"] | cfg.baudRate;
  if (!isValidBaud(baud)) { server.send(400, "text/plain", "invalid baud rate"); return; }
  bool baudChanged = baud != cfg.baudRate;
  cfg.baudRate = baud;

  // -- admin auth --
  String authUser = doc["authUser"] | String(cfg.authUser);
  if (authUser.length() == 0 || authUser.length() >= sizeof(cfg.authUser)) {
    server.send(400, "text/plain", "invalid admin username"); return;
  }
  strlcpy(cfg.authUser, authUser.c_str(), sizeof(cfg.authUser));
  if (doc["authPass"].is<const char*>() && strlen(doc["authPass"].as<const char*>()) > 0) {
    String p = doc["authPass"].as<String>();
    if (p.length() < 6 || p.length() >= sizeof(cfg.authPass)) {
      server.send(400, "text/plain", "admin password must be 6-32 chars");
      return;
    }
    strlcpy(cfg.authPass, p.c_str(), sizeof(cfg.authPass));
  }
  cfg.authDisabled = doc["authDisabled"] | cfg.authDisabled;

  // -- ESP-NOW --
  bool espnowEnabled = doc["espnowEnabled"] | cfg.espnowEnabled;
  uint8_t espnowMode = doc["espnowMode"] | cfg.espnowMode;
  if (espnowMode > 1) { server.send(400, "text/plain", "invalid espnowMode"); return; }
  String peerMac = doc["espnowPeerMac"] | String(cfg.espnowPeerMac);
  peerMac.toUpperCase();
  if (peerMac.length() > 0 && !isValidMac(peerMac)) {
    server.send(400, "text/plain", "invalid ESP-NOW MAC address"); return;
  }
  uint8_t espnowChannel = doc["espnowChannel"] | cfg.espnowChannel;
  if (espnowChannel < 1 || espnowChannel > 13) { server.send(400, "text/plain", "invalid channel"); return; }

  cfg.espnowEnabled = espnowEnabled;
  cfg.espnowMode = espnowMode;
  strlcpy(cfg.espnowPeerMac, peerMac.c_str(), sizeof(cfg.espnowPeerMac));
  cfg.espnowChannel = espnowChannel;
  cfg.serialToWs     = doc["serialToWs"]     | cfg.serialToWs;
  cfg.wsToSerial     = doc["wsToSerial"]     | cfg.wsToSerial;
  cfg.serialToEspnow = doc["serialToEspnow"] | cfg.serialToEspnow;
  cfg.espnowToSerial = doc["espnowToSerial"] | cfg.espnowToSerial;
  cfg.espnowToWs     = doc["espnowToWs"]     | cfg.espnowToWs;
  cfg.provisioned = true;

  if (!saveConfig()) {
    server.send(500, "text/plain", "failed to persist config");
    return;
  }

  // Apply what can be applied live, without a reboot.
  if (baudChanged) applyBaudRateLive(cfg.baudRate);
  if (cfg.espnowEnabled) espnowApplyPeer(); else espnowDeinit();

  JsonDocument resp;
  resp["ok"] = true;
  resp["rebootRequired"] = wifiChanged;
  String out;
  serializeJson(resp, out);
  server.send(200, "application/json", out);

  if (wifiChanged) requestReboot(800);
}

// Lightweight endpoint for the Serial Monitor tab: changes the baud rate
// immediately without touching the rest of the config payload, so it can't
// accidentally clobber concurrent edits made on other tabs.
inline void handlePostBaud() {
  if (!requireAuth()) return;
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "missing body"); return; }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "text/plain", "invalid json");
    return;
  }

  uint32_t baud = doc["baudRate"] | 0UL;
  if (!isValidBaud(baud)) { server.send(400, "text/plain", "invalid baud rate"); return; }

  cfg.baudRate = baud;
  if (!saveConfig()) {
    server.send(500, "text/plain", "failed to persist config");
    return;
  }
  applyBaudRateLive(cfg.baudRate);

  JsonDocument resp;
  resp["ok"] = true;
  resp["baudRate"] = cfg.baudRate;
  String out;
  serializeJson(resp, out);
  server.send(200, "application/json", out);
}

inline void handleGetStatus() {
  if (!requireAuth()) return;
  JsonDocument doc;
  doc["fwVersion"] = FW_VERSION;
  doc["mode"] = cfg.wifiOpMode == WIFI_OPMODE_AP ? "AP" :
                (cfg.wifiOpMode == WIFI_OPMODE_AP_STA ? "AP+STA" : "STA");
  doc["ip"] = cfg.wifiOpMode == WIFI_OPMODE_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  if (cfg.wifiOpMode == WIFI_OPMODE_AP_STA) doc["apIp"] = WiFi.softAPIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["rssi"] = cfg.wifiOpMode != WIFI_OPMODE_AP ? WiFi.RSSI() : 0;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptimeMs"] = millis();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

inline void handleGetScan() {
  if (!requireAuth()) return;
  // Synchronous scan: simpler and avoids a two-click UX for a rare, manual
  // admin action. Briefly pauses the bridge loop for ~2-4s, which is fine.
  int n = WiFi.scanNetworks();

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 30; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["secure"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
  }
  WiFi.scanDelete();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

inline void handleGetWsToken() {
  if (!requireAuth()) return;
  JsonDocument doc;
  doc["token"] = wsGenerateToken();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

inline void handlePostReboot() {
  if (!requireAuth()) return;
  server.send(200, "application/json", "{\"ok\":true}");
  requestReboot(500);
}

inline void handleNotFound() {
  server.send(404, "text/plain", "not found");
}

inline void webServerInit() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/baud", HTTP_POST, handlePostBaud);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/scan", HTTP_GET, handleGetScan);
  server.on("/api/wstoken", HTTP_GET, handleGetWsToken);
  server.on("/api/reboot", HTTP_POST, handlePostReboot);
  server.onNotFound(handleNotFound);
  server.begin();
}

#endif // WEBSERVER_HANDLERS_H
