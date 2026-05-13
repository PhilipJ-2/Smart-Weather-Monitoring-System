#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <RTClib.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include "Arduino_GigaDisplay_GFX.h"

// ---------- DISPLAY ----------
GigaDisplay_GFX display;

// ---------- SENSOR PINS ----------
#define DHTPIN 2
#define DHTTYPE DHT11

#define TRIG_PIN 3
#define ECHO_PIN 4

#define ACTIVE_BUZZER 5
#define PASSIVE_BUZZER 6

#define PIR_PIN 7

#define RGB_RED 9
#define RGB_GREEN 10
#define RGB_BLUE 11

#define RELAY_PIN 12

#define RAIN_PIN A0
#define LDR_PIN A1

// ---------- RFID PINS ----------
#define RFID_SS_PIN 29
#define RFID_RST_PIN 31

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// ---------- LCD ----------
LiquidCrystal lcd(28, 30, 32, 34, 36, 38);

// ---------- OBJECTS ----------
DHT dht(DHTPIN, DHTTYPE);
RTC_DS1307 rtc;

// ---------- RFID UID VALUES ----------
// Replace these after scanning your card/keychain in Serial Monitor.
String accessCardUID = "75 8C F9 03";      // card = activate/deactivate
String maintenanceUID = "F7 51 8C 04";     // keychain = maintenance on/off

// ---------- VARIABLES ----------
float tempC, tempF, humidity;
int rainValue, lightValue;
int pirState;
float distanceCM;

bool systemActive = true;
bool maintenanceMode = false;
bool motionAlert = false;

String weatherStatus = "NORMAL";
String lastRFID = "None";

unsigned long lastUpdate = 0;
unsigned long lastMotionBeep = 0;

// ---------- COLORS ----------
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define ORANGE  0xFD20
#define GRAY    0x8410
#define PURPLE  0x8010

void setup() {
  Serial.begin(9600);

  Wire.begin();
  SPI.begin();

  dht.begin();
  rtc.begin();
  rfid.PCD_Init();

  lcd.begin(16, 2);

  display.begin();
  display.setRotation(1);
  display.fillScreen(BLACK);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(ACTIVE_BUZZER, OUTPUT);
  pinMode(PASSIVE_BUZZER, OUTPUT);

  pinMode(PIR_PIN, INPUT);

  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);

  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(ACTIVE_BUZZER, LOW);
  digitalWrite(PASSIVE_BUZZER, LOW);
  digitalWrite(RELAY_PIN, LOW);

  setRGB(0, 0, 0);

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  startupScreen();
  rgbStartupTest();
  startupBeep();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Stage 3 RFID");
  lcd.setCursor(0, 1);
  lcd.print("PIR Enabled");
}

void loop() {
  checkRFID();

  if (millis() - lastUpdate >= 2000) {
    lastUpdate = millis();

    readSensors();
    classifyWeather();
    updateOutputs();
    updateLCD();
    updateGigaDisplay();
    printSerial();
  }
}

// ---------- RFID ----------
void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uid = getUID();
  lastRFID = uid;

  Serial.print("RFID UID: ");
  Serial.println(uid);

  if (uid == accessCardUID) {
    systemActive = !systemActive;

    if (!systemActive) {
      maintenanceMode = false;
      digitalWrite(RELAY_PIN, LOW);
      setRGB(0, 0, 0);
      beepDeactivate();
    } else {
      beepActivate();
    }
  }
  else if (uid == maintenanceUID) {
    maintenanceMode = !maintenanceMode;

    if (maintenanceMode) {
      systemActive = true;
      beepMaintenance();
    } else {
      beepActivate();
    }
  }
  else {
    deniedBeep();
  }

  delay(800);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

String getUID() {
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }

  uid.toUpperCase();
  return uid;
}

// ---------- READ SENSORS ----------
void readSensors() {
  humidity = dht.readHumidity();
  tempC = dht.readTemperature();

  if (!isnan(tempC)) {
    tempF = tempC * 9.0 / 5.0 + 32.0;
  } else {
    tempF = NAN;
  }

  rainValue = analogRead(RAIN_PIN);
  lightValue = analogRead(LDR_PIN);

  pirState = digitalRead(PIR_PIN);
  motionAlert = pirState == HIGH;

  distanceCM = readUltrasonic();
}

float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  return duration * 0.0343 / 2.0;
}

// ---------- WEATHER CLASSIFICATION ----------
void classifyWeather() {
  bool dhtGood = !isnan(tempF) && !isnan(humidity);
  bool rainDetected = rainValue < 400;
  bool darkOutside = lightValue < 300;
  bool sunnyOutside = lightValue > 700;
  bool stormWarning = dhtGood && humidity > 80 && darkOutside;
  bool waterLevelAlert = distanceCM > 0 && distanceCM < 6;

  if (!systemActive) {
    weatherStatus = "SYSTEM OFF";
  }
  else if (maintenanceMode) {
    weatherStatus = "MAINTENANCE";
  }
  else if (!dhtGood) {
    weatherStatus = "DHT ERROR";
  }
  else if (waterLevelAlert) {
    weatherStatus = "WATER ALERT";
  }
  else if (rainDetected && humidity > 70) {
    weatherStatus = "RAINING";
  }
  else if (stormWarning) {
    weatherStatus = "STORM WARNING";
  }
  else if (sunnyOutside) {
    weatherStatus = "SUNNY";
  }
  else {
    weatherStatus = "NORMAL";
  }
}

