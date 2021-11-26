echo "Using device /dev/ttyUSB$1"
stty -F /dev/ttyUSB$1 raw 115200
cat /dev/ttyUSB$1
