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
ActuSensorator nodes[6] = {
  {0x07, 13, 5},
  {0x08, 12, 6},
  {0x09, 11, 7},
  {0x0A, 10, 8},
  {0x0B, A2, -1},
  {0x0C, A3, -1}
};

#include "shared.h"