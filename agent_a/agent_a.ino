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

// red A2
// green A3

// Actuator/button pairs this agent controls
ActuSensorator nodes[4] = {
  {0x01, 10, 5},  // Actuator 0x01 on digi pin 10, button on pin 5
  {0x02, 11, 6},  // Actuator 0x02 on digi pin 11, button on pin 6
  {0x03, 12, 7},  // Actuator 0x03 on digi pin 12, button on pin 7
  {0x04, 13, 8}   // Actuator 0x04 on digi pin 13, button on pin 8
};

#include "shared.h"