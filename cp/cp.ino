// =========================
// INCLUDES DE BIBLIOTECAS
// =========================

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// =========================
// CONFIGURACAO DO WIFI
// =========================


#define ssid "Wokwi-GUEST";
#define password "";

// =========================
// Variaveis
// ========================

#define BTN1 33
#define BTN2 25
#define LED1 26
#define LED2 27
#define DHTPIN 18
#define DHTTYPE DHT22

WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);


bool led1State = false;
bool led2State = false;

float temp = 0;
float hum = 0;

float extTemp = 0;
float extHum = 0;
bool weatherValid = false;

int lcdMode = 0;

unsigned long lastSensorRead = 0;
unsigned long lastBtn1 = 0;
unsigned long lastBtn2 = 0;

// =========================
// funções de configuração e auxiliares
// ========================


// FUNÇÕES
void readDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    temp = t;
    hum = h;
  }
}

void fetchWeather() {
  HTTPClient http;
  http.begin("http://api.open-meteo.com/v1/forecast?latitude=-23.55&longitude=-46.63&current_weather=true");

  int httpCode = http.GET();

  if (httpCode == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());

    extTemp = doc["current_weather"]["temperature"];
    extHum = 0;
    weatherValid = true;
  } else {
    weatherValid = false;
  }

  http.end();
}

void updateLCD() {
  lcd.clear();

  switch (lcdMode) {
    case 0:
      lcd.setCursor(0,0);
      lcd.print(WiFi.localIP());
      lcd.setCursor(0,1);
      lcd.print(ssid);
      break;

    case 1:
      lcd.print("T:");
      lcd.print(temp);
      lcd.setCursor(0,1);
      lcd.print("H:");
      lcd.print(hum);
      break;

    case 2:
      lcd.print("L1:");
      lcd.print(led1State ? "ON":"OFF");
      lcd.setCursor(0,1);
      lcd.print("L2:");
      lcd.print(led2State ? "ON":"OFF");
      break;

    case 3:
      lcd.print("EXT:");
      lcd.print(extTemp);
      break;
  }
}


void apiTempHum() {
  readDHT();

  DynamicJsonDocument doc(256);
  doc["source"] = "DHT22";
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["status"] = "ok";

  String res;
  serializeJson(doc, res);

  server.send(200, "application/json", res);
}


void apiLedGet(int led) {
  bool state = (led == 1) ? led1State : led2State;

  DynamicJsonDocument doc(128);
  doc["led"] = led;
  doc["state"] = state ? "on" : "off";
  doc["status"] = "ok";

  String res;
  serializeJson(doc, res);
  server.send(200, "application/json", res);
}

void apiLedPost(int led) {
  bool *state = (led == 1) ? &led1State : &led2State;

  *state = !(*state);
  digitalWrite((led == 1) ? LED1 : LED2, *state);

  apiLedGet(led);
}

void apiWeather() {
  fetchWeather();

  DynamicJsonDocument doc(256);
  doc["source"] = "Open-Meteo";
  doc["temperature"] = extTemp;
  doc["humidity"] = extHum;
  doc["status"] = weatherValid ? "ok" : "error";

  String res;
  serializeJson(doc, res);
  server.send(200, "application/json", res);
}

void apiStatus() {
  DynamicJsonDocument doc(512);

  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = ssid;

  doc["leds"]["led1"]["state"] = led1State ? "on" : "off";
  doc["leds"]["led2"]["state"] = led2State ? "on" : "off";

  doc["sensor"]["temperature"] = temp;
  doc["sensor"]["humidity"] = hum;
  doc["sensor"]["status"] = "ok";

  doc["weather"]["temperature"] = extTemp;
  doc["weather"]["humidity"] = extHum;
  doc["weather"]["status"] = weatherValid ? "ok" : "error";

  doc["status"] = "ok";

  String res;
  serializeJson(doc, res);
  server.send(200, "application/json", res);
}

void notFound() {
  DynamicJsonDocument doc(128);
  doc["status"] = "error";
  doc["message"] = "Rota não encontrada";

  String res;
  serializeJson(doc, res);
  server.send(404, "application/json", res);
}

void setup() {
  Serial.begin(115200);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  dht.begin();
  lcd.init();
  lcd.backlight();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {}

  // ROTAS
  server.on("/", handleRoot);

  server.on("/api/sensor/temphum", HTTP_GET, apiTempHum);

  server.on("/api/led/1", HTTP_GET, [](){ apiLedGet(1); });
  server.on("/api/led/1", HTTP_POST, [](){ apiLedPost(1); });

  server.on("/api/led/2", HTTP_GET, [](){ apiLedGet(2); });
  server.on("/api/led/2", HTTP_POST, [](){ apiLedPost(2); });

  server.on("/api/weather", HTTP_GET, apiWeather);
  server.on("/api/status", HTTP_GET, apiStatus);

  server.onNotFound([]() {
    sendJsonError("Rota nao encontrada", 404);
  });
 
  server.enableCORS();
  server.begin();
  Serial.println("HTTP server iniciado");

  updateLCD();
}



// =========================
// função principal loop no final do código
// ========================

void loop() {
  server.handleClient();

  // leitura periódica
  if (millis() - lastSensorRead > 2000) {
    readDHT();
    lastSensorRead = millis();
  }

  // BTN1 - clima
  if (!digitalRead(BTN1) && millis() - lastBtn1 > 300) {
    fetchWeather();
    updateLCD();
    lastBtn1 = millis();
  }

  // BTN2 - LCD
  if (!digitalRead(BTN2) && millis() - lastBtn2 > 300) {
    lcdMode = (lcdMode + 1) % 4;
    updateLCD();
    lastBtn2 = millis();
  }
}