// ---------- OUTPUTS ----------
void updateOutputs() {
  if (!systemActive) {
    setRGB(0, 0, 0);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(ACTIVE_BUZZER, LOW);
    return;
  }

  if (maintenanceMode) {
    setRGB(255, 0, 255);
    digitalWrite(RELAY_PIN, LOW);
    return;
  }

  if (motionAlert && millis() - lastMotionBeep > 5000) {
    tone(PASSIVE_BUZZER, 1500, 100);
    lastMotionBeep = millis();
  }

  if (weatherStatus == "DHT ERROR") {
    setRGB(255, 255, 255);
    digitalWrite(RELAY_PIN, LOW);
  }
  else if (weatherStatus == "WATER ALERT") {
    setRGB(255, 0, 0);
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(ACTIVE_BUZZER, HIGH);
    delay(100);
    digitalWrite(ACTIVE_BUZZER, LOW);
  }
  else if (weatherStatus == "RAINING") {
    setRGB(0, 0, 255);
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(ACTIVE_BUZZER, HIGH);
    delay(80);
    digitalWrite(ACTIVE_BUZZER, LOW);
  }
  else if (weatherStatus == "STORM WARNING") {
    setRGB(255, 0, 0);
    digitalWrite(RELAY_PIN, HIGH);
    tone(PASSIVE_BUZZER, 1000, 150);
  }
  else if (weatherStatus == "SUNNY") {
    setRGB(255, 120, 0);
    digitalWrite(RELAY_PIN, LOW);
  }
  else {
    setRGB(0, 255, 0);
    digitalWrite(RELAY_PIN, LOW);
  }
}

// ---------- LCD ----------
void updateLCD() {
  lcd.clear();

  if (!systemActive) {
    lcd.setCursor(0, 0);
    lcd.print("SYSTEM OFF");
    lcd.setCursor(0, 1);
    lcd.print("Scan card ON");
    return;
  }

  if (maintenanceMode) {
    lcd.setCursor(0, 0);
    lcd.print("MAINTENANCE");
    lcd.setCursor(0, 1);
    lcd.print("Scan tag OFF");
    return;
  }

  lcd.setCursor(0, 0);

  if (isnan(tempF) || isnan(humidity)) {
    lcd.print("DHT ERROR");
  } else {
    lcd.print("T:");
    lcd.print(tempF, 1);
    lcd.print(" H:");
    lcd.print(humidity, 0);
  }

  lcd.setCursor(0, 1);

  if (motionAlert) {
    lcd.print("MOTION ");
  } else {
    lcd.print(weatherStatus);
  }
}

// ---------- GIGA DISPLAY ----------
void updateGigaDisplay() {
  DateTime now = rtc.now();

  display.fillScreen(BLACK);

  drawHeader(now);
  drawMainTempBox();
  drawHumidityBox();
  drawRainBox();
  drawLightBox();
  drawDistanceBox();
  drawStatusBox();
  drawModeBox();
}

void drawHeader(DateTime now) {
  display.fillRect(0, 0, 800, 55, BLUE);

  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(20, 15);
  display.print("GIGA WEATHER MONITOR");

  display.setTextSize(2);
  display.setCursor(570, 20);

  if (now.hour() < 10) display.print("0");
  display.print(now.hour());
  display.print(":");
  if (now.minute() < 10) display.print("0");
  display.print(now.minute());
}

void drawMainTempBox() {
  display.fillRoundRect(25, 80, 350, 130, 15, GRAY);

  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(45, 100);
  display.print("Temperature");

  display.setTextSize(5);
  display.setCursor(60, 145);

  if (isnan(tempF)) {
    display.print("ERR");
  } else {
    display.print(tempF, 1);
    display.print(" F");
  }
}

void drawHumidityBox() {
  display.fillRoundRect(425, 80, 350, 130, 15, CYAN);

  display.setTextColor(BLACK);
  display.setTextSize(3);
  display.setCursor(450, 100);
  display.print("Humidity");

  display.setTextSize(5);
  display.setCursor(465, 145);

  if (isnan(humidity)) {
    display.print("ERR");
  } else {
    display.print(humidity, 0);
    display.print(" %");
  }
}

void drawRainBox() {
  display.fillRoundRect(25, 235, 230, 105, 15, rainValue < 400 ? BLUE : GREEN);

  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(45, 255);
  display.print("Rain");

  display.setTextSize(3);
  display.setCursor(45, 300);
  display.print(rainValue < 400 ? "DETECTED" : "DRY");
}

void drawLightBox() {
  display.fillRoundRect(285, 235, 230, 105, 15, YELLOW);

  display.setTextColor(BLACK);
  display.setTextSize(3);
  display.setCursor(305, 255);
  display.print("Light");

  display.setTextSize(3);
  display.setCursor(305, 300);
  display.print(lightValue);
}

