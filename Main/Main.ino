// ESP32 C3 Main - Simon Says Game Master

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
// Dummy class to do nothing when Debug is disabled
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

// Configuration
const int PHASE_TRANSITION_DELAY_MS = 100;
const int PATTERN_LENGTH_MIN = 3;
const int PATTERN_LENGTH_MAX = 10;

// Feedback timing constants
const int FB_SHORT_DURATION_MS = 100;
const int FB_BLINK_ON_MS = 400;
const int FB_BLINK_OFF_MS = 400;
const int FB_BLINK_TOTAL = 5;

// Input timing constants
const unsigned long INPUT_TIMEOUT_MS = 5000;
const unsigned long INPUT_POLL_INTERVAL_MS = 20;

// I2C Protocol Messages
const uint8_t MSG_STOP = 0x00;
const uint8_t MSG_PHASE1 = 0xF1;
const uint8_t MSG_PHASE2 = 0xF2;

// Player configuration
struct PlayerConfig
{
  int i2cAddress;
  const int *actuatorIndexes;
  int numIndexes;
  uint8_t redLED;
  uint8_t greenLED;
};

const PlayerConfig players[2] = {
    {0x10, new int[]{1, 2, 3, 4}, 4, 0x05, 0x06}, // Player A
    {0x11, new int[]{7, 8, 9, 10}, 4, 0x0B, 0x0C} // Player B
};

// Game state machine
enum GameState
{
  INIT,          // Generate pattern, prepare to show
  SHOW_PATTERN,  // Non-blocking pattern display
  AWAIT_INPUT,   // Polling for button presses
  TURN_COMPLETE, // Player succeeded, switch to other player
  GAME_OVER,     // One player failed, showing feedback
  RESTART_DELAY  // Waiting 5s before new game
};
GameState gameState = INIT;

// Polling result enum
enum PollResult
{
  POLL_CONTINUE,
  POLL_PATTERN_COMPLETE,
  POLL_WRONG,
  POLL_TIMEOUT
};

// Difficulty parameters
struct DifficultyParams
{
  int patternLength;
  int actuatorActiveMs;
  int interElementDelayMs;
};

DifficultyParams getDifficulty(int round)
{
  DifficultyParams params;

  // Increasing pattern length
  params.patternLength = 3 + min(round, 6);
  // Decreasing actuator active time
  params.actuatorActiveMs = max(1500 - round * 150, 400);
  // Decreasing inter-element delay
  params.interElementDelayMs = max(500 - round * 60, 150);

  // TODO: Test if this feels good, consider a logarithmic scale

  return params;
}

// Pattern data
uint8_t activePattern[PATTERN_LENGTH_MAX];
int patternLength;
int patternIndex; // Current position during input phase

// Pattern display state
int patternDisplayIndex = 0;
enum PatternDisplayStep
{
  PD_ACTIVATE,
  PD_WAIT_ACTIVE,
  PD_WAIT_DELAY
};
PatternDisplayStep pdStep = PD_ACTIVATE;

// Current difficulty values (set when pattern generated)
int currentActiveMs;
int currentDelayMs;

// Game tracking
int currentPlayer = 0; // 0=A, 1=B
int roundNumber = 0;   // Increments after both players complete
int startingPlayer = 0;
bool playerCompletedThisRound[2] = {false, false};

// Timing
unsigned long stateStartTime = 0;
unsigned long lastPollTime = 0;

// Feedback system (non-blocking)
enum FeedbackType
{
  FB_NONE,
  FB_SHORT_GREEN,
  FB_WIN_LOSE_SEQUENCE
};
FeedbackType feedbackType = FB_NONE;
int feedbackPlayerIndex = 0;
unsigned long feedbackStartTime = 0;
int feedbackBlinkCount = 0;
bool feedbackLedOn = false;

// Generate pattern for specific player at current difficulty
void generatePattern(int playerIndex, int round)
{
  DifficultyParams diff = getDifficulty(round);
  patternLength = diff.patternLength;
  currentActiveMs = diff.actuatorActiveMs;
  currentDelayMs = diff.interElementDelayMs;

  const PlayerConfig &player = players[playerIndex];

  webSerial.print("Player ");
  webSerial.print(playerIndex == 0 ? "A" : "B");
  webSerial.print(" Round ");
  webSerial.print(round);
  webSerial.print(" - Difficulty: len=");
  webSerial.print(patternLength);
  webSerial.print(" activeMs=");
  webSerial.print(currentActiveMs);
  webSerial.print(" delayMs=");
  webSerial.print(currentDelayMs);
  webSerial.print(" Pattern: ");

  for (int i = 0; i < patternLength; i++)
  {
    activePattern[i] = player.actuatorIndexes[random(0, player.numIndexes)];
    webSerial.print(activePattern[i]);
    if (i < patternLength - 1)
      webSerial.print(", ");
  }
  webSerial.println();
}

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

