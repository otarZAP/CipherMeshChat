#pragma once
#include <stdint.h>

void displayInit();
void displayShowBoot(const char* callsign);
void displayShowReady(const char* callsign, const char* ap_ssid);

// Called from loop() — refreshes status screen.
// clients  = number of WebSocket clients currently connected
// last_rssi = RSSI of last received LoRa packet (0 if none yet)
void displayUpdate(const char* callsign, uint8_t msg_count,
                   uint8_t clients, int16_t last_rssi);

// Flash a preview of a newly received message for a few seconds.
void displayShowNewMessage(const char* from, const char* text, int16_t rssi);
