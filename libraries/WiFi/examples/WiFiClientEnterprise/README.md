# ESP32-Eduroam
* Eduroam wifi connection with university login identity
* Working under Eduroam networks worldwide
* Methods: PEAP + MsCHAPv2

# Format
* IDENTITY = youridentity   --> if connecting from different university, use youridentity@youruniversity.domain format
* PASSWORD = yourpassword

# Usage
* Change IDENTITY
* Change password
* Upload sketch and enjoy!
* After sucessful assign of IP address, board will connect to HTTP page on internet to verify your authentification
* Board will auto reconnect to Eduroam if it lost connection

# Tested locations
|University|Board|Method|Result|
|-------------|-------------| -----|------|
|Technical University in Košice (Slovakia)|ESP32 Devkit v1|PEAP + MsCHAPv2|Working|
|Technical University in Košice (Slovakia)|ESP32 Devmodule v4|PEAP + MsCHAPv2|Working on 6th attempt in loop|
|Slovak Technical University in Bratislava (Slovakia)|ESP32 Devkit v1|PEAP + MsCHAPv2|Working|
|University of Antwerp (Belgium)|Lolin32|PEAP + MsCHAPv2|Working|
|UPV Universitat Politècnica de València (Spain)|ESP32 Devmodule v4|PEAP + MsCHAPv2|Working|
|Local Zeroshell powered network|ESP32 Devkit v1|PEAP + MsCHAPv2|*Not working*|
|Hasselt University (Belgium)|xxx|PEAP + MsCHAPv2|Working with fix sketch|
|Universidad de Granada (Spain)|Lolin D32 Pro|PEAP + MsCHAPv2|Working|
|Universidad de Granada (Spain)|Lolin D32|PEAP + MsCHAPv2|Working|
|Universidade Federal de Santa Catarina (Brazil)|xxx|EAP-TTLS + MsCHAPv2|Working|
|University of Central Florida (Orlando, Florida)|ESP32 Built-in OLED – Heltec WiFi Kit 32|PEAP + MsCHAPv2|Working|
|Université de Montpellier (France)|NodeMCU-32S|PEAP + MsCHAPv2|Working|

# Common errors - Switch to Debug mode for Serial monitor prints
|Error|Appearance|Solution|
|-------------|-------------|-------------|
|wifi: Set status to INIT|Frequent|Hold EN button for few seconds|
|HANDSHAKE_TIMEOUT|Rare|Bug was found under Zeroshell RADIUS authentization - Unsucessful connection|
|AUTH_EXPIRE|Common|In the case of weak wifi network signal, this error is quite common, bring your device closer to AP|
|ASSOC_EXPIRE|Rare|-|
# Sucessful connection example
 ![alt text](https://i.nahraj.to/f/24Kc.png)
# Unsucessful connection example
 ![alt text](https://camo.githubusercontent.com/87e47d1b27f4e8ace87423e40e8edbce7983bafa/68747470733a2f2f692e6e616872616a2e746f2f662f323435572e504e47)
