Installation instructions for Mac OS
=====================================

- Install latest Arduino IDE from [arduino.cc](https://www.arduino.cc/en/Main/Software)
- Open Terminal and execute the following command (copy->paste and hit enter):

  ```bash
  mkdir -p ~/Documents/Arduino/hardware/espressif && \
  cd ~/Documents/Arduino/hardware/espressif && \
  git clone https://github.com/espressif/arduino-esp32.git esp32 && \
  cd esp32 && \
  git submodule update --init --recursive && \
  cd tools && \
  python get.py
  ```
- If you get the error below. Install the command line dev tools with xcode-select --install and try the command above again:
  
```xcrun: error: invalid active developer path (/Library/Developer/CommandLineTools), missing xcrun at: /Library/Developer/CommandLineTools/usr/bin/xcrun```

```xcode-select --install```

- Restart Arduino IDE

