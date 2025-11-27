#include <Wire.h>

// Actuator/button pair data structure
struct ActuSensorator {
  public:
    uint8_t id; // Global index of the actuator
    int actuatorPin;
    int buttonPin;
    int lastBtnState; // Tracking for edge detection
};

const uint8_t I2C_SELF_ADDRESS = 0x11;

// Actuator/button pairs this agent controls
ActuSensorator nodes[4] = {
  {0x05, 10, 5},  // Actuator 0x05 on digi pin 10, button on pin 5
  {0x06, 11, 6},  // Actuator 0x06 on digi pin 11, button on pin 6
  {0x07, 12, 7},  // Actuator 0x07 on digi pin 12, button on pin 7
  {0x08, 13, 8}   // Actuator 0x08 on digi pin 13, button on pin 8
};

#include "shared.h"