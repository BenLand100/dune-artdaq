import socket
import time
import struct
import sys

destAddr = "127.0.0.1"
destPort = 8989
#message = "Hello, world"

try:
    numFrags = int(sys.argv[1]) if len(sys.argv) > 1 else 1
except ValueError:
    print "Non-integer argument specified for number of fragments:", sys.argv[1]
    sys.exit(1)
    
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

numSamples = 40
startSample = 1000
data = range(startSample, startSample+numSamples)
packFormat = '<' + str(numSamples) + 'H'

message = struct.pack(packFormat, *data)

for count in range(numFrags):
    sock.sendto(message, (destAddr, destPort))
    time.sleep(0.1)
