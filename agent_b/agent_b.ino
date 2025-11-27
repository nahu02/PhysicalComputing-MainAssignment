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
  {0x07, 10, 5},  // Actuator 0x07 on digi pin 10, button on digi pin 5
  {0x08, 11, 6},  // Actuator 0x08 on digi pin 11, button on digi pin 6
  {0x09, 12, 7},  // Actuator 0x09 on digi pin 12, button on digi pin 7
  {0x0A, 13, 8},  // Actuator 0x0A on digi pin 13, button on digi pin 8
  {0x0B, A2, -1}, // Actuator 0x0B: Red LED on analog pin A2
  {0x0C, A3, -1}  // Actuator 0x0C: Green LED on analog pin A3
};

#include "shared.h"