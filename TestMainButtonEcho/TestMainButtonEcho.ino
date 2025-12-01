// ESP32 C3 Main - Button Echo Test

#include <Wire.h>

// Comment out the define to disable WebSerial and Wifi completely
#define DEBUG

#ifdef DEBUG
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <MycilaWebSerial.h>
#include <DNSServer.h>

DNSServer dnsServer;
AsyncWebServer server(80);
WebSerial webSerial;
#else
class DummySerial
{
public:
  template <typename T>
  void print(T) {}
  template <typename T, typename U>
  void print(T, U) {}
  template <typename T>
  void println(T) {}
  template <typename T, typename U>
  void println(T, U) {}
  void println() {}
};
DummySerial webSerial;
#endif

// I2C Protocol Messages
const uint8_t MSG_STOP = 0x00;
const uint8_t MSG_PHASE2 = 0xF2;

// Test timing constants
const unsigned long ACTIVATION_DURATION_MS = 1000;
const unsigned long INPUT_POLL_INTERVAL_MS = 20;

// Agent addresses
const int AGENT_ADDRESSES[] = {0x10, 0x11};
const int NUM_AGENTS = 2;

// Mapping from global ID to ATmega local pins (actuatorPin, buttonPin)
// Based on agent_a.ino and agent_b.ino configurations
// TODO: THIS IS DIRTY HARD-CODED DATA FOR TESTING ONLY, MAKE SURE IT'S UP-TO-DATE BEFORE USE
struct NodePinMapping {
  uint8_t globalId;
  int actuatorPin;
  int buttonPin;
  const char* agentName;
};

const NodePinMapping NODE_MAPPINGS[] = {
  // Agent A (0x10) - nodes
  {0x01, 13,  5, "Agent A"},
  {0x02, 12,  6, "Agent A"},
  {0x03, 11,  7, "Agent A"},
  {0x04, 10,  8, "Agent A"},
  {0x05, 16, -1, "Agent A"},  // A2
  {0x06, 17, -1, "Agent A"},  // A3
  // Agent B (0x11) - nodes
  {0x07, 13,  5, "Agent B"},
  {0x08, 12,  6, "Agent B"},
  {0x09, 11,  7, "Agent B"},
  {0x0A, 10,  8, "Agent B"},
  {0x0B, 16, -1, "Agent B"},  // A2
  {0x0C, 17, -1, "Agent B"},  // A3
};
const int NUM_NODE_MAPPINGS = sizeof(NODE_MAPPINGS) / sizeof(NODE_MAPPINGS[0]);

// Get pin mapping for a global ID, returns nullptr if not found
const NodePinMapping* getPinMapping(uint8_t globalId) {
  for (int i = 0; i < NUM_NODE_MAPPINGS; i++) {
    if (NODE_MAPPINGS[i].globalId == globalId) {
      return &NODE_MAPPINGS[i];
    }
  }
  return nullptr;
}

// Log the ATmega local pins for a given global ID
void logLocalPins(uint8_t globalId) {
  const NodePinMapping* mapping = getPinMapping(globalId);
  if (mapping) {
    webSerial.print("  -> ");
    webSerial.print(mapping->agentName);
    webSerial.print(" local pins: actuator=");
    webSerial.print(mapping->actuatorPin);
    webSerial.print(", button=");
    if (mapping->buttonPin >= 0) {
      webSerial.println(mapping->buttonPin);
    } else {
      webSerial.println("N/A");
    }
  }
}

// Activation tracking
bool activationActive = false;
unsigned long activationStartTime = 0;
uint8_t activeIndex = 0x00;
int activeAgentAddr = 0;

// Polling timing
unsigned long lastPollTime = 0;

// Broadcast a single byte to all agents
void broadcastByte(uint8_t data)
{
  Wire.beginTransmission(0); // Broadcast address
  Wire.write(data);
  Wire.endTransmission();
}

// Send a byte to a specific agent
void sendToAgent(int agentAddr, uint8_t data)
{
  Wire.beginTransmission(agentAddr);
  Wire.write(data);
  Wire.endTransmission();
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(50);
  }

  // Web debug setup
#ifdef DEBUG
  WiFi.softAP("TactileSimonButtonEcho");
  webSerial.onMessage([](const std::string &msg)
                      { Serial.println(msg.c_str()); });
  webSerial.begin(&server);
  webSerial.setBuffer(100);
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->redirect("/webserial"); });
  server.begin();
  dnsServer.start(53, "*", WiFi.softAPIP());
#endif

  delay(50);

  webSerial.println("Button Echo Test - Main Controller");
  webSerial.println("==================================");

  // Initialize I2C as main
  Wire.begin();
  webSerial.println("I2C initialized as main");

  // Check that all I2C agents are reachable
  bool errorPresent;
  do
  {
    errorPresent = false;
    for (int i = 0; i < NUM_AGENTS; i++)
    {
      int agent = AGENT_ADDRESSES[i];
      Wire.beginTransmission(agent);
      uint8_t error = Wire.endTransmission();
      if (error == 0)
      {
        webSerial.print("I2C agent 0x");
        webSerial.print(agent, HEX);
        webSerial.println(" is reachable");
      }
      else
      {
        errorPresent = true;
        webSerial.print("I2C agent 0x");
        webSerial.print(agent, HEX);
        webSerial.println(" is NOT reachable");
      }
    }
    if (errorPresent)
      delay(500);
  } while (errorPresent);

  // Send Phase 2 signal to all agents
  broadcastByte(MSG_PHASE2);
  webSerial.println("\nPhase 2 signal sent to all agents");
  webSerial.println("Ready to echo button presses...\n");

  delay(500);
}

void loop()
{
  // Handle active activation timeout
  if (activationActive && (millis() - activationStartTime >= ACTIVATION_DURATION_MS))
  {
    sendToAgent(activeAgentAddr, MSG_STOP);
    webSerial.print("Deactivated index 0x");
    webSerial.println(activeIndex, HEX);
    activationActive = false;
  }

  // Rate-limit I2C polling
  if (millis() - lastPollTime < INPUT_POLL_INTERVAL_MS)
  {
    return;
  }
  lastPollTime = millis();

  // Poll all agents for button presses
  for (int i = 0; i < NUM_AGENTS; i++)
  {
    int agentAddr = AGENT_ADDRESSES[i];
    int bytesReceived = Wire.requestFrom(agentAddr, 1);

    if (bytesReceived > 0)
    {
      uint8_t receivedByte = Wire.read();

      // Check if agent reported a button press (non-zero, valid range)
      if (receivedByte > 0x00 && receivedByte <= 0x0F)
      {
        webSerial.print("Button detected from agent 0x");
        webSerial.print(agentAddr, HEX);
        webSerial.print(": index 0x");
        webSerial.print(receivedByte, HEX);
        webSerial.println(" - Activating...");
        logLocalPins(receivedByte);

        // Determine which agent controls this index
        // Assuming Player A (0x10) = indexes 1-6, Player B (0x11) = indexes 7-12
        int targetAgent;
        if (receivedByte >= 0x01 && receivedByte <= 0x06)
        {
          targetAgent = AGENT_ADDRESSES[0]; // 0x10
        }
        else if (receivedByte >= 0x07 && receivedByte <= 0x0C)
        {
          targetAgent = AGENT_ADDRESSES[1]; // 0x11
        }
        else
        {
          targetAgent = agentAddr; // Default to reporting agent
        }

        // Activate the index
        sendToAgent(targetAgent, receivedByte);
        activationActive = true;
        activationStartTime = millis();
        activeIndex = receivedByte;
        activeAgentAddr = targetAgent;
      }
    }
  }
}
