#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include <LiquidCrystal.h>

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

// LCD pins: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(28, 30, 32, 34, 36, 38);

DHT dht(DHTPIN, DHTTYPE);
RTC_DS1307 rtc;

// ---------- VARIABLES ----------
float tempC;
float tempF;
float humidity;

int rainValue;
int lightValue;
int pirState;

long duration;
float distanceCM;

unsigned long lastUpdate = 0;

// ---------- SETUP ----------
void setup() {
  Serial.begin(9600);

  dht.begin();
  Wire.begin();
  lcd.begin(16, 2);

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
  lcd.print("Stage 1 Test");

  delay(2000);
  lcd.clear();

  if (!rtc.begin()) {
    Serial.println("RTC NOT FOUND");
    lcd.setCursor(0, 0);
    lcd.print("RTC NOT FOUND");
    delay(2000);
  }

  if (!rtc.isrunning()) {
    Serial.println("RTC not running.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  startupBeep();
}

// ---------- LOOP ----------
void loop() {
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();

    readSensors();
    printSerial();
    updateLCD();
    weatherStatusLogic();
  }
}

// ---------- READ ALL SENSORS ----------
void readSensors() {
  humidity = dht.readHumidity();
  tempC = dht.readTemperature();
  tempF = (tempC * 9.0 / 5.0) + 32.0;

  rainValue = analogRead(RAIN_PIN);
  lightValue = analogRead(LDR_PIN);
  pirState = digitalRead(PIR_PIN);

  distanceCM = readUltrasonic();
}

// ---------- ULTRASONIC FUNCTION ----------
float readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  return duration * 0.0343 / 2.0;
}

// ---------- SERIAL OUTPUT ----------
void printSerial() {
  DateTime now = rtc.now();

  Serial.println("--------- WEATHER STAGE 1 ---------");

  Serial.print("Time: ");
  Serial.print(now.hour());
  Serial.print(":");
  if (now.minute() < 10) Serial.print("0");
  Serial.print(now.minute());
  Serial.print(":");
  if (now.second() < 10) Serial.print("0");
  Serial.println(now.second());

  Serial.print("Temp: ");
  Serial.print(tempF);
  Serial.println(" F");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Rain Sensor: ");
  Serial.println(rainValue);

  Serial.print("Light Sensor: ");
  Serial.println(lightValue);

  Serial.print("Distance: ");
  Serial.print(distanceCM);
  Serial.println(" cm");

  Serial.print("PIR Motion: ");
  Serial.println(pirState == HIGH ? "MOTION" : "NO MOTION");

  Serial.println("-----------------------------------");
  Serial.println();
}

// ---------- LCD UPDATE ----------
void updateLCD() {
  lcd.clear();

  if (isnan(tempF) || isnan(humidity)) {
    lcd.setCursor(0, 0);
    lcd.print("DHT ERROR");
    lcd.setCursor(0, 1);
    lcd.print("Check wiring");
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(tempF, 1);
  lcd.print("F ");

  lcd.print("H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);

  if (rainValue < 400) {
    lcd.print("RAIN ");
  } else {
    lcd.print("DRY  ");
  }

  lcd.print("L:");
  lcd.print(lightValue);
}

// ---------- WEATHER LOGIC ----------
void weatherStatusLogic() {
  bool rainDetected = rainValue < 400;
  bool darkOutside = lightValue < 300;
  bool motionDetected = pirState == HIGH;

  if (rainDetected) {
    // Rainy = Blue
    setRGB(0, 0, 255);
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(ACTIVE_BUZZER, HIGH);
    delay(100);
    digitalWrite(ACTIVE_BUZZER, LOW);
  }
  else if (humidity > 80 && darkOutside) {
    // Possible storm = Red
    setRGB(255, 0, 0);
    tone(PASSIVE_BUZZER, 1000, 150);
    digitalWrite(RELAY_PIN, HIGH);
  }
  else if (lightValue > 700) {
    // Sunny = Yellow
    setRGB(255, 120, 0);
    digitalWrite(RELAY_PIN, LOW);
  }
  else {
    // Normal = Green
    setRGB(0, 255, 0);
    digitalWrite(RELAY_PIN, LOW);
  }

  if (motionDetected) {
    tone(PASSIVE_BUZZER, 1500, 80);
  }
}

// ---------- RGB FUNCTION ----------
void setRGB(int redVal, int greenVal, int blueVal) {
  analogWrite(RGB_RED, redVal);
  analogWrite(RGB_GREEN, greenVal);
  analogWrite(RGB_BLUE, blueVal);
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
