// https://howtomechatronics.com/tutorials/arduino/arduino-wireless-communication-nrf24l01-tutorial/
// SPI pins on esp8266, used by the nRF24L01 module:
// GPIO12: MISO
// GPIO13: MOSI
// GPIO14: SCLK
// GPIO15: CS

// Import required libraries
#include <Arduino.h>
// #include <SPIFFS.h>
#include <LittleFS.h>
#include <Hash.h>
// #include <EasyButton.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "credentials.h"

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
// https://github.com/mathieucarbou/ESPAsyncWebServer
#include <ESPAsyncWebServer.h>

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#include "poliner.h"

// Pins for the nRF24L01 module
#define CE_PIN 4
#define CSN_PIN 5

MotorStatus motor_status;

RF24 radio(CE_PIN, CSN_PIN);

WiFiUDP ntpUDP;
const unsigned short int ntp_offset = 60*60; // UTC+1
const unsigned long ntp_update_interval = 60000;
// Specify the time server pool and the offset (in seconds),
// additionally specify the update interval (in milliseconds).
NTPClient timeClient(ntpUDP, "it.pool.ntp.org", ntp_offset, ntp_update_interval);

// Define a time variable to keep track of the last time the websocket cleanup was run
unsigned long websocket_cleanup_last_run = 0;
const unsigned long websocket_cleanup_interval_ms = 10000;

// Define the hour at which the door should be opened automatically.
const unsigned short int hour_door_opening = 7;
// Store whether the door was already opened automatically today.
bool door_was_already_opened_automatically = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[ip]/ws

String motor_status_to_json() {
  return ("{\"motor_turning\": "
          + String(motor_status.motor_turning)
          + ", \"motor_turning_up\": "
          + String(motor_status.turning_direction_up)
          + ", \"upper_limit_switch_pressed\": "
          + String(motor_status.upper_limit_switch_pressed)
          + ", \"lower_limit_switch_pressed\": "
          + String(motor_status.lower_limit_switch_pressed)
          + "}");
}

void notifyClients() {
  Serial.println("Notifying clients with data: " + motor_status_to_json());
  ws.textAll(motor_status_to_json());
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        if (strcmp((char*)data, "downButton") == 0 && !motor_status.lower_limit_switch_pressed) {
          motor_status.motor_turning = true;
          motor_status.turning_direction_up = false;
        } else if (strcmp((char*)data, "upButton") == 0 && !motor_status.upper_limit_switch_pressed) {
            motor_status.motor_turning = true;
            motor_status.turning_direction_up = true;
        }
        radio.stopListening();
        Serial.println("Sending new motor status: " + motor_status_to_json());
        radio.write(&motor_status, sizeof(motor_status));
        // Clients are updated after any request, even those that don't change the motor state like
        // "state". We may want to reconsider this and only trigger updates on valid WS messages.
        notifyClients();
    }
}

void onEvent(AsyncWebSocket       *server,
             AsyncWebSocketClient *client,
             AwsEventType          type,
             void                 *arg,
             uint8_t              *data,
             size_t                len) {

    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_PING:
        case WS_EVT_ERROR:
            break;
    }
}

void setup(){
  // Serial port speed for debugging purposes.
  Serial.begin(115200);

  radio.begin();
  radio.openWritingPipe(nrf24l01_radio_addresses[0]); // 00001
  radio.openReadingPipe(1, nrf24l01_radio_addresses[1]); // 00002
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();

  Serial.println("Proxy ready");
  pinMode(LED_BUILTIN, OUTPUT); // Use builtin LED pin as output pin
  // Make the onboard LED blip
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);

  // Serial.print("\n\n\nSetting AP (Access Point)…");
  // // Remove the password parameter, if you want the AP (Access Point) to be open
  // WiFi.softAP(ssid, password);

  // IPAddress IP = WiFi.softAPIP();
  // Serial.println("AP IP address: " + IP);
  // // Print ESP8266 Local IP Address
  // Serial.println(WiFi.localIP());

  Serial.print("Connecting to ");
  Serial.println(ssid);
  // Serial.println();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Print local IP address and start web server
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  websocket_cleanup_last_run = millis();

  server.begin();

  timeClient.begin();
  timeClient.update();
  Serial.print("Current time: ");
  Serial.println(timeClient.getFormattedTime());
}

void loop(){
  radio.startListening();
  if (radio.available()) {
    radio.read(&motor_status, sizeof(MotorStatus)); // Read the whole data and store it into the 'data' structure
    notifyClients();

    // TODO: Here we should have some sanity checks to make sure the data is valid.
    // Sometimes we get weird data, like a boolean with value 128 or 255

    Serial.print("Got new data: motor_turning: ");
    Serial.print((bool)motor_status.motor_turning);
    Serial.print(", turning_direction_up: ");
    Serial.print((bool)motor_status.turning_direction_up);
    Serial.print(", upper_limit_switch_pressed: ");
    Serial.print((bool)motor_status.upper_limit_switch_pressed);
    Serial.print(", lower_limit_switch_pressed: ");
    Serial.println((bool)motor_status.lower_limit_switch_pressed);
  }

  // Perform a periodic WebSocket cleanup.
  if ((millis() - websocket_cleanup_last_run) > websocket_cleanup_interval_ms) {
    ws.cleanupClients();
    websocket_cleanup_last_run = millis();

    Serial.println(timeClient.getFormattedTime());
  }

  int currentHour = timeClient.getHours();
  // Open the door automatically at the specified hour.
  if (currentHour == hour_door_opening && !door_was_already_opened_automatically) {
    motor_status.motor_turning = true;
    motor_status.turning_direction_up = true;
    radio.stopListening();
    Serial.println("Sending new motor status: " + motor_status_to_json());
    radio.write(&motor_status, sizeof(motor_status));
    notifyClients();

    door_was_already_opened_automatically = true;
    Serial.println("Opening the door automatically since it's " + String(hour_door_opening) + " o'clock.");
  }
  if (currentHour != hour_door_opening) {
    door_was_already_opened_automatically = false;
  }
}
