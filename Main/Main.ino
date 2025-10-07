// ESP Main

// list of what global addresses exist
int GLOBAL_INDEXES[] = {1, 2, 3, 4};

// active pattern array
// current index

// list of I2C agent addresses

// phase 0: set up I2C, generate pattern

// phase 1: send out phase 1 signal; send indexes one-by-one, sending a stop signal after each.

// phase 2: poll each agent, whenever they have anything compare it against pattern. 


void setup() {
  Serial.begin(9600);
}

void loop() {
}
