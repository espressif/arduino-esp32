void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println("Hello Arduino!");
}

void loop() {}
