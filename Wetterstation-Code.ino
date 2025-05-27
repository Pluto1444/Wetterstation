#define BLYNK_TEMPLATE_ID "TMPLxxx"
#define BLYNK_TEMPLATE_NAME "Wetterstation"
#define BLYNK_AUTH_TOKEN "CJUZrd3I0Fc7wZ4LpHrrSd8XFmNvAUqL"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include "DHT.h"
#include "time.h"
#include <Adafruit_NeoPixel.h>
#include <BlynkSimpleEsp32.h> 
#include <WiFiClientSecure.h>


// --- RGB LED ---
#define NEOPIXEL_PIN 8
#define NUMPIXELS 1
Adafruit_NeoPixel pixel(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
bool rgbEnabled = true;
bool sleepMode = false;

// --- WLAN & Server ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const char* hostName = "http://192.168.0.89";
const char* pathName = "/insert_temp.php";

// --- Blynk ---


// Discord Webhook Pfad
const char* discordHost = "discord.com";
const char* discordPath = "/api/webhooks/1376515388275294320/wURRkvGEHb2kDzdUseKzS11mEl0iEnDX-j2P7RLLwKq_KZ9Mu_fN79LNisdmgA8do72V";

WiFiManager wifiManager;
AsyncWebServer server(80);

// --- DHT Sensor ---
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- Display ---
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,
  5, // SCL
  4, // SDA
  U8X8_PIN_NONE
);

// --- LED & Shock Sensor ---
const int ledPin = 2;
#define SHOCK_PIN 10
volatile bool shockDetected = false;
unsigned long shockDetectedMillis = 0;
const unsigned long shockDisplayDuration = 10000;

// --- Variablen ---
float lastTemp = NAN;
float lastHum = NAN;
bool ledEnabled = true;
unsigned long lastMeasurement = 0;
const unsigned long measurementInterval = 10000;

// Discord Sendzähler
uint8_t discordCounter = 0;

// --- URL-Encoding ---
String urlEncode(const String& str) {
  String encoded = "";
  char buf[4];
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c)) encoded += c;
    else if (c == ' ') encoded += "%20";
    else if (c == ':') encoded += "%3A";
    else if (c == '-') encoded += "-";
    else {
      sprintf(buf, "%%%02X", c);
      encoded += buf;
    }
  }
  return encoded;
}

// --- LED Statussteuerung ---
void setStatusLED(uint8_t status) {
  if (!ledEnabled) {
    digitalWrite(ledPin, HIGH);
    return;
  }

  static unsigned long prevMillis = 0;
  static bool state = false;
  unsigned long currMillis = millis();

  switch (status) {
    case 0:
      if (currMillis - prevMillis >= 250) {
        prevMillis = currMillis;
        state = !state;
        digitalWrite(ledPin, state ? LOW : HIGH);
      }
      break;
    case 1:
      digitalWrite(ledPin, LOW);
      break;
    case 2:
      if (currMillis - prevMillis >= 1000) {
        prevMillis = currMillis;
        state = !state;
        digitalWrite(ledPin, state ? LOW : HIGH);
      }
      break;
    default:
      digitalWrite(ledPin, HIGH);
      break;
  }
}

// --- Shock Interrupt ---
void IRAM_ATTR handleShock() {
  shockDetected = true;
  shockDetectedMillis = millis();
}

// --- Daten senden ---
void sendDataToServer(float temperature, float humidity, const String& timestamp) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(hostName) + pathName +
                 "?temperature=" + String(temperature, 1) +
                 "&humidity=" + String(humidity, 1) +
                 "&timestamp=" + urlEncode(timestamp);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("HTTP-Code: %d\n", httpCode);
      Serial.println(http.getString());
    } else {
      Serial.printf("HTTP Fehler: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Kein WLAN verbunden!");
  }
}

