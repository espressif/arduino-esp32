## Steps to install Arduino ESP32 support on Windows
### Tested on 32 and 64 bit Windows 10 machines

1. Download and install the latest Arduino IDE ```Windows Installer``` from [arduino.cc](https://www.arduino.cc/en/Main/Software)
2. Download and install Git from [git-scm.com](https://git-scm.com/download/win)
3. Start ```Git GUI``` and run through the following steps:
    - Select ```Clone Existing Repository```

        ![Step 1](win-screenshots/win-gui-1.png)

    - Select source and destination
        - Sketchbook Directory: Usually ```C:/Users/[YOUR_USER_NAME]/Documents/Arduino``` and is listed underneath the "Sketchbook location" in Arduino preferences.
        - Source Location: ```https://github.com/espressif/arduino-esp32.git```
        - Target Directory: ```[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32```
        - Click ```Clone``` to start cloning the repository

            ![Step 2](win-screenshots/win-gui-2.png)
            ![Step 3](win-screenshots/win-gui-3.png)
    - open a `Git Bash` session pointing to ```[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32``` and execute ```git submodule update --init --recursive``` 
    - Open ```[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32/tools``` and double-click ```get.exe```

        ![Step 4](win-screenshots/win-gui-4.png)

    - When ```get.exe``` finishes, you should see the following files in the directory

        ![Step 5](win-screenshots/win-gui-5.png)

4. Plug your ESP32 board and wait for the drivers to install (or install manually any that might be required)
5. Start Arduino IDE
6. Select your board in ```Tools > Board``` menu
7. Select the COM port that the board is attached to
8. Compile and upload (You might need to hold the boot button while uploading)

    ![Arduino IDE Example](win-screenshots/arduino-ide.png)

### How to update to the latest code

1. Start ```Git GUI``` and you should see the repository under ```Open Recent Repository```. Click on it!

    ![Update Step 1](win-screenshots/win-gui-update-1.png)

2. From menu ```Remote``` select ```Fetch from``` > ```origin```

    ![Update Step 2](win-screenshots/win-gui-update-2.png)

3. Wait for git to pull any changes and close ```Git GUI```
4. Open ```[ARDUINO_SKETCHBOOK_DIR]/hardware/espressif/esp32/tools``` and double-click ```get.exe```

    ![Step 4](win-screenshots/win-gui-4.png)
