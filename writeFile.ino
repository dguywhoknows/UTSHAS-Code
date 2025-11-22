#include <SPI.h>
#include <SD.h>
#include <RH_ASK.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

const int GPS_RX_PIN = 4;
const int GPS_TX_PIN = 3;
const int SD_CHIP_SELECT = 10;
const int FS1000A_TX_PIN = 7;
const int RADIO_DUMMY_RX = 2;
const int SOUND_PIN = 5;
const unsigned long SOUND_SAMPLE_MS = 100UL;

TinyGPSPlus gps;
SoftwareSerial ss(GPS_TX_PIN, GPS_RX_PIN);
RH_ASK driver(2000, RADIO_DUMMY_RX, FS1000A_TX_PIN);

bool sdReady = false;

bool sampling = false;
unsigned long sampStart = 0;
unsigned long sampLastMicros = 0;
unsigned long sampHigh = 0;
unsigned long sampTotal = 0;

char logBuf[96];
char txBuf[32];

void setup() {
  pinMode(SOUND_PIN, INPUT);
  Serial.begin(9600);
  ss.begin(9600);
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV8);

  sdReady = SD.begin(SD_CHIP_SELECT);
  if (sdReady && !SD.exists("log.txt")) {
    File f = SD.open("log.txt", FILE_WRITE);
    if (f) f.close();
  }

  driver.init();

  sampStart = millis();
  sampLastMicros = micros();
  sampling = true;
}

void readGPS() { while (ss.available()) gps.encode(ss.read()); }

void sampleSound() {
  if (!sampling) return;
  if (millis() - sampStart >= SOUND_SAMPLE_MS) {
    sampling = false;
    return;
  }
  unsigned long now = micros();
  if (now - sampLastMicros >= 50UL) {
    sampLastMicros = now;
    sampTotal++;
    if (digitalRead(SOUND_PIN)) sampHigh++;
  }
}

void logAndTransmit() {
  float pct = sampTotal ? (100.0 * sampHigh / sampTotal) : 0.0;
  float db = pct * 1.2f + 35.0f;
  if (db < 0) db = 0;
  if (db > 140) db = 140;
  unsigned long t = millis();

  if (gps.location.isValid()) {
    float la = gps.location.lat();
    float ln = gps.location.lng();
    uint8_t s = gps.satellites.isValid() ? gps.satellites.value() : 0;
    snprintf(logBuf, sizeof(logBuf), "%lu,LAT=%.6f,LNG=%.6f,Sats=%u,Pct=%.1f,Db=%.1f\n",t,la,ln,s,pct,db);
    snprintf(txBuf, sizeof(txBuf), "%.4f,%.4f", la, ln);
  } else {
    snprintf(logBuf, sizeof(logBuf), "%lu,NOFIX,Pct=%.1f,Db=%.1f\n",t,pct,db);
    snprintf(txBuf, sizeof(txBuf), "NF");
  }

  if (sdReady) {
    File f = SD.open("log.txt", FILE_WRITE);
    if (f) {
      f.print(logBuf);
      f.flush();
      f.close();
      Serial.print("Logged: ");
      Serial.print(logBuf);
    } else Serial.println("SD write fail");
  }

  driver.send((uint8_t*)txBuf, strlen(txBuf));
  driver.waitPacketSent();
  Serial.print("TX: ");
  Serial.println(txBuf);
}

unsigned long nextLogTime = 0;

void loop() {
  readGPS();
  sampleSound();

  unsigned long now = millis();

  if (!sampling && now >= nextLogTime) {
    logAndTransmit();

    nextLogTime = now + 1000;

    sampStart = millis();
    sampLastMicros = micros();
    sampHigh = 0;
    sampTotal = 0;
    sampling = true;
  }
}
