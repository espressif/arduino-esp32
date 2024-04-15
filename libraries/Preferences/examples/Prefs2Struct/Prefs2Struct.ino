/*
This example shows how to use Preferences (nvs) to store a
structure.  Note that the maximum size of a putBytes is 496K
or 97% of the nvs partition size.  nvs has significant overhead,
so should not be used for data that will change often.
*/
#include <Preferences.h>
Preferences prefs;

typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t setting1;
  uint8_t setting2;
} schedule_t;

void setup() {
  Serial.begin(115200);
  prefs.begin("schedule");                                // use "schedule" namespace
  uint8_t content[] = { 9, 30, 235, 255, 20, 15, 0, 1 };  // two entries
  prefs.putBytes("schedule", content, sizeof(content));
  size_t schLen = prefs.getBytesLength("schedule");
  char buffer[schLen];  // prepare a buffer for the data
  prefs.getBytes("schedule", buffer, schLen);
  if (schLen % sizeof(schedule_t)) {  // simple check that data fits
    log_e("Data is not correct size!");
    return;
  }
  schedule_t *schedule = (schedule_t *)buffer;  // cast the bytes into a struct ptr
  Serial.printf("%02d:%02d %d/%d\n",
                schedule[1].hour, schedule[1].minute,
                schedule[1].setting1, schedule[1].setting2);
  schedule[2] = { 8, 30, 20, 21 };  // add a third entry (unsafely)
                                    // force the struct array into a byte array
  prefs.putBytes("schedule", schedule, 3 * sizeof(schedule_t));
  schLen = prefs.getBytesLength("schedule");
  char buffer2[schLen];
  prefs.getBytes("schedule", buffer2, schLen);
  for (int x = 0; x < schLen; x++) Serial.printf("%02X ", buffer[x]);
  Serial.println();
}

void loop() {}
