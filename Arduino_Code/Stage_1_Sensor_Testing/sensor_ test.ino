// Environmental Sensor Testing Firmware
// Smart Weather Monitoring System
//
// Stage 1:
// Initial environmental sensor testing and serial monitor validation.
//
// This firmware was used to verify:
// - Temperature readings
// - Humidity readings
// - Stable sensor communication
// - Serial monitor output formatting
// - Real-time environmental monitoring

#include <DHT.h>

// -----------------------------
// Sensor Configuration
// -----------------------------
#define DHTPIN 2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// -----------------------------
// Setup
// -----------------------------
void setup() {

    // Initialize serial monitor
    Serial.begin(9600);

    // Start DHT sensor
    dht.begin();

    Serial.println("=================================");
    Serial.println(" Smart Weather Monitoring System ");
    Serial.println(" Stage 1 - Sensor Testing ");
    Serial.println("=================================");

    Serial.println("Initializing environmental sensors...");
    delay(1500);

    Serial.println("Sensor initialization complete.");
    Serial.println();
}

// -----------------------------
// Main Loop
// -----------------------------
void loop() {

    // Read sensor values
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // Check for sensor read failure
    if (isnan(temperature) || isnan(humidity)) {

        Serial.println("ERROR: Failed to read DHT sensor.");
        Serial.println();

        delay(2000);
        return;
    }

    // Display temperature
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    // Display humidity
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    // Environmental condition check
    if (temperature > 30) {
        Serial.println("Condition: High temperature detected.");
    }
    else if (temperature < 15) {
        Serial.println("Condition: Low temperature detected.");
    }
    else {
        Serial.println("Condition: Normal operating range.");
    }

    // Separator line
    Serial.println("---------------------------------");

    // Delay between sensor updates
    delay(2000);
}
