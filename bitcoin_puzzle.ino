// BitcoinPuzzle by Uefi1
// https://github.com/Uefi1/esp32_Bitcoin_Puzzle
// 2025


#include "Bitcoin.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "bootloader_random.h"

// LED pin
#define LED_PIN 2

// WiFi настройки
const char* WIFI_SSID = "BitcoinPuzzle";
const char* WIFI_PASSWORD = "BitPuzzle";

// Диапазоны поиска приватного ключа в HEX-формате
String MIN_HEX = "0000000000000000000000008000000000000000000000000000000000000000";
String MAX_HEX = "000000000000000000000000ffffffffffffffffffffffffffffffffffffffff";

// Целевой адрес
String TARGET_ADDRESS = "1NBC8uXJy1GiJ6drkiZa1WuKn51ps7EPTv";

// Диапазоны приватных ключей
uint8_t MIN_KEY[32];
uint8_t MAX_KEY[32];

// Глобальные переменные
AsyncWebServer server(80);
Preferences preferences;
bool keyFound = false;
bool searchRunning = false;
//TaskHandle_t searchTaskHandle = NULL;
String foundPrivateKey = "";
String foundPublicAddress = "";

// Последние сгенерированные ключи
String lastPrivateKey = "";
String lastAddress = "";

// Конвертация HEX-строки в массив байтов
inline void hexToBytes(const String& hex, uint8_t* bytes, size_t length) {
  for (size_t i = 0; i < length; i++) {
    char byteStr[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
    bytes[i] = (uint8_t)strtol(byteStr, NULL, 16);
  }
}

// Конвертация массива байтов в HEX-строку
inline String bytesToHexString(const uint8_t* data, size_t length) {
  String hex = "";
  for (size_t i = 0; i < length; i++) {
    char hexByte[3];
    sprintf(hexByte, "%02X", data[i]);
    hex += hexByte;
  }
  return hex;
}

// Генерация случайного приватного ключа в заданном диапазоне
inline void generateRandomKey(uint8_t* keyBuffer) {
  bool inRange = true;
  for (int i = 0; i < 32; i++) {
    uint8_t minByte = MIN_KEY[i];
    uint8_t maxByte = MAX_KEY[i];
    randomSeed(analogRead(34) ^ esp_random() ^ analogRead(0));
    if (inRange) {
      if (minByte == maxByte) {
        keyBuffer[i] = minByte;
      } else {
        keyBuffer[i] = minByte + random(maxByte - minByte + 1);
      }
      inRange = (keyBuffer[i] == minByte);
    } else {
      keyBuffer[i] = random(256);
    }
  }
}

// Конвертация приватного ключа в Bitcoin-адрес
inline String privateKeyToAddress(const uint8_t* privateKey) {
  PrivateKey key(privateKey, 32);
  PublicKey publicKey = key.publicKey();
  return publicKey.address();
}

// Generate HTML content
inline String getHtmlContent() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>Bitcoin Puzzle</title>";
  html += "</head><body>";
  html += "<h1>Bitcoin Puzzle</h1>";

  if (keyFound) {
    html += "<h2 style='color:green'>🔑 Key Found!</h2>";
    html += "<p>Private Key: " + foundPrivateKey + "</p>";
    html += "<p>Address: " + foundPublicAddress + "</p>";
  } else {
    html += "<p>Last Private Key: " + lastPrivateKey + "</p>";
    html += "<p>Last Address: " + lastAddress + "</p>";
  }

  html += "<h3>Current Ranges</h3>";
  html += "<p>Min Hex: <input type='text' id='minHex' value='" + MIN_HEX + "' size='70'></p>";
  html += "<p>Max Hex: <input type='text' id='maxHex' value='" + MAX_HEX + "' size='70'></p>";
  html += "<p>Target Address: <input type='text' id='targetAddress' value='" + TARGET_ADDRESS + "' size='40'></p>";

  if (searchRunning) {
    html += "<button onclick='stopSearch()'>Stop Search</button>";
  } else {
    html += "<button onclick='startSearch()'>Start Search</button>";
  }

  html += "<script>";
  html += "function startSearch() {";
  html += "  var minHex = document.getElementById('minHex').value.trim();";
  html += "  var maxHex = document.getElementById('maxHex').value.trim();";
  html += "  var targetAddress = document.getElementById('targetAddress').value.trim();";
  html += "  fetch('/startSearch?minHex=' + minHex + '&maxHex=' + maxHex + '&targetAddress=' + targetAddress)";
  html += "    .then(() => location.reload());";
  html += "}";

  html += "function stopSearch() {";
  html += "  fetch('/stopSearch')";
  html += "    .then(() => location.reload());";
  html += "}";
  html += "</script>";
  html += "</body></html>";

  return html;
}

