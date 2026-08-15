#pragma once
#include <RadioLib.h>

// Callback fired whenever a valid CIPHER message arrives over LoRa.
// from  — sender callsign (null-terminated, max 11 chars)
// text  — message text   (null-terminated, max CHAT_MAX_TEXT chars)
// rssi  — received signal strength in dBm
typedef void (*OnMessageCb)(const char* from, const char* text, int16_t rssi);

void    loraMgrInit(SX1262* radio);
void    loraMgrSetCallback(OnMessageCb cb);

// Send a message from 'from' over LoRa. Returns true on success.
bool    loraMgrSend(const char* from, const char* text);

// Send a keepalive ping with just our callsign (no message body).
void    loraMgrSendPing(const char* callsign);

// Must be called from loop() — handles RX flag and keepalive timer.
void    loraMgrTick(const char* callsign);

int16_t loraMgrLastRssi();   // RSSI of the most recently received packet
bool    loraMgrReady();      // true once radio initialised successfully
