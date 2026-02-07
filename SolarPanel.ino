/*
 * ESP32 Solar Panel Monitor & Web Dashboard
 *
 * Features:
 * - Reads Temperature (Thermistor on A4/GPIO 36)
 * - Reads Light/Irradiance (Photoresistor on GPIO 34)
 * - Hosts a Web Dashboard (WiFi: "SolarPanel_Monitor", IP: 192.168.4.1)
 */

#include <WebServer.h>
#include <WiFi.h>
#include <DNSServer.h>

// --- WiFi & Server Settings ---
const char *ssid = "SolarPanel_Monitor"; // Name of the WiFi network
WebServer server(80);
DNSServer dnsServer; // DNS Server for Captive Portal

// --- Sensor Settings ---
// Thermistor
// CRITICAL: WiFi uses ADC2 pins (GPIO 0, 2, 4, 12-15, 25-27).
// We MUST use ADC1 pins (GPIO 32-39) when WiFi is on.
// A1 is usually GPIO 25 (ADC2) -> Conflict!
// New Plan: Use A4 (GPIO 36 - often labeled "VP" or "A4" or "36").
#if defined(A4)
const int thermistorPin = A4;
#else
const int thermistorPin = 36;
#endif
const float SERIES_RESISTOR = 10000.0;
const float NOMINAL_RESISTANCE = 10000.0;
const float NOMINAL_TEMPERATURE = 25.0;
const float B_COEFFICIENT = 3950.0;
const float ADC_MAX = 4095.0;
const float VCC = 3.3;
const float TEMP_THRESHOLD = 30.0;

// Photoresistor
const int photoresistorPin = 34;

// --- Global Variables for Sensor Data ---
float currentTemp = 0.0;
int currentLightPercent = 0;
String currentStatus = "OK";

// --- HTML Dashboard (Stored in Program Memory) ---
// Note: We use raw string literal R"()" for cleaner HTML
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Solar Panel Monitor</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; text-align: center; background-color: #121212; color: #e0e0e0; margin: 0; padding: 20px; }
    h1 { color: #4caf50; }
    .card { background: #1e1e1e; max-width: 400px; margin: 0 auto 20px; padding: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    .value { font-size: 2.5rem; font-weight: bold; }
    .unit { font-size: 1.2rem; color: #888; }
    .label { font-size: 1rem; color: #bbb; text-transform: uppercase; letter-spacing: 1px; }
    .warning { color: #ff5252; animation: blink 1s infinite; }
    @keyframes blink { 50% { opacity: 0.5; } }
  </style>
</head>
<body>
  <h1>Solar Monitor</h1>
  
  <div class="card">
    <div class="label">Temperature</div>
    <div id="temp" class="value">--</div>
    <div class="unit">&deg;C</div>
  </div>
  
  <div class="card">
    <div class="label">Irradiance (Light)</div>
    <div id="light" class="value">--</div>
    <div class="unit">%</div>
  </div>

  <div class="card">
    <div class="label">Status</div>
    <div id="status" class="value" style="font-size: 1.5rem;">--</div>
  </div>

<script>
setInterval(function() {
  fetch("/readings")
    .then(response => response.json())
    .then(data => {
      document.getElementById("temp").innerHTML = data.temp.toFixed(1);
      document.getElementById("light").innerHTML = data.light;
      document.getElementById("status").innerHTML = data.status;
      
      // Update Status Color
      if(data.status.includes("WARNING")) {
        document.getElementById("status").className = "value warning";
      } else {
        document.getElementById("status").className = "value";
        document.getElementById("status").style.color = "#4caf50";
      }
    });
}, 1000); // Update every 1 second
</script>
</body>
</html>
)rawliteral";

// --- Server Handlers ---
void handleRoot() { server.send(200, "text/html", index_html); }

void handleReadings() {
  // Create JSON string: {"temp": 25.5, "light": 80, "status": "OK"}
  String json = "{";
  json += "\"temp\":" + String(currentTemp) + ",";
  json += "\"light\":" + String(currentLightPercent) + ",";
  json += "\"status\":\"" + currentStatus + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// Redirect unknown paths to root (essential for Captive Portal)
void handleNotFound() {
  server.sendHeader("Location", "/", true); // Redirect to our IP
  server.send(302, "text/plain", "");
}

// --- Setup ---
void setup() {
  Serial.begin(115200);

  // ADC Config
  analogReadResolution(12);

  // WiFi Config (SoftAP)
  Serial.println("\n--- Starting Solar Panel Monitor ---");

  // Force disconnect/reset to clear old settings
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);

  Serial.print("Setting up WiFi AP: ");
  Serial.println(ssid);
  WiFi.softAP(ssid);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // --- DNS Server Config (Captive Portal) ---
  // Redirect ALL traffic (wildcard "*") to our own IP
  dnsServer.start(53, "*", IP);
  Serial.println("DNS Server started (Captive Portal active)");

  // Server Config
  server.on("/", handleRoot);
  server.on("/readings", handleReadings);
  server.onNotFound(handleNotFound); // Catch-all for other URLs
  server.begin();
  Serial.println("Web Server Started.");
}

// --- Loop (Non-Blocking) ---
unsigned long lastTime = 0;
const long interval = 1000; // Read sensors every 1s

void loop() {
  // 1. Handle Web Clients & DNS (Must run constantly)
  dnsServer.processNextRequest(); // Handle DNS requests
  server.handleClient();

  // 2. Read Sensors (Periodic)
  if (millis() - lastTime > interval) {
    lastTime = millis();

    // --- Read Thermistor ---
    int adcTherm = analogRead(thermistorPin);
    float resistance = 0;
    if (adcTherm == 0)
      resistance = 1000000;
    else if (adcTherm >= ADC_MAX)
      resistance = 0;
    else
      resistance = SERIES_RESISTOR / ((ADC_MAX / (float)adcTherm) - 1);

    float steinhart = resistance / NOMINAL_RESISTANCE;
    steinhart = log(steinhart);
    steinhart /= B_COEFFICIENT;
    steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);
    steinhart = 1.0 / steinhart;
    steinhart -= 273.15;

    currentTemp = steinhart; // Update global var

    // --- Read Photoresistor ---
    int adcLDR = analogRead(photoresistorPin);
    long lightPercent = map(adcLDR, 4095, 0, 0, 100);
    if (lightPercent < 0)
      lightPercent = 0;
    if (lightPercent > 100)
      lightPercent = 100;

    currentLightPercent = lightPercent; // Update global var

    // --- Logic Checks ---
    if (currentTemp > TEMP_THRESHOLD) {
      currentStatus = "WARNING: HIGH TEMP!";
      // Fan Logic Placeholder
    } else {
      currentStatus = "System Normal";
    }

    // --- Serial Debug ---
    Serial.print("Web Clients: ");
    Serial.print(WiFi.softAPgetStationNum());
    Serial.print(" | Temp: ");
    Serial.print(currentTemp, 1);
    Serial.print(" C | Light: ");
    Serial.print(currentLightPercent);
    Serial.print("% | Status: ");
    Serial.println(currentStatus);
  }
}