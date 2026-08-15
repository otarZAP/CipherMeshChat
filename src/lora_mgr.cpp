#include "lora_mgr.h"
#include "config.h"
#include "chat_store.h"
#include "lora_protocol.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// ─── Payload layout ───────────────────────────────────────────────────────
// Bytes 0..11 : sender callsign, null-padded to exactly 12 bytes
// Bytes 12..N : message text, null-terminated
// Special case: payload_len == 12 with text empty = keepalive ping
#define CALLSIGN_FIELD  12

static SX1262*    g_radio       = nullptr;
static OnMessageCb g_cb         = nullptr;
static bool        g_ready      = false;
static volatile bool g_rx_flag  = false;
static int16_t     g_last_rssi  = 0;
static uint32_t    g_last_tx_ms = 0;
static uint8_t     g_seq        = 0;

// ISR — must be IRAM_ATTR on ESP32
static void IRAM_ATTR onReceive() {
    g_rx_flag = true;
}

void loraMgrInit(SX1262* radio) {
    g_radio = radio;

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    int state = radio->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                             LORA_SYNC, LORA_POWER, 8, 1.6, false);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] Init failed: %d\n", state);
        return;
    }

    radio->setDio1Action(onReceive);
    radio->startReceive();
    g_ready = true;
    Serial.println("[LoRa] Ready");
}

void loraMgrSetCallback(OnMessageCb cb) {
    g_cb = cb;
}

bool loraMgrReady() { return g_ready; }

int16_t loraMgrLastRssi() { return g_last_rssi; }

// ─── Build and transmit a LoRa packet ─────────────────────────────────────
static bool txPacket(const char* callsign, const char* text) {
    if (!g_ready || !g_radio) return false;

    LoraPacket pkt;
    proto_init(&pkt, NODE_ID, FW_CIPHER, DIR_BROADCAST, SEV_INFO);
    pkt.seq       = g_seq++;
    pkt.timestamp = (uint32_t)millis();

    // Build payload: callsign field (12 bytes) + text
    uint8_t cs_len   = (uint8_t)strnlen(callsign, CALLSIGN_FIELD - 1);
    uint8_t txt_len  = (uint8_t)strnlen(text, CHAT_MAX_TEXT);
    uint8_t pld_len  = CALLSIGN_FIELD + txt_len + 1;  // +1 for null terminator

    if (pld_len > PROTO_MAX_PAYLOAD) pld_len = PROTO_MAX_PAYLOAD;

    memset(pkt.payload, 0, CALLSIGN_FIELD);
    memcpy(pkt.payload, callsign, cs_len);
    if (txt_len > 0) {
        memcpy(pkt.payload + CALLSIGN_FIELD, text, txt_len);
    }
    pkt.payload[CALLSIGN_FIELD + txt_len] = '\0';
    pkt.payload_len = pld_len;

    proto_encrypt_payload(&pkt);
    proto_sign(&pkt);

    g_radio->clearDio1Action();
    size_t pkt_bytes = PROTO_OVERHEAD + pkt.payload_len;
    int state = g_radio->transmit((uint8_t*)&pkt, pkt_bytes);
    g_radio->setDio1Action(onReceive);
    g_radio->startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        g_last_tx_ms = millis();
        return true;
    }
    Serial.printf("[LoRa] TX error: %d\n", state);
    return false;
}

bool loraMgrSend(const char* from, const char* text) {
    return txPacket(from, text);
}

void loraMgrSendPing(const char* callsign) {
    txPacket(callsign, "");   // empty text = keepalive
}

// ─── Process a received raw packet ────────────────────────────────────────
static void processRx() {
    uint8_t raw[sizeof(LoraPacket)];
    size_t  len = sizeof(raw);

    int state = g_radio->readData(raw, len);
    g_last_rssi = g_radio->getRSSI();
    g_radio->startReceive();

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] RX error: %d\n", state);
        return;
    }

    if (len < PROTO_OVERHEAD) return;

    LoraPacket* pkt = (LoraPacket*)raw;

    if (!proto_valid(pkt))          { Serial.println("[LoRa] Invalid magic"); return; }
    if (pkt->fw_type != FW_CIPHER)  { return; }   // ignore non-CIPHER traffic
    if (pkt->node_id == NODE_ID)    { return; }   // ignore own echoes

    if (!proto_decrypt_payload(pkt)) {
        Serial.println("[LoRa] Decrypt failed — wrong key?");
        return;
    }
    if (!proto_authenticated(pkt)) {
        Serial.println("[LoRa] Auth failed");
        return;
    }

    if (pkt->payload_len < CALLSIGN_FIELD) return;

    // Extract callsign and message
    char from[CALLSIGN_FIELD];
    memcpy(from, pkt->payload, CALLSIGN_FIELD - 1);
    from[CALLSIGN_FIELD - 1] = '\0';

    char* text = (char*)(pkt->payload + CALLSIGN_FIELD);
    // Ensure null termination within payload bounds
    uint8_t text_max = pkt->payload_len - CALLSIGN_FIELD;
    if (text_max > 0) text[text_max - 1] = '\0';
    else text = (char*)"";

    Serial.printf("[LoRa] RX from %s RSSI=%d: %s\n", from, g_last_rssi, text);

    if (g_cb && strlen(text) > 0) {
        g_cb(from, text, g_last_rssi);
    }
}

void loraMgrTick(const char* callsign) {
    if (!g_ready) return;

    if (g_rx_flag) {
        g_rx_flag = false;
        processRx();
    }

    // Send keepalive ping if we haven't transmitted for a while
    if (millis() - g_last_tx_ms > LORA_KEEPALIVE_MS) {
        loraMgrSendPing(callsign);
    }
}
