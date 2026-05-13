#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include "Arduino_GigaDisplay_GFX.h"

// ---------- DISPLAY ----------
GigaDisplay_GFX display;

// ---------- PIN SETUP ----------
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

// LCD: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(28, 30, 32, 34, 36, 38);

DHT dht(DHTPIN, DHTTYPE);
RTC_DS1307 rtc;

// ---------- VARIABLES ----------
float tempC, tempF, humidity;
int rainValue, lightValue, pirState;
float distanceCM;

unsigned long lastUpdate = 0;

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

void setup() {
  Serial.begin(9600);

  dht.begin();
  Wire.begin();

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

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GIGA Weather");
  lcd.setCursor(0, 1);
  lcd.print("Stage 2 Ready");

  if (!rtc.begin()) {
    Serial.println("RTC NOT FOUND");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC NOT FOUND");
    delay(1500);
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC was not running. Setting time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  startupScreen();
  rgbStartupTest();
  startupBeep();
}

void loop() {
  if (millis() - lastUpdate >= 2000) {
    lastUpdate = millis();

    readSensors();
    updateLogic();
    updateLCD();
    updateGigaDisplay();
    printSerial();
  }
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

  distanceCM = readUltrasonic();
}

// ---------- ULTRASONIC ----------
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

// ---------- WEATHER LOGIC ----------
void updateLogic() {
  bool dhtGood = !isnan(tempF) && !isnan(humidity);
  bool rainDetected = rainValue < 400;
  bool darkOutside = lightValue < 300;
  bool sunnyOutside = lightValue > 700;
  bool stormWarning = dhtGood && humidity > 80 && darkOutside;

  Serial.println("------ RGB DEBUG ------");
  Serial.print("Rain Value: ");
  Serial.println(rainValue);
  Serial.print("Light Value: ");
  Serial.println(lightValue);
  Serial.print("Humidity: ");
  Serial.println(humidity);

  if (!dhtGood) {
    Serial.println("STATUS: DHT ERROR -> WHITE");
    setRGB(255, 255, 255);
    digitalWrite(RELAY_PIN, LOW);
  }
  else if (rainDetected) {
    Serial.println("STATUS: RAINING -> BLUE");
    setRGB(0, 0, 255);

    digitalWrite(RELAY_PIN, HIGH);

    digitalWrite(ACTIVE_BUZZER, HIGH);
    delay(80);
    digitalWrite(ACTIVE_BUZZER, LOW);
  }
  else if (stormWarning) {
    Serial.println("STATUS: STORM -> RED");
    setRGB(255, 0, 0);

    digitalWrite(RELAY_PIN, HIGH);
    tone(PASSIVE_BUZZER, 1000, 150);
  }
  else if (sunnyOutside) {
    Serial.println("STATUS: SUNNY -> YELLOW");
    setRGB(255, 120, 0);

    digitalWrite(RELAY_PIN, LOW);
  }
  else {
    Serial.println("STATUS: NORMAL -> GREEN");
    setRGB(0, 255, 0);

    digitalWrite(RELAY_PIN, LOW);
  }

  if (pirState == HIGH) {
    Serial.println("MOTION DETECTED");
    tone(PASSIVE_BUZZER, 1500, 80);
  }

  Serial.println("------------------------");
}

// ---------- LCD UPDATE ----------
void updateLCD() {
  lcd.clear();

  if (isnan(tempF) || isnan(humidity)) {
    lcd.setCursor(0, 0);
    lcd.print("DHT ERROR");
    lcd.setCursor(0, 1);
    lcd.print("Check D2");
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(tempF, 1);
  lcd.print("F H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print(rainValue < 400 ? "RAIN " : "DRY  ");
  lcd.print("L:");
  lcd.print(lightValue);
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
  display.fillRoundRect(25, 80, 350, 150, 15, GRAY);

  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(45, 100);
  display.print("Temperature");

  display.setTextSize(5);
  display.setCursor(60, 150);

  if (isnan(tempF)) {
    display.print("DHT ERR");
  } else {
    display.print(tempF, 1);
    display.print(" F");
  }
}

void drawHumidityBox() {
  display.fillRoundRect(425, 80, 350, 150, 15, CYAN);

  display.setTextColor(BLACK);
  display.setTextSize(3);
  display.setCursor(450, 100);
  display.print("Humidity");

  display.setTextSize(5);
  display.setCursor(465, 150);

  if (isnan(humidity)) {
    display.print("ERR");
  } else {
    display.print(humidity, 0);
    display.print(" %");
  }
}

void drawRainBox() {
  display.fillRoundRect(25, 260, 230, 120, 15, rainValue < 400 ? BLUE : GREEN);

  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(45, 280);
  display.print("Rain");

  display.setTextSize(3);
  display.setCursor(45, 330);
  display.print(rainValue < 400 ? "DETECTED" : "DRY");
}

void drawLightBox() {
  display.fillRoundRect(285, 260, 230, 120, 15, YELLOW);

  display.setTextColor(BLACK);
  display.setTextSize(3);
  display.setCursor(305, 280);
  display.print("Light");

  display.setTextSize(3);
  display.setCursor(305, 330);
  display.print(lightValue);
}

void drawDistanceBox() {
  display.fillRoundRect(545, 260, 230, 120, 15, ORANGE);

  display.setTextColor(BLACK);
  display.setTextSize(3);
  display.setCursor(565, 280);
  display.print("Level");

  display.setTextSize(3);
  display.setCursor(565, 330);

  if (distanceCM < 0) {
    display.print("No Echo");
  } else {
    display.print(distanceCM, 1);
    display.print(" cm");
  }
}

void drawStatusBox() {
  display.fillRoundRect(25, 410, 750, 70, 15, WHITE);

  display.setTextColor(BLACK);
  display.setTextSize(3);
  display.setCursor(45, 430);

  if (isnan(tempF) || isnan(humidity)) {
    display.print("STATUS: DHT ERROR");
  }
  else if (rainValue < 400) {
    display.print("STATUS: RAINING - RELAY ON");
  }
  else if (humidity > 80 && lightValue < 300) {
    display.print("STATUS: POSSIBLE STORM");
  }
  else if (lightValue > 700) {
    display.print("STATUS: SUNNY CONDITIONS");
  }
  else {
    display.print("STATUS: NORMAL WEATHER");
  }
}

// ---------- SERIAL ----------
void printSerial() {
  DateTime now = rtc.now();

  Serial.println("------ STAGE 2 WEATHER DASHBOARD ------");

  Serial.print("Time: ");
  if (now.hour() < 10) Serial.print("0");
  Serial.print(now.hour());
  Serial.print(":");
  if (now.minute() < 10) Serial.print("0");
  Serial.println(now.minute());

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

  Serial.print("PIR: ");
  Serial.println(pirState == HIGH ? "Motion" : "No Motion");

  Serial.println("--------------------------------------");
  Serial.println();
}

// ---------- RGB COMMON CATHODE ----------
void setRGB(int redVal, int greenVal, int blueVal) {
  analogWrite(RGB_RED, redVal);
  analogWrite(RGB_GREEN, greenVal);
  analogWrite(RGB_BLUE, blueVal);
}

// ---------- STARTUP SCREEN ----------
void startupScreen() {
  display.fillScreen(BLACK);

  display.setTextColor(WHITE);
  display.setTextSize(4);
  display.setCursor(120, 150);
  display.print("GIGA WEATHER");

  display.setTextSize(3);
  display.setCursor(190, 220);
  display.print("Stage 2 Dashboard");

  delay(1500);
}

// ---------- RGB STARTUP TEST ----------
void rgbStartupTest() {
  setRGB(255, 0, 0);
  delay(500);

  setRGB(0, 255, 0);
  delay(500);

  setRGB(0, 0, 255);
  delay(500);

  setRGB(255, 120, 0);
  delay(500);

  setRGB(0, 255, 0);
}

// ---------- STARTUP BEEP ----------
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
