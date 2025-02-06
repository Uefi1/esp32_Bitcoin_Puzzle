// BitcoinPuzzle by Uefi1
// https://github.com/Uefi1/esp32_Bitcoin_Puzzle
// 2025

#include "Bitcoin.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "bootloader_random.h"

// LED pin
#define LED_PIN 2

// WiFi settings
const char* WIFI_SSID = "BitcoinPuzzle";
const char* WIFI_PASSWORD = "BitPuzzle";

// Ranges for searching the private key in hex format
String MIN_HEX = "0000000000000000000000008000000000000000000000000000000000000000";
String MAX_HEX = "000000000000000000000000ffffffffffffffffffffffffffffffffffffffff";

// Target address
String TARGET_ADDRESS = "1NBC8uXJy1GiJ6drkiZa1WuKn51ps7EPTv";

// Private key ranges
uint8_t MIN_KEY[32];
uint8_t MAX_KEY[32];

// Global variables
WebServer server(80);
Preferences preferences; // Preferences object
bool keyFound = false;
bool searchRunning = false;
uint8_t currentPrivateKey[32];
String foundPrivateKey = "";
String foundPublicAddress = "";

// Convert hex string to byte array
inline void hexToBytes(const String& hex, uint8_t* bytes, size_t length) {
    for (size_t i = 0; i < length; i++) {
        char byteStr[3] = {hex[i*2], hex[i*2+1], 0};
        bytes[i] = (uint8_t)strtol(byteStr, NULL, 16);
    }
}

// Helper function: bytes to hex string
inline String bytesToHexString(const uint8_t* data, size_t length) {
    String hex = "";
    for (size_t i = 0; i < length; i++) {
        char hexByte[3];
        sprintf(hexByte, "%02X", data[i]);
        hex += hexByte;
    }
    return hex;
}

// Function to convert a string to lowercase
inline String toLowerCase(const String& str) {
    String lowerStr = "";
    for (size_t i = 0; i < str.length(); i++) {
        lowerStr += tolower(str.charAt(i));
    }
    return lowerStr;
}

// Function to generate a random private key within the range
inline void generateRandomKey() {
    bool inRange = true;  // Флаг, чтобы пока что придерживаться границ
    for (int i = 0; i < 32; i++) {  
        uint8_t minByte = MIN_KEY[i];
        uint8_t maxByte = MAX_KEY[i];
        randomSeed(esp_random() ^ analogRead(A0));

        if (inRange) {  
            if (minByte == maxByte) {
                currentPrivateKey[i] = minByte;  // Если min и max одинаковые, ставим фиксированное значение
            } else {
                currentPrivateKey[i] = minByte + random(maxByte - minByte + 1);
            }
            inRange = (currentPrivateKey[i] == minByte);  // Если взяли minByte, продолжаем ограничивать
        } else {
            currentPrivateKey[i] = random(256);  // Полностью случайный байт
        }
    }
}

// Convert private key to public address
inline String privateKeyToAddress() {
    PrivateKey privateKey(currentPrivateKey, 32);
    PublicKey publicKey = privateKey.publicKey();
    return publicKey.address();
}

