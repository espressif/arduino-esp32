# This python script listens on UDP port 3333 
# for messages from the ESP32 board and prints them
import socket
import sys

try :
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
except socket.error, msg :
    print 'Failed to create socket. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
    sys.exit()

try:
    s.bind(('', 3333))
except socket.error , msg:
    print 'Bind failed. Error: ' + str(msg[0]) + ': ' + msg[1]
    sys.exit()
     
print 'Server listening'

while 1:
    d = s.recvfrom(1024)
    data = d[0]
     
    if not data: 
        break
    
    print data.strip()
    
s.close()