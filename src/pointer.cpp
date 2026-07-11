#include "config.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Stepper.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// ── Motor config ─────────────────────────────────────────────────────────────

const int stepsPerRevolution = 2048;

#define IN1azimuth   19
#define IN2azimuth   18
#define IN3azimuth   23
#define IN4azimuth   22

#define IN1elevation 12
#define IN2elevation 13
#define IN3elevation  2
#define IN4elevation  4

Stepper AzimuthStepper(stepsPerRevolution,   IN1azimuth,   IN3azimuth,   IN2azimuth,   IN4azimuth);
Stepper ElevationStepper(stepsPerRevolution, IN1elevation, IN3elevation, IN2elevation, IN4elevation);

int azimuthStepperCurrentPosition   = 0;
int elevationStepperCurrentPosition = 0;

// ── Runtime state ─────────────────────────────────────────────────────────────

int pointerAzimuth   = 0;
int pointerElevation = 0;

unsigned long lastMoveTime     = 0;
unsigned long lastIamAliveTime = 0;
const int moveInterval         = 1000;
const int IamAliveInterval     = 20000;

// ── WebSocket ─────────────────────────────────────────────────────────────────

WebSocketsClient webSocket;
volatile bool wsConnected    = false;
unsigned long lastWsMsgTime  = 0;
const unsigned long WS_WATCHDOG_MS = 15000; // reconnect if silent for 15 s

const int maxRetryAttempts = 50;
int currentRetryAttempt    = 0;
const int retryInterval    = 5000;

// ── Motor functions ───────────────────────────────────────────────────────────

void releaseMotors() {
  digitalWrite(IN1azimuth,   LOW); digitalWrite(IN2azimuth,   LOW);
  digitalWrite(IN3azimuth,   LOW); digitalWrite(IN4azimuth,   LOW);
  digitalWrite(IN1elevation, LOW); digitalWrite(IN2elevation, LOW);
  digitalWrite(IN3elevation, LOW); digitalWrite(IN4elevation, LOW);
}

void AzimuthStepperGotoAngle(int angle) {
  angle = constrain(angle, 0, 359);
  int newpos;
  if (angle <= 180) {
    newpos = map(angle, 0, 180, 0, 1024);
  } else {
    newpos = (angle - 180) * (0 - (-1024)) / (360 - 180) + (-1024);
  }
  int stepsToMove = newpos - azimuthStepperCurrentPosition;
  AzimuthStepper.setSpeed(abs(stepsToMove) > 10 ? 10 : 5);
  AzimuthStepper.step(stepsToMove);
  azimuthStepperCurrentPosition = newpos;
}

void ElevationStepperGotoAngle(int angle) {
  angle = constrain(angle, 0, 90);
  int newpos      = map(angle, 0, 90, 0, 512);
  int stepsToMove = newpos - elevationStepperCurrentPosition;
  ElevationStepper.setSpeed(abs(stepsToMove) > 10 ? 10 : 5);
  ElevationStepper.step(stepsToMove);
  elevationStepperCurrentPosition = newpos;
}

void setHomePosition() {
  Serial.println("[Homing] Starting elevation homing...");
  Serial.println("[Homing]   Stepping -1034 to mechanical stop");
  ElevationStepper.setSpeed(5);
  ElevationStepper.step(-1024 - 10);
  elevationStepperCurrentPosition = 0;
  Serial.println("[Homing]   Settling to 32° then zeroing position");
  ElevationStepperGotoAngle(32);
  elevationStepperCurrentPosition = 0;
  Serial.println("[Homing] Elevation homed ✓");

  releaseMotors();
  Serial.println("[Homing] Starting azimuth homing...");
  Serial.println("[Homing]   Stepping +2098 CW to mechanical stop (chunked)");
  AzimuthStepper.setSpeed(5);
  for (int remaining = 2048 + 50; remaining > 0; remaining -= 200) {
    AzimuthStepper.step(min(remaining, 200));
    delay(20);
  }
  Serial.println("[Homing]   Stepping -1024 CCW to North (0°)");
  AzimuthStepper.step(-2048 / 2);
  azimuthStepperCurrentPosition = 0;
  releaseMotors();
  Serial.println("[Homing] Azimuth homed ✓");
  Serial.println("[Homing] All axes at home position");
}

void testRun() {
  Serial.println("[Test] 360°/90° test run...");
  for (int az = 0; az < 360; az++) {
    AzimuthStepperGotoAngle(az);
    ElevationStepperGotoAngle((int)(sin(PI / 360.0 * az) * 90));
  }
  AzimuthStepperGotoAngle(0);
  ElevationStepperGotoAngle(0);
  releaseMotors();
  Serial.println("[Test] Test run completed");
}

// ── WiFi ──────────────────────────────────────────────────────────────────────

void connectToWiFi(int maxTries) {
  Serial.printf("[WiFi] Connecting to SSID: %s\n", ssid);
  WiFi.begin(ssid, password);
  for (int attempts = 1; WiFi.status() != WL_CONNECTED; attempts++) {
    delay(1000);
    Serial.printf("[WiFi] Attempt %d/%d...\n", attempts, maxTries);

    // Wiggle elevation ±15° per attempt as a visual "I'm alive" indicator.
    // Motor is not yet homed — intentional, homing resets it later.
    ElevationStepper.setSpeed(10);
    ElevationStepper.step(85);   // +15°
    ElevationStepper.step(-85);  // -15°
    releaseMotors();

    if (attempts >= maxTries) {
      Serial.println("[WiFi] Max attempts reached — rebooting");
      delay(1000);
      ESP.restart();
    }
  }
  Serial.printf("[WiFi] Connected — RSSI: %d dBm\n", WiFi.RSSI());
  Serial.print("[WiFi] Waiting for DHCP");
  for (int i = 0; WiFi.localIP() == IPAddress(0, 0, 0, 0) && i < 20; i++) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println("[WiFi] DHCP timeout — rebooting");
    delay(500);
    ESP.restart();
  }
  Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  delay(500);
}

