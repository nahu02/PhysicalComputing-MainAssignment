
bool isPhase1 = true;
uint8_t directive;
uint8_t mostRecentButtonPress = 0x00;


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