// Основная функция поиска
inline void Execute(void* pvParameters) {
  uint8_t privateKey[32];
  while (true) {
    if (searchRunning) {
      generateRandomKey(privateKey);
      String currentAddress = privateKeyToAddress(privateKey);
      lastPrivateKey = bytesToHexString(privateKey, 32);
      lastAddress = currentAddress;
      if (currentAddress.equalsIgnoreCase(TARGET_ADDRESS)) {
        searchRunning = false;
        keyFound = true;
        foundPrivateKey = bytesToHexString(privateKey, 32);
        foundPublicAddress = currentAddress;

        preferences.putString("foundPrivateKey", foundPrivateKey);
        preferences.putString("foundPublicAddr", foundPublicAddress);
        preferences.putBool("keyFound", keyFound);
        preferences.putBool("searchRunning", searchRunning);
      }
    } else if (keyFound) {
      digitalWrite(LED_PIN, (millis() / 500) % 2);
    }
  }
}

void setup() {
  preferences.begin("bitcoin_puzzle", false);
  // Load saved values
  MIN_HEX = preferences.getString("minHex", MIN_HEX);
  MAX_HEX = preferences.getString("maxHex", MAX_HEX);
  TARGET_ADDRESS = preferences.getString("targetAddress", TARGET_ADDRESS);
  keyFound = preferences.getBool("keyFound", false);
  searchRunning = preferences.getBool("searchRunning", false);
  foundPrivateKey = preferences.getString("foundPrivateKey", "");
  foundPublicAddress = preferences.getString("foundPublicAddr", "");

  hexToBytes(MIN_HEX, MIN_KEY, 32);
  hexToBytes(MAX_HEX, MAX_KEY, 32);

  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", getHtmlContent());
  });

  // Route for starting search
  server.on("/startSearch", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (request->hasParam("minHex") && request->hasParam("maxHex") && request->hasParam("targetAddress")) {
      MIN_HEX = request->getParam("minHex")->value();
      MAX_HEX = request->getParam("maxHex")->value();
      TARGET_ADDRESS = request->getParam("targetAddress")->value();

      hexToBytes(MIN_HEX, MIN_KEY, 32);
      hexToBytes(MAX_HEX, MAX_KEY, 32);

      searchRunning = true;
      keyFound = false;
      lastPrivateKey = "";  // Сбрасываем последние значения
      lastAddress = "";

      preferences.putString("minHex", MIN_HEX);
      preferences.putString("maxHex", MAX_HEX);
      preferences.putString("targetAddress", TARGET_ADDRESS);
      preferences.putBool("searchRunning", searchRunning);
      preferences.putBool("keyFound", keyFound);
    }
    request->send(200, "text/plain", "Search Started");
  });

  // Route for stopping search
  server.on("/stopSearch", HTTP_GET, [](AsyncWebServerRequest* request) {
    searchRunning = false;
    preferences.putBool("searchRunning", searchRunning);
    request->send(200, "text/plain", "Search Stopped");
  });

  server.begin();

  bootloader_random_enable();
  xTaskCreatePinnedToCore(Execute, "ExecuteTask", 8192, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}
