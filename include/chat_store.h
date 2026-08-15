#pragma once
#include <stdint.h>
#include <stdbool.h>

#define CALLSIGN_LEN  12   // max 11 chars + null

struct CipherMessage {
    char     from[CALLSIGN_LEN];
    char     text[188];        // CHAT_MAX_TEXT + 1
    uint32_t ts_ms;            // millis() at time of receipt/send
    int16_t  rssi;             // -1 if own outgoing message
    bool     own;              // true = sent by this node
};

void                  chatStoreInit();
void                  chatStoreAdd(const char* from, const char* text,
                                   int16_t rssi, bool own);
uint8_t               chatStoreCount();
const CipherMessage*  chatStoreGet(uint8_t idx);   // 0 = oldest
void                  chatStoreClear();
