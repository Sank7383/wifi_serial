#ifndef PROGRAMMER_HANDLER_H
#define PROGRAMMER_HANDLER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "util.h"
#include "websocket_handler.h"

// Implemented in the main sketch.
void applyBaudRateLive(uint32_t baud);

// ---------------------------------------------------------------------------
// ESP Programmer: lets this unit act as a dedicated WiFi<->UART bridge for
// flashing a THIRD ESP with esptool.py, instead of the normal serial
// monitor / ESP-NOW bridging.
//
// Wiring:
//   This unit's UART TX/RX -> target's RX/TX (crossed), plus a shared GND.
//   This unit's pgmGpio0Pin -> target's GPIO0/BOOT strap pin.
//   This unit's pgmResetPin -> target's EN/RST pin.
//
// On the PC, point esptool at this unit instead of a local COM port:
//   esptool.py --port socket://<this-device-ip>:<pgmTcpPort> --baud <pgmBaudRate> ...
//
// On TCP connect, the target is auto-pulsed into bootloader mode; on
// disconnect it's pulsed back into normal run mode. Only one flashing
// session is accepted at a time.
//
// Limitation: the baud rate is fixed for the whole session (no runtime
// baud-switch support) — pass a matching --baud and avoid esptool's
// stub-based speed upgrade for reliability. See README.
// ---------------------------------------------------------------------------

static WiFiServer *g_pgmServer = nullptr;
static WiFiClient g_pgmClient;
static bool g_pgmClientWasConnected = false;

inline void pgmWritePin(uint8_t pin, bool assertLine, bool activeLow) {
  digitalWrite(pin, activeLow ? (assertLine ? LOW : HIGH) : (assertLine ? HIGH : LOW));
}

inline void pgmSetGpio0(bool selectBootloader) {
  pgmWritePin(cfg.pgmGpio0Pin, selectBootloader, cfg.pgmGpio0ActiveLow);
}

inline void pgmSetReset(bool held) {
  pgmWritePin(cfg.pgmResetPin, held, cfg.pgmResetActiveLow);
}

// Standard esptool-style auto-reset sequence: select the bootloader strap,
// pulse EN/RST, then release GPIO0 once the target has restarted.
inline void pgmEnterBootloader() {
  pgmSetGpio0(true);
  pgmSetReset(true);
  delay(50);
  pgmSetReset(false);
  delay(100); // let the ROM bootloader sample GPIO0 at boot
  pgmSetGpio0(false);
}

inline void pgmEnterRunMode() {
  pgmSetGpio0(false);
  pgmSetReset(true);
  delay(50);
  pgmSetReset(false);
}

inline bool pgmClientConnected() {
  return g_pgmClient && g_pgmClient.connected();
}

inline void pgmStopServer() {
  if (g_pgmClient) g_pgmClient.stop();
  g_pgmClientWasConnected = false;
  if (g_pgmServer) {
    g_pgmServer->close();
    delete g_pgmServer;
    g_pgmServer = nullptr;
  }
}

inline void pgmStartServer() {
  pgmStopServer();
  g_pgmServer = new WiFiServer(cfg.pgmTcpPort);
  g_pgmServer->begin();
  g_pgmServer->setNoDelay(true);
}

// Re-applies pins/baud/server — call on boot and whenever programmer
// settings are saved.
inline void pgmApplySettings() {
  pgmStopServer();
  pinMode(cfg.pgmGpio0Pin, OUTPUT);
  pinMode(cfg.pgmResetPin, OUTPUT);
  pgmSetGpio0(false);
  pgmSetReset(false);
  if (cfg.programmerEnabled) {
    applyBaudRateLive(cfg.pgmBaudRate);
    pgmStartServer();
    wsSendLog("info", "[pgm] Programmer mode enabled, listening on TCP " + String(cfg.pgmTcpPort));
  } else {
    applyBaudRateLive(cfg.baudRate);
  }
}

inline void pgmInit() {
  pinMode(cfg.pgmGpio0Pin, OUTPUT);
  pinMode(cfg.pgmResetPin, OUTPUT);
  pgmSetGpio0(false);
  pgmSetReset(false);
  if (cfg.programmerEnabled) {
    applyBaudRateLive(cfg.pgmBaudRate); // this unit booted directly into Programmer mode
    pgmStartServer();
  }
}

// Called every loop() iteration INSTEAD OF the normal serial<->WS/ESP-NOW
// bridging while cfg.programmerEnabled is true, so the UART is exclusively
// reserved for the esptool <-> target byte stream.
inline void pgmLoop() {
  if (!cfg.programmerEnabled || !g_pgmServer) return;

  if (g_pgmServer->hasClient()) {
    WiFiClient incoming = g_pgmServer->available();
    if (pgmClientConnected()) {
      incoming.stop(); // one flashing session at a time
    } else {
      g_pgmClient = incoming;
      g_pgmClient.setNoDelay(true);
      g_pgmClientWasConnected = true;
      wsSendLog("info", "[pgm] esptool connected, entering bootloader mode");
      pgmEnterBootloader();
    }
  }

  if (pgmClientConnected()) {
    uint8_t buf[256];
    int n;
    while ((n = g_pgmClient.available()) > 0) {
      int chunk = g_pgmClient.read(buf, min(n, (int)sizeof(buf)));
      if (chunk > 0) Serial.write(buf, chunk);
    }
    while ((n = Serial.available()) > 0) {
      int chunk = Serial.readBytes(buf, min(n, (int)sizeof(buf)));
      if (chunk > 0) g_pgmClient.write(buf, chunk);
    }
  } else if (g_pgmClientWasConnected) {
    g_pgmClientWasConnected = false;
    g_pgmClient.stop();
    wsSendLog("info", "[pgm] esptool disconnected, returning target to run mode");
    pgmEnterRunMode();
  }
}

#endif // PROGRAMMER_HANDLER_H
