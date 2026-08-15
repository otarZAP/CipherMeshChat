#include "chat_store.h"
#include "config.h"
#include <string.h>

static CipherMessage g_store[CHAT_MAX_MESSAGES];
static uint8_t       g_count = 0;
static uint8_t       g_head  = 0;   // index of oldest entry when full

void chatStoreInit() {
    memset(g_store, 0, sizeof(g_store));
    g_count = 0;
    g_head  = 0;
}

void chatStoreAdd(const char* from, const char* text, int16_t rssi, bool own) {
    uint8_t idx;

    if (g_count < CHAT_MAX_MESSAGES) {
        idx = g_count++;
    } else {
        // Buffer full — overwrite oldest entry
        idx    = g_head;
        g_head = (g_head + 1) % CHAT_MAX_MESSAGES;
    }

    CipherMessage* m = &g_store[idx];
    strncpy(m->from, from, CALLSIGN_LEN - 1);
    m->from[CALLSIGN_LEN - 1] = '\0';

    strncpy(m->text, text, sizeof(m->text) - 1);
    m->text[sizeof(m->text) - 1] = '\0';

    m->ts_ms = millis();
    m->rssi  = rssi;
    m->own   = own;
}

uint8_t chatStoreCount() {
    return g_count;
}

// Returns messages in chronological order (oldest first).
const CipherMessage* chatStoreGet(uint8_t idx) {
    if (idx >= g_count) return nullptr;

    uint8_t real_idx;
    if (g_count < CHAT_MAX_MESSAGES) {
        real_idx = idx;
    } else {
        real_idx = (g_head + idx) % CHAT_MAX_MESSAGES;
    }
    return &g_store[real_idx];
}

void chatStoreClear() {
    memset(g_store, 0, sizeof(g_store));
    g_count = 0;
    g_head  = 0;
}
