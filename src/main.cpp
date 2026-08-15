// =============================================================================
//  CIPHER — Encrypted LoRa Mesh Chat
//  Heltec WiFi LoRa 32 V4 (ESP32-S3 + SX1262)
//
//  Each node:
//    1. Broadcasts a WiFi AP named "CIPHER-<CALLSIGN>"
//    2. Serves a real-time chat web app at 192.168.4.1
//    3. Transmits and receives AES-256-EAX encrypted messages over LoRa
//
//  Setup:
//    - Copy include/secrets.example.h → include/secrets.h
//    - Set NODE_CALLSIGN and NODE_ID uniquely per device in include/config.h
//    - All nodes in the same mesh must share the same crypto keys in secrets.h
//    - Flash with: pio run -t upload
//
//  Usage:
//    - Power on the Heltec
//    - Connect your phone/PC to the WiFi AP that appears (CIPHER-ALPHA etc.)
//    - Open a browser — the chat page loads automatically (or go to 192.168.4.1)
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#include "config.h"
#include "lora_mgr.h"
#include "chat_store.h"
#include "display_mgr.h"

// Forward declarations from wifi_ap.cpp
typedef void (*OnSendRequestCb)(const char* text);
void wifiApInit(const char* callsign, OnSendRequestCb cb);
void wifiApBroadcast(const char* from, const char* text, int16_t rssi, bool own);
void wifiApTick();
uint8_t wifiApClientCount();

// ─── LoRa radio instance ─────────────────────────────────────────────────
static SX1262 radio = new Module(
    LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY
);

// ─── Display refresh state ────────────────────────────────────────────────
static uint32_t g_last_display = 0;

// ─── Callback: LoRa message received ─────────────────────────────────────
// Called from loraMgrTick() when a valid, decrypted message arrives.
static void onLoraMessage(const char* from, const char* text, int16_t rssi) {
    chatStoreAdd(from, text, rssi, false);
    wifiApBroadcast(from, text, rssi, false);
    displayShowNewMessage(from, text, rssi);
    Serial.printf("[CHAT] %s: %s  (RSSI %d)\n", from, text, rssi);
}

// ─── Callback: user typed a message in the browser ───────────────────────
// Called from wifi_ap.cpp when a WS "send" event arrives.
static void onUserSend(const char* text) {
    chatStoreAdd(NODE_CALLSIGN, text, -1, true);
    wifiApBroadcast(NODE_CALLSIGN, text, -1, true);
    loraMgrSend(NODE_CALLSIGN, text);
    Serial.printf("[CHAT] Me: %s\n", text);
}

// ─────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.printf("\n[CIPHER] Node %s  v1.0.0  booting...\n", NODE_CALLSIGN);

    chatStoreInit();

    displayInit();
    displayShowBoot(NODE_CALLSIGN);

    // LoRa — SPI is started inside loraMgrInit
    loraMgrInit(&radio);
    loraMgrSetCallback(onLoraMessage);

    // WiFi AP + web server + WebSocket
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "CIPHER-%s", NODE_CALLSIGN);
    wifiApInit(NODE_CALLSIGN, onUserSend);

    displayShowReady(NODE_CALLSIGN, ap_ssid);

    Serial.printf("[CIPHER] Ready.  Connect to '%s'  →  http://192.168.4.1\n",
                  ap_ssid);
}

// ─────────────────────────────────────────────────────────────────────────
void loop() {
    loraMgrTick(NODE_CALLSIGN);
    wifiApTick();

    if (millis() - g_last_display >= DISPLAY_REFRESH_MS) {
        g_last_display = millis();
        displayUpdate(NODE_CALLSIGN,
                      chatStoreCount(),
                      wifiApClientCount(),
                      loraMgrLastRssi());
    }
}