// Start a short green flash for a player (non-blocking)
void startShortGreenFlash(int playerIndex)
{
  feedbackType = FB_SHORT_GREEN;
  feedbackPlayerIndex = playerIndex;
  feedbackStartTime = millis();
  feedbackLedOn = true;
  sendToAgent(players[playerIndex].i2cAddress, players[playerIndex].greenLED);
}

// Start the win/lose blink sequence (non-blocking)
// Loser gets red blinks, winner (other player) gets green blinks
void startWinLoseSequence(int loserIndex)
{
  feedbackType = FB_WIN_LOSE_SEQUENCE;
  feedbackPlayerIndex = loserIndex;
  feedbackStartTime = millis();
  feedbackBlinkCount = 0;
  feedbackLedOn = true;
  // Turn on both LEDs: loser red, winner green
  sendToAgent(players[loserIndex].i2cAddress, players[loserIndex].redLED);
  sendToAgent(players[1 - loserIndex].i2cAddress, players[1 - loserIndex].greenLED);
}

// Update feedback state machine (called every loop iteration)
void updateFeedback()
{
  if (feedbackType == FB_NONE)
    return;

  unsigned long elapsed = millis() - feedbackStartTime;

  if (feedbackType == FB_SHORT_GREEN)
  {
    if (elapsed >= FB_SHORT_DURATION_MS)
    {
      sendToAgent(players[feedbackPlayerIndex].i2cAddress, MSG_STOP);
      feedbackType = FB_NONE;
    }
  }
  else if (feedbackType == FB_WIN_LOSE_SEQUENCE)
  {
    int blinkDuration = feedbackLedOn ? FB_BLINK_ON_MS : FB_BLINK_OFF_MS;
    if (elapsed >= (unsigned long)blinkDuration)
    {
      feedbackStartTime = millis();
      if (feedbackLedOn)
      {
        // Turn off both LEDs
        broadcastByte(MSG_STOP);
        feedbackLedOn = false;
      }
      else
      {
        feedbackBlinkCount++;
        if (feedbackBlinkCount >= FB_BLINK_TOTAL)
        {
          feedbackType = FB_NONE;
        }
        else
        {
          // Turn on both LEDs again
          sendToAgent(players[feedbackPlayerIndex].i2cAddress, players[feedbackPlayerIndex].redLED);
          sendToAgent(players[1 - feedbackPlayerIndex].i2cAddress, players[1 - feedbackPlayerIndex].greenLED);
          feedbackLedOn = true;
        }
      }
    }
  }
}

// Non-blocking pattern display - returns true when no more pattern elements left
bool updatePatternDisplay()
{
  unsigned long elapsed = millis() - stateStartTime;

  switch (pdStep)
  {
  case PD_ACTIVATE:
    sendToAgent(players[currentPlayer].i2cAddress, activePattern[patternDisplayIndex]);
    webSerial.print("Pattern Display - Player ");
    webSerial.print(currentPlayer == 0 ? "A" : "B");
    webSerial.print("  Activate: 0x");
    webSerial.println(activePattern[patternDisplayIndex], HEX);
    stateStartTime = millis();
    pdStep = PD_WAIT_ACTIVE;
    break;

  case PD_WAIT_ACTIVE:
    if (elapsed >= (unsigned long)currentActiveMs)
    {
      sendToAgent(players[currentPlayer].i2cAddress, MSG_STOP);
      if (patternDisplayIndex >= patternLength - 1)
      {
        return true; // Pattern complete
      }
      stateStartTime = millis();
      pdStep = PD_WAIT_DELAY;
    }
    break;

  case PD_WAIT_DELAY:
    if (elapsed >= (unsigned long)currentDelayMs)
    {
      patternDisplayIndex++;
      pdStep = PD_ACTIVATE;
    }
    break;
  }
  return false;
}

