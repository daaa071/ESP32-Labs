#include <Wire.h>
#include <BluetoothSerial.h>

/**
 * @brief Configuration constants for ESP32 Bluetooth and I2C communication
 */
namespace Config {
  constexpr uint8_t I2C_SLAVE_ADDRESS = 0x08;

  // ESP32 default I2C pins
  constexpr uint8_t SDA_PIN = 21;
  constexpr uint8_t SCL_PIN = 22;

  constexpr size_t I2C_BUFFER_SIZE = 32;
  constexpr unsigned long I2C_READ_TIMEOUT_MS = 100;

  constexpr uint32_t SERIAL_BAUD_RATE = 115200;
  constexpr char BLUETOOTH_DEVICE_NAME[] = "ESP32_Sensors_BT";
}

// Global Bluetooth object
BluetoothSerial SerialBT;

/**
 * @brief Initializes Serial, Bluetooth, and I2C interfaces
 */
void setup() {
  Serial.begin(Config::SERIAL_BAUD_RATE);
  SerialBT.begin(Config::BLUETOOTH_DEVICE_NAME);

  Serial.println(F("✅ Bluetooth ready. Type 'stats' to request sensor data."));

  Wire.begin(Config::SDA_PIN, Config::SCL_PIN);
  delay(1000);  // Ensure I2C devices have time to boot
  Serial.println(F("✅ I2C initialized."));
}

/**
 * @brief Main loop checks for incoming Bluetooth commands
 */
void loop() {
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();  // Remove whitespace and newline characters

    Serial.print(F("📥 Received command: "));
    Serial.println(command);

    handleCommand(command);
  }
}

/**
 * @brief Handles recognized Bluetooth commands
 * 
 * @param command The received command string
 */
void handleCommand(const String& command) {
  if (command.equalsIgnoreCase("stats")) {
    String data = requestSensorData();

    if (data == "ERR") {
      SerialBT.println("⚠️ Sensor error: Arduino reported 'ERR'");
    } else if (!data.isEmpty()) {
      SerialBT.println("📊 Sensor data from Arduino:");
      SerialBT.println(data);
    } else {
      SerialBT.println("❌ No response from Arduino.");
    }
  } else {
    SerialBT.println("❓ Unknown command. Try 'stats'.");
  }
}

/**
 * @brief Requests sensor data from Arduino via I2C
 * 
 * @return String containing sensor data, or "ERR"/empty on failure
 */
String requestSensorData() {
  String result;
  Wire.requestFrom(Config::I2C_SLAVE_ADDRESS, Config::I2C_BUFFER_SIZE);

  const unsigned long startTime = millis();

  while (Wire.available()) {
    char c = Wire.read();
    if (c == '\n') break;
    result += c;

    if (millis() - startTime > Config::I2C_READ_TIMEOUT_MS) {
      Serial.println(F("⚠️ I2C read timeout."));
      break;
    }
  }

  return result;
}
