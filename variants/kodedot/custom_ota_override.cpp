extern "C" bool verifyRollbackLater() {
    // Returning true prevents the OTA image from being marked as valid
    // until you explicitly confirm it after the first boot.
    return true;
}