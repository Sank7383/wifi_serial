#ifndef UTIL_H
#define UTIL_H

#include <Arduino.h>
#include <IPAddress.h>

// ---- Validation -------------------------------------------------------
inline bool isValidMac(const String &s) {
  if (s.length() != 17) return false;
  for (uint8_t i = 0; i < 17; i++) {
    char c = s[i];
    if (i % 3 == 2) {
      if (c != ':') return false;
    } else if (!isHexadecimalDigit(c)) {
      return false;
    }
  }
  return true;
}

inline bool macStringToBytes(const String &s, uint8_t out[6]) {
  if (!isValidMac(s)) return false;
  for (uint8_t i = 0; i < 6; i++) {
    out[i] = strtoul(s.substring(i * 3, i * 3 + 2).c_str(), nullptr, 16);
  }
  return true;
}

inline String macBytesToString(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

inline bool isValidIpOrEmpty(const String &s) {
  if (s.length() == 0) return true;
  IPAddress ip;
  return ip.fromString(s);
}

inline bool isValidBaud(uint32_t b) {
  switch (b) {
    case 300: case 1200: case 2400: case 4800: case 9600:
    case 19200: case 38400: case 57600: case 74880:
    case 115200: case 230400: case 460800: case 921600:
    case 1000000: case 1500000: case 2000000: case 3000000:
      return true;
    default:
      return false;
  }
}

// GPIO0/EN control lines for the ESP Programmer feature are limited to pins
// that are free on a typical ESP8266 module: not the UART pins (1, 3 — used
// by hardware Serial for the target link) and not the flash-SPI pins (6-11).
inline bool isValidPgmPin(uint8_t p) {
  switch (p) {
    case 0: case 2: case 4: case 5:
    case 12: case 13: case 14: case 15: case 16:
      return true;
    default:
      return false;
  }
}

inline bool isValidPgmPort(uint16_t p) {
  return p != 0 && p != 80 && p != 81; // avoid clashing with the HTTP/WS servers
}

// ---- Base64 (small, dependency-free) -----------------------------------
inline size_t base64EncodedLen(size_t inLen) { return ((inLen + 2) / 3) * 4; }

inline size_t base64Encode(const uint8_t *data, size_t len, char *out, size_t outCap) {
  static const char tbl[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t need = base64EncodedLen(len);
  if (outCap < need + 1) return 0;

  size_t oi = 0;
  size_t i = 0;
  while (i + 3 <= len) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
    out[oi++] = tbl[(n >> 18) & 0x3F];
    out[oi++] = tbl[(n >> 12) & 0x3F];
    out[oi++] = tbl[(n >> 6) & 0x3F];
    out[oi++] = tbl[n & 0x3F];
    i += 3;
  }
  size_t rem = len - i;
  if (rem == 1) {
    uint32_t n = ((uint32_t)data[i] << 16);
    out[oi++] = tbl[(n >> 18) & 0x3F];
    out[oi++] = tbl[(n >> 12) & 0x3F];
    out[oi++] = '=';
    out[oi++] = '=';
  } else if (rem == 2) {
    uint32_t n = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8);
    out[oi++] = tbl[(n >> 18) & 0x3F];
    out[oi++] = tbl[(n >> 12) & 0x3F];
    out[oi++] = tbl[(n >> 6) & 0x3F];
    out[oi++] = '=';
  }
  out[oi] = '\0';
  return oi;
}

inline int8_t b64Val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// Decodes in place-safe manner into out buffer; returns decoded length or -1 on error.
inline int base64Decode(const char *in, size_t inLen, uint8_t *out, size_t outCap) {
  if (inLen % 4 != 0) return -1;
  size_t oi = 0;
  for (size_t i = 0; i < inLen; i += 4) {
    int8_t a = b64Val(in[i]);
    int8_t b = b64Val(in[i + 1]);
    bool pad2 = in[i + 2] == '=';
    bool pad3 = in[i + 3] == '=';
    int8_t c = pad2 ? 0 : b64Val(in[i + 2]);
    int8_t d = pad3 ? 0 : b64Val(in[i + 3]);
    if (a < 0 || b < 0 || (!pad2 && c < 0) || (!pad3 && d < 0)) return -1;

    if (oi >= outCap) return -1;
    out[oi++] = (a << 2) | (b >> 4);
    if (!pad2) {
      if (oi >= outCap) return -1;
      out[oi++] = (b << 4) | (c >> 2);
    }
    if (!pad2 && !pad3) {
      if (oi >= outCap) return -1;
      out[oi++] = (c << 6) | d;
    }
  }
  return (int)oi;
}

#endif // UTIL_H
