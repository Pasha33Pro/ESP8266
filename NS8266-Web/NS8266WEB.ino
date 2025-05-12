#include <Ticker.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

extern "C" {
#include "user_interface.h"
}

// Конфигурация пинов
#define SD_CS_PIN D8
#define WEB_SERVER_PORT 80

// Настройки обновления
#define SCAN_DELAY_MS 10
#define CHANNEL_CHANGE_INTERVAL 0.5

LiquidCrystal_I2C lcd(0x27, 20, 4);
Ticker channelTicker, updateTicker;
ESP8266WebServer webServer(WEB_SERVER_PORT);

// Структуры данных
volatile uint32_t channelCounts[14] = {0};
volatile bool dataUpdated = false;

struct TrafficGroups {
    uint32_t high;
    uint32_t medium;
    uint32_t low;
} currentTraffic;

// Прототипы функций
void handleRoot();
void handleScanData();
void handleChannelChange();
void updateTrafficGroups();
void saveToSD();

// Структуры для работы с Wi-Fi пакетами
#pragma pack(push, 1)
struct RxControl {
    signed rssi:8;
    unsigned rate:4;
    unsigned is_group:1;
    unsigned:1;
    unsigned sig_mode:2;
    unsigned legacy_length:12;
    unsigned damatch0:1;
    unsigned damatch1:1;
    unsigned bssidmatch0:1;
    unsigned bssidmatch1:1;
    unsigned MCS:7;
    unsigned CWB:1;
    unsigned HT_length:16;
    unsigned Smoothing:1;
    unsigned Not_Sounding:1;
    unsigned:1;
    unsigned Aggregation:1;
    unsigned STBC:2;
    unsigned FEC_CODING:1;
    unsigned SGI:1;
    unsigned rxend_state:8;
    unsigned ampdu_cnt:8;
    unsigned channel:4;
    unsigned:12;
};

struct SnifferPacket {
    struct RxControl rx_ctrl;
    uint8_t payload[0];
};
#pragma pack(pop)

void snifferCallback(uint8_t *buf, uint16_t len) {
    if (len < sizeof(SnifferPacket)) return;
    
    SnifferPacket *packet = (SnifferPacket*)buf;
    uint8_t channel = packet->rx_ctrl.channel;
    
    if (channel >= 1 && channel <= 14) {
        channelCounts[channel - 1]++;
    }
}

void setup() {
    Serial.begin(115200);
    
    // Инициализация LCD
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("NS8266 Starting...");
    
    // Инициализация SD-карты
    if (!SD.begin(SD_CS_PIN)) {
        lcd.setCursor(0, 3);
        lcd.print("SD Card Error!");
    }
    
    // Настройка Wi-Fi
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("NS8266-Monitor", "12345678");
    
    // Настройка веб-сервера
    webServer.on("/", handleRoot);
    webServer.on("/api/data", handleScanData);
    webServer.begin();
    
    // Настройка сниффера
    wifi_set_promiscuous_rx_cb(snifferCallback);
    wifi_promiscuous_enable(true);
    
    // Таймеры
    channelTicker.attach(CHANNEL_CHANGE_INTERVAL, handleChannelChange);
    updateTicker.attach(1, []() { dataUpdated = true; });
}

void loop() {
    webServer.handleClient();
    
    if (dataUpdated) {
        noInterrupts();
        dataUpdated = false;
        interrupts();
        
        updateTrafficGroups();
        updateDisplay();
        saveToSD();
    }
    
    delay(SCAN_DELAY_MS);
}

// Веб-обработчики
void handleRoot() {
    String html = R"(
        <html>
        <head><title>NS8266 Monitor</title>
        <meta http-equiv="refresh" content="2">
        <style>
            .traffic-bar { height: 30px; margin: 10px; }
            #high { width: %HIGH%%; background: #ff4444; }
            #medium { width: %MEDIUM%%; background: #ffa500; }
            #low { width: %LOW%%; background: #44ff44; }
        </style></head>
        <body>
            <h1>Wi-Fi Traffic Monitor</h1>
            <div class="traffic-bar" id="high"></div>
            <div class="traffic-bar" id="medium"></div>
            <div class="traffic-bar" id="low"></div>
            <p>High: %HIGH% packets</p>
            <p>Medium: %MEDIUM% packets</p>
            <p>Low: %LOW% packets</p>
        </body>
        </html>
    )";
    
    html.replace("%HIGH%", String(currentTraffic.high));
    html.replace("%MEDIUM%", String(currentTraffic.medium));
    html.replace("%LOW%", String(currentTraffic.low));
    
    webServer.send(200, "text/html", html);
}

void handleScanData() {
    String json = String("{") +
        "\"high\":" + currentTraffic.high + "," +
        "\"medium\":" + currentTraffic.medium + "," +
        "\"low\":" + currentTraffic.low +
    "}";
    
    webServer.send(200, "application/json", json);
}

// Логика обработки данных
void handleChannelChange() {
    static uint8_t currentChannel = 1;
    currentChannel = (currentChannel % 14) + 1;
    wifi_set_channel(currentChannel);
}

void updateTrafficGroups() {
    currentTraffic = {0, 0, 0};
    
    for (int i = 0; i < 14; i++) {
        if (i < 3) currentTraffic.high += channelCounts[i];
        else if (i < 10) currentTraffic.medium += channelCounts[i];
        else currentTraffic.low += channelCounts[i];
    }
}

void updateDisplay() {
    lcd.setCursor(0, 1);
    lcd.print("H:");
    lcd.print(currentTraffic.high);
    lcd.print(" M:");
    lcd.print(currentTraffic.medium);
    
    lcd.setCursor(0, 2);
    lcd.print("L:");
    lcd.print(currentTraffic.low);
    lcd.print(" Total:");
    lcd.print(currentTraffic.high + currentTraffic.medium + currentTraffic.low);
}

void saveToSD() {
    File logFile = SD.open("log.csv", FILE_WRITE);
    if (logFile) {
        logFile.printf(
            "%lu,%lu,%lu,%lu\n",
            millis(),
            currentTraffic.high,
            currentTraffic.medium,
            currentTraffic.low
        );
        logFile.close();
    }
}