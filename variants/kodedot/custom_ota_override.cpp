// custom_ota_override.cpp
// This function overrides the weak definition of `verifyRollbackLater()` in the kode dot board.

extern "C" {
// Declare the weak function symbol to override it
bool verifyRollbackLater() __attribute__((weak));
}

// Custom implementation of verifyRollbackLater()
// Returning `true` prevents the OTA image from being automatically marked as valid.
// This ensures that the system will roll back to the previous image unless it is explicitly validated later.
bool verifyRollbackLater() {
  return true;
}
