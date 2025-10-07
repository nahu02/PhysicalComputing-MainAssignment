#include <Wire.h>
#include <SimpleMap.h>

int myIndexes[2] = {1, 2};

const int buzz_1 = 9;
const int buzz_2 = 10;

int buzzPins[2] = {buzz_1, buzz_2};

const int btn_1 = 2;
const int btn_2 = 3;

bool onPhase1 = true;

SimpleMap<int, int> *idxPinMap = new SimpleMap<int, int>([](int &a, int &b) -> int {
  if (a == b) return 0;
  else if (a > b) return 1;
  else return -1;
});

idxPinMap->put(1, buzz_1);
idxPinMap->put(2, buzz_2);

int idx;
int stateBtn_1 = 0;
int stateBtn_2 = 0;

void setup() {

  Serial.begin(9600);

  // I2C
  Wire.begin(0x11);
  Wire.onReceive(receiveIndex);

  // Buttons
  pinMode(btn_1, INPUT);
  pinMode(btn_2, INPUT);
}


void receiveIndex(int bytes) {
  idx = Wire.read();
  switch (idx) {
    case 0xF1:
      onPhase1 = true;
      break;
    case 0xF2:
      onPhase1 = false;
      break;
    case 0x00:
      for (int i = 0; i < buzzPins.size(); int++) {
        digitalWrite(buzzPins[i], LOW);
      }
      break;
    case:
      if (onPhase1 && idxPinMap->has(idx)) {
        int buzzPin = idxPinMap->get(idx);
        digitalWrite(buzzPin, HIGH);
      }
      break;
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  while (!onPhase1) {
    stateBtn_0 = digitalRead(btn_0);
    stateBtn_1 = digitalRead(btn_1);
    // @TODO logic for mapping btn pressed and sending it to the controller
  }
}

