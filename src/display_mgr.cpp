#include "display_mgr.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <stdio.h>

static Adafruit_SSD1306 g_oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST);
static bool              g_ready       = false;
static uint32_t          g_msg_flash   = 0;      // millis() when flash started
static char              g_flash_from[CALLSIGN_LEN];
static char              g_flash_text[33];        // truncated preview
static int16_t           g_flash_rssi  = 0;
static const uint32_t    FLASH_DUR_MS  = 4000;

static void oledHardReset() {
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(10);
    digitalWrite(OLED_RST, HIGH);
    delay(10);
}

void displayInit() {
    Wire.begin(OLED_SDA, OLED_SCL);
    oledHardReset();

    if (!g_oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[OLED] Init failed");
        return;
    }
    g_oled.clearDisplay();
    g_oled.setTextColor(SSD1306_WHITE);
    g_oled.setTextSize(1);
    g_ready = true;
}

void displayShowBoot(const char* callsign) {
    if (!g_ready) return;
    g_oled.clearDisplay();
    g_oled.setTextSize(2);
    g_oled.setCursor(2, 8);
    g_oled.print("CIPHER");
    g_oled.setTextSize(1);
    g_oled.setCursor(2, 30);
    g_oled.print(callsign);
    g_oled.setCursor(2, 44);
    g_oled.print("Starting...");
    g_oled.display();
}

void displayShowReady(const char* callsign, const char* ap_ssid) {
    if (!g_ready) return;
    g_oled.clearDisplay();

    // Header
    g_oled.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    g_oled.setTextColor(SSD1306_BLACK);
    g_oled.setTextSize(1);
    g_oled.setCursor(3, 2);
    g_oled.printf("CIPHER  %s", callsign);
    g_oled.setTextColor(SSD1306_WHITE);

    g_oled.setCursor(2, 16);
    g_oled.print("WiFi:");
    g_oled.setCursor(2, 26);
    g_oled.print(ap_ssid);
    g_oled.setCursor(2, 40);
    g_oled.print("IP: 192.168.4.1");
    g_oled.setCursor(2, 54);
    g_oled.print("Waiting for clients...");
    g_oled.display();
}

void displayUpdate(const char* callsign, uint8_t msg_count,
                   uint8_t clients, int16_t last_rssi) {
    if (!g_ready) return;

    // If a new message flash is active, show that instead
    if (g_msg_flash > 0 && (millis() - g_msg_flash) < FLASH_DUR_MS) {
        g_oled.clearDisplay();

        g_oled.fillRect(0, 0, 128, 12, SSD1306_WHITE);
        g_oled.setTextColor(SSD1306_BLACK);
        g_oled.setTextSize(1);
        g_oled.setCursor(3, 2);
        g_oled.printf("MSG  %s", g_flash_from);
        g_oled.setTextColor(SSD1306_WHITE);

        // Word-wrap the preview text across up to 3 lines
        g_oled.setCursor(2, 16);
        g_oled.print(g_flash_text);

        g_oled.setCursor(2, 54);
        g_oled.printf("RSSI: %d dBm", g_flash_rssi);
        g_oled.display();
        return;
    }
    g_msg_flash = 0;

    // Normal status screen
    g_oled.clearDisplay();

    g_oled.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    g_oled.setTextColor(SSD1306_BLACK);
    g_oled.setTextSize(1);
    g_oled.setCursor(3, 2);
    g_oled.printf("CIPHER  %s", callsign);
    g_oled.setTextColor(SSD1306_WHITE);

    g_oled.setCursor(2, 16);
    g_oled.printf("WiFi clients: %d", clients);

    g_oled.setCursor(2, 28);
    g_oled.printf("Messages: %d", msg_count);

    g_oled.setCursor(2, 40);
    if (last_rssi != 0) {
        g_oled.printf("Last RSSI: %d dBm", last_rssi);
    } else {
        g_oled.print("No LoRa contact yet");
    }

    g_oled.setCursor(2, 54);
    g_oled.print("192.168.4.1");
    g_oled.display();
}

void displayShowNewMessage(const char* from, const char* text, int16_t rssi) {
    strncpy(g_flash_from, from, CALLSIGN_LEN - 1);
    g_flash_from[CALLSIGN_LEN - 1] = '\0';
    strncpy(g_flash_text, text, 32);
    g_flash_text[32] = '\0';
    g_flash_rssi   = rssi;
    g_msg_flash    = millis();
}
