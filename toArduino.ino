#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <RH_ASK.h>

RH_ASK driver;

String data = "";

const int chipSelect = 10;
const unsigned long interval = 10000;
unsigned long previousTime = 0;

static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;
static const int chipSelect = 10;

static const int soundPin = 2;
unsigned long sampleTime = 100;

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);
bool sdInitialized = false;

void setup(){
  Serial.begin(9600);
  while (!Serial) {}

  if (!SD.begin(chipSelect)) return;

  if (!SD.exists("gps_log.txt")) {
    File dataFile = SD.open("gps_log.txt", FILE_WRITE);
    if (dataFile) {
      dataFile.close();
    }
  }
  pinMode(soundPin, INPUT);
  ss.begin(GPSBaud);
  if (!SD.begin(chipSelect)) Serial.println("Error: SD card initialization failed");
  else {
    Serial.println("SD card initialized");
    sdInitialized = true;
  }

  if (!driver.init()) Serial.println("init failed");
}

void loop(){
  if (!sdInitialized){
    delay(1000);
    return;
  }
  unsigned long currentTime = millis();
  if (currentTime - previousTime >= interval) {
    previousTime = currentTime;
    readSensors();
    transmit();
    logToSD();
    sleepForInterval(8);
  }
}


void readSensors() {
  while (ss.available() > 0){
    char c = ss.read();
    gps.encode(c);
    if (gps.location.isUpdated()){
      if (!gps.location.isValid()) Serial.println("Error: GPS location invalid or no fix");
      data += "Latitude= " + String(gps.location.lat(), 6) + " Longitude= " + String(gps.location.lng(), 6) + "\n";
      data += "Raw latitude = " + String(gps.location.rawLat().negative ? "-" : "+") + String(gps.location.rawLat().deg) + "\n";
      data += String(gps.location.rawLat().billionths) + "\n";
      data += "Raw longitude = " + String(gps.location.rawLng().negative ? "-" : "+") + String(gps.location.rawLng().deg) + "\n";
      data += String(gps.location.rawLng().billionths) + "\n";
      data += "Raw date DDMMYY = " + String(gps.date.value()) + "\n";
      data += "Year = " + String(gps.date.year()) + "\n";
      data += "Month = " + String(gps.date.month()) + "\n";
      data += "Day = " + String(gps.date.day()) + "\n";
      data += "Raw time in HHMMSSCC = " + String(gps.time.value()) + "\n";
      data += "Hour = " + String(gps.time.hour()) + "\n";
      data += "Minute = " + String(gps.time.minute()) + "\n";
      data += "Second = " + String(gps.time.second()) + "\n";
      data += "Centisecond = " + String(gps.time.centisecond()) + "\n";
      data += "Raw speed in 100ths/knot = " + String(gps.speed.value()) + "\n";
      data += "Speed in knots/h = " + String(gps.speed.knots()) + "\n";
      data += "Speed in miles/h = " + String(gps.speed.mph()) + "\n";
      data += "Speed in m/s = " + String(gps.speed.mps()) + "\n";
      data += "Speed in km/h = " + String(gps.speed.kmph()) + "\n";
      data += "Raw course in degrees = " + String(gps.course.value()) + "\n";
      data += "Course in degrees = " + String(gps.course.deg()) + "\n";
      data += "Raw altitude in centimeters = " + String(gps.altitude.value()) + "\n";
      data += "Altitude in meters = " + String(gps.altitude.meters()) + "\n";
      data += "Altitude in miles = " + String(gps.altitude.miles()) + "\n";
      data += "Altitude in kilometers = " + String(gps.altitude.kilometers()) + "\n";
      data += "Altitude in feet = " + String(gps.altitude.feet()) + "\n";
      data += "Number of satellites in use = " + String(gps.satellites.value()) + "\n";
      data += "HDOP = " + String(gps.hdop.value()) + "\n";
      
      unsigned long startTime = millis();
      unsigned long highTime = 0;

      while (millis() - startTime < sampleTime) {
        if (digitalRead(soundPin) == HIGH) {
          highTime++;
        }
      }

      float percentHigh = (highTime / float(sampleTime)) * 100.0;
      float dB = percentHigh * 1.2 + 35;

      if(dB < 0) dB = 0;
      if(dB > 120) dB = 120;

      data += "Sound in decibels = " + String(db) + "\n\n";

      Serial.print(data);

      File dataFile = SD.open("gps_log.txt", FILE_WRITE);
      if (dataFile){
        dataFile.print(data);
        dataFile.close();
        Serial.println("Data successfully written to SD");
      } else Serial.println("Error: Failed to open gps_log.txt for writing");

      if (gps.satellites.value() == 0) Serial.println("Warning: No satellites in use");
    }
  }
}

void transmit() {
  const char *msg = "Hello World!";
  driver.send((uint8_t *)msg, strlen(msg));
  driver.waitPacketSent();
}

void logToSD() {
  File dataFile = SD.open("datalog.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println("sensor data here");
    dataFile.close();
  }
}

void sleepForInterval(int seconds) {
  WDTCSR = (1 << WDCE) | (1 << WDE);
  WDTCSR = (1 << WDP3) | (1 << WDP0);
  WDTCSR |= (1 << WDIE);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();
  sleep_disable();
}

ISR(WDT_vect) {
}