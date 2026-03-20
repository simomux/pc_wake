#include <WiFiS3.h>
#include <Keyboard.h>
#include "config.h"   // copy config.h.example → config.h and fill in your values

// ── Configurazione ──────────────────────────────────────────────
IPAddress LOCAL_IP(LOCAL_IP_0, LOCAL_IP_1, LOCAL_IP_2, LOCAL_IP_3);
IPAddress GATEWAY(GATEWAY_0,   GATEWAY_1,  GATEWAY_2,  GATEWAY_3);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS(8, 8, 8, 8);

const int  RELAY_PIN      = 2;          // D2 → base transistor PN2222A
const int  PULSE_MS       = 200;        // durata pressione power button
const int  WIN_BOOT_DELAY = 35;         // secondi attesa boot Windows
// ────────────────────────────────────────────────────────────────

WiFiServer server(80);

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Keyboard.begin();

  // IP statico
  WiFi.config(LOCAL_IP, DNS, GATEWAY, SUBNET);

  Serial.print("Connessione a ");
  Serial.println(WIFI_SSID);

  while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED) {
    delay(3000);
    Serial.print(".");
  }

  Serial.println("\nWiFi connesso");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("HTTP server avviato");
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  // Leggi prima riga della request
  String requestLine = "";
  while (client.connected() && client.available()) {
    char c = client.read();
    if (c == '\n') break;
    requestLine += c;
  }
  // Consuma gli header rimanenti
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
    // Endpoint tutto-in-uno: power + attesa boot + digita password
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

// ── Handler: digita password (da chiamare dopo boot manuale) ─────
void handleTypePassword(WiFiClient& client) {
  sendResponse(client, 200, "{\"action\":\"typing\"}");
  client.stop();
  typePassword();
}

// ── Handler: wake completo (power + attesa + password) ───────────
void handleWake(WiFiClient& client) {
  sendResponse(client, 200,
    "{\"action\":\"wake\",\"boot_delay\":" + String(WIN_BOOT_DELAY) + "}");
  client.stop();

  pulsePower();

  Serial.println("Attendo boot Windows...");
  for (int i = 0; i < WIN_BOOT_DELAY; i++) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();

  typePassword();
}

// ── Pulse D2 per PULSE_MS ms ─────────────────────────────────────
void pulsePower() {
  Serial.println("Pulse power button");
  digitalWrite(RELAY_PIN, HIGH);
  delay(PULSE_MS);
  digitalWrite(RELAY_PIN, LOW);
}

// ── Digita password + Invio tramite HID ──────────────────────────
void typePassword() {
  Serial.println("Digitazione password...");
  // Premi un tasto qualsiasi per svegliare lo schermo (se in sleep)
  Keyboard.press(KEY_LEFT_SHIFT);
  delay(100);
  Keyboard.releaseAll();
  delay(500);

  Keyboard.print(WIN_PASSWORD);
  delay(100);
  Keyboard.press(KEY_RETURN);
  delay(100);
  Keyboard.releaseAll();
  Serial.println("Password inviata");
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
