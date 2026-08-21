# Name,   Type, SubType, Offset,  Size, Flags
# Note: Firmware partition offset needs to be 64K aligned, initial 36K (9 sectors) are reserved for bootloader and partition table
esp_secure_cert,  0x3F, ,0xd000,    0x2000, encrypted
nvs,      data, nvs,     0x10000,   0xC000,
nvs_keys, data, nvs_keys,,          0x1000, encrypted
otadata,  data, ota,     ,          0x2000
phy_init, data, phy,     ,          0x1000,
ota_0,    app,  ota_0,   0x20000,   0x1E0000,
ota_1,    app,  ota_1,   0x200000,  0x1E0000,
fctry,    data, nvs,     0x3E0000,  0x6000
