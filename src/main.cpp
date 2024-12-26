// Import required libraries
#include <time.h>
#include <Arduino.h>
// #include <SPIFFS.h>
#include <LittleFS.h>
#include <Hash.h>
#include <EasyButton.h>
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

// Somehow these are included in the arduino cli compiler, which bypasses the libraries installed as deps?
// #include <AsyncJson.h>
// #include <ArduinoJson.h>

// Define stepper motor connections and steps per revolution:
#define DIR_PIN 13
#define STEP_PIN 12
// How long the motor step should be in microseconds. Bigger is slower.
#define MOTOR_INTERVAL_US 2000
// Define limit switch pins
#define LOWER_LIMIT_SWITCH_PIN 0
// How long the limit switch should be pressed for before the motor stops, in milliseconds
#define LIMIT_SWITCH_PRESSED_FOR_DURATION 50

EasyButton lower_limit_switch(LOWER_LIMIT_SWITCH_PIN);

// Define a time variable to keep track of the last time the websocket cleanup was run
unsigned long websocket_cleanup_last_run = 0;
const unsigned long websocket_cleanup_interval_ms = 10000;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

// void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
//   if(type == WS_EVT_CONNECT){
//     //client connected
//     os_printf("ws[%s][%u] connect\n", server->url(), client->id());
//     client->printf("Hello Client %u :)", client->id());
//     client->ping();
//   } else if(type == WS_EVT_DISCONNECT){
//     //client disconnected
//     os_printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
//   } else if(type == WS_EVT_ERROR){
//     //error was received from the other end
//     os_printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
//   } else if(type == WS_EVT_PONG){
//     //pong message was received (in response to a ping request maybe)
//     os_printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
//   } else if(type == WS_EVT_DATA){
//     //data packet
//     AwsFrameInfo * info = (AwsFrameInfo*)arg;
//     if(info->final && info->index == 0 && info->len == len){
//       //the whole message is in a single frame and we got all of its data
//       os_printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
//       if(info->opcode == WS_TEXT){
//         data[len] = 0;
//         os_printf("%s\n", (char*)data);
//       } else {
//         for(size_t i=0; i < info->len; i++){
//           os_printf("%02x ", data[i]);
//         }
//         os_printf("\n");
//       }
//       if(info->opcode == WS_TEXT)
//         client->text("I got your text message");
//       else
//         client->binary("I got your binary message");
//     } else {
//       //message is comprised of multiple frames or the frame is split into multiple packets
//       if(info->index == 0){
//         if(info->num == 0)
//           os_printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
//         os_printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
//       }

//       os_printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
//       if(info->message_opcode == WS_TEXT){
//         data[len] = 0;
//         os_printf("%s\n", (char*)data);
//       } else {
//         for(size_t i=0; i < len; i++){
//           os_printf("%02x ", data[i]);
//         }
//         os_printf("\n");
//       }

//       if((info->index + len) == info->len){
//         os_printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
//         if(info->final){
//           os_printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
//           if(info->message_opcode == WS_TEXT)
//             client->text("I got your text message");
//           else
//             client->binary("I got your binary message");
//         }
//       }
//     }
//   }
// }

// Whether the motor is turning
bool motor_turning = false;
// Whether the motor is turning in the up direction
bool motor_turning_up = false;

String motor_status_to_json() {
  return "{\"motor_turning\": " + String(motor_turning) + ", \"button_pressed\": " + String(lower_limit_switch.isPressed()) + ", \"motor_turning_up\": " + String(motor_turning_up) + "}";
}

void notifyClients() {
    ws.textAll(motor_status_to_json());
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        if (strcmp((char*)data, "downButton") == 0 && !lower_limit_switch.isPressed()) {
            motor_turning = true;
            motor_turning_up = false;
            digitalWrite(DIR_PIN, HIGH);
        } else if (strcmp((char*)data, "upButton") == 0) { // && !upper_limit_switch.isPressed()) {
            motor_turning = true;
            motor_turning_up = true;
            digitalWrite(DIR_PIN, LOW);
        }
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

const char index_html[] PROGMEM = R"rawliteral(
)rawliteral";

