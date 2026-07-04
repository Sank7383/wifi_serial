#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "util.h"

// Serial data is bridged in modest chunks so a single JSON/WS frame never
// grows unbounded and so espnowSend() can chunk cleanly under 250 bytes.
#define BRIDGE_CHUNK_MAX   200

#define MAX_WS_CLIENTS     4
#define TOKEN_TTL_MS        60000UL   // a WS auth token is single-use, valid for 60s

WebSocketsServer webSocket(81);

// Implemented in the main sketch.
void handleBridgeDataFromWs(const uint8_t *data, size_t len);

static bool g_wsAuthed[MAX_WS_CLIENTS] = {false};

struct WsToken {
  char value[33] = "";
  unsigned long createdAt = 0;
  bool valid = false;
};
static WsToken g_wsTokens[MAX_WS_CLIENTS];

inline String wsGenerateToken() {
  static const char hex[] = "0123456789abcdef";
  char buf[33];
  for (uint8_t i = 0; i < 32; i++) buf[i] = hex[secureRandom(0, 16)];
  buf[32] = '\0';

  // reuse the oldest slot
  uint8_t slot = 0;
  unsigned long oldest = 0xFFFFFFFF;
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    if (!g_wsTokens[i].valid) { slot = i; break; }
    if (g_wsTokens[i].createdAt < oldest) { oldest = g_wsTokens[i].createdAt; slot = i; }
  }
  strlcpy(g_wsTokens[slot].value, buf, sizeof(g_wsTokens[slot].value));
  g_wsTokens[slot].createdAt = millis();
  g_wsTokens[slot].valid = true;
  return String(buf);
}

inline bool wsConsumeToken(const String &token) {
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    if (g_wsTokens[i].valid && token.equals(g_wsTokens[i].value)) {
      if (millis() - g_wsTokens[i].createdAt > TOKEN_TTL_MS) {
        g_wsTokens[i].valid = false;
        return false;
      }
      g_wsTokens[i].valid = false; // single-use
      return true;
    }
  }
  return false;
}

inline void wsBroadcastJson(JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    if (g_wsAuthed[i]) webSocket.sendTXT(i, out);
  }
}

inline void wsSendSerialFrame(const uint8_t *data, size_t len) {
  char b64[BRIDGE_CHUNK_MAX * 4 / 3 + 8];
  if (base64Encode(data, len, b64, sizeof(b64)) == 0) return;
  JsonDocument doc;
  doc["type"] = "serial";
  doc["data"] = b64;
  wsBroadcastJson(doc);
}

inline void wsSendEspNowFrame(const uint8_t mac[6], const uint8_t *data, size_t len) {
  // Sized for the full ESP-NOW payload limit (255 bytes), not just our own
  // BRIDGE_CHUNK_MAX — a third-party sender's packet may be larger than
  // what we ourselves chunk outgoing data to.
  char b64[256 * 4 / 3 + 8];
  if (base64Encode(data, len, b64, sizeof(b64)) == 0) return;
  JsonDocument doc;
  doc["type"] = "espnow";
  doc["mac"] = macBytesToString(mac);
  doc["data"] = b64;
  wsBroadcastJson(doc);
}

inline void wsSendLog(const char *level, const String &msg) {
  JsonDocument doc;
  doc["type"] = "log";
  doc["level"] = level;
  doc["msg"] = msg;
  wsBroadcastJson(doc);
}

inline void wsSendStatus() {
  JsonDocument doc;
  doc["type"] = "status";
  doc["espnowEnabled"] = cfg.espnowEnabled;
  doc["espnowMode"] = cfg.espnowMode == ESPNOW_MODE_BROADCAST ? "broadcast" : "unicast";
  doc["baudRate"] = cfg.baudRate;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptimeMs"] = millis();
  wsBroadcastJson(doc);
}

inline void wsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      if (num < MAX_WS_CLIENTS) g_wsAuthed[num] = false;
      break;

    case WStype_CONNECTED: {
      // payload is the requested resource, e.g. "/?token=<32hex>"
      String url = String((char *)payload);
      int idx = url.indexOf("token=");
      String token = idx >= 0 ? url.substring(idx + 6) : "";
      int amp = token.indexOf('&');
      if (amp >= 0) token = token.substring(0, amp);

      if (num < MAX_WS_CLIENTS && token.length() == 32 && wsConsumeToken(token)) {
        g_wsAuthed[num] = true;
      } else {
        webSocket.disconnect(num);
      }
      break;
    }

    case WStype_TEXT: {
      if (num >= MAX_WS_CLIENTS || !g_wsAuthed[num]) return;
      JsonDocument doc;
      if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

      const char *msgType = doc["type"] | "";
      if (strcmp(msgType, "serial") == 0) {
        const char *b64 = doc["data"] | "";
        size_t b64Len = strlen(b64);
        uint8_t raw[BRIDGE_CHUNK_MAX];
        int n = base64Decode(b64, b64Len, raw, sizeof(raw));
        if (n > 0) handleBridgeDataFromWs(raw, (size_t)n);
      }
      break;
    }

    default:
      break;
  }
}

inline void wsInit() {
  webSocket.begin();
  webSocket.onEvent(wsEvent);
}

#endif // WEBSOCKET_HANDLER_H