// ── WebSocket ─────────────────────────────────────────────────────────────────

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
void handleWebSocketMessage(const char* payload);
void retryWebSocketConnection();

void connectWebSocket() {
  Serial.printf("[WS] Connecting to ws://%s:%d/\n", websocket_host, websocket_port);
  wsConnected = false;
  webSocket.begin(websocket_host, websocket_port, "/");
  webSocket.onEvent(webSocketEvent);
}

// Shake azimuth ±15° twice — "NO" signal used before reboot.
// Motors are not homed when this is called; steps are relative.
void doNoShake() {
  Serial.println("[!] Signalling NO (azimuth shake)");
  AzimuthStepper.setSpeed(10);
  for (int i = 0; i < 2; i++) {
    AzimuthStepper.step(85);   // +15°
    AzimuthStepper.step(-170); // −30° (swing to -15°)
    AzimuthStepper.step(85);   // back to centre
  }
  releaseMotors();
}

// Wait up to timeoutMs for the WebSocket handshake to complete.
bool waitForWebSocket(int timeoutMs) {
  Serial.printf("[WS] Waiting for connection (timeout %d s)...\n", timeoutMs / 1000);
  unsigned long t = millis();
  while (millis() - t < (unsigned long)timeoutMs && !wsConnected) {
    webSocket.loop();
    yield();
  }
  return wsConnected;
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  Serial.println("\n============================================");
  Serial.println("  ISS POINTER — booting");
  Serial.println("============================================");

  Serial.println("\n[1/5] WiFi");
  connectToWiFi(10);
  MDNS.begin("iss-pointer");
  Serial.println("[WiFi] mDNS started — this device is iss-pointer.local");

  Serial.println("\n[2/5] WebSocket check");
  connectWebSocket();
  if (!waitForWebSocket(10000)) {
    Serial.println("[!] WebSocket server not reachable — signalling NO and rebooting");
    doNoShake();
    delay(2000);
    ESP.restart();
  }
  Serial.println("[WS] Server confirmed reachable ✓");
  lastWsMsgTime = millis();

  Serial.println("\n[3/5] Homing");
  setHomePosition();

  Serial.println("\n[4/5] North alignment — align device to North now");
  for (int i = 0; i < 10; i++) {
    ElevationStepperGotoAngle(60);
    ElevationStepperGotoAngle(0);
  }
  releaseMotors();

  Serial.println("\n[5/5] Test run");
  testRun();

  Serial.println("\n============================================");
  Serial.printf("  Ready — server: %s:%d\n", websocket_host, websocket_port);
  Serial.println("============================================\n");
}

void loop() {
  webSocket.loop();

  unsigned long now = millis();

  // Watchdog: if messages have stopped arriving, signal NO and force reconnect.
  if (now - lastWsMsgTime > WS_WATCHDOG_MS) {
    Serial.printf("[WS] No message for %lu ms — signalling NO and reconnecting\n",
      now - lastWsMsgTime);
    pointerAzimuth   = 0;
    pointerElevation = 0;
    ElevationStepperGotoAngle(0);
    AzimuthStepperGotoAngle(0);
    doNoShake();
    lastWsMsgTime = now;
    webSocket.disconnect(); // triggers WStype_DISCONNECTED → retryWebSocketConnection()
    return;
  }

  if (now - lastMoveTime < moveInterval) return;
  lastMoveTime = now;

  if (pointerElevation > 0) {
    Serial.printf("  Az: %d  El: %d\n", pointerAzimuth, pointerElevation);
    AzimuthStepperGotoAngle(pointerAzimuth);
    ElevationStepperGotoAngle(pointerElevation);
    releaseMotors();
  } else {
    AzimuthStepperGotoAngle(0);
    ElevationStepperGotoAngle(0);
    releaseMotors();

    if (now - lastIamAliveTime >= IamAliveInterval) {
      lastIamAliveTime = now;
      for (int i = 0; i < 5; i++) {
        ElevationStepperGotoAngle(15);
        ElevationStepperGotoAngle(0);
      }
      releaseMotors();
    }
  }
}

// ── WebSocket callbacks ───────────────────────────────────────────────────────

void handleWebSocketMessage(const char* payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return;

  lastWsMsgTime    = millis();
  pointerAzimuth   = round((float)doc["azimuth"]);
  pointerElevation = round((float)doc["elevation"]);

  const char* satName = doc["satName"] | "?";
  Serial.printf("[WS] Sat: %s  Az: %.1f  El: %.1f\n",
    satName, (float)doc["azimuth"], (float)doc["elevation"]);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      retryWebSocketConnection();
      break;
    case WStype_CONNECTED:
      Serial.println("[WS] Connected");
      wsConnected = true;
      currentRetryAttempt = 0;
      lastWsMsgTime = millis(); // start watchdog window from this moment
      break;
    case WStype_TEXT:
      handleWebSocketMessage((const char*)payload);
      break;
    default:
      break;
  }
}

void retryWebSocketConnection() {
  currentRetryAttempt++;
  Serial.printf("[WS] Retry %d/%d in %d s...\n",
    currentRetryAttempt, maxRetryAttempts, retryInterval / 1000);
  delay(retryInterval);
  connectWebSocket();
}
