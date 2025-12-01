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
  {0x01, 13, 5},
  {0x02, 12, 6},
  {0x03, 11, 7},
  {0x04, 10, 8},
  {0x05, A2, -1},
  {0x06, A3, -1} 
};

#include "shared.h"