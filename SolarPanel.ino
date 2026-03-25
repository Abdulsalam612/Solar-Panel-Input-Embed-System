/*
 * ESP32 Solar Panel Monitor & Web Dashboard
 *
 * Features:
 * - Reads Temperature (Thermistor on GPIO 36)
 * - Reads Light/Irradiance (Photoresistor on GPIO 34)
 * - Buzzer Alarm (on GPIO 17 - PWM)
 * - Hosts a Web Dashboard (WiFi: "SolarPanel_Monitor", IP: 192.168.4.1)
 * - Simulates Locations based on sensor data
 */

#include "DFRobotDFPlayerMini.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

// --- WiFi & Server Settings ---
const char *ssid = "SolarPanel_Monitor"; // Name of the WiFi network
WebServer server(80);
DNSServer dnsServer; // DNS Server for Captive Portal

// --- Sensor Settings ---
// Thermistor (GPIO 36 / VP)
const int thermistorPin = 36;
const float SERIES_RESISTOR = 10000.0;
const float NOMINAL_RESISTANCE = 10000.0;
const float NOMINAL_TEMPERATURE = 25.0;
const float B_COEFFICIENT = 3950.0;
const float ADC_MAX = 4095.0;
const float VCC = 3.3;
const float TEMP_THRESHOLD = 30.0;

// Photoresistor (GPIO 34)
const int photoresistorPin = 34;

// DFPlayer Mini Settings (UART)
const int dfplayerRX = 16;  // ESP32 RX (Connect to DFPlayer TX)
const int dfplayerTX = 4;   // ESP32 TX (Connect to DFPlayer RX)
HardwareSerial dfSerial(1); // Use UART1 for DFPlayer
DFRobotDFPlayerMini myDFPlayer;

// Buzzer Settings (PWM for Passive Buzzer on GPIO 17)
const int buzzerPin = 17;
const int buzzerChannel = 0;
const int buzzerFreq = 2000;    // 2 kHz tone
const int buzzerResolution = 8; // 8-bit resolution (0-255)

// --- Global Variables for Sensor Data ---
float currentTemp = 0.0;
int currentLightPercent = 0;
String currentStatus = "OK";
String currentLocation = "Unknown"; // New: Simulated Location
bool isPlayingMP3 = false; // Tracks MP3 state to prevent spamming play commands

