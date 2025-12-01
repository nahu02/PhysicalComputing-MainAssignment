#include <Wire.h>

// Actuator/button pair data structure
struct ActuSensorator {
  public:
    uint8_t id; // Global index of the actuator
    int actuatorPin;
    int buttonPin;
    int lastBtnState; // Tracking for edge detection
};

const uint8_t I2C_SELF_ADDRESS = 0x10;

// Actuator/button pairs this agent controls
ActuSensorator nodes[6] = {
  {0x01, 10, 5},  // Actuator 0x01 on digi pin 10, button on digi pin 5
  {0x02, 11, 6},  // Actuator 0x02 on digi pin 11, button on digi pin 6
  {0x03, 12, 7},  // Actuator 0x03 on digi pin 12, button on digi pin 7
  {0x04, 13, 8},  // Actuator 0x04 on digi pin 13, button on digi pin 8
  {0x05, A2, -1}, // Actuator 0x05: Red LED on analog pin A2
  {0x06, A3, -1}  // Actuator 0x06: Green LED on analog pin A3
};

#include "shared.h"