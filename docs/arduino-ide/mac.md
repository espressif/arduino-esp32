Installation instructions for Mac OS
=====================================

- Install latest Arduino IDE from [arduino.cc](https://www.arduino.cc/en/Main/Software)
- Open Terminal and execute the following command (copy->paste and hit enter):

  ```bash
  mkdir -p ~/Documents/Arduino/hardware/espressif && \
  cd ~/Documents/Arduino/hardware/espressif && \
  git clone https://github.com/espressif/arduino-esp32.git esp32 --depth 1 && \
  cd esp32 && \
  git submodule update --init --recursive --depth 1 && \
  cd tools && \
  python get.py 
  ```
  Where `~/Documents/Arduino` represents your sketch book location as per "Arduino" > "Preferences" > "Sketchbook location" (in the IDE once started). Adjust the command above accordingly if necessary!
  
- If you get the error below. Install the command line dev tools with xcode-select --install and try the command above again:
  
  ```xcrun: error: invalid active developer path (/Library/Developer/CommandLineTools), missing xcrun at: /Library/Developer/CommandLineTools/usr/bin/xcrun```
  
  ```xcode-select --install```
  
- Try `python3` instead of `python` if you get the error: `IOError: [Errno socket error] [SSL: TLSV1_ALERT_PROTOCOL_VERSION] tlsv1 alert protocol version (_ssl.c:590)` when running `python get.py`

- If you get the following error when running `python get.py` urllib.error.URLError: <urlopen error SSL: CERTIFICATE_VERIFY_FAILED, go to Macintosh HD > Applications > Python3.6 folder (or any other python version), and run the following scripts: Install Certificates.command and Update Shell Profile.command

- Restart Arduino IDE

