// ESP8266 WiFi-Serial Bridge
// ---------------------------------------------------------------------------
// Streams the hardware UART over a browser-based WebSocket serial monitor,
// with optional ESP-NOW forwarding (broadcast or unicast) of the same data.
// Full network (STA/AP) configuration is exposed through the same web UI.
//
// Required libraries (Arduino Library Manager):
//   - ArduinoJson        (>= 7.0)
//   - WebSockets         by Markus Sattler / Links2004  (>= 2.4)
// Board package: ESP8266 core for Arduino (>= 3.0), Flash size w/ LittleFS.
//
// See README.md for wiring, first-boot credentials and library setup.
// ---------------------------------------------------------------------------

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>

#include "config.h"
#include "util.h"
#include "espnow_handler.h"
#include "websocket_handler.h"
#include "programmer_handler.h"
#include "webserver_handlers.h"

AppConfig cfg;

static bool g_rebootPending = false;
static unsigned long g_rebootAt = 0;

#define SERIAL_BRIDGE_FLUSH_MS 15

// ---------------------------------------------------------------------------
// Wi-Fi setup
// ---------------------------------------------------------------------------
// Assumes the caller has already set the desired WiFi.mode() (AP, STA or
// AP_STA) — this only brings the AP interface up within that mode.
static void startAccessPoint() {
  WiFi.softAP(cfg.apSsid, cfg.apPass);
  Serial.print(F("[wifi] AP started: "));
  Serial.print(cfg.apSsid);
  Serial.print(F("  IP: "));
  Serial.println(WiFi.softAPIP());
}

// Assumes the caller has already set the desired WiFi.mode().
static bool connectStation() {
  if (cfg.useStaticIp && strlen(cfg.ip) && strlen(cfg.gateway) && strlen(cfg.subnet)) {
    IPAddress ip, gw, sn, dns;
    ip.fromString(cfg.ip);
    gw.fromString(cfg.gateway);
    sn.fromString(cfg.subnet);
    if (strlen(cfg.dns)) dns.fromString(cfg.dns); else dns = gw;
    WiFi.config(ip, gw, sn, dns);
  }
  WiFi.begin(cfg.staSsid, cfg.staPass);
  Serial.print(F("[wifi] Connecting to "));
  Serial.print(cfg.staSsid);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[wifi] Connected, IP: "));
    Serial.println(WiFi.localIP());
    return true;
  }
  return false;
}

static void setupNetwork() {
  if (cfg.wifiOpMode == WIFI_OPMODE_AP_STA) {
    WiFi.mode(WIFI_AP_STA);
    startAccessPoint();
    if (strlen(cfg.staSsid) > 0 && !connectStation()) {
      Serial.println(F("[wifi] STA connect failed in AP+STA mode; AP interface remains reachable."));
    }
  } else if (cfg.wifiOpMode == WIFI_OPMODE_STA && strlen(cfg.staSsid) > 0) {
    WiFi.mode(WIFI_STA);
    if (!connectStation()) {
      Serial.println(F("[wifi] STA connect failed, falling back to AP so the device stays reachable."));
      WiFi.mode(WIFI_AP);
      startAccessPoint();
    }
  } else {
    WiFi.mode(WIFI_AP);
    startAccessPoint();
  }
}

static void setupMdns() {
  String host = "espwifiserial";
  if (strlen(cfg.deviceName) > 0) {
    host = "";
    for (size_t i = 0; i < strlen(cfg.deviceName); i++) {
      char c = tolower(cfg.deviceName[i]);
      if (isalnum(c)) host += c;
      else if (host.length() && host[host.length() - 1] != '-') host += '-';
    }
    if (host.length() == 0) host = "espwifiserial";
  }
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.print(F("[mdns] http://"));
    Serial.print(host);
    Serial.println(F(".local/"));
  }
}

// ---------------------------------------------------------------------------
// Bridge glue — called from the WebSocket and ESP-NOW handlers.
// ---------------------------------------------------------------------------
void handleBridgeDataFromWs(const uint8_t *data, size_t len) {
  if (cfg.programmerEnabled) return; // UART is exclusively reserved for the esptool bridge
  if (cfg.wsToSerial) Serial.write(data, len);
  if (cfg.serialToEspnow) espnowSend(data, len);
}

void handleEspNowRecv(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  if (cfg.programmerEnabled) return; // UART is exclusively reserved for the esptool bridge
  if (cfg.espnowToSerial) Serial.write(data, len);
  if (cfg.espnowToWs) wsSendEspNowFrame(mac, data, len);
}

void applyBaudRateLive(uint32_t baud) {
  Serial.flush();
  delay(20);
  Serial.begin(baud);
}

void requestReboot(uint32_t delayMs) {
  g_rebootPending = true;
  g_rebootAt = millis() + delayMs;
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); // provisional; switched to cfg.baudRate once loaded
  delay(200);

  if (!LittleFS.begin()) {
    Serial.println(F("[fs] LittleFS mount failed, formatting..."));
    LittleFS.format();
    LittleFS.begin();
  }

  bool haveConfig = loadConfig();
  applyFirstBootDefaults(!haveConfig || !cfg.provisioned);
  if (!haveConfig) saveConfig();

  Serial.flush();
  Serial.begin(cfg.baudRate);

  setupNetwork();
  setupMdns();

  espnowInit();
  webServerInit();
  wsInit();
  pgmInit(); // may re-apply baud/pins if this unit boots directly into Programmer mode

  Serial.println(F("[boot] ESP WiFi-Serial bridge ready."));
}

void loop() {
  server.handleClient();
  webSocket.loop();
  MDNS.update();

  if (cfg.programmerEnabled) {
    // Dedicated esptool <-> target UART bridge; the normal serial monitor
    // and ESP-NOW bridging below are suspended so nothing else touches Serial.
    pgmLoop();
  } else {
    // Buffer incoming UART bytes and flush as one bridge frame either when
    // the buffer fills or the line goes quiet, instead of one frame/byte.
    static uint8_t serialBuf[BRIDGE_CHUNK_MAX];
    static size_t serialLen = 0;
    static unsigned long lastByteMs = 0;

    while (Serial.available() && serialLen < BRIDGE_CHUNK_MAX) {
      serialBuf[serialLen++] = (uint8_t)Serial.read();
      lastByteMs = millis();
    }
    if (serialLen > 0 && (serialLen >= BRIDGE_CHUNK_MAX || millis() - lastByteMs > SERIAL_BRIDGE_FLUSH_MS)) {
      if (cfg.serialToWs) wsSendSerialFrame(serialBuf, serialLen);
      if (cfg.serialToEspnow) espnowSend(serialBuf, serialLen);
      serialLen = 0;
    }
  }

  if (g_rebootPending && millis() >= g_rebootAt) {
    Serial.println(F("[boot] Rebooting..."));
    Serial.flush();
    ESP.restart();
  }
}
