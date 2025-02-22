#include <Ticker.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>

extern "C" {
#include "user_interface.h"
}

#define SD_CS_PIN D8
#define __delay__ 10
#define __dlay_ChannelChange__ 0.5

LiquidCrystal_I2C lcd(0x27, 20, 4);
Ticker ts, updateTicker;

volatile uint32_t channelCounts[14] = {0}; // Channels 1-14
volatile bool needUpdateGroups = false;

uint32_t highCount = 0;
uint32_t mediCount = 0;
uint32_t lowCount = 0;

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

struct LenSeq {
    uint16_t length;
    uint16_t seq;
    uint8_t  address3[6];
};

struct sniffer_buf {
    struct RxControl rx_ctrl;
    uint8_t buf[36];
    uint16_t cnt;
    struct LenSeq lenseq[1];
};

struct sniffer_buf2 {
    struct RxControl rx_ctrl;
    uint8_t buf[112];
    uint16_t cnt;
    uint16_t len;
};

void printMAC(uint8_t *buf, uint8_t i) {
    Serial.printf("\t%02X:%02X:%02X:%02X:%02X:%02X", buf[i], buf[i+1], buf[i+2], buf[i+3], buf[i+4], buf[i+5]);
}

void promisc_cb(uint8_t *buf, uint16_t len) {
    if (len == 12) return;

    struct RxControl *rx_ctrl = (struct RxControl*)buf;
    uint8_t channel = rx_ctrl->channel;

    if (channel < 1 || channel > 14) return;
    channelCounts[channel - 1]++;

    uint8_t* buffi;
    if (len == 128) {
        struct sniffer_buf2 *sniffer = (struct sniffer_buf2*)buf;
        buffi = sniffer->buf;
    } else {
        struct sniffer_buf *sniffer = (struct sniffer_buf*)buf;
        buffi = sniffer->buf;
    }

    Serial.printf("Channel %2d: Len %3d", channel, len);
    printMAC(buffi, 4);
    printMAC(buffi, 10);
    printMAC(buffi, 16);
    if (bitRead(buffi[1], 7) && bitRead(buffi[1], 6)) {
        printMAC(buffi, 24);
    }
    Serial.printf("\n");
}

void channelCh() {
    uint8_t new_channel = wifi_get_channel() % 12 + 1;
    wifi_set_channel(new_channel);
}

struct ChannelInfo {
    uint8_t channel;
    uint32_t count;
};

void updateChannelGroups() {
    ChannelInfo channels[14];
    for (int i = 0; i < 14; i++) {
        channels[i].channel = i + 1;
        channels[i].count = channelCounts[i];
    }

    // Bubble sort descending
    for (int i = 0; i < 13; i++) {
        for (int j = 0; j < 13 - i; j++) {
            if (channels[j].count < channels[j+1].count) {
                ChannelInfo temp = channels[j];
                channels[j] = channels[j+1];
                channels[j+1] = temp;
            }
        }
    }

    highCount = mediCount = lowCount = 0;
    for (int i = 0; i < 14; i++) {
        if (i < 4) highCount += channels[i].count;
        else if (i < 8) mediCount += channels[i].count;
        else lowCount += channels[i].count;
    }
}

void setUpdateFlag() {
    needUpdateGroups = true;
}

void updateLCD() {
    lcd.setCursor(0, 0);
    lcd.print("                "); // Очистка строки
    lcd.setCursor(0, 0);
    lcd.print("NS8266");

    lcd.setCursor(0, 1);
    lcd.print("                "); // Очистка строки
    lcd.setCursor(0, 1);
    lcd.print("High (1/6/11): ");
    lcd.print(highCount);

    lcd.setCursor(0, 2);
    lcd.print("                "); // Очистка строки
    lcd.setCursor(0, 2);
    lcd.print("Medi: ");
    lcd.print(mediCount);

    lcd.setCursor(0, 3);
    lcd.print("                "); // Очистка строки
    lcd.setCursor(0, 3);
    lcd.print("Low (12/13):  ");
    lcd.print(lowCount);
}

void logToSD() {
    File file = SD.open("log.txt", FILE_WRITE);
    if (file) {
        file.printf("High:%lu Medi:%lu Low:%lu\n", highCount, mediCount, lowCount);
        file.close();
    }
}

void setup() {
    Serial.begin(115200);
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("NS8266");

    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD Card Error");
    }

    wifi_set_opmode(STATION_MODE);
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(promisc_cb);
    wifi_promiscuous_enable(1);

    ts.attach(__dlay_ChannelChange__, channelCh);
    updateTicker.attach(1, setUpdateFlag);
}

void loop() {
    if (needUpdateGroups) {
        noInterrupts();
        needUpdateGroups = false;
        interrupts();

        updateChannelGroups();
        updateLCD();
        logToSD();
    }
    delay(__delay__);
}
