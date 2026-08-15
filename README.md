# CIPHER — Encrypted LoRa Mesh Chat

**Status:** Complete — generate real keys and set callsigns before deploy
**Board:** Heltec WiFi LoRa 32 V4 (ESP32-S3 + SX1262)
**Role:** Peer-to-peer encrypted text messaging over LoRa — no internet, no cell towers, no app

---

## What It Does

Each CIPHER node broadcasts its own WiFi AP. Connect a phone or PC to that AP, open a browser, and a real-time chat window loads. Messages are AES-256-EAX encrypted and sent over LoRa to other CIPHER nodes up to 1–3 km away. No app, no account, no internet.

```
[Your Phone]                        [Their Phone]
     |                                    |
  Browser                             Browser
  (WiFi)                              (WiFi)
     |                                    |
CIPHER-ALPHA  ←── LoRa ~1-3km ──→ CIPHER-BRAVO
```

| Feature | Detail |
|---|---|
| **WiFi AP** | Each node broadcasts its own AP — `CIPHER-ALPHA`, `CIPHER-BRAVO`, etc. |
| **Chat UI** | Mobile-first dark-theme bubble UI, served directly from the board's flash |
| **Real-time** | WebSocket — messages appear instantly, no page refresh |
| **Encryption** | AES-256-EAX per packet, HMAC auth, replay protection |
| **OLED** | Shows WiFi AP name, IP, connected clients, last message preview |
| **Desktop app** | Native PySide6 GUI with system tray and notifications (`gui/cipher_gui.py`) |

---

## Hardware

| Item | Notes |
|---|---|
| Heltec WiFi LoRa 32 V4 | ESP32-S3 + SX1262 + onboard OLED — one board per person |

---

## Wire Protocol

`include/lora_protocol.h` defines a compact, self-contained packet format over LoRa:

- 37-byte header (magic, version, node ID, sequence, timestamp, per-packet nonce) + up to 200 bytes of payload
- AES-256-EAX authenticated encryption — every packet is both encrypted and tamper-checked in one pass, keyed from a CSPRNG-generated shared secret plus a 4-word auth seed
- A 64-bit nonce derived from timestamp, node ID, sequence, and direction feeds the AEAD IV, so replayed or duplicated packets fail the tag check
- Runtime key override hooks (`proto_set_keys`, `proto_set_aes_key_text`) exist for loading keys from flash/NVS instead of compile-time constants, though this project uses compile-time keys

---

## Configuration

Each node needs a unique callsign and node ID. Edit `include/config.h` per board:

```cpp
#define NODE_CALLSIGN   "ALPHA"   // shown in chat (max 11 chars)
#define NODE_ID         0x20      // unique per node: 0x20, 0x21, 0x22 ...
```

Copy `include/secrets.example.h` → `include/secrets.h`. **Every node in the mesh must use the same key values.**

```cpp
#define AP_PASSWORD     ""                          // leave "" for open WiFi
#define PROTO_AES_KEY_TEXT   "your-base64-key="    // generate with: openssl rand -base64 32
#define PROTO_AUTH_KEY_A     0x...UL                // generate 4× random 32-bit values
```

---

## Flash

```bash
cd CipherMeshChat
pio run -t upload
pio device monitor   # 115200 baud
```

Flash each board with its own `config.h` (unique callsign + node ID), same `secrets.h`.

---

## Using It

**Phone or PC (browser):**
1. Connect to the WiFi AP — `CIPHER-ALPHA` etc.
2. Browser opens automatically (captive portal redirect) — or go to `http://192.168.4.1`
3. Type and send. Messages appear on the other person's screen instantly.

**PC (native desktop app):**
```bash
pip install PySide6 websocket-client
python gui/cipher_gui.py --host 192.168.4.1
```

---

## Display Views

The OLED has one status screen (no view cycling needed):

| Section | Shows |
|---|---|
| Header | `CIPHER  CALLSIGN` |
| Body | WiFi AP name, IP address (`192.168.4.1`), connected client count |
| Alert | 4-second message preview flash when a new message arrives |

---

## Adding More Nodes

Each additional node needs:
1. A unique `NODE_CALLSIGN` and `NODE_ID` in `include/config.h`
2. The same `secrets.h` crypto keys
3. `pio run -t upload`

All nodes receive all messages — it's a group mesh, not point-to-point. Up to 254 nodes (0x20–0xFE).

---

## Notes for Reviewers

This is a portfolio-scoped extraction of a larger personal LoRa-networking project; `include/lora_protocol.h` here is a trimmed, standalone copy of a shared wire format used across several sibling projects, kept self-contained for this repo. The AEAD tag check on decrypt zeroes the payload on failure rather than dropping the packet outright — functionally equivalent here since the caller checks `payload_len` before using the text, but worth flagging for anyone reusing the protocol in a context where that distinction matters.
