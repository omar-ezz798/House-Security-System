/*
 * HOUSE SECURITY SYSTEM — ESP32 + Telegram
 * Exit delay is intentionally 10 seconds so the user can close the door and leave.
 * Non-blocking state machine: DISARM works during the exit countdown.
 * Educational prototype; not a certified security system.
 */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
#define CHAT_ID "YOUR_CHAT_ID"
#define BOT_TOKEN "YOUR_BOT_TOKEN"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Existing wiring preserved.
const int ledRed = 23;
const int ledYellow = 22;
const int ledBlue = 21;
const int btnArm = 18;
const int btnDisarm = 19;
const int reed = 4;
const int buzzer = 5;

const unsigned long EXIT_DELAY_MS = 10000UL;
const unsigned long DEBOUNCE_MS = 40UL;
const unsigned long WIFI_RETRY_MS = 10000UL;
const unsigned long TELEGRAM_RETRY_MS = 15000UL;

enum SecurityState { DISARMED, EXIT_DELAY, ARMED, ALARM_ACTIVE };
SecurityState state = DISARMED;
unsigned long exitStartedAt = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastTelegramAttempt = 0;
bool alertDelivered = false;
bool wifiWasConnected = false;

// Buttons are active-LOW and configured with INPUT_PULLUP.
struct DebouncedButton {
  int pin;
  bool lastRaw;
  bool stable;
  unsigned long changedAt;
};

DebouncedButton armButton = {btnArm, HIGH, HIGH, 0};
DebouncedButton disarmButton = {btnDisarm, HIGH, HIGH, 0};

// Returns true once per stable press, not on every loop iteration.
bool pressed(DebouncedButton& button, unsigned long now) {
  bool raw = digitalRead(button.pin);
  if (raw != button.lastRaw) {
    button.changedAt = now;
    button.lastRaw = raw;
  }
  if (raw != button.stable && now - button.changedAt >= DEBOUNCE_MS) {
    button.stable = raw;
    return button.stable == LOW;
  }
  return false;
}

void updateOutputs() {
  digitalWrite(ledBlue, state == DISARMED ? HIGH : LOW);
  digitalWrite(ledYellow, state == EXIT_DELAY ? HIGH : LOW);
  digitalWrite(ledRed, (state == ARMED || state == ALARM_ACTIVE) ? HIGH : LOW);
  digitalWrite(buzzer, state == ALARM_ACTIVE ? HIGH : LOW);
}

// Wi-Fi reconnection is initiated periodically without a blocking wait loop.
void maintainWiFi(unsigned long now) {
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected != wifiWasConnected) {
    Serial.println(connected ? "WiFi Connected" : "WiFi Disconnected");
    wifiWasConnected = connected;
  }

  if (!connected && now - lastWiFiAttempt >= WIFI_RETRY_MS) {
    lastWiFiAttempt = now;
    WiFi.begin(ssid, password);
  }
}

bool sendTelegram(const String& message) {
  if (WiFi.status() != WL_CONNECTED) return false;
  return bot.sendMessage(CHAT_ID, message, "");
}

void beginExitDelay(unsigned long now) {
  state = EXIT_DELAY;
  exitStartedAt = now;
  alertDelivered = false;
  Serial.println("Exit delay started: 10 seconds to close the door.");
  updateOutputs();
}

void disarmSystem() {
  if (state == DISARMED) return;
  state = DISARMED;
  alertDelivered = false;
  updateOutputs();
  Serial.println("System disarmed.");
  // Best-effort status message; local disarm must not depend on Wi-Fi.
  sendTelegram("🔵 HOUSE SECURITY\n\nSystem Disarmed.");
}

void finishArming() {
  state = ARMED;
  updateOutputs();
  Serial.println("System armed.");
  sendTelegram("🟢 HOUSE SECURITY\n\nSystem Armed Successfully.");
}

void triggerAlarm() {
  state = ALARM_ACTIVE;  // Latched until DISARM, as in the original sketch.
  alertDelivered = false;
  // Allow the first send immediately, even near millis() rollover.
  lastTelegramAttempt = millis() - TELEGRAM_RETRY_MS;
  updateOutputs();
  Serial.println("Door opened: alarm triggered.");
}

void retryAlarmNotification(unsigned long now) {
  if (state != ALARM_ACTIVE || alertDelivered) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (now - lastTelegramAttempt < TELEGRAM_RETRY_MS) return;
  lastTelegramAttempt = now;
  alertDelivered = sendTelegram(
    "🚨 HOUSE SECURITY ALERT 🚨\n\nDoor Opened!\nIntrusion Detected!"
  );
  Serial.println(alertDelivered ? "Telegram alert sent." : "Telegram alert failed; will retry.");
}

void setup() {
  Serial.begin(115200);
  pinMode(ledRed, OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(ledBlue, OUTPUT);
  pinMode(btnArm, INPUT_PULLUP);
  pinMode(btnDisarm, INPUT_PULLUP);
  pinMode(reed, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);

  updateOutputs();

  // TODO: Replace with trusted Telegram root CA certificate for production.
  // setInsecure() does NOT validate the remote TLS certificate.
  client.setInsecure();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lastWiFiAttempt = millis();
}

void loop() {
  unsigned long now = millis();

  // Always read both buttons; DISARM takes priority, even during exit delay.
  bool disarmPressed = pressed(disarmButton, now);
  bool armPressed = pressed(armButton, now);

  if (disarmPressed) {
    disarmSystem();
  } else if (armPressed && state == DISARMED) {
    beginExitDelay(now);
  }

  // Intentional 10-second exit grace period, now without delay(10000).
  if (state == EXIT_DELAY && now - exitStartedAt >= EXIT_DELAY_MS) {
    finishArming();
  }

  // Original reed-switch polarity preserved (LOW means triggered).
  if (state == ARMED && digitalRead(reed) == LOW) {
    triggerAlarm();
  }

  maintainWiFi(now);
  retryAlarmNotification(now);
}
