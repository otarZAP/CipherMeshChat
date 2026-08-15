#pragma once

// ─── Secrets (AP password, shared crypto keys) ────────────────────────────
// Copy include/secrets.example.h → include/secrets.h and fill in values.
// All nodes in the same mesh MUST share identical crypto key values.
#include "secrets.h"

// ─── Node identity ────────────────────────────────────────────────────────
// Each node in the mesh must have a unique CALLSIGN and NODE_ID.
#define NODE_CALLSIGN   "ALPHA"     // shown in chat UI (max 11 chars)
#define NODE_ID         0x20        // 0x20+ reserved for CIPHER nodes

// ─── WiFi Access Point ────────────────────────────────────────────────────
// AP_SSID is auto-built as "CIPHER-" + NODE_CALLSIGN in main.cpp.
// AP_PASSWORD is defined in secrets.h (leave "" for open network).
#define AP_CHANNEL      6
#define AP_MAX_CLIENTS  4           // max simultaneous phone/PC connections

// ─── LoRa RF — must be IDENTICAL on every CIPHER node in the mesh ─────────
#define LORA_NSS        8
#define LORA_DIO1       14
#define LORA_RST        12
#define LORA_BUSY       13
#define LORA_SCK        9
#define LORA_MOSI       10
#define LORA_MISO       11

#define LORA_FREQ       915.0   // MHz — US 915, EU use 868.0
#define LORA_BW         125.0   // kHz
#define LORA_SF         9       // spreading factor
#define LORA_CR         5       // coding rate (4/5)
#define LORA_SYNC       0x12    // private network sync word
#define LORA_POWER      17      // dBm TX power

// ─── OLED I2C (Heltec V4 onboard SSD1306) ────────────────────────────────
#define OLED_SDA        17
#define OLED_SCL        18
#define OLED_RST        21
#define OLED_ADDR       0x3C
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

// ─── Chat history ─────────────────────────────────────────────────────────
#define CHAT_MAX_MESSAGES   30      // circular buffer — oldest dropped first
#define CHAT_MAX_TEXT       187     // max characters per message

// ─── Timing ───────────────────────────────────────────────────────────────
#define DISPLAY_REFRESH_MS       1000
#define NODE_ONLINE_TIMEOUT_MS  120000  // node marked offline after 2 min silence
#define LORA_KEEPALIVE_MS        60000  // TX a keepalive ping if idle this long