// --- Discord senden ---
void sendToDiscord(float temperature, float humidity, const String& timestamp) {
  WiFiClientSecure client;
  client.setInsecure();
  if (!client.connect(discordHost, 443)) {
    Serial.println("❌ Verbindung zu Discord fehlgeschlagen.");
    return;
  }

  String message = "🌡 Temperatur: " + String(temperature, 1) + " °C\\n💧 Feuchtigkeit: " + String(humidity, 1) + " %\\n🕒 " + timestamp;
  String payload = "{\"content\":\"" + message + "\"}";

  String request =
    "POST " + String(discordPath) + " HTTP/1.1\r\n"
    "Host: discord.com\r\n"
    "User-Agent: ESP32\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: " + String(payload.length()) + "\r\n"
    "Connection: close\r\n\r\n" +
    payload;

  client.print(request);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  Serial.println("✅ Discord-Nachricht gesendet.");
  client.stop();
}

// --- Display aktualisieren ---
void updateDisplay(struct tm timeinfo) {
  if (sleepMode) return;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%d.%m.%Y", &timeinfo);
  u8g2.drawStr(0, 12, timeStr);

  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  u8g2.drawStr(0, 28, timeStr);

  String tempStr = "Temp: " + String(lastTemp, 1) + " C";
  String humStr = "Feuchte: " + String(lastHum, 1) + " %";
  u8g2.drawStr(0, 44, tempStr.c_str());
  u8g2.drawStr(0, 60, humStr.c_str());

  if (millis() - shockDetectedMillis <= shockDisplayDuration) {
    u8g2.drawDisc(120, 5, 3); // kleiner Kreis oben rechts
  }

  u8g2.sendBuffer();
}

void setPixelColor(uint8_t r, uint8_t g, uint8_t b) {
  if (!rgbEnabled) {
    pixel.clear();
    pixel.show();
    return;
  }

  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

// --- Webinterface ---
String createHTML() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char datetime[30];
  strftime(datetime, sizeof(datetime), "%d.%m.%Y %H:%M:%S", &timeinfo);

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 Wetterstation</title>
  <style>
    body {
      font-family: 'Segoe UI', sans-serif;
      background: linear-gradient(to right, #232526, #414345);
      color: #fff;
      text-align: center;
      padding: 20px;
    }
    h1 { font-size: 2.5em; }
    .card {
      background: #333;
      border-radius: 15px;
      padding: 20px;
      max-width: 400px;
      margin: auto;
      box-shadow: 0 0 15px rgba(0,0,0,0.5);
    }
    .value { font-size: 1.5em; margin: 10px 0; }
    .btn {
      padding: 10px 20px;
      font-size: 1em;
      border: none;
      border-radius: 10px;
      background: #4CAF50;
      color: white;
      cursor: pointer;
      margin-top: 20px;
      transition: background 0.3s ease;
    }
    .btn:hover { background: #45a049; }
  </style>
</head>
<body>
  <div class="card">
    <h1>ESP32 Wetterstation</h1>
    <div class="value">🌡 Temperatur: <strong id="tempValue">--</strong> &deg;C</div>
    <div class="value">💧 Feuchtigkeit: <strong id="humValue">--</strong> %</div>
    <div class="value">📅 Datum & Uhrzeit:<br><strong id="datetime">--</strong></div>
    <div class="value">Status-Anzeige: <strong id="rgbStatus">--</strong></div>
    <button class="btn" onclick="toggleRGB()">RGB-LED umschalten</button>
    <button class="btn" onclick="toggleSleep()">Sleep Mode an/aus</button>
  </div>
  <script>
    async function fetchData() {
      const res = await fetch('/data');
      if(res.ok) {
        const json = await res.json();
        document.getElementById('tempValue').textContent = json.temperature.toFixed(1);
        document.getElementById('humValue').textContent = json.humidity.toFixed(1);
        document.getElementById('datetime').textContent = json.datetime;
        document.getElementById('rgbStatus').textContent = json.rgb ? 'An' : 'Aus';
      }
    }
    function toggleRGB() {
      fetch('/rgb').then(fetchData);
    }
    function toggleSleep() {
      fetch('/sleep').then(fetchData);
    }
    setInterval(fetchData, 5000);
    window.onload = fetchData;
  </script>
</body>
</html>
)rawliteral";

  return html;
}

void handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", createHTML());
}

