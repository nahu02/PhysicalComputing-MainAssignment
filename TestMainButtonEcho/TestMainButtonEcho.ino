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
  WiFi.softAP("TactileSimonDebug");
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
