#include <Wire.h>


int I2C_SELF_ADDRESS = 0x11;

int myIndexes[2] = {1, 2};

const int btn_1 = 2;
const int btn_2 = 3;

bool onPhase1 = true;

struct Actuator {
  public:
    int id;
    int pin;
};



Actuator actuators[2];

int directive;
int stateBtn_1 = 0;
int stateBtn_2 = 0;

void setup() {

  Serial.begin(9600);

  // I2C
  TWAR = (I2C_SELF_ADDRESS << 1) | 1;  // enable broadcasts to be received
  Wire.begin(I2C_SELF_ADDRESS);
  Wire.onReceive(receiveIndex);

  // Buttons
  pinMode(btn_1, INPUT);
  pinMode(btn_2, INPUT);

  Actuator ACTUATOR1, ACTUATOR2;
  ACTUATOR1.id = 1;
  ACTUATOR2.id = 2;
  ACTUATOR1.pin = 9;
  ACTUATOR2.pin = 10;

  actuators[0] = ACTUATOR1;
  actuators[1] = ACTUATOR2;
}


void receiveIndex(int bytes) {
  directive = Wire.read();
  switch (directive) {
    case 0xF1:
      onPhase1 = true;
      break;
    case 0xF2:
      onPhase1 = false;
      break;
    case 0x00:
      for (auto act : actuators) {
        digitalWrite(act.pin, LOW);
      }
      break;
    default:
      if (onPhase1) {
        for (auto act : actuators) {
          if (act.id == directive) {
            digitalWrite(act.pin, HIGH);
          }
        }
      }
      break;
  }
}

void loop() {

  while (!onPhase1) {
    stateBtn_1 = digitalRead(btn_1);
    stateBtn_2 = digitalRead(btn_2);
    // @TODO logic for mapping btn pressed and sending it to the controller
  }
}