// <script>
// setInterval(function() {
//   var xhttp = new XMLHttpRequest();
//   xhttp.onreadystatechange = function() {
//     if (this.readyState == 4 && this.status == 200) {
//       document.getElementById("temperature").innerHTML = this.responseText;
//     }
//   };
//   xhttp.open("GET", "/temperature", true);
//   xhttp.send();
// }, 10000);
// setInterval(function() {
//   var xhttp = new XMLHttpRequest();
//   xhttp.onreadystatechange = function() {
//     if (this.readyState == 4 && this.status == 200) {
//       document.getElementById("humidity").innerHTML = this.responseText;
//     }
//   };
//   xhttp.open("GET", "/humidity", true);
//   xhttp.send();
// }, 10000);
// </script>

// Replaces placeholder with DHT values
String processor(const String& var){
  Serial.println("Variable received: ");
  Serial.println(var);
  if (var == "LIMIT_SWITCH_STATE") {
    return String(lower_limit_switch.isPressed() ? "chiuso" : "aperto");
  }
  else if (var == "MOTOR_DIRECTION") {
    return String(motor_turning_up ? "su" : "giù");
  }
  return String();
}


// Callback function to be called when the lower_limit_switch is pressed.
void onPressed() {
  Serial.println("Button pressed");
  motor_turning = false;
  Serial.println("Turning off motor");
  // Update clients so that they know that a switch was pressed
  notifyClients();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("Cannot mount LittleFS volume...");
  }

  // Declare pins as output:
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  // Initialize the lower_limit_switch.
  lower_limit_switch.begin();
  // Add the callback function to be called when the lower_limit_switch is pressed.
  lower_limit_switch.onPressedFor(LIMIT_SWITCH_PRESSED_FOR_DURATION, onPressed);

  // Serial.print("\n\n\nSetting AP (Access Point)…");
  // // Remove the password parameter, if you want the AP (Access Point) to be open
  // WiFi.softAP(ssid, password);

  // IPAddress IP = WiFi.softAPIP();
  // Serial.println("AP IP address: " + IP);
  // // Print ESP8266 Local IP Address
  // Serial.println(WiFi.localIP());

  Serial.print("\n\nConnecting to ");
  Serial.println(ssid);
  // Serial.println();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Print local IP address and start web server
  Serial.println("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  File file = LittleFS.open("/hello.txt", "r");
  if (!file) {
      Serial.println("file open failed");
  }
  while (file.available()) {
    Serial.write(file.read());
  }
  //Close the file
  file.close();

  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/up", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("up");
    // Define direction for the stepper motor
    motor_turning_up = true;
    digitalWrite(DIR_PIN, LOW);
    motor_turning = true;
    request->send(200);
  });

  server.on("/down", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("down");
    // Define direction for the stepper motor
    motor_turning_up = false;
    digitalWrite(DIR_PIN, HIGH);
    motor_turning = true;
    request->send(200);
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println(motor_status_to_json());

    // AsyncResponseStream *response = request->beginResponseStream("application/json");
    // JsonDocument doc;
    // JsonObject &root = jsonBuffer.createObject();
    // root["heap"] = ESP.getFreeHeap();
    // root["ssid"] = WiFi.SSID();
    // root.printTo(*response);
    // request->send(response);

    request->send(200, "application/json", motor_status_to_json());
  });

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  websocket_cleanup_last_run = millis();

  // Start webserver
  server.begin();
}

void loop(){
  if (motor_turning) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(MOTOR_INTERVAL_US);
    digitalWrite(STEP_PIN, LOW);
    // Serial.println("Motor on");
  }

  lower_limit_switch.read();

  if ((millis() - websocket_cleanup_last_run) > websocket_cleanup_interval_ms) {
    Serial.println(motor_status_to_json());

    ws.cleanupClients();
    websocket_cleanup_last_run = millis();
    Serial.println("Cleaned up websocket clients");
  }
}
