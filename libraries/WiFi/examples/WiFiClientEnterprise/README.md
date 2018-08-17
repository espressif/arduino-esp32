# ESP32-Eduroam
* Eduroam wifi connection with university login identity
* Working under Eduroam networks under FreeRadius service for 802.1x
* Eduroam is using methods: PEAP + MsCHAPv2

# Format
* IDENTITY = nickname@university.domain
* PASSWORD = your password

# Usage
* Change IDENTITY
* Change password
* Upload sketch and enjoy!
* After sucessful assign of IP address, board will connect to HTTP page on internet to verify your authentification

# Tested locations
* Technical University in Košice (Slovakia) - Working
* Slovak Technical University in Bratislava (Slovakia) - Working
* University of Antwerp (Belgium) - Working
* UPV Universitat Politècnica de València (Spain) - Working
* Local RADIUS network under Linux Zeroshell - Unsuccessful connection

# Sucessful connection example
 ![alt text](https://i.nahraj.to/f/24Kc.png)
# Unsucessful connection example
 ![alt text](https://camo.githubusercontent.com/87e47d1b27f4e8ace87423e40e8edbce7983bafa/68747470733a2f2f692e6e616872616a2e746f2f662f323435572e504e47)

# Boards
* Tested Under ESP32 Devkit v1