// Root page handler with form inputs
inline void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>"; // Добавляем мета-тег для кодировки
    html += "<title>Bitcoin Puzzle</title>"; // Можно добавить заголовок страницы
    html += "</head><body>";
    html += "<h1>Bitcoin Puzzle</h1>";
    
    // Display current state
    if (keyFound) {
        html += "<h2 style='color:green'>🔑 Key Found!</h2>";
        html += "<p>Private Key: " + foundPrivateKey + "</p>";
        html += "<p>Address: " + foundPublicAddress + "</p>";
    } else {
        html += "<p>Last Private Key: " + bytesToHexString(currentPrivateKey, 32) + "</p>";
        html += "<p>Last Address: " + privateKeyToAddress() + "</p>";
    }
    
    // Display the current ranges
    html += "<h3>Current Ranges</h3>";
    html += "<p>Min Hex: <input type='text' id='minHex' value='" + MIN_HEX + "'></p>";
    html += "<p>Max Hex: <input type='text' id='maxHex' value='" + MAX_HEX + "'></p>";
    html += "<p>Target Address: <input type='text' id='targetAddress' value='" + TARGET_ADDRESS + "'></p>";

    // Start/Stop buttons
    if (searchRunning) {
        html += "<button onclick='stopSearch()'>Stop Search</button>";
    } else {
        html += "<button onclick='startSearch()'>Start Search</button>";
    }

    html += "</body></html>";

    html += "<script>";
    html += "function startSearch() {";
    html += "  var minHex = document.getElementById('minHex').value.trim();";
    html += "  var maxHex = document.getElementById('maxHex').value.trim();";
    html += "  var targetAddress = document.getElementById('targetAddress').value.trim();";
    html += "  fetch('/startSearch?minHex=' + minHex + '&maxHex=' + maxHex + '&targetAddress=' + targetAddress);";
    html += "  location.reload();";
    html += "}";
    html += "function stopSearch() {";
    html += "  fetch('/stopSearch');";
    html += "  location.reload();";
    html += "}";
    html += "</script>";

    server.send(200, "text/html", html);
}

inline void Execute(void *pvParameters) {
    while (true) {  // Бесконечный цикл для задачи
        if (searchRunning && !keyFound) {
            generateRandomKey();
            String currentAddress = privateKeyToAddress();
            if (toLowerCase(currentAddress) == toLowerCase(TARGET_ADDRESS)) {
                keyFound = true;
                foundPrivateKey = bytesToHexString(currentPrivateKey, 32);
                foundPublicAddress = currentAddress;

                // Save found key and address to preferences
                preferences.putString("foundPrivateKey", foundPrivateKey);
                preferences.putString("foundPublicAddress", foundPublicAddress);
                preferences.putBool("keyFound", keyFound);

                // Blink LED like in example
                for (int i = 0; i < 5; i++) {
                    digitalWrite(LED_PIN, HIGH);
                    delay(1000);
                    digitalWrite(LED_PIN, LOW);
                    delay(1000);
                }
            }
        } else if (keyFound) {
            digitalWrite(LED_PIN, (millis() / 500) % 2);  // Blink LED when key is found
        }
          server.handleClient();
    }
}

void setup() {
    preferences.begin("bitcoin_puzzle", false); // Initialize preferences
    bootloader_random_enable();
    // Load saved values
    MIN_HEX = preferences.getString("minHex", MIN_HEX);
    MAX_HEX = preferences.getString("maxHex", MAX_HEX);
    TARGET_ADDRESS = preferences.getString("targetAddress", TARGET_ADDRESS);
    keyFound = preferences.getBool("keyFound", false);
    searchRunning = preferences.getBool("searchRunning", false);

    hexToBytes(MIN_HEX, MIN_KEY, 32);
    hexToBytes(MAX_HEX, MAX_KEY, 32);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    server.on("/", handleRoot);
    server.on("/startSearch", HTTP_GET, []() {
        MIN_HEX = server.arg("minHex");
        MAX_HEX = server.arg("maxHex");
        TARGET_ADDRESS = server.arg("targetAddress");
        
        hexToBytes(MIN_HEX, MIN_KEY, 32);
        hexToBytes(MAX_HEX, MAX_KEY, 32);

        searchRunning = true;
        keyFound = false;

        // Save to preferences
        preferences.putString("minHex", MIN_HEX);
        preferences.putString("maxHex", MAX_HEX);
        preferences.putString("targetAddress", TARGET_ADDRESS);
        preferences.putBool("searchRunning", searchRunning);
        preferences.putBool("keyFound", keyFound);

        server.send(200, "text/plain", "Search Started");
    });
    
    server.on("/stopSearch", HTTP_GET, []() {
        searchRunning = false;
        preferences.putBool("searchRunning", searchRunning);
        server.send(200, "text/plain", "Search Stopped");
    });

    server.begin();

    // Start search automatically if it was running before
    if (searchRunning) {
        keyFound = false; // Reset keyFound to false
    }
    xTaskCreatePinnedToCore(Execute, "ExecuteTask", 10000, NULL, 1, NULL, 1);
}

void loop() {
    vTaskDelete(NULL);  // Останавливаем loop(), если он не нужен
}
