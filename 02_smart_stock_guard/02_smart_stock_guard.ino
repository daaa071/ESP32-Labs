/**
 * @file 02_smart_stock_guard.ino
 * @brief ESP32-based stock price monitor with RGB LED and buzzer alerts.
 * 
 * Fetches live stock data from AlphaVantage API and visualizes it
 * via RGB LED and buzzer alerts when the stock price falls below a user-defined threshold.
 * The configuration and current data can be accessed through a local web interface.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

/** @brief Wi-Fi credentials */
namespace WiFiConfig {
  constexpr const char* SSID = "YOUR_WIFI_SSID";
  constexpr const char* PASSWORD = "YOUR_WIFI_PASSWORD";
}

/** @brief AlphaVantage stock API configuration */
namespace StockAPI {
  constexpr const char* API_KEY = "YOUR_ALPHA_VANTAGE_API_KEY";
}

/** @brief Hardware pin configuration */
namespace PinConfig {
  constexpr int RED_PIN    = 21;
  constexpr int GREEN_PIN  = 22;
  constexpr int BLUE_PIN   = 23;
  constexpr int BUZZER_PIN = 19;
}

/** @brief Timing configuration */
namespace Timing {
  constexpr unsigned long CHECK_INTERVAL_MS = 60'000UL;  ///< 1 minute
}

// -----------------------------------------------------------------------------
// Global State
// -----------------------------------------------------------------------------

String stockSymbol = "AAPL";
float alertPrice = 260.0F;
float currentPrice = 0.0F;

WebServer server(80);
unsigned long lastCheck = 0UL;

// -----------------------------------------------------------------------------
// HTML Web Interface
// -----------------------------------------------------------------------------

