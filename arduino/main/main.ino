#include <WiFiS3.h>
#include <ArduinoOTA.h>
#include "config.h"   // copy config.h.example → config.h and fill in your values

// ── Configuration ───────────────────────────────────────────────
IPAddress LOCAL_IP(LOCAL_IP_0, LOCAL_IP_1, LOCAL_IP_2, LOCAL_IP_3);
IPAddress GATEWAY(GATEWAY_0,   GATEWAY_1,  GATEWAY_2,  GATEWAY_3);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS(8, 8, 8, 8);

const int RELAY_PIN = 2;    // D2 → PN2222A base
const int PULSE_MS  = 200;  // power button press duration (ms)
// ────────────────────────────────────────────────────────────────

WiFiServer server(80);

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

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

  // OTA — reachable from Arduino IDE as a network port (same LAN only)
  ArduinoOTA.begin(WiFi.localIP(), "Arduino_UNO_R4", OTA_PASSWORD, InternalStorage);
  ArduinoOTA.onStart([]() { Serial.println("OTA start"); });
  ArduinoOTA.onError([](int code, const char* msg) {
    Serial.print("OTA error ");
    Serial.print(code);
    Serial.print(": ");
    Serial.println(msg);
  });
  Serial.println("OTA ready");
}

void loop() {
  ArduinoOTA.poll();

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
    pulsePower();
    sendResponse(client, 200, "{\"action\":\"power_pulse\",\"ms\":" + String(PULSE_MS) + "}");
  } else if (requestLine.indexOf("GET /status") >= 0) {
    sendResponse(client, 200, "{\"status\":\"ok\"}");
  } else {
    sendResponse(client, 404, "{\"error\":\"not found\"}");
  }

  client.stop();
}

// ── Pulse D2 for PULSE_MS ms ─────────────────────────────────────
void pulsePower() {
  Serial.println("Pulsing power button");
  digitalWrite(RELAY_PIN, HIGH);
  delay(PULSE_MS);
  digitalWrite(RELAY_PIN, LOW);
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
