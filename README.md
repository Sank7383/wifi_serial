# ESP8266 WiFi-Serial Bridge

Turns an ESP8266 into a browser-based serial monitor: UART data is streamed
over a WebSocket, with an optional ESP-NOW fan-out (broadcast or unicast) of
the same data. Full network (STA/AP) and bridging configuration is done from
the same web dashboard — no re-flash needed to change settings.

## Module flow

```
UART (Serial) <----> [ ESP8266 bridge core ] <----> WebSocket (port 81) <---> Browser dashboard
                              |
                              v
                          ESP-NOW  <----> other ESP8266/ESP32 peer(s)
```

- Serial RX bytes are buffered (flushed on a short idle gap or when the
  chunk fills) and pushed to: connected WebSocket clients, and/or ESP-NOW,
  per the toggles in the ESP-NOW tab.
- Data typed into the browser's Monitor tab is sent to Serial TX and,
  optionally, ESP-NOW — i.e. the web UI behaves like a normal serial
  terminal, with ESP-NOW as an extra fan-out.
- Incoming ESP-NOW packets are written to Serial TX and/or pushed to the
  WebSocket clients, per the same toggles.

## API structure

All HTTP endpoints require HTTP Basic Auth (`admin` credentials, see
**First boot** below).

| Method | Path            | Purpose                                    |
|--------|-----------------|---------------------------------------------|
| GET    | `/`             | Web dashboard (HTML/CSS/JS, single page)   |
| GET    | `/api/config`   | Current configuration (secrets masked)     |
| POST   | `/api/config`   | Update configuration (validated, JSON)     |
| GET    | `/api/status`   | Live status: IP, RSSI, heap, uptime        |
| GET    | `/api/scan`     | Scan nearby Wi-Fi networks (sync, ~2-4s)   |
| GET    | `/api/wstoken`  | Issue a single-use token for the WS upgrade|
| POST   | `/api/reboot`   | Reboot the device                          |
| WS     | `:81/?token=…`  | Bidirectional serial/ESP-NOW data stream   |

WebSocket messages are JSON text frames, payloads base64-encoded so binary
serial data survives intact:

```jsonc
// browser -> device
{ "type": "serial", "data": "<base64 bytes>" }

// device -> browser
{ "type": "serial", "data": "<base64 bytes>" }
{ "type": "espnow", "mac": "AA:BB:CC:DD:EE:FF", "data": "<base64 bytes>" }
{ "type": "log", "level": "info|error", "msg": "..." }
```

The WebSocket itself isn't Basic-Auth'd (browsers won't attach cached HTTP
credentials to a `ws://` upgrade reliably), so the dashboard first fetches a
short-lived, single-use token from the authenticated `/api/wstoken` endpoint
and passes it as `?token=` on the WS connection; the server validates and
consumes it during the WS handshake.

## Development approach / file layout

- `wifi_serial.ino` — setup/loop, Wi-Fi/mDNS bring-up, UART bridging loop.
- `config.h` — persisted settings (LittleFS `config.json`), defaults.
- `util.h` — MAC/IP/baud validation, dependency-free base64 codec.
- `espnow_handler.h` — ESP-NOW init, peer management, chunked send.
- `websocket_handler.h` — WS server, token auth, JSON framing.
- `webserver_handlers.h` — HTTP routes, input validation, config persistence.
- `webpage.h` — the dashboard UI (PROGMEM, no external CDN dependencies).

## Required libraries (Arduino IDE Library Manager)

- **ArduinoJson** >= 7.0 (bblanchon)
- **WebSockets** >= 2.4 (Markus Sattler / Links2004)
- ESP8266 board package >= 3.0 (includes `LittleFS`, `espnow.h`, `ESP8266mDNS`)

Board settings: any ESP8266 module (NodeMCU / Wemos D1 mini / generic).
Under **Tools > Flash Size**, pick an option that reserves space for a
filesystem (e.g. "4MB (FS:1MB)") since configuration is stored in LittleFS.

## First boot

Each device generates **unique** default credentials from its chip ID the
first time it boots with no saved config — nothing is hardcoded/shared
across units. Open the Arduino Serial Monitor at 115200 baud once to read:

```
AP SSID       : ESP-WiFiSerial-XXXX
AP Password   : <random>
Admin user    : admin
Admin password: <random>
```

1. Connect to the `ESP-WiFiSerial-XXXX` access point with the printed
   password.
2. Browse to `http://192.168.4.1/` (or `http://espwifiserial.local/` if
   your OS supports mDNS) and sign in with the printed admin credentials.
3. In the **Admin** tab, set a memorable admin password. In the **Wi-Fi**
   tab, switch to Station mode and join your normal network if desired.

If a configured Station connection fails at boot, the device automatically
falls back to AP mode so it's never left unreachable.

## ESP-NOW notes

- Broadcast mode reaches every ESP-NOW-listening device in range on the
  same Wi-Fi channel; Unicast mode targets one peer MAC address you enter
  in the ESP-NOW tab.
- ESP-NOW packets are capped at 250 bytes by the protocol; longer bursts
  are chunked automatically.
- **Channel matching matters**: ESP-NOW peers must share a Wi-Fi channel.
  When this device is in Station mode, it automatically uses the router's
  channel. When it's in AP mode (or the peer is AP-only), set the
  **ESP-NOW channel** field in the dashboard to match the peer's channel.

## Security notes

- Every unit ships with unique, randomly generated AP and admin
  credentials — change the admin password from the Admin tab after first
  login.
- All config-changing endpoints require authentication and validate input
  (SSID/password lengths, MAC/IP formats, baud rate whitelist) server-side.
- Consider this dashboard suitable for trusted LAN/local use; it serves
  plain HTTP/WS (no TLS — ESP8266 resources make TLS on both HTTP and WS
  simultaneously impractical). Don't expose it directly to the public
  internet/port-forward without a reverse proxy that terminates TLS.

## Possible future extensions

- OTA firmware updates (`ArduinoOTA`), guarded by the same admin auth.
- Multiple simultaneous ESP-NOW peers (currently one active target at a
  time, selected by broadcast/unicast mode).
- Encrypted ESP-NOW pairing (PMK/LMK) for sensitive links.
