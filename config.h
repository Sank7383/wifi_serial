#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ---- Tunables -------------------------------------------------------------
#define CONFIG_PATH        "/config.json"
#define CONFIG_MAX_BYTES   1536
#define FW_VERSION         "1.0.0"

enum WifiOpMode : uint8_t { WIFI_OPMODE_STA = 0, WIFI_OPMODE_AP = 1, WIFI_OPMODE_AP_STA = 2 };
enum EspNowMode : uint8_t { ESPNOW_MODE_BROADCAST = 0, ESPNOW_MODE_UNICAST = 1 };

struct AppConfig {
  char deviceName[32]     = "ESP-WiFiSerial";

  // Network
  uint8_t wifiOpMode      = WIFI_OPMODE_AP;     // AP until user configures STA
  char staSsid[33]        = "";
  char staPass[65]        = "";
  char apSsid[33]         = "";                 // generated at first boot if empty
  char apPass[65]         = "";                 // generated at first boot if empty
  bool useStaticIp        = false;
  char ip[16]             = "";
  char gateway[16]        = "";
  char subnet[16]         = "255.255.255.0";
  char dns[16]            = "";

  // Serial
  uint32_t baudRate       = 115200;

  // Admin auth (HTTP Basic + WebSocket)
  char authUser[24]       = "admin";
  char authPass[33]       = "";                 // generated at first boot if empty
  bool authDisabled       = false;              // true => dashboard/API open to anyone on the network, no login

  // ESP-NOW
  bool espnowEnabled      = false;
  uint8_t espnowMode      = ESPNOW_MODE_BROADCAST;
  char espnowPeerMac[18]  = "";                 // "AA:BB:CC:DD:EE:FF"
  uint8_t espnowChannel   = 1;

  // Bridging toggles. "Outgoing" covers both UART RX and text typed into
  // the web monitor — from the bridge's point of view they're the same
  // logical data stream, just entering from two different sources.
  bool serialToWs         = true;   // UART RX        -> WebSocket clients
  bool wsToSerial         = true;   // WebSocket input -> UART TX
  bool serialToEspnow     = false;  // UART RX + WebSocket input -> ESP-NOW
  bool espnowToSerial     = true;   // ESP-NOW        -> UART TX
  bool espnowToWs         = true;   // ESP-NOW        -> WebSocket clients

  bool provisioned        = false;  // false => first-boot, forces setup flow
};

extern AppConfig cfg;

// Generates a short unique suffix from the chip id, e.g. "A3F1"
inline String chipSuffix() {
  char buf[9];
  snprintf(buf, sizeof(buf), "%08X", ESP.getChipId());
  return String(buf + 4); // last 4 hex chars
}

// Cryptographically-adequate-enough random password for a local admin UI.
// Uses the hardware RNG (ESP.random / os RNG) seeded per device, so every
// unit ships with a UNIQUE default credential instead of a shared one.
inline String randomPassword(uint8_t len) {
  static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  String out;
  out.reserve(len);
  for (uint8_t i = 0; i < len; i++) {
    out += alphabet[secureRandom(0, sizeof(alphabet) - 1)];
  }
  return out;
}

inline void applyFirstBootDefaults(bool printToSerial) {
  if (cfg.apSsid[0] == '\0') {
    String ssid = "ESP-WiFiSerial-" + chipSuffix();
    strlcpy(cfg.apSsid, ssid.c_str(), sizeof(cfg.apSsid));
  }
  if (cfg.apPass[0] == '\0') {
    strlcpy(cfg.apPass, randomPassword(10).c_str(), sizeof(cfg.apPass));
  }
  if (cfg.authPass[0] == '\0') {
    strlcpy(cfg.authPass, randomPassword(10).c_str(), sizeof(cfg.authPass));
  }
  if (printToSerial) {
    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F(" FIRST BOOT - device provisioned with unique credentials"));
    Serial.print(F("   AP SSID      : ")); Serial.println(cfg.apSsid);
    Serial.print(F("   AP Password  : ")); Serial.println(cfg.apPass);
    Serial.print(F("   Admin user   : ")); Serial.println(cfg.authUser);
    Serial.print(F("   Admin password: ")); Serial.println(cfg.authPass);
    Serial.println(F(" Change these from the web UI (WiFi/Admin tab) after connecting."));
    Serial.println(F("=================================================="));
  }
}