void drawDistanceBox() {
  display.fillRoundRect(545, 235, 230, 105, 15, ORANGE);

  display.setTextColor(BLACK);
  display.setTextSize(3);
  display.setCursor(565, 255);
  display.print("Level");

  display.setTextSize(3);
  display.setCursor(565, 300);

  if (distanceCM < 0) {
    display.print("No Echo");
  } else {
    display.print(distanceCM, 1);
    display.print("cm");
  }
}

void drawStatusBox() {
  uint16_t color = WHITE;

  if (weatherStatus == "SYSTEM OFF") color = GRAY;
  else if (weatherStatus == "MAINTENANCE") color = PURPLE;
  else if (weatherStatus == "STORM WARNING") color = RED;
  else if (weatherStatus == "RAINING") color = BLUE;
  else if (weatherStatus == "SUNNY") color = YELLOW;
  else if (weatherStatus == "NORMAL") color = GREEN;

  display.fillRoundRect(25, 365, 750, 65, 15, color);

  display.setTextColor(BLACK);
  display.setTextSize(3);
  display.setCursor(45, 385);

  if (motionAlert && systemActive && !maintenanceMode) {
    display.print("STATUS: MOTION DETECTED");
  } else {
    display.print("STATUS: ");
    display.print(weatherStatus);
  }
}

void drawModeBox() {
  display.fillRoundRect(25, 445, 750, 45, 12, WHITE);

  display.setTextColor(BLACK);
  display.setTextSize(2);
  display.setCursor(45, 460);

  display.print("RFID: ");
  display.print(lastRFID);

  display.setCursor(360, 460);
  display.print(systemActive ? "ACTIVE" : "OFF");

  display.setCursor(470, 460);
  display.print(maintenanceMode ? "MAINT ON" : "MAINT OFF");

  display.setCursor(620, 460);
  display.print(motionAlert ? "PIR: YES" : "PIR: NO");
}

// ---------- SERIAL ----------
void printSerial() {
  Serial.println("------ STAGE 3 RFID + PIR WEATHER SYSTEM ------");

  Serial.print("System Active: ");
  Serial.println(systemActive ? "YES" : "NO");

  Serial.print("Maintenance Mode: ");
  Serial.println(maintenanceMode ? "ON" : "OFF");

  Serial.print("PIR Motion: ");
  Serial.println(motionAlert ? "YES" : "NO");

  Serial.print("Last RFID: ");
  Serial.println(lastRFID);

  Serial.print("Weather Status: ");
  Serial.println(weatherStatus);

  Serial.print("Temp: ");
  Serial.print(tempF);
  Serial.println(" F");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Rain: ");
  Serial.println(rainValue);

  Serial.print("Light: ");
  Serial.println(lightValue);

  Serial.print("Distance: ");
  Serial.print(distanceCM);
  Serial.println(" cm");

  Serial.println("----------------------------------------------");
  Serial.println();
}

// ---------- RGB COMMON CATHODE ----------
void setRGB(int redVal, int greenVal, int blueVal) {
  analogWrite(RGB_RED, redVal);
  analogWrite(RGB_GREEN, greenVal);
  analogWrite(RGB_BLUE, blueVal);
}

// ---------- BEEP PATTERNS ----------
void beepActivate() {
  tone(PASSIVE_BUZZER, 1000, 120);
  delay(150);
  tone(PASSIVE_BUZZER, 1400, 120);
}

void beepDeactivate() {
  tone(PASSIVE_BUZZER, 700, 150);
  delay(180);
  tone(PASSIVE_BUZZER, 500, 150);
}

void beepMaintenance() {
  tone(PASSIVE_BUZZER, 900, 100);
  delay(120);
  tone(PASSIVE_BUZZER, 900, 100);
  delay(120);
  tone(PASSIVE_BUZZER, 1300, 150);
}

void deniedBeep() {
  digitalWrite(ACTIVE_BUZZER, HIGH);
  delay(300);
  digitalWrite(ACTIVE_BUZZER, LOW);
}

// ---------- STARTUP ----------
void startupScreen() {
  display.fillScreen(BLACK);

  display.setTextColor(WHITE);
  display.setTextSize(4);
  display.setCursor(110, 140);
  display.print("GIGA WEATHER");

  display.setTextSize(3);
  display.setCursor(125, 215);
  display.print("Stage 3 RFID + PIR");

  delay(1500);
}

void rgbStartupTest() {
  setRGB(255, 0, 0);
  delay(350);

  setRGB(0, 255, 0);
  delay(350);

  setRGB(0, 0, 255);
  delay(350);

  setRGB(255, 120, 0);
  delay(350);

  setRGB(0, 255, 0);
}

void startupBeep() {
  tone(PASSIVE_BUZZER, 800, 150);
  delay(200);

  tone(PASSIVE_BUZZER, 1200, 150);
  delay(200);

  tone(PASSIVE_BUZZER, 1600, 150);
  delay(200);

  noTone(PASSIVE_BUZZER);

  digitalWrite(ACTIVE_BUZZER, HIGH);
  delay(100);
  digitalWrite(ACTIVE_BUZZER, LOW);
}
