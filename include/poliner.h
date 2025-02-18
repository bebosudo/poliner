struct MotorStatus {
  bool motor_turning = false;
  bool turning_direction_up = false;
  bool upper_limit_switch_pressed = false;
  bool lower_limit_switch_pressed = false;
};

// Make sure the addresses are the same between all devices talking.
const byte nrf24l01_radio_addresses[][6] = {"00001", "00002"};