void handleData(AsyncWebServerRequest *request) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  String datetime;
  char dtBuff[30];
  strftime(dtBuff, sizeof(dtBuff), "%d.%m.%Y %H:%M:%S", &timeinfo);
  datetime = dtBuff;

  String json = "{\"temperature\":" + String(lastTemp, 1) +
                ",\"humidity\":" + String(lastHum, 1) +
                ",\"datetime\":\"" + datetime +
                "\",\"rgb\":" + (rgbEnabled ? "true" : "false") + "}";

  request->send(200, "application/json", json);
}

void handleRGB(AsyncWebServerRequest *request) {
  rgbEnabled = !rgbEnabled;
  if (!rgbEnabled) {
    pixel.clear();
    pixel.show();
  }
  request->send(200, "text/plain", rgbEnabled ? "RGB AN" : "RGB AUS");
}

void handleSleep(AsyncWebServerRequest *request) {
  sleepMode = !sleepMode;
  
  if (sleepMode) {
    u8g2.clearBuffer();
    u8g2.sendBuffer(); // Display ausschalten
    setPixelColor(0, 0, 0); // RGB LED aus
    Serial.println("[/sleep] Sleep Mode toggled: AN");
  } else {
    Serial.println("[/sleep] Sleep Mode toggled: AUS");
    // Display wird automatisch im nächsten Loop-Zyklus aktualisiert
  }

  request->send(200, "text/plain", sleepMode ? "Sleep Mode AN" : "Sleep Mode AUS");
}



// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  dht.begin();

  pixel.begin();
  pixel.clear();
  pixel.show();

  wifiManager.setTimeout(180);
  wifiManager.autoConnect("ESP32-AccessPoint");
  Serial.println("WLAN verbunden");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  u8g2.begin();
  u8g2.enableUTF8Print();

  attachInterrupt(digitalPinToInterrupt(SHOCK_PIN), handleShock, RISING);

  // Webserver-Routen
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/rgb", HTTP_GET, handleRGB);
  server.on("/sleep", HTTP_GET, handleSleep);

  server.begin();

  // Blynk starten
  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());

  Blynk.run();

  lastMeasurement = millis();
}

// --- Loop ---
void loop() {
  Blynk.run();
  yield();

  // Zeit holen
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Schockanzeige ausschalten nach Dauer
  if (shockDetected && millis() - shockDetectedMillis > shockDisplayDuration) {
    shockDetected = false;
  }

  if (!sleepMode) {
  if (shockDetected) {
    setPixelColor(255, 0, 0);
  } else if (rgbEnabled) {
    setPixelColor(0, 255, 0);
  } else {
    setPixelColor(0, 0, 0);
  }

  updateDisplay(timeinfo);
}



  // Neue Messung
  if (millis() - lastMeasurement >= measurementInterval) {
    lastMeasurement = millis();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
      lastTemp = temperature;
      lastHum = humidity;

      String timestamp;
      char tsBuff[30];
      strftime(tsBuff, sizeof(tsBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
      timestamp = tsBuff;

      sendDataToServer(temperature, humidity, timestamp);

      // Blynk Werte setzen
      Blynk.virtualWrite(V0, temperature);
      Blynk.virtualWrite(V1, humidity);

      // Discord alle 3 Messungen
      discordCounter++;
      if (discordCounter >= 30) {
        sendToDiscord(temperature, humidity, timestamp);
        discordCounter = 0;
      }

      Serial.printf("Messung: %.1f C, %.1f %%\n", temperature, humidity);
    } else {
      Serial.println("Fehler beim Lesen des DHT11");
    }
  }

  // Status LED blinken
  setStatusLED(shockDetected ? 1 : (WiFi.status() == WL_CONNECTED ? 0 : 2));

  // NeoPixel Farbe (rot wenn Shock, sonst grün wenn rgb an, sonst aus)
  
  // Display aktualisieren
  updateDisplay(timeinfo);
}