inline bool loadConfig() {
  if (!LittleFS.exists(CONFIG_PATH)) return false;
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;
  if (f.size() == 0 || f.size() > CONFIG_MAX_BYTES) { f.close(); return false; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  strlcpy(cfg.deviceName, doc["deviceName"] | cfg.deviceName, sizeof(cfg.deviceName));
  cfg.wifiOpMode  = doc["wifiOpMode"]  | cfg.wifiOpMode;
  strlcpy(cfg.staSsid, doc["staSsid"] | "", sizeof(cfg.staSsid));
  strlcpy(cfg.staPass, doc["staPass"] | "", sizeof(cfg.staPass));
  strlcpy(cfg.apSsid,  doc["apSsid"]  | "", sizeof(cfg.apSsid));
  strlcpy(cfg.apPass,  doc["apPass"]  | "", sizeof(cfg.apPass));
  cfg.useStaticIp = doc["useStaticIp"] | false;
  strlcpy(cfg.ip,      doc["ip"]      | "", sizeof(cfg.ip));
  strlcpy(cfg.gateway,  doc["gateway"] | "", sizeof(cfg.gateway));
  strlcpy(cfg.subnet,  doc["subnet"]  | "255.255.255.0", sizeof(cfg.subnet));
  strlcpy(cfg.dns,     doc["dns"]     | "", sizeof(cfg.dns));
  cfg.baudRate = doc["baudRate"] | 115200UL;
  strlcpy(cfg.authUser, doc["authUser"] | "admin", sizeof(cfg.authUser));
  strlcpy(cfg.authPass, doc["authPass"] | "", sizeof(cfg.authPass));
  cfg.authDisabled = doc["authDisabled"] | false;
  cfg.espnowEnabled = doc["espnowEnabled"] | false;
  cfg.espnowMode    = doc["espnowMode"]    | ESPNOW_MODE_BROADCAST;
  strlcpy(cfg.espnowPeerMac, doc["espnowPeerMac"] | "", sizeof(cfg.espnowPeerMac));
  cfg.espnowChannel = doc["espnowChannel"] | 1;
  cfg.serialToWs     = doc["serialToWs"]     | true;
  cfg.wsToSerial     = doc["wsToSerial"]     | true;
  cfg.serialToEspnow = doc["serialToEspnow"] | false;
  cfg.espnowToSerial = doc["espnowToSerial"] | true;
  cfg.espnowToWs     = doc["espnowToWs"]     | true;
  cfg.provisioned    = doc["provisioned"]    | false;
  return true;
}

inline bool saveConfig() {
  JsonDocument doc;
  doc["deviceName"]     = cfg.deviceName;
  doc["wifiOpMode"]     = cfg.wifiOpMode;
  doc["staSsid"]        = cfg.staSsid;
  doc["staPass"]        = cfg.staPass;
  doc["apSsid"]         = cfg.apSsid;
  doc["apPass"]         = cfg.apPass;
  doc["useStaticIp"]    = cfg.useStaticIp;
  doc["ip"]             = cfg.ip;
  doc["gateway"]        = cfg.gateway;
  doc["subnet"]         = cfg.subnet;
  doc["dns"]            = cfg.dns;
  doc["baudRate"]       = cfg.baudRate;
  doc["authUser"]       = cfg.authUser;
  doc["authPass"]       = cfg.authPass;
  doc["authDisabled"]   = cfg.authDisabled;
  doc["espnowEnabled"]  = cfg.espnowEnabled;
  doc["espnowMode"]     = cfg.espnowMode;
  doc["espnowPeerMac"]  = cfg.espnowPeerMac;
  doc["espnowChannel"]  = cfg.espnowChannel;
  doc["serialToWs"]     = cfg.serialToWs;
  doc["wsToSerial"]     = cfg.wsToSerial;
  doc["serialToEspnow"] = cfg.serialToEspnow;
  doc["espnowToSerial"] = cfg.espnowToSerial;
  doc["espnowToWs"]     = cfg.espnowToWs;
  doc["provisioned"]    = cfg.provisioned;

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

#endif // CONFIG_H