// Non-blocking input polling - returns result each call
PollResult updateInputPolling()
{
  // Check timeout
  if (millis() - stateStartTime >= INPUT_TIMEOUT_MS)
  {
    webSerial.println("  TIMEOUT!");
    return POLL_TIMEOUT;
  }

  // Rate-limit I2C polling
  if (millis() - lastPollTime < INPUT_POLL_INTERVAL_MS)
  {
    return POLL_CONTINUE;
  }
  lastPollTime = millis();

  // Poll only current player's agent
  int agentAddr = players[currentPlayer].i2cAddress;
  int bytesReceived = Wire.requestFrom(agentAddr, 1);

  if (bytesReceived > 0)
  {
    uint8_t receivedByte = Wire.read();

    // Check if agent reported a button press (non-zero, valid range)
    if (receivedByte > 0x00 && receivedByte <= 0x0F)
    {
      webSerial.print("  Button press: 0x");
      webSerial.print(receivedByte, HEX);
      webSerial.print(" (expected: 0x");
      webSerial.print(activePattern[patternIndex], HEX);
      webSerial.println(")");

      if (receivedByte == activePattern[patternIndex])
      {
        webSerial.println("  Correct!");
        startShortGreenFlash(currentPlayer);
        patternIndex++;
        stateStartTime = millis(); // Reset timeout for next button

        if (patternIndex >= patternLength)
        {
          return POLL_PATTERN_COMPLETE;
        }
      }
      else
      {
        webSerial.println("  WRONG!");
        return POLL_WRONG;
      }
    }
  }
  return POLL_CONTINUE;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(50); // Wait for serial port to connect
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

  webSerial.println("Simon Says - Main Controller");
  webSerial.println("============================");

  // Initialize I2C as main
  Wire.begin();
  webSerial.println("I2C initialized as main");

  // Check that all I2C agents are reachable
  bool errorPresent;
  do
  {
    errorPresent = false;
    for (int i = 0; i < 2; i++)
    {
      int agent = players[i].i2cAddress;
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
    delay(500);
  } while (errorPresent);

  webSerial.println("\nStarting game...");
  webSerial.print("First player: ");
  webSerial.println(startingPlayer == 0 ? "A" : "B");

  // Small delay before starting
  delay(1000);

  // Game state machine starts in INIT
  gameState = INIT;
}

void loop()
{
  updateFeedback(); // Always update feedback first (non-blocking)

  switch (gameState)
  {
  case INIT:
    generatePattern(currentPlayer, roundNumber);
    patternDisplayIndex = 0;
    pdStep = PD_ACTIVATE;
    broadcastByte(MSG_PHASE1); // Tell all agents: pattern display mode
    stateStartTime = millis();
    gameState = SHOW_PATTERN;
    webSerial.print("\n=== Player ");
    webSerial.print(currentPlayer == 0 ? "A" : "B");
    webSerial.println(" - Showing Pattern ===");
    break;

  case SHOW_PATTERN:
    if (updatePatternDisplay())
    {                            // Pattern display complete
      broadcastByte(MSG_PHASE2); // Tell all agents: input mode
      patternIndex = 0;
      stateStartTime = millis();
      lastPollTime = 0;
      gameState = AWAIT_INPUT;
      webSerial.print("=== Player ");
      webSerial.print(currentPlayer == 0 ? "A" : "B");
      webSerial.println(" - Awaiting Input ===");
    }
    break;

  case AWAIT_INPUT:
  {
    PollResult result = updateInputPolling();
    if (result == POLL_PATTERN_COMPLETE)
    {
      webSerial.print("Player ");
      webSerial.print(currentPlayer == 0 ? "A" : "B");
      webSerial.println(" completed pattern successfully!");
      playerCompletedThisRound[currentPlayer] = true;
      gameState = TURN_COMPLETE;
    }
    else if (result == POLL_WRONG || result == POLL_TIMEOUT)
    {
      webSerial.print("Player ");
      webSerial.print(currentPlayer == 0 ? "A" : "B");
      webSerial.println(" FAILED!");
      startWinLoseSequence(currentPlayer); // currentPlayer is the loser
      stateStartTime = millis();
      gameState = GAME_OVER;
    }
    break;
  }

  case TURN_COMPLETE:
    // Switch to other player
    currentPlayer = 1 - currentPlayer;

    // Check if round complete (both players succeeded)
    if (playerCompletedThisRound[0] && playerCompletedThisRound[1])
    {
      roundNumber++;
      playerCompletedThisRound[0] = false;
      playerCompletedThisRound[1] = false;
      webSerial.print("\n========== ROUND ");
      webSerial.print(roundNumber);
      webSerial.println(" ==========");
    }

    gameState = INIT;
    break;

  case GAME_OVER:
    // Wait for feedback blink sequence to complete
    if (feedbackType == FB_NONE)
    {
      webSerial.println("\n=== GAME OVER ===");
      webSerial.print("Winner: Player ");
      webSerial.println(currentPlayer == 0 ? "B" : "A");
      webSerial.print("Rounds completed: ");
      webSerial.println(roundNumber);
      stateStartTime = millis();
      gameState = RESTART_DELAY;
    }
    break;

  case RESTART_DELAY:
    if (millis() - stateStartTime >= 5000)
    {
      // Reset for new game
      startingPlayer = 1 - startingPlayer; // Alternate who starts
      currentPlayer = startingPlayer;
      roundNumber = 0;
      playerCompletedThisRound[0] = false;
      playerCompletedThisRound[1] = false;
      webSerial.println("\n\n========== NEW GAME ==========");
      webSerial.print("Starting player: ");
      webSerial.println(startingPlayer == 0 ? "A" : "B");
      gameState = INIT;
    }
    break;
  }
}