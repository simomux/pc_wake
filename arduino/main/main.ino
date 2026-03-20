#include <WiFiS3.h>
#include <Keyboard.h>
#include "config.h"   // copy config.h.example → config.h and fill in your values

// ── Configuration ───────────────────────────────────────────────
IPAddress LOCAL_IP(LOCAL_IP_0, LOCAL_IP_1, LOCAL_IP_2, LOCAL_IP_3);
IPAddress GATEWAY(GATEWAY_0,   GATEWAY_1,  GATEWAY_2,  GATEWAY_3);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS(8, 8, 8, 8);

const int  RELAY_PIN      = 2;          // D2 → PN2222A base
const int  PULSE_MS       = 200;        // power button press duration (ms)
const int  WIN_BOOT_DELAY = 35;         // seconds to wait for Windows to boot
// ────────────────────────────────────────────────────────────────

WiFiServer server(80);

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Keyboard.begin();

  // Static IP
  WiFi.config(LOCAL_IP, DNS, GATEWAY, SUBNET);

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED) {
    delay(3000);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  // Read first line of the request
  String requestLine = "";
  while (client.connected() && client.available()) {
    char c = client.read();
    if (c == '\n') break;
    requestLine += c;
  }
  // Drain remaining headers
  while (client.connected() && client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  Serial.println("Request: " + requestLine);

  if (requestLine.indexOf("POST /power") >= 0) {
    handlePower(client);
  } else if (requestLine.indexOf("POST /type-password") >= 0) {
    handleTypePassword(client);
  } else if (requestLine.indexOf("POST /wake") >= 0) {
    // All-in-one: power + wait for boot + type PIN
    handleWake(client);
  } else if (requestLine.indexOf("GET /status") >= 0) {
    sendResponse(client, 200, "{\"status\":\"ok\"}");
  } else {
    sendResponse(client, 404, "{\"error\":\"not found\"}");
  }

  client.stop();
}

// ── Handler: pulse power button ──────────────────────────────────
void handlePower(WiFiClient& client) {
  pulsePower();
  sendResponse(client, 200, "{\"action\":\"power_pulse\",\"ms\":" + String(PULSE_MS) + "}");
}

// ── Handler: type PIN (call when PC is already on) ───────────────
void handleTypePassword(WiFiClient& client) {
  sendResponse(client, 200, "{\"action\":\"typing\"}");
  client.stop();
  typePin();
}

// ── Handler: full wake sequence (power + wait + PIN) ─────────────
void handleWake(WiFiClient& client) {
  sendResponse(client, 200,
    "{\"action\":\"wake\",\"boot_delay\":" + String(WIN_BOOT_DELAY) + "}");
  client.stop();

  pulsePower();

  Serial.println("Waiting for Windows to boot...");
  for (int i = 0; i < WIN_BOOT_DELAY; i++) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();

  typePin();
}

// ── Pulse D2 for PULSE_MS ms ─────────────────────────────────────
void pulsePower() {
  Serial.println("Pulsing power button");
  digitalWrite(RELAY_PIN, HIGH);
  delay(PULSE_MS);
  digitalWrite(RELAY_PIN, LOW);
}

// ── Type PIN + Enter via HID ─────────────────────────────────────
void typePin() {
  Serial.println("Typing PIN...");
  // Press a key to wake the screen in case it went to sleep
  Keyboard.press(KEY_LEFT_SHIFT);
  delay(100);
  Keyboard.releaseAll();
  delay(500);

  Keyboard.print(WIN_PIN);
  delay(100);
  Keyboard.press(KEY_RETURN);
  delay(100);
  Keyboard.releaseAll();
  Serial.println("PIN sent");
}

// ── HTTP response helper ──────────────────────────────────────────
void sendResponse(WiFiClient& client, int code, const String& body) {
  String status = (code == 200) ? "OK" : (code == 404 ? "Not Found" : "Error");
  client.println("HTTP/1.1 " + String(code) + " " + status);
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println(body);
}
