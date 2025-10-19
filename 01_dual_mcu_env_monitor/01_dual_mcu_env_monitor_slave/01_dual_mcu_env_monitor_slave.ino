#include <Wire.h>
#include <DHT.h>

/**
 * @brief Configuration for sensors and I2C communication
 */
namespace SensorConfig {
  constexpr uint8_t DHT_PIN = 2;
  constexpr uint8_t DHT_TYPE = DHT11;

  constexpr uint8_t WATER_SENSOR_PIN = A0;
  constexpr uint8_t LIGHT_SENSOR_PIN = A1;

  constexpr uint8_t I2C_ADDRESS = 0x08;
  constexpr unsigned long READ_INTERVAL_MS = 2000;
}

/**
 * @brief Sensor data structure
 */
struct SensorData {
  float temperature = 0.0f;
  float humidity = 0.0f;
  int waterLevel = 0;  // percentage
  int lightLevel = 0;  // percentage
};

// Global state
DHT dht(SensorConfig::DHT_PIN, SensorConfig::DHT_TYPE);
SensorData cachedData;

unsigned long lastReadTimestamp = 0;

/**
 * @brief Arduino initialization
 */
void setup() {
  Serial.begin(9600);
  dht.begin();

  Wire.begin(SensorConfig::I2C_ADDRESS);
  Wire.onRequest(onI2CRequest);

  Serial.println(F("Sensor node initialized."));
}

/**
 * @brief Arduino main loop
 */
void loop() {
  const unsigned long now = millis();

  if (now - lastReadTimestamp >= SensorConfig::READ_INTERVAL_MS) {
    lastReadTimestamp = now;
    readSensors();
    logSensorData();
  }
}

/**
 * @brief Reads all sensors and updates cached data
 */
void readSensors() {
  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();

  if (!isnan(humidity)) {
    cachedData.humidity = humidity;
  }

  if (!isnan(temperature)) {
    cachedData.temperature = temperature;
  }

  int waterRaw = analogRead(SensorConfig::WATER_SENSOR_PIN);
  cachedData.waterLevel = map(waterRaw, 0, 1023, 0, 100);

  int lightRaw = analogRead(SensorConfig::LIGHT_SENSOR_PIN);
  cachedData.lightLevel = map(lightRaw, 0, 1023, 0, 100);
}

/**
 * @brief Prints the latest sensor values to Serial
 */
void logSensorData() {
  Serial.print(F("Updated: T="));
  Serial.print(cachedData.temperature, 1);
  Serial.print(F("°C, H="));
  Serial.print(cachedData.humidity, 0);
  Serial.print(F("%, W="));
  Serial.print(cachedData.waterLevel);
  Serial.print(F("%, L="));
  Serial.print(cachedData.lightLevel);
  Serial.println(F("%"));
}

/**
 * @brief Sends sensor data to I2C master
 */
void onI2CRequest() {
  if (isnan(cachedData.temperature) || isnan(cachedData.humidity)) {
    Wire.write("ERR\n");
    return;
  }

  String payload = "T:" + String(cachedData.temperature, 1) +
                   ",H:" + String(cachedData.humidity, 0) +
                   ",W:" + String(cachedData.waterLevel) +
                   ",L:" + String(cachedData.lightLevel) +
                   "\n";

  // Prevent exceeding 32-byte I2C buffer
  if (payload.length() > 32) {
    Wire.write("DATA_TOO_LONG\n");
    return;
  }

  Wire.write(payload.c_str(), payload.length());
}
