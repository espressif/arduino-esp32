// macro SET_TIME_BEFORE_STARTING_SKETCH_MS(time_ms) can set a time in milliseconds
// before the sketch would start its execution. It gives the user time to open the Serial Monitor

// This will force the Sketch execution to wait for 5 seconds before starting it execution
// setup() will be executed only after this time
SET_TIME_BEFORE_STARTING_SKETCH_MS(5000);

void setup() {
  Serial.begin(115200);
  Serial.println("After 5 seconds... this message will be seen in the Serial Monitor.");
}

void loop() {}
