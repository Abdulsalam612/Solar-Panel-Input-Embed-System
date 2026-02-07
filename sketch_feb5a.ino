/*
 * ESP32 Solar Panel Monitor
 * Reads:
 * 1. Temperature from Thermistor (Pin A1)
 * 2. Irradiance (Light) from Photoresistor (Pin A2)
 *
 * Hardware Connection:
 * - Thermistor Analog Pin -> A1 (GPIO 25)
 * - Photoresistor Analog Pin -> GPIO 34 (Labelled "34" or "A6")
 * - VCC -> 3.3V
 * - GND -> GND
 */

// --- Thermistor Settings ---
// If A1 is not defined, use GPIO 25
#if defined(A1)
const int thermistorPin = A1;
#else
const int thermistorPin = 25;
#endif

// Thermistor Parameters
const float SERIES_RESISTOR = 10000.0;
const float NOMINAL_RESISTANCE = 10000.0;
const float NOMINAL_TEMPERATURE = 25.0;
const float B_COEFFICIENT = 3950.0;
const float ADC_MAX = 4095.0;
const float VCC = 3.3;

// Thresholds
const float TEMP_THRESHOLD =
    30.0; // Warning threshold in Celsius (Set low for testing)

// --- Photoresistor Settings ---
// "A2" can be ambiguous on different ESP32 boards.
// We will use GPIO 34 (often labeled "34" or "A6" on headers) to be sure.
// GPIO 34 is an Input-Only pin, perfect for sensors.
const int photoresistorPin = 34;

void setup() {
  Serial.begin(115200);

  // Configure ADC resolution to 12-bit (0-4095)
  analogReadResolution(12);

  Serial.println("--- Solar Panel Monitor Started ---");
  Serial.print("Thermistor Pin: ");
  Serial.println(thermistorPin);
  Serial.print("Photoresistor Pin: ");
  Serial.println(photoresistorPin);
  Serial.print("Temp Threshold: ");
  Serial.print(TEMP_THRESHOLD);
  Serial.println(" C");
}

void loop() {
  // ==========================
  // 1. READ TEMPERATURE
  // ==========================
  int adcTherm = analogRead(thermistorPin);

  float resistance = 0;
  if (adcTherm == 0) {
    resistance = 1000000;
  } else if (adcTherm >= ADC_MAX) {
    resistance = 0;
  } else {
    resistance = SERIES_RESISTOR / ((ADC_MAX / (float)adcTherm) - 1);
  }

  float steinhart;
  steinhart = resistance / NOMINAL_RESISTANCE;
  steinhart = log(steinhart);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;

  // ==========================
  // 2. READ IRRADIANCE
  // ==========================
  int adcLDR = analogRead(photoresistorPin);

  // User reports: Bright = 0V (ADC ~0), Dark = 3.3V (ADC ~4095)
  // We want: Bright = 100%, Dark = 0%
  // So we map: 4095 -> 0, 0 -> 100
  long lightPercent = map(adcLDR, 4095, 0, 0, 100);

  // Clamp values to 0-100 range in case of slight overflow
  if (lightPercent < 0)
    lightPercent = 0;
  if (lightPercent > 100)
    lightPercent = 100;

  // ==========================
  // 3. LOGIC CHECKS
  // ==========================
  String statusMsg = "OK";

  if (steinhart > TEMP_THRESHOLD) {
    statusMsg = "WARNING: HIGH TEMP!";

    // --- FAN CONTROL LOGIC (Placeholder) ---
    // Example: digitalWrite(FAN_PIN, HIGH);
    // Serial.println("Fan ON");
  } else {
    // Example: digitalWrite(FAN_PIN, LOW);
  }

  // ==========================
  // 4. SERIAL OUTPUT
  // ==========================
  // Format: [Temp C] [Light %] [Status]
  Serial.print("Temp: ");
  Serial.print(steinhart, 1);
  Serial.print(" C\t");

  Serial.print("Light: ");
  Serial.print(lightPercent);
  Serial.print("%\t");

  Serial.println(statusMsg);

  delay(1000);
}