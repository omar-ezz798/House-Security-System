/*************************************************
 * SMART HOUSE SECURITY SYSTEM WITH TELEGRAM
 * ESP32 + Telegram Bot
 *************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

//====================
// WIFI
//====================

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

//====================
// TELEGRAM
//====================

#define CHAT_ID "YOUR_CHAT_ID"

#define BOT_TOKEN "YOUR_BOT_TOKEN"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

//====================
// LEDS
//====================

const int ledRed = 23;
const int ledYellow = 22;
const int ledBlue = 21;

//====================
// BUTTONS
//====================

const int btnArm = 18;
const int btnDisarm = 19;

//====================
// SENSOR
//====================

const int reed = 4;

//====================
// BUZZER
//====================

const int buzzer = 5;

//====================
// VARIABLES
//====================

bool armed = false;
bool Activealarm = false;
bool wifiConnected = false;
bool messageSent = false;

//====================
// WIFI CONNECTION
//====================

void connectWiFi() {

  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

  int timeout = 0;

  while (WiFi.status() != WL_CONNECTED && timeout < 20) {

    delay(500);

    Serial.print(".");

    timeout++;

  }

  if (WiFi.status() == WL_CONNECTED) {

    wifiConnected = true;

    Serial.println("");

    Serial.println("WiFi Connected");

    Serial.println(WiFi.localIP());

  }

  else {

    wifiConnected = false;

    Serial.println("");

    Serial.println("WiFi Failed");

  }

}

//====================
// SETUP
//====================

void setup() {

  Serial.begin(115200);

  pinMode(ledRed, OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(ledBlue, OUTPUT);

  pinMode(btnArm, INPUT_PULLUP);
  pinMode(btnDisarm, INPUT_PULLUP);

  pinMode(reed, INPUT_PULLUP);

  pinMode(buzzer, OUTPUT);

  digitalWrite(ledBlue, HIGH);

  client.setInsecure();

  connectWiFi();

}
//=====================================
// SEND TELEGRAM MESSAGE
//=====================================

void sendTelegram(String message) {

  if (wifiConnected) {

    bot.sendMessage(CHAT_ID, message, "");

  }

}

//=====================================
// ARM MESSAGE
//=====================================

void sendArmMessage() {

  sendTelegram(
    "🟢 HOUSE SECURITY\n\n"
    "System Armed Successfully."
  );

}

//=====================================
// DISARM MESSAGE
//=====================================

void sendDisarmMessage() {

  sendTelegram(
    "🔵 HOUSE SECURITY\n\n"
    "System Disarmed."
  );

}

//=====================================
// ALARM MESSAGE
//=====================================

void sendAlarmMessage() {

  sendTelegram(
    "🚨 HOUSE SECURITY ALERT 🚨\n\n"
    "Door Opened!\n"
    "Intrusion Detected!"
  );

}

//=====================================
// WIFI CHECK
//=====================================

void checkWiFi() {

  if (WiFi.status() != WL_CONNECTED) {

    wifiConnected = false;

    WiFi.disconnect();

    WiFi.begin(ssid, password);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < 5000) {

      delay(500);

    }

    if (WiFi.status() == WL_CONNECTED) {

      wifiConnected = true;

    }

  }

  else {

    wifiConnected = true;

  }

}
//==============================
// LOOP
//==============================

void loop() {

  checkWiFi();

  //=========================
  // ARM SYSTEM
  //=========================

  if (digitalRead(btnArm) == LOW && !armed) {

    digitalWrite(ledBlue, LOW);

    digitalWrite(ledYellow, HIGH);

    delay(10000);

    digitalWrite(ledYellow, LOW);

    digitalWrite(ledRed, HIGH);

    armed = true;

    Activealarm = false;

    messageSent = false;

    sendArmMessage();

    

  }

  //=========================
  // DISARM SYSTEM
  //=========================

  if (digitalRead(btnDisarm) == LOW) {

    armed = false;

    Activealarm = false;

    messageSent = false;

    digitalWrite(ledRed, LOW);

    digitalWrite(ledYellow, LOW);

    digitalWrite(ledBlue, HIGH);

    digitalWrite(buzzer, LOW);

    sendDisarmMessage();

    

  }

  //=========================
  // DOOR DETECTION
  //=========================

  if (armed) {

    if (digitalRead(reed) == LOW) {

      Activealarm = true;

    }

  }

  //=========================
  // ALARM
  //=========================

  if (Activealarm) {

    digitalWrite(buzzer, HIGH);

    digitalWrite(ledRed, HIGH);

    if (!messageSent) {

      sendAlarmMessage();

      messageSent = true;

    }

  }

  else {

    digitalWrite(buzzer, LOW);

  }

}
