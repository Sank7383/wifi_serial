#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
  #include <espnow.h>
}
#include "config.h"
#include "util.h"

// ESP-NOW payload limit on ESP8266
#define ESPNOW_MAX_PAYLOAD 250

static const uint8_t ESPNOW_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Implemented in the main sketch — forwards received ESP-NOW data to
// Serial / WebSocket according to the bridging toggles in cfg.
void handleEspNowRecv(const uint8_t *mac, const uint8_t *data, uint8_t len);

static uint8_t g_espnowActivePeer[6] = {0};
static bool g_espnowPeerAdded = false;

inline void espnowCurrentTarget(uint8_t out[6]) {
  if (cfg.espnowMode == ESPNOW_MODE_UNICAST && isValidMac(String(cfg.espnowPeerMac))) {
    macStringToBytes(String(cfg.espnowPeerMac), out);
  } else {
    memcpy(out, ESPNOW_BROADCAST_MAC, 6);
  }
}

inline void espnowApplyPeer() {
  uint8_t target[6];
  espnowCurrentTarget(target);

  if (g_espnowPeerAdded && memcmp(target, g_espnowActivePeer, 6) != 0) {
    esp_now_del_peer(g_espnowActivePeer);
    g_espnowPeerAdded = false;
  }

  if (!g_espnowPeerAdded) {
    WiFiMode_t m = WiFi.getMode();
    bool staConnected = (m == WIFI_STA || m == WIFI_AP_STA) && WiFi.status() == WL_CONNECTED;
    uint8_t channel = staConnected ? (uint8_t)WiFi.channel() : cfg.espnowChannel;
    if (esp_now_add_peer((uint8_t *)target, ESP_NOW_ROLE_COMBO, channel, NULL, 0) == 0) {
      memcpy(g_espnowActivePeer, target, 6);
      g_espnowPeerAdded = true;
    }
  }
}

inline void onEspNowSent(uint8_t *mac, uint8_t status) {
  (void)mac; (void)status; // hook available for status/log reporting if needed
}

inline void onEspNowRecvRaw(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  handleEspNowRecv(mac, incomingData, len);
}

inline bool espnowInit() {
  // ESP-NOW requires the STA interface to be active on ESP8266; add it
  // without disturbing an already-active AP (AP -> AP_STA, OFF -> STA).
  WiFiMode_t m = WiFi.getMode();
  if (m == WIFI_OFF) WiFi.mode(WIFI_STA);
  else if (m == WIFI_AP) WiFi.mode(WIFI_AP_STA);

  if (esp_now_init() != 0) return false;
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(onEspNowRecvRaw);
  espnowApplyPeer();
  return true;
}

inline void espnowDeinit() {
  if (g_espnowPeerAdded) {
    esp_now_del_peer(g_espnowActivePeer);
    g_espnowPeerAdded = false;
  }
  esp_now_deinit();
}

// Sends data over ESP-NOW to the currently configured target
// (broadcast or unicast), chunked to the 250-byte ESP-NOW payload limit.
inline void espnowSend(const uint8_t *data, size_t len) {
  if (!cfg.espnowEnabled) return;
  espnowApplyPeer();
  size_t offset = 0;
  while (offset < len) {
    size_t chunk = min((size_t)ESPNOW_MAX_PAYLOAD, len - offset);
    esp_now_send(g_espnowActivePeer, (uint8_t *)(data + offset), chunk);
    offset += chunk;
  }
}

#endif // ESPNOW_HANDLER_H
