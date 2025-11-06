#include <Wire.h>


const uint8_t I2C_SELF_ADDRESS = 0x11;

// Actuator/button pair data structure
struct ActuSensorator {
  public:
    uint8_t id; // Global index of the actuator
    int actuatorPin;
    int buttonPin;
    int lastBtnState; // Tracking for edge detection
};

// Actuator/button pairs this agent controls
ActuSensorator nodes[4] = {
  {0x05, 5, 6},  // Actuator 0x05 on digi pin 5, button on pin 6
  {0x06, 7, 8},  // Actuator 0x06 on digi pin 7, button on pin 8
  {0x07, 9, 10}, // Actuator 0x07 on digi pin 9, button on pin 10
  {0x08, 11, 12} // Actuator 0x08 on digi pin 11, button on pin 12 
};

bool isPhase1 = true;
uint8_t directive;
uint8_t mostRecentButtonPress = 0x00;

void setup() {
  // I2C
  Wire.begin(I2C_SELF_ADDRESS);
  TWAR = (I2C_SELF_ADDRESS << 1) | 1;  // enable broadcasts to be received
  Wire.onReceive(receiveIndex);
  Wire.onRequest(sendButtonPress);

  // Pins
  for (auto node : nodes) {
    pinMode(node.buttonPin, INPUT);
    pinMode(node.actuatorPin, OUTPUT);
    digitalWrite(node.actuatorPin, LOW);
  }
}


void receiveIndex(int bytes) {
  directive = Wire.read();
  switch (directive) {
    case 0xF1:
      isPhase1 = true;
      mostRecentButtonPress = 0x00;
      break;
    case 0xF2:
      isPhase1 = false;
      mostRecentButtonPress = 0x00;
      break;
    case 0x00:
      for (auto node : nodes) {
        digitalWrite(node.actuatorPin, LOW);
      }
      break;
    default:
      if (isPhase1) {
        for (auto node : nodes) {
          if (node.id == directive) {
            digitalWrite(node.actuatorPin, HIGH);
          }
        }
      }
      break;
  }
}

void sendButtonPress() {
  Wire.write((byte)mostRecentButtonPress);
  mostRecentButtonPress = 0x00;
}

void loop() {

  // Phase 2: Read button states
  if (!isPhase1) {
    for (auto& node : nodes) {
      int buttonState = digitalRead(node.buttonPin);
      // Edge detection: only trigger on button press
      if (buttonState == HIGH && node.lastBtnState == LOW) {
        mostRecentButtonPress = node.id;
        delay(50);
      }
      node.lastBtnState = buttonState;
    }
  }
}