constexpr char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Stock Watcher</title>
  <style>
    body { background: #0d1117; color: #c9d1d9; font-family: "Segoe UI", sans-serif; text-align: center; margin-top: 40px; }
    h1 { color: #58a6ff; }
    .card { background: #161b22; border-radius: 10px; padding: 20px; display: inline-block; box-shadow: 0 0 10px #0d419d; }
    input, button { padding: 8px; margin: 6px; border-radius: 5px; border: none; font-size: 1em; }
    button { background-color: #238636; color: white; cursor: pointer; }
    button:hover { background-color: #2ea043; }
  </style>
</head>
<body>
  <h1>📈 ESP32 Stock Watcher</h1>

  <div class="card">
    <p><b>Current Symbol:</b> <span id="symbol">-</span></p>
    <p><b>Current Price:</b> $<span id="price">--.--</span></p>
    <p><b>Alert Price:</b> $<span id="alert">--.--</span></p>
  </div>

  <form action="/set" method="get">
    <p><input type="text" name="symbol" placeholder="Stock symbol (AAPL, TSLA)" required></p>
    <p><input type="number" step="0.01" name="alert" placeholder="Alert price" required></p>
    <button type="submit">💾 Save</button>
  </form>

  <script>
    async function updateData() {
      const res = await fetch('/getData');
      const data = await res.json();
      document.getElementById('symbol').textContent = data.symbol;
      document.getElementById('price').textContent = data.price.toFixed(2);
      document.getElementById('alert').textContent = data.alert.toFixed(2);
    }
    updateData();
    setInterval(updateData, 30000);
  </script>
</body>
</html>
)rawliteral";

// -----------------------------------------------------------------------------
// Function Declarations
// -----------------------------------------------------------------------------

void setColor(int red, int green, int blue);
void checkStockPrice();
void handleRoot();
void handleGetData();
void handleSet();

// -----------------------------------------------------------------------------
// Setup & Main Loop
// -----------------------------------------------------------------------------

/**
 * @brief Initializes hardware, Wi-Fi connection, and web server routes.
 */
void setup() {
  Serial.begin(115200);

  pinMode(PinConfig::RED_PIN, OUTPUT);
  pinMode(PinConfig::GREEN_PIN, OUTPUT);
  pinMode(PinConfig::BLUE_PIN, OUTPUT);
  pinMode(PinConfig::BUZZER_PIN, OUTPUT);

  setColor(0, 0, 255);  // Blue: connecting to Wi-Fi

  WiFi.begin(WiFiConfig::SSID, WiFiConfig::PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi connected!");
  Serial.println(WiFi.localIP());

  setColor(0, 255, 0);  // Green: connection successful

  // Register HTTP routes
  server.on("/", handleRoot);
  server.on("/getData", handleGetData);
  server.on("/set", handleSet);
  server.begin();
}

/**
 * @brief Main loop: handles HTTP requests and periodically checks stock price.
 */
void loop() {
  server.handleClient();

  const auto now = millis();
  if (now - lastCheck > Timing::CHECK_INTERVAL_MS) {
    lastCheck = now;
    checkStockPrice();
  }
}

// -----------------------------------------------------------------------------
// Function Definitions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

/**
 * @brief Sets the RGB LED color intensity.
 */
void setColor(int red, int green, int blue) {
  analogWrite(PinConfig::RED_PIN, red);
  analogWrite(PinConfig::GREEN_PIN, green);
  analogWrite(PinConfig::BLUE_PIN, blue);
}

/**
 * @brief Fetches the current stock price from AlphaVantage API and triggers alerts.
 */
void checkStockPrice() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  const String url = "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=" 
                   + stockSymbol + "&apikey=" + StockAPI::API_KEY;

  http.begin(url);
  const int httpCode = http.GET();

  if (httpCode == 200) {
    const String payload = http.getString();
    Serial.println(payload);

    DynamicJsonDocument doc(4096);
    if (auto error = deserializeJson(doc, payload)) {
      Serial.printf("JSON parse error: %s\n", error.c_str());
      http.end();
      return;
    }

    const char* priceStr = doc["Global Quote"]["05. price"];
    if (priceStr) {
      currentPrice = atof(priceStr);
      Serial.printf("Current %s price: $%.2f\n", stockSymbol.c_str(), currentPrice);

      if (currentPrice <= alertPrice) {
        // Red alert (price below threshold)
        setColor(255, 0, 0);
        for (int i = 0; i < 10; ++i) {
          digitalWrite(PinConfig::BUZZER_PIN, HIGH);
          delay(100);
          digitalWrite(PinConfig::BUZZER_PIN, LOW);
          delay(100);
        }
      } else {
        // Green (normal state)
        setColor(0, 255, 0);
        digitalWrite(PinConfig::BUZZER_PIN, HIGH);
        delay(1000);
        digitalWrite(PinConfig::BUZZER_PIN, LOW);
        delay(1000);
      }
    } else {
      Serial.println("Price field not found in JSON response");
      setColor(255, 255, 255);  // White: unknown data
    }
  } else {
    Serial.println("Error fetching data from API");
    setColor(255, 255, 255);  // White: error
  }

  http.end();
}

// -----------------------------------------------------------------------------
// Web Handlers
// -----------------------------------------------------------------------------

/**
 * @brief Serves the main web interface.
 */
void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

/**
 * @brief Returns the current stock data in JSON format for frontend updates.
 */
void handleGetData() {
  const String json = "{\"symbol\":\"" + stockSymbol +
                      "\",\"price\":" + String(currentPrice, 2) +
                      ",\"alert\":" + String(alertPrice, 2) + "}";
  server.send(200, "application/json", json);
}

/**
 * @brief Handles user input for stock symbol and alert price via HTTP form.
 */
void handleSet() {
  bool symbolChanged = false;

  if (server.hasArg("symbol")) {
    const String newSymbol = server.arg("symbol");
    if (newSymbol != stockSymbol) {
      stockSymbol = newSymbol;
      symbolChanged = true;
    }
  }

  if (server.hasArg("alert")) {
    alertPrice = server.arg("alert").toFloat();
  }

  // Reset the displayed price when the stock symbol changes
  if (symbolChanged) {
    currentPrice = 0.0F;
  }

  server.sendHeader("Location", "/");
  server.send(303);
}
