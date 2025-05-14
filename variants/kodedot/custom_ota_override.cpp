// custom_ota_override.cpp
// Esta función sobrescribe la definición débil en el core de Arduino-ESP32.
extern "C" {
    bool verifyRollbackLater() __attribute__((weak));
}

bool verifyRollbackLater() {
    // Retorna true para evitar que se marque la imagen OTA como válida automáticamente.
    return true;
}
