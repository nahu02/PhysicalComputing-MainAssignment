// ESP32 C3 Main - Simon Says Game Master

#include <Wire.h>

// Configuration
const int ACTUATOR_ACTIVE_LENGTH_MS = 500;
const int INTER_ELEMENT_DELAY_MS = 300;
const int PHASE_TRANSITION_DELAY_MS = 100;
const int I2C_BROADCAST_ADDR = 0x00;

// I2C Protocol Messages
const byte MSG_STOP = 0x00;
const byte MSG_PHASE1 = 0xF1;
const byte MSG_PHASE2 = 0xF2;

// Global indexes that exist (actuator/button pairs)
const int GLOBAL_INDEXES[] = {1, 2, 3, 4};
const int NUM_GLOBAL_INDEXES = 4;

// I2C agent addresses
const int I2C_AGENTS[] = {0x11, 0x12};
const int NUM_AGENTS = 2;

// Game state
int activePattern[6];  // Max pattern length is 6
int patternLength = 0;
int currentPhase = 0;  // 0=setup, 1=phase1, 2=phase2, 3=phase3
int patternIndex = 0;  // Current position in pattern during phase 2

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(50); // Wait for serial port to connect
  }
  
  Serial.println("Simon Says - Main Controller");
  Serial.println("============================");
  
  // Initialize I2C as main
  Wire.begin();
  Serial.println("I2C initialized as main");
  
  // Generate random pattern
  generatePattern();
  
  // Small delay before starting
  delay(1000);
  
  // Move to Phase 1
  currentPhase = 1;
}

void loop() {
  if (currentPhase == 1) {
    executePhase1();
    currentPhase = 2;
  } 
  else if (currentPhase == 2) {
    executePhase2();
    currentPhase = 3;
  }
  else if (currentPhase == 3) {
    executePhase3();
    currentPhase = 4; // Done
  }
  else {
    // Game finished, do nothing
    delay(1000);
  }
}

// Generate random pattern of length 3-6
void generatePattern() {
  patternLength = random(3, 7); // Random between 3 and 6 inclusive
  
  Serial.print("Generated pattern of length ");
  Serial.print(patternLength);
  Serial.print(": ");
  
  for (int i = 0; i < patternLength; i++) {
    activePattern[i] = GLOBAL_INDEXES[random(0, NUM_GLOBAL_INDEXES)];
    Serial.print(activePattern[i]);
    if (i < patternLength - 1) Serial.print(", ");
  }
  Serial.println();
}

// Broadcast a single byte to all agents
void broadcastByte(byte data) {
  Wire.beginTransmission(I2C_BROADCAST_ADDR);
  Wire.write(data);
  Wire.endTransmission();
}

// Phase 1: Broadcast the pattern
void executePhase1() {
  Serial.println("\n=== PHASE 1: Broadcasting Pattern ===");
  
  // Send phase 1 start signal
  broadcastByte(MSG_PHASE1);
  Serial.println("Sent: 0xF1 (PHASE1)");
  delay(100);
  
  // Send each pattern element
  for (int i = 0; i < patternLength; i++) {
    // Send the global index as a byte (0x01, 0x02, 0x03, 0x04)
    byte indexByte = (byte)activePattern[i];
    broadcastByte(indexByte);
    Serial.print("Sent: 0x0");
    Serial.print(activePattern[i]);
    Serial.print(" (Activate index ");
    Serial.print(activePattern[i]);
    Serial.println(")");
    
    // Wait for actuator to be active
    delay(ACTUATOR_ACTIVE_LENGTH_MS);
    
    // Send stop signal
    broadcastByte(MSG_STOP);
    Serial.println("Sent: 0x00 (STOP)");
    
    // Inter-element delay (except after last element)
    if (i < patternLength - 1) {
      delay(INTER_ELEMENT_DELAY_MS);
    }
  }
  
  // Phase transition delay
  delay(PHASE_TRANSITION_DELAY_MS);
  
  Serial.println("Phase 1 complete");
}

// Phase 2: Poll agents for button presses
void executePhase2() {
  Serial.println("\n=== PHASE 2: Polling for Input ===");
  
  // Send phase 2 start signal
  broadcastByte(MSG_PHASE2);
  Serial.println("Sent: 0xF2 (PHASE2)");
  delay(100);
  
  patternIndex = 0;
  bool patternComplete = false;
  bool patternFailed = false;
  
  // Poll agents until pattern is complete or fails
  while (!patternComplete && !patternFailed) {
    // Poll each agent
    for (int i = 0; i < NUM_AGENTS; i++) {
      int agentAddr = I2C_AGENTS[i];
      
      // Request 1 byte from agent
      int bytesReceived = Wire.requestFrom(agentAddr, 1);
      
      if (bytesReceived > 0) {
        byte receivedByte = Wire.read();
        
        // Check if agent reported a button press (non-zero value)
        if (receivedByte > 0x00 && receivedByte <= 0x0F) {  // Valid button press (1-15)
          int receivedIndex = (int)receivedByte;
          
          Serial.print("Agent 0x");
          Serial.print(agentAddr, HEX);
          Serial.print(" reported: 0x0");
          Serial.print(receivedIndex);
          Serial.print(" (index ");
          Serial.print(receivedIndex);
          Serial.println(")");
          
          // Compare against expected pattern
          if (receivedIndex == activePattern[patternIndex]) {
            Serial.print("  Correct! (Expected ");
            Serial.print(activePattern[patternIndex]);
            Serial.println(")");
            patternIndex++;
            
            // Check if pattern is complete
            if (patternIndex >= patternLength) {
              patternComplete = true;
              break;
            }
          } else {
            Serial.print("  Wrong! (Expected ");
            Serial.print(activePattern[patternIndex]);
            Serial.println(")");
            patternFailed = true;
            break;
          }
        }
      }
    }
    
    // Small delay between polling cycles
    delay(50);
  }
  
  Serial.println("Phase 2 complete");
}

// Phase 3: Report results
void executePhase3() {
  Serial.println("\n=== PHASE 3: Results ===");
  
  if (patternIndex >= patternLength) {
    Serial.println("!!!!!! WIN !!!!!!");
    Serial.println("All pattern elements matched correctly!");
  } else {
    Serial.println("XXXXXX LOSE XXXXXX");
    Serial.print("Pattern failed at position ");
    Serial.print(patternIndex + 1);
    Serial.print(" of ");
    Serial.println(patternLength);
  }
  
  Serial.println("\nGame Over");
  Serial.println("============================");
}
