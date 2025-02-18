// https://howtomechatronics.com/tutorials/arduino/arduino-wireless-communication-nrf24l01-tutorial/

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#include <EasyButton.h>

#include "poliner.h"

#define CE_PIN 10
#define CSN_PIN 9

// Define stepper motor connections and steps per revolution:
#define DIR_PIN 2
#define STEP_PIN 15
// How long the motor step should be in microseconds. Bigger is slower.
#define MOTOR_INTERVAL_US 2000
// Define limit switch pins
#define LOWER_LIMIT_SWITCH_PIN 4
#define UPPER_LIMIT_SWITCH_PIN 5
// How long the limit switch should be pressed for before the motor stops, in milliseconds
#define LIMIT_SWITCH_PRESSED_FOR_DURATION 50

EasyButton lower_limit_switch(LOWER_LIMIT_SWITCH_PIN);
EasyButton upper_limit_switch(UPPER_LIMIT_SWITCH_PIN);

MotorStatus motor_status;

RF24 radio(CE_PIN, CSN_PIN);

// Callback function to be called when the limit switches are pressed.
void onLowerLimitSwitchPressed() {
  Serial.println("lower limit switch pressed, turning off motor");
  motor_status.lower_limit_switch_pressed = true;
  motor_status.motor_turning = false;
  radio.stopListening();
  radio.write(&motor_status, sizeof(MotorStatus));

  Serial.println("Motor status: turning: " + String(motor_status.motor_turning) +
                 ", direction_up: " + String(motor_status.turning_direction_up)
                 + ", upper_pressed: " + String(motor_status.upper_limit_switch_pressed)
                 + ", lower_pressed: " + String(motor_status.lower_limit_switch_pressed));
}
void onUpperLimitSwitchPressed() {
  Serial.println("upper limit switch pressed, turning off motor");
  motor_status.upper_limit_switch_pressed = true;
  motor_status.motor_turning = false;
  radio.stopListening();
  radio.write(&motor_status, sizeof(MotorStatus));

  Serial.println("Motor status: turning: " + String(motor_status.motor_turning) +
                 ", direction_up: " + String(motor_status.turning_direction_up)
                 + ", upper_pressed: " + String(motor_status.upper_limit_switch_pressed)
                 + ", lower_pressed: " + String(motor_status.lower_limit_switch_pressed));
}

// Update clients so that they know that the switch was released
void onLowerLimitSwitchReleased() {
  motor_status.lower_limit_switch_pressed = false;
  radio.stopListening();
  radio.write(&motor_status, sizeof(MotorStatus));
}
void onUpperLimitSwitchReleased() {
  motor_status.upper_limit_switch_pressed = false;
  radio.stopListening();
  radio.write(&motor_status, sizeof(MotorStatus));
}

void setup() {
  // Serial port speed for debugging purposes.
  Serial.begin(115200);

  radio.begin();
  radio.openWritingPipe(nrf24l01_radio_addresses[1]); // 00002
  radio.openReadingPipe(1, nrf24l01_radio_addresses[0]); // 00001
  radio.setPALevel(RF24_PA_MIN);

  pinMode(LED_BUILTIN, OUTPUT); // Use builtin LED pin as output pin
  Serial.println("Ready to send data");
  // Make the onboard LED blip
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);

  // Declare pins as output:
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  // Initialize the limit switches.
  lower_limit_switch.begin();
  upper_limit_switch.begin();
  // Add callback functions to be called when the limit switches are pressed. Use the same function
  // since we just turn off the motor when any switch is pressed.
  lower_limit_switch.onPressedFor(LIMIT_SWITCH_PRESSED_FOR_DURATION, onLowerLimitSwitchPressed);
  upper_limit_switch.onPressedFor(LIMIT_SWITCH_PRESSED_FOR_DURATION, onUpperLimitSwitchPressed);
}

void loop() {
  radio.startListening();
  if (radio.available()) {
    // Here we probably don't want to overwrite the motor status with the new data, but rather just
    // take the motor_turning and turning_direction_up values from the received data, to avoid race
    // conditions where the motor is turned off by the limit switch but the transmitter still thinks
    // it's on.
    radio.read(&motor_status, sizeof(MotorStatus));
    Serial.print("Got new data: motor_turning: ");
    Serial.print(motor_status.motor_turning);
    Serial.print(", turning_direction_up: ");
    Serial.println(motor_status.turning_direction_up);
  }
  // radio.stopListening();

  if (motor_status.motor_turning) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(MOTOR_INTERVAL_US);
    digitalWrite(STEP_PIN, LOW);
    // Serial.println("Motor on");
  }

  // Read the limit switches and call the onPressed function if they are pressed.
  lower_limit_switch.read();
  upper_limit_switch.read();
  if (lower_limit_switch.wasReleased()) {
    onLowerLimitSwitchReleased();
  } else if (upper_limit_switch.wasReleased()) {
    onUpperLimitSwitchReleased();
  }
}