// --- HTML Dashboard (Stored in Program Memory) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Solar Monitor</title>
  <style>
    body { font-family: 'Segoe UI', Roboto, sans-serif; text-align: center; background-color: #0f0f0f; color: #e0e0e0; margin: 0; padding: 20px; }
    h1 { color: #fbc02d; text-shadow: 0 0 10px rgba(251, 192, 45, 0.3); margin-bottom: 30px; }
    
    .dashboard { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; }
    
    .card { 
      background: #1e1e1e; 
      width: 100%; max-width: 360px; 
      padding: 20px; border-radius: 15px; 
      box-shadow: 0 8px 16px rgba(0,0,0,0.5); 
      border: 1px solid #333;
      transition: transform 0.2s;
    }
    .card:hover { transform: translateY(-2px); border-color: #555; }
    
    .label { font-size: 0.9rem; color: #aaa; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 10px; }
    .value-box { display: flex; align-items: baseline; justify-content: center; margin-bottom: 15px; }
    .value { font-size: 3rem; font-weight: 700; color: #fff; }
    .unit { font-size: 1.2rem; color: #666; margin-left: 5px; }
    
    .status-card { max-width: 500px; border-left: 5px solid #4caf50; }
    .status-ok { color: #4caf50; }
    .status-warn { color: #ff5252; animation: blink 1s infinite; }
    
    .location-text { font-size: 1.5rem; color: #4fc3f7; font-weight: 600; }
    
    canvas { 
      background: #121212; 
      border-radius: 8px; 
      width: 100%; 
      height: 120px; 
      border: 1px solid #2a2a2a;
    }
  </style>
</head>
<body>
  <h1>&#9728; Solar Panel Monitor</h1>
  
  <div class="dashboard">
    <!-- Temperature Card -->
    <div class="card">
      <div class="label">Temperature</div>
      <div class="value-box">
        <div id="temp" class="value">--</div><div class="unit">&deg;C</div>
      </div>
      <canvas id="tempChart"></canvas>
    </div>
    
    <!-- Light Card -->
    <div class="card">
      <div class="label">Irradiance</div>
      <div class="value-box">
        <div id="light" class="value">--</div><div class="unit">%</div>
      </div>
      <canvas id="lightChart"></canvas>
    </div>
  </div>

  <br>
  
  <div class="dashboard">
    <!-- Location Card (New) -->
    <div class="card" style="border-top: 5px solid #4fc3f7;">
      <div class="label">Simulated Location</div>
      <div id="loc" class="value location-text">--</div>
    </div>

    <!-- Status Card -->
    <div class="card status-card" id="statusCard">
      <div class="label">System Status</div>
      <div id="status" class="value" style="font-size: 1.5rem;">--</div>
    </div>
  </div>

<script>
// --- Graph Logic ---
const maxPoints = 60; 
let tempData = new Array(maxPoints).fill(0);
let lightData = new Array(maxPoints).fill(0);

function drawChart(id, data, color, min, max) {
  const canvas = document.getElementById(id);
  const ctx = canvas.getContext('2d');
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  canvas.width = rect.width * dpr;
  canvas.height = rect.height * dpr;
  ctx.scale(dpr, dpr);
  const w = rect.width; const h = rect.height;
  
  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = '#222'; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(0, h/2); ctx.lineTo(w, h/2); ctx.stroke();

  ctx.beginPath();
  const step = w / (maxPoints - 1);
  data.forEach((val, i) => {
    let normalized = (val - min) / (max - min); 
    if(normalized < 0) normalized = 0; if(normalized > 1) normalized = 1;
    const y = h - (normalized * h);
    if(i === 0) ctx.moveTo(0, y); else ctx.lineTo(i * step, y);
  });
  ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.stroke();
  ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.fillStyle = color + "22"; ctx.fill();
}

// Update Loop
setInterval(function() {
  fetch("/readings")
    .then(response => response.json())
    .then(data => {
      document.getElementById("temp").innerHTML = data.temp.toFixed(1);
      document.getElementById("light").innerHTML = data.light;
      document.getElementById("status").innerHTML = data.status;
      document.getElementById("loc").innerHTML = data.location; // Update Location
      
      tempData.push(data.temp); tempData.shift();
      lightData.push(data.light); lightData.shift();
      drawChart("tempChart", tempData, "#ff5252", 0, 50);
      drawChart("lightChart", lightData, "#fbc02d", 0, 100);
      
      const statusCard = document.getElementById("statusCard");
      if(data.status.includes("WARNING")) {
        document.getElementById("status").className = "value status-warn";
        statusCard.style.borderLeftColor = "#ff5252";
      } else {
        document.getElementById("status").className = "value status-ok";
        statusCard.style.borderLeftColor = "#4caf50";
      }
    });
}, 1000);
</script>
</body>
</html>
)rawliteral";

// --- Server Handlers ---
void handleRoot() { server.send(200, "text/html", index_html); }

void handleReadings() {
  String json = "{";
  json += "\"temp\":" + String(currentTemp) + ",";
  json += "\"light\":" + String(currentLightPercent) + ",";
  json += "\"location\":\"" + currentLocation + "\","; // Send Location
  json += "\"status\":\"" + currentStatus + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// Redirect unknown paths to root
void handleNotFound() {
  server.sendHeader("Location", "http://solar.panel", true);
  server.send(302, "text/plain", "");
}

void determineLocation(float temp, int light) {
  // Logic to simulate locations based on sensor readings
  if (temp > 35) {
    if (light > 80)
      currentLocation = "Sahara Desert";
    else
      currentLocation = "Death Valley (Night)";
  } else if (temp > 25) {
    if (light > 70)
      currentLocation = "Spain / Florida";
    else
      currentLocation = "Brazil (Rainforest)";
  } else if (temp > 15) {
    if (light > 50)
      currentLocation = "France / Italy";
    else
      currentLocation = "UK (Summer)";
  } else if (temp > 5) {
    if (light > 40)
      currentLocation = "London (Cloudy)";
    else
      currentLocation = "Seattle / Bergen";
  } else {
    if (light > 80)
      currentLocation = "Antarctica (Sunny)";
    else
      currentLocation = "Arctic Circle";
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);

  // ADC Config
  analogReadResolution(12);

  // DFPlayer Config
  dfSerial.begin(9600, SERIAL_8N1, dfplayerRX, dfplayerTX);
  Serial.println("Initializing DFPlayer Mini... Waiting 3 seconds for it to boot.");
  
  // The DFPlayer takes a few seconds to boot up and read the SD Card
  // If we talk to it too fast, it will error out!
  delay(3000); 

  if (!myDFPlayer.begin(dfSerial)) {
    Serial.println("DFPlayer error! Check RX/TX wiring or SD Card.");
  } else {
    Serial.println("DFPlayer online.");
    myDFPlayer.volume(30); // Max Volume (0-30)
    // Removed unconditional loop(1) from here!
  }

// Buzzer Config (Passive - Needs PWM)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(buzzerPin, buzzerFreq, buzzerResolution);
#else
  ledcSetup(buzzerChannel, buzzerFreq, buzzerResolution);
  ledcAttachPin(buzzerPin, buzzerChannel);
#endif

  // WiFi Config (SoftAP)
  Serial.println("\n--- Starting Solar Panel Monitor ---");

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);

  Serial.print("Setting up WiFi AP: ");
  Serial.println(ssid);
  WiFi.softAP(ssid);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // --- DNS Server Config ---
  dnsServer.start(53, "*", IP);
  Serial.println("DNS Server started");

  // Server Config
  server.on("/", handleRoot);
  server.on("/readings", handleReadings);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web Server Started.");
}

// --- Loop ---
unsigned long lastTime = 0;
const long interval = 1000;

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if (millis() - lastTime > interval) {
    lastTime = millis();

    // Read Thermistor
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
    currentTemp = steinhart;

    // Read Photoresistor
    int adcLDR = analogRead(photoresistorPin);
    long lightPercent = map(adcLDR, 4095, 0, 0, 100);
    if (lightPercent < 0)
      lightPercent = 0;
    if (lightPercent > 100)
      lightPercent = 100;
    currentLightPercent = lightPercent;

    // Determine Location Simulation
    determineLocation(currentTemp, currentLightPercent);

    // --- Logic Checks ---
    if (currentTemp > TEMP_THRESHOLD) {
      currentStatus = "WARNING: HIGH TEMP!";

// Select PWM function based on Core version
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(buzzerPin, 128); // 50% duty
#else
      ledcWrite(buzzerChannel, 128);
#endif

      // Start MP3 Alarm if not already playing
      if (!isPlayingMP3) {
        myDFPlayer.loop(1);
        isPlayingMP3 = true;
      }

    } else {
      currentStatus = "System Normal";

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(buzzerPin, 0); // 0% duty
#else
      ledcWrite(buzzerChannel, 0);
#endif

      // Stop MP3 Alarm if temperature drops
      if (isPlayingMP3) {
        myDFPlayer.pause();
        isPlayingMP3 = false;
      }
    }

    // Serial Debug
    Serial.print("Web Clients: ");
    Serial.print(WiFi.softAPgetStationNum());
    Serial.print(" | Temp: ");
    Serial.print(currentTemp, 1);
    Serial.print(" C | Light: ");
    Serial.print(currentLightPercent);
    Serial.print("% | Location: ");
    Serial.print(currentLocation);
    Serial.print(" | Status: ");
    Serial.println(currentStatus);
  }
}
