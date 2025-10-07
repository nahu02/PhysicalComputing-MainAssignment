// ESP Main

int ACTUATOR_ACTIVE_LENGTH_MS = 500;
// list of all global addresses that exist. (A global address corresponds to an actuator and a correspoonding a button)
int GLOBAL_INDEXES[] = {1, 2, 3, 4};
// list of I2C agent addresses
int I2C_AGENTS[] = {0x11, 0x12};  // placeholder

// active pattern array
int activePattern[];

// current index
int currIdx = 0;


// phase 0: set up I2C, generate pattern

// phase 1: send out phase 1 signal; send indexes one-by-one, sending a stop signal after each.

// phase 2: poll each agent, whenever they have anything compare it against pattern. 

// phase 3: report "WIN" or "LOOSE"


void setup() {
  Serial.begin(9600);
}

void loop() {
}
