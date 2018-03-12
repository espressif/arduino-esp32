Installation instructions for Debian / Ubuntu OS
=================================================

- Install latest Arduino IDE from [arduino.cc](https://www.arduino.cc/en/Main/Software)
- Open Terminal and execute the following command (copy->paste and hit enter):

  ```bash
  sudo usermod -a -G dialout $USER && \
  sudo apt-get install git && \
  wget https://bootstrap.pypa.io/get-pip.py && \
  sudo python get-pip.py && \
  sudo pip install pyserial && \
  mkdir -p ~/Arduino/hardware/espressif && \
  cd ~/Arduino/hardware/espressif && \
  git clone https://github.com/espressif/arduino-esp32.git esp32 && \
  cd esp32 && \
  git submodule update --init --recursive && \
  cd tools && \
  python2 get.py
  ```
- Restart Arduino IDE



- If you have Arduino.app installed to /Applications/, modify the installation as follows, beginning at `mkdir -p ~/Arduino...`:

```bash
  cd /Applications/Arduino_*/Contents/java/hardware/
  mkdir -p espressif && \
  cd espressif && \
  git clone https://github.com/espressif/arduino-esp32.git esp32 && \
  cd esp32 && \
  git submodule update --init --recursive && \
  cd tools && \
  python2 get.py```